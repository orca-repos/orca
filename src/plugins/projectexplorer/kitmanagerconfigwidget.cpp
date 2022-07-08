// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitmanagerconfigwidget.hpp"
#include "projectconfiguration.hpp"

#include "devicesupport/idevicefactory.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "projectexplorerconstants.hpp"
#include "task.hpp"

#include <utils/algorithm.hpp>
#include <utils/detailswidget.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/macroexpander.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <QAction>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QSizePolicy>

static const char WORKING_COPY_KIT_ID[] = "modified kit";

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

KitManagerConfigWidget::KitManagerConfigWidget(Kit *k) : m_iconButton(new QToolButton), m_nameEdit(new QLineEdit), m_fileSystemFriendlyNameLineEdit(new QLineEdit), m_kit(k), m_modifiedKit(std::make_unique<Kit>(Id(WORKING_COPY_KIT_ID)))
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  auto label = new QLabel(tr("Name:"));
  label->setToolTip(tr("Kit name and icon."));

  const auto toolTip = tr("<html><head/><body><p>The name of the kit suitable for generating " "directory names. This value is used for the variable <i>%1</i>, " "which for example determines the name of the shadow build directory." "</p></body></html>").arg(QLatin1String("Kit:FileSystemName"));
  m_fileSystemFriendlyNameLineEdit->setToolTip(toolTip);
  const QRegularExpression fileSystemFriendlyNameRegexp(QLatin1String("^[A-Za-z0-9_-]*$"));
  Q_ASSERT(fileSystemFriendlyNameRegexp.isValid());
  m_fileSystemFriendlyNameLineEdit->setValidator(new QRegularExpressionValidator(fileSystemFriendlyNameRegexp, m_fileSystemFriendlyNameLineEdit));

  label = new QLabel(tr("File system name:"));
  label->setToolTip(toolTip);
  connect(m_fileSystemFriendlyNameLineEdit, &QLineEdit::textChanged, this, &KitManagerConfigWidget::setFileSystemFriendlyName);

  using namespace Layouting;
  Grid{AlignAsFormLabel(label), m_nameEdit, m_iconButton, Break(), AlignAsFormLabel(label), m_fileSystemFriendlyNameLineEdit}.attachTo(this);

  m_iconButton->setToolTip(tr("Kit icon."));
  const auto setIconAction = new QAction(tr("Select Icon..."), this);
  m_iconButton->addAction(setIconAction);
  const auto resetIconAction = new QAction(tr("Reset to Device Default Icon"), this);
  m_iconButton->addAction(resetIconAction);

  discard();

  connect(m_iconButton, &QAbstractButton::clicked, this, &KitManagerConfigWidget::setIcon);
  connect(setIconAction, &QAction::triggered, this, &KitManagerConfigWidget::setIcon);
  connect(resetIconAction, &QAction::triggered, this, &KitManagerConfigWidget::resetIcon);
  connect(m_nameEdit, &QLineEdit::textChanged, this, &KitManagerConfigWidget::setDisplayName);

  const auto km = KitManager::instance();
  connect(km, &KitManager::unmanagedKitUpdated, this, &KitManagerConfigWidget::workingCopyWasUpdated);
  connect(km, &KitManager::kitUpdated, this, &KitManagerConfigWidget::kitWasUpdated);

  const auto chooser = new VariableChooser(this);
  chooser->addSupportedWidget(m_nameEdit);
  chooser->addMacroExpanderProvider([this]() { return m_modifiedKit->macroExpander(); });

  for (const auto aspect : KitManager::kitAspects())
    addAspectToWorkingCopy(aspect);

  updateVisibility();

  if (k && k->isAutoDetected())
    makeStickySubWidgetsReadOnly();
  setVisible(false);
}

KitManagerConfigWidget::~KitManagerConfigWidget()
{
  qDeleteAll(m_widgets);
  m_widgets.clear();

  // Make sure our workingCopy did not get registered somehow:
  QTC_CHECK(!Utils::contains(KitManager::kits(), Utils::equal(&Kit::id, Utils::Id(WORKING_COPY_KIT_ID))));
}

auto KitManagerConfigWidget::displayName() const -> QString
{
  if (m_cachedDisplayName.isEmpty())
    m_cachedDisplayName = m_modifiedKit->displayName();
  return m_cachedDisplayName;
}

auto KitManagerConfigWidget::displayIcon() const -> QIcon
{
  // Special case: Extra warning if there are no errors but name is not unique.
  if (m_modifiedKit->isValid() && !m_hasUniqueName) {
    static const auto warningIcon(Icons::WARNING.icon());
    return warningIcon;
  }

  return m_modifiedKit->displayIcon();
}

auto KitManagerConfigWidget::apply() -> void
{
  // TODO: Rework the mechanism so this won't be necessary.
  const auto wasDefaultKit = m_isDefaultKit;

  const auto copyIntoKit = [this](Kit *k) { k->copyFrom(m_modifiedKit.get()); };
  if (m_kit) {
    copyIntoKit(m_kit);
    KitManager::notifyAboutUpdate(m_kit);
  } else {
    m_isRegistering = true;
    m_kit = KitManager::registerKit(copyIntoKit);
    m_isRegistering = false;
  }
  m_isDefaultKit = wasDefaultKit;
  if (m_isDefaultKit)
    KitManager::setDefaultKit(m_kit);
  emit dirty();
}

auto KitManagerConfigWidget::discard() -> void
{
  if (m_kit) {
    m_modifiedKit->copyFrom(m_kit);
    m_isDefaultKit = (m_kit == KitManager::defaultKit());
  } else {
    // This branch will only ever get reached once during setup of widget for a not-yet-existing
    // kit.
    m_isDefaultKit = false;
  }
  m_iconButton->setIcon(m_modifiedKit->icon());
  m_nameEdit->setText(m_modifiedKit->unexpandedDisplayName());
  m_cachedDisplayName.clear();
  m_fileSystemFriendlyNameLineEdit->setText(m_modifiedKit->customFileSystemFriendlyName());
  emit dirty();
}

auto KitManagerConfigWidget::isDirty() const -> bool
{
  return !m_kit || !m_kit->isEqual(m_modifiedKit.get()) || m_isDefaultKit != (KitManager::defaultKit() == m_kit);
}

auto KitManagerConfigWidget::validityMessage() const -> QString
{
  Tasks tmp;
  if (!m_hasUniqueName)
    tmp.append(CompileTask(Task::Warning, tr("Display name is not unique.")));

  return m_modifiedKit->toHtml(tmp);
}

auto KitManagerConfigWidget::addAspectToWorkingCopy(KitAspect *aspect) -> void
{
  QTC_ASSERT(aspect, return);
  const auto widget = aspect->createConfigWidget(workingCopy());
  QTC_ASSERT(widget, return);
  QTC_ASSERT(!m_widgets.contains(widget), return);

  widget->addToLayoutWithLabel(this);
  m_widgets.append(widget);

  connect(widget->mutableAction(), &QAction::toggled, this, &KitManagerConfigWidget::dirty);
}

auto KitManagerConfigWidget::updateVisibility() -> void
{
  const int count = m_widgets.count();
  for (auto i = 0; i < count; ++i) {
    const auto widget = m_widgets.at(i);
    const auto ki = widget->kitInformation();
    const auto visibleInKit = ki->isApplicableToKit(m_modifiedKit.get());
    const auto irrelevant = m_modifiedKit->irrelevantAspects().contains(ki->id());
    widget->setVisible(visibleInKit && !irrelevant);
  }
}

auto KitManagerConfigWidget::setHasUniqueName(bool unique) -> void
{
  m_hasUniqueName = unique;
}

auto KitManagerConfigWidget::makeStickySubWidgetsReadOnly() -> void
{
  foreach(KitAspectWidget *w, m_widgets) {
    if (w->kit()->isSticky(w->kitInformation()->id()))
      w->makeReadOnly();
  }
}

auto KitManagerConfigWidget::workingCopy() const -> Kit*
{
  return m_modifiedKit.get();
}

auto KitManagerConfigWidget::configures(Kit *k) const -> bool
{
  return m_kit == k;
}

auto KitManagerConfigWidget::setIsDefaultKit(bool d) -> void
{
  if (m_isDefaultKit == d)
    return;
  m_isDefaultKit = d;
  emit dirty();
}

auto KitManagerConfigWidget::isDefaultKit() const -> bool
{
  return m_isDefaultKit;
}

auto KitManagerConfigWidget::removeKit() -> void
{
  if (!m_kit)
    return;
  KitManager::deregisterKit(m_kit);
}

auto KitManagerConfigWidget::setIcon() -> void
{
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(m_modifiedKit.get());
  auto allDeviceFactories = IDeviceFactory::allDeviceFactories();
  if (deviceType.isValid()) {
    const auto less = [deviceType](const IDeviceFactory *f1, const IDeviceFactory *f2) {
      if (f1->deviceType() == deviceType)
        return true;
      if (f2->deviceType() == deviceType)
        return false;
      return f1->displayName() < f2->displayName();
    };
    sort(allDeviceFactories, less);
  }
  QMenu iconMenu;
  for (const IDeviceFactory *const factory : qAsConst(allDeviceFactories)) {
    if (factory->icon().isNull())
      continue;
    const auto action = iconMenu.addAction(factory->icon(), tr("Default for %1").arg(factory->displayName()), [this, factory] {
      m_iconButton->setIcon(factory->icon());
      m_modifiedKit->setDeviceTypeForIcon(factory->deviceType());
      emit dirty();
    });
    action->setIconVisibleInMenu(true);
  }
  iconMenu.addSeparator();
  iconMenu.addAction(PathChooser::browseButtonLabel(), [this] {
    const auto path = FileUtils::getOpenFilePath(this, tr("Select Icon"), m_modifiedKit->iconPath(), tr("Images (*.png *.xpm *.jpg)"));
    if (path.isEmpty())
      return;
    const QIcon icon(path.toString());
    if (icon.isNull())
      return;
    m_iconButton->setIcon(icon);
    m_modifiedKit->setIconPath(path);
    emit dirty();
  });
  iconMenu.exec(mapToGlobal(m_iconButton->pos()));
}

auto KitManagerConfigWidget::resetIcon() -> void
{
  m_modifiedKit->setIconPath(FilePath());
  emit dirty();
}

auto KitManagerConfigWidget::setDisplayName() -> void
{
  const auto pos = m_nameEdit->cursorPosition();
  m_cachedDisplayName.clear();
  m_modifiedKit->setUnexpandedDisplayName(m_nameEdit->text());
  m_nameEdit->setCursorPosition(pos);
}

auto KitManagerConfigWidget::setFileSystemFriendlyName() -> void
{
  const auto pos = m_fileSystemFriendlyNameLineEdit->cursorPosition();
  m_modifiedKit->setCustomFileSystemFriendlyName(m_fileSystemFriendlyNameLineEdit->text());
  m_fileSystemFriendlyNameLineEdit->setCursorPosition(pos);
}

auto KitManagerConfigWidget::workingCopyWasUpdated(Kit *k) -> void
{
  if (k != m_modifiedKit.get() || m_fixingKit)
    return;

  m_fixingKit = true;
  k->fix();
  m_fixingKit = false;

  foreach(KitAspectWidget *w, m_widgets)
    w->refresh();

  m_cachedDisplayName.clear();

  if (k->unexpandedDisplayName() != m_nameEdit->text())
    m_nameEdit->setText(k->unexpandedDisplayName());

  m_fileSystemFriendlyNameLineEdit->setText(k->customFileSystemFriendlyName());
  m_iconButton->setIcon(k->icon());
  updateVisibility();
  emit dirty();
}

auto KitManagerConfigWidget::kitWasUpdated(Kit *k) -> void
{
  if (m_kit == k) {
    const auto emitSignal = m_kit->isAutoDetected() != m_modifiedKit->isAutoDetected();
    discard();
    if (emitSignal) emit isAutoDetectedChanged();
  }
  updateVisibility();
}

auto KitManagerConfigWidget::showEvent(QShowEvent *event) -> void
{
  Q_UNUSED(event)
  foreach(KitAspectWidget *widget, m_widgets)
    widget->refresh();
}

} // namespace Internal
} // namespace ProjectExplorer
