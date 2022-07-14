// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-wizard-factory-interface.hpp"

#include "core-action-manager.hpp"
#include "core-document-manager.hpp"
#include "core-feature-provider.hpp"
#include "core-interface.hpp"

#include <extensionsystem/pluginmanager.hpp>
#include <extensionsystem/pluginspec.hpp>

#include <utils/fileutils.hpp>
#include <utils/icon.hpp>
#include <utils/qtcassert.hpp>
#include <utils/wizard.hpp>

#include <QAction>
#include <QPainter>

/*!
    \class Core::IWizardFactory
    \inheaderfile coreplugin/iwizardfactory.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The IWizardFactory class is the base class for all wizard factories.

    \note Instead of using this class, we recommend that you create JSON-based
    wizards, as instructed in \l{https://doc.qt.io/orca/creator-project-wizards.html}
    {Adding New Custom Wizards}.

    The wizard interface is a very thin abstraction for the wizards in
    \uicontrol File > \uicontrol {New File} and \uicontrol{New Project}.
    Basically, it defines what to show to the user in the wizard selection dialogs,
    and a hook that is called if the user selects the wizard.

    Wizards can then perform any operations they like, including showing dialogs and
    creating files. Often it is not necessary to create your own wizard from scratch.
    Use one of the predefined wizards and adapt it to your needs.

    To make your wizard known to the system, add your IWizardFactory instance to the
    plugin manager's object pool in your plugin's initialize function:
    \code
        bool MyPlugin::initialize(const QStringList &arguments, QString *errorString)
        {
            // ... do setup
            addAutoReleasedObject(new MyWizardFactory);
            // ... do more setup
        }
    \endcode

    \sa Core::BaseFileWizardFactory, Core::BaseFileWizard
*/

/*!
    \enum Core::IWizardFactory::WizardKind
    Used to specify what kind of objects the wizard creates. This information is used
    to show e.g. only wizards that create projects when selecting a \uicontrol{New Project}
    menu item.
    \value FileWizard
        The wizard creates one or more files.
    \value ProjectWizard
        The wizard creates a new project.
*/

/*!
    \fn Core::IWizardFactory::WizardKind Core::IWizardFactory::kind() const
    Returns what kind of objects are created by the wizard.
*/

/*!
    \fn QIcon Core::IWizardFactory::icon() const
    Returns an icon to show in the wizard selection dialog.
*/

/*!
    \fn QString Core::IWizardFactory::description() const
    Returns a translated description to show when this wizard is selected
    in the dialog.
*/

/*!
    \fn QString Core::IWizardFactory::displayName() const
    Returns the translated name of the wizard, how it should appear in the
    dialog.
*/

/*!
    \fn QString Core::IWizardFactory::id() const
    Returns an arbitrary id that is used for sorting within the category.
*/

/*!
    \fn QString Core::IWizardFactory::category() const
    Returns a category ID to add the wizard to.
*/

/*!
    \fn QString Core::IWizardFactory::displayCategory() const
    Returns the translated string of the category, how it should appear
    in the dialog.
*/

using namespace Utils;

namespace Orca::Plugin::Core {
namespace {

QList<IFeatureProvider*> s_providerList;
QList<IWizardFactory*> s_allFactories;
QList<IWizardFactory::FactoryCreator> s_factoryCreators;
QAction *s_inspectWizardAction = nullptr;
bool s_areFactoriesLoaded = false;
bool s_isWizardRunning = false;
QWidget *s_currentWizard = nullptr;

// NewItemDialog reopening data:
class NewItemDialogData {
public:
  auto setData(const QString &t, const QList<IWizardFactory*> &f, const FilePath &dl, const QVariantMap &ev) -> void
  {
    QTC_ASSERT(!hasData(), return);
    QTC_ASSERT(!t.isEmpty(), return);
    QTC_ASSERT(!f.isEmpty(), return);

    title = t;
    factories = f;
    default_location = dl;
    extra_variables = ev;
  }

  auto hasData() const -> bool { return !factories.isEmpty(); }

  auto clear() -> void
  {
    title.clear();
    factories.clear();
    default_location.clear();
    extra_variables.clear();
  }

  auto reopen() -> void
  {
    if (!hasData())
      return;

    ICore::showNewItemDialog(title, factories, default_location, extra_variables);
    clear();
  }

private:
  QString title;
  QList<IWizardFactory*> factories;
  FilePath default_location;
  QVariantMap extra_variables;
};

NewItemDialogData s_reopenData;

} // namespace

static auto actionId(const IWizardFactory *factory) -> Id
{
  return factory->id().withPrefix("Wizard.Impl.");
}

auto IWizardFactory::allWizardFactories() -> QList<IWizardFactory*>
{
  if (!s_areFactoriesLoaded) {
    QTC_ASSERT(s_allFactories.isEmpty(), return s_allFactories);

    s_areFactoriesLoaded = true;

    QHash<Id, IWizardFactory*> sanity_check;
    for(const auto &fc: s_factoryCreators) {
      for(auto tmp = fc(); auto new_factory: tmp) {
        QTC_ASSERT(new_factory, continue);
        const auto existing_factory = sanity_check.value(new_factory->id());

        QTC_ASSERT(existing_factory != new_factory, continue);
        if (existing_factory) {
          qWarning("%s", qPrintable(tr("Factory with id=\"%1\" already registered. Deleting.") .arg(existing_factory->id().toString())));
          delete new_factory;
          continue;
        }

        QTC_ASSERT(!new_factory->m_action, continue);
        new_factory->m_action = new QAction(new_factory->displayName(), new_factory);
        ActionManager::registerAction(new_factory->m_action, actionId(new_factory));

        connect(new_factory->m_action, &QAction::triggered, new_factory, [new_factory] {
          if (!ICore::isNewItemDialogRunning()) {
            const auto path = new_factory->runPath({});
            new_factory->runWizard(path, ICore::dialogParent(), Id(), QVariantMap());
          }
        });

        sanity_check.insert(new_factory->id(), new_factory);
        s_allFactories << new_factory;
      }
    }
  }

  return s_allFactories;
}

auto IWizardFactory::runPath(const FilePath &default_path) const -> FilePath
{
  auto path = default_path;

  if (path.isEmpty()) {
    switch (kind()) {
    case ProjectWizard:
      // Project wizards: Check for projects directory or
      // use last visited directory of file dialog. Never start
      // at current.
      path = DocumentManager::useProjectsDirectory() ? DocumentManager::projectsDirectory() : DocumentManager::fileDialogLastVisitedDirectory();
      break;
    default:
      path = DocumentManager::fileDialogInitialDirectory();
      break;
    }
  }

  return path;
}

/*!
    Creates the wizard that the user selected for execution on the operating
    system \a platform with \a variables.

    Any dialogs the wizard opens should use the given \a parent.
    The \a path argument is a suggestion for the location where files should be
    created. The wizard should fill this in its path selection elements as a
    default path.
*/
auto IWizardFactory::runWizard(const FilePath &path, QWidget *parent, const Id platform, const QVariantMap &variables, const bool show_wizard) -> Wizard*
{
  QTC_ASSERT(!s_isWizardRunning, return nullptr);

  s_isWizardRunning = true;
  ICore::updateNewItemDialogState();

  auto wizard = runWizardImpl(path, parent, platform, variables, show_wizard);

  if (wizard) {
    s_currentWizard = wizard;
    // Connect while wizard exists:
    if (m_action)
      connect(m_action, &QAction::triggered, wizard, [wizard] { ICore::raiseWindow(wizard); });
    connect(s_inspectWizardAction, &QAction::triggered, wizard, [wizard] { wizard->showVariables(); });
    connect(wizard, &Wizard::finished, this, [wizard](const int result) {
      if (result != QDialog::Accepted)
        s_reopenData.clear();
      wizard->deleteLater();
    });
    connect(wizard, &QObject::destroyed, this, [] {
      s_isWizardRunning = false;
      s_currentWizard = nullptr;
      s_inspectWizardAction->setEnabled(false);
      ICore::updateNewItemDialogState();
      s_reopenData.reopen();
    });
    s_inspectWizardAction->setEnabled(true);
    if (show_wizard) {
      wizard->show();
      ICore::registerWindow(wizard, Context("Core.NewWizard"));
    }
  } else {
    s_isWizardRunning = false;
    ICore::updateNewItemDialogState();
    s_reopenData.reopen();
  }

  return wizard;
}

auto IWizardFactory::isAvailable(const Id platform_id) const -> bool
{
  if (!platform_id.isValid())
    return true;

  return availableFeatures(platform_id).contains(requiredFeatures());
}

auto IWizardFactory::supportedPlatforms() const -> QSet<Id>
{
  QSet<Id> platform_ids;

  for(auto& platform: allAvailablePlatforms()) {
    if (isAvailable(platform))
      platform_ids.insert(platform);
  }

  return platform_ids;
}

auto IWizardFactory::registerFactoryCreator(const FactoryCreator &creator) -> void
{
  s_factoryCreators << creator;
}

auto IWizardFactory::allAvailablePlatforms() -> QSet<Id>
{
  QSet<Id> platforms;

  for(const auto &feature_manager: s_providerList)
    platforms.unite(feature_manager->availablePlatforms());

  return platforms;
}

auto IWizardFactory::displayNameForPlatform(const Id i) -> QString
{
  for(const auto &feature_manager: s_providerList) {
    if (auto display_name = feature_manager->displayNameForPlatform(i); !display_name.isEmpty())
      return display_name;
  }
  return {};
}

auto IWizardFactory::registerFeatureProvider(IFeatureProvider *provider) -> void
{
  QTC_ASSERT(!s_providerList.contains(provider), return);
  s_providerList.append(provider);
}

auto IWizardFactory::isWizardRunning() -> bool
{
  return s_isWizardRunning;
}

auto IWizardFactory::currentWizard() -> QWidget*
{
  return s_currentWizard;
}

auto IWizardFactory::requestNewItemDialog(const QString &title, const QList<IWizardFactory*> &factories, const FilePath &default_location, const QVariantMap &extra_variables) -> void
{
  s_reopenData.setData(title, factories, default_location, extra_variables);
}

auto IWizardFactory::themedIcon(const FilePath &icon_mask_path) -> QIcon
{
  return Icon({{icon_mask_path, Theme::PanelTextColorDark}}, Icon::Tint).icon();
}

auto IWizardFactory::destroyFeatureProvider() -> void
{
  qDeleteAll(s_providerList);
  s_providerList.clear();
}

auto IWizardFactory::clearWizardFactories() -> void
{
  foreach(IWizardFactory *factory, s_allFactories)
    ActionManager::unregisterAction(factory->m_action, actionId(factory));

  qDeleteAll(s_allFactories);
  s_allFactories.clear();
  s_areFactoriesLoaded = false;
}

auto IWizardFactory::pluginFeatures() -> QSet<Id>
{
  static QSet<Id> plugins;

  if (plugins.isEmpty()) {
    // Implicitly create a feature for each plugin loaded:
    for(const auto s: ExtensionSystem::PluginManager::plugins()) {
      if (s->state() == ExtensionSystem::PluginSpec::Running)
        plugins.insert(Id::fromString(s->name()));
    }
  }

  return plugins;
}

auto IWizardFactory::availableFeatures(const Id platform_id) -> QSet<Id>
{
  QSet<Id> available_features;

  foreach(const IFeatureProvider *featureManager, s_providerList)
    available_features.unite(featureManager->availableFeatures(platform_id));

  return available_features;
}

auto IWizardFactory::initialize() -> void
{
  connect(ICore::instance(), &ICore::coreAboutToClose, &IWizardFactory::clearWizardFactories);

  auto reset_action = new QAction(tr("Reload All Wizards"), ActionManager::instance());
  ActionManager::registerAction(reset_action, "Wizard.Factory.Reset");

  connect(reset_action, &QAction::triggered, &IWizardFactory::clearWizardFactories);
  connect(ICore::instance(), &ICore::newItemDialogStateChanged, reset_action, [reset_action] { reset_action->setEnabled(!ICore::isNewItemDialogRunning()); });

  s_inspectWizardAction = new QAction(tr("Inspect Wizard State"), ActionManager::instance());
  ActionManager::registerAction(s_inspectWizardAction, "Wizard.Inspect");
}

static auto iconWithText(const QIcon &icon, const QString &text) -> QIcon
{
  if (icon.isNull()) {
    static const auto fall_back = IWizardFactory::themedIcon(":/utils/images/wizardicon-file.png");
    return iconWithText(fall_back, text);
  }

  if (text.isEmpty())
    return icon;

  QIcon icon_with_text;

  for (const auto &pixmap_size : icon.availableSizes()) {
    auto pixmap = icon.pixmap(pixmap_size);
    const auto original_pixmap_dpr = pixmap.devicePixelRatio();
    pixmap.setDevicePixelRatio(1); // Hack for ORCABUG-26315
    const auto font_size = pixmap.height() / 4;
    const auto margin = pixmap.height() / 8;

    QFont font;
    font.setPixelSize(font_size);
    font.setStretch(85);

    QPainter p(&pixmap);
    p.setPen(orcaTheme()->color(Theme::PanelTextColorDark));
    p.setFont(font);

    QTextOption text_option(Qt::AlignHCenter | Qt::AlignBottom);
    text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    p.drawText(pixmap.rect().adjusted(margin, margin, -margin, -margin), text, text_option);
    p.end();

    pixmap.setDevicePixelRatio(original_pixmap_dpr);
    icon_with_text.addPixmap(pixmap);
  }

  return icon_with_text;
}

auto IWizardFactory::setIcon(const QIcon &icon, const QString &icon_text) -> void
{
  m_icon = iconWithText(icon, icon_text);
}

auto IWizardFactory::setDetailsPageQmlPath(const QString &file_path) -> void
{
  if (file_path.isEmpty())
    return;

  if (file_path.startsWith(':')) {
    m_details_page_qml_path.setScheme(QLatin1String("qrc"));
    auto path = file_path;
    path.remove(0, 1);
    m_details_page_qml_path.setPath(path);
  } else {
    m_details_page_qml_path = QUrl::fromLocalFile(file_path);
  }
}

} // namespace Orca::Plugin::Core
