// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtoptionspage.hpp"

#include "qtconfigwidget.hpp"
#include "ui_showbuildlog.h"
#include "ui_qtversionmanager.h"
#include "ui_qtversioninfo.h"
#include "qtsupportconstants.hpp"
#include "qtversionmanager.hpp"
#include "qtversionfactory.hpp"

#include <app/app_version.hpp>

#include <core/coreconstants.hpp>
#include <core/dialogs/restartdialog.hpp>
#include <core/icore.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectexplorericons.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/buildablehelperlibrary.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/treemodel.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QTextBrowser>

#include <utility>

using namespace ProjectExplorer;
using namespace Utils;

constexpr char kInstallSettingsKey[] = "Settings/InstallSettings";

namespace QtSupport {
namespace Internal {

class QtVersionItem : public TreeItem {
  Q_DECLARE_TR_FUNCTIONS(QtSupport::QtVersion)

public:
  explicit QtVersionItem(QtVersion *version) : m_version(version) {}

  ~QtVersionItem()
  {
    delete m_version;
  }

  auto setVersion(QtVersion *version) -> void
  {
    m_version = version;
    update();
  }

  auto uniqueId() const -> int
  {
    return m_version ? m_version->uniqueId() : -1;
  }

  auto version() const -> QtVersion*
  {
    return m_version;
  }

  auto data(int column, int role) const -> QVariant final
  {
    if (!m_version)
      return TreeItem::data(column, role);

    if (role == Qt::DisplayRole) {
      if (column == 0)
        return m_version->displayName();
      if (column == 1)
        return m_version->qmakeFilePath().toUserOutput();
    }

    if (role == Qt::FontRole && m_changed) {
      QFont font;
      font.setBold(true);
      return font;
    }

    if (role == Qt::DecorationRole && column == 0)
      return m_icon;

    if (role == Qt::ToolTipRole) {
      const QString row = "<tr><td>%1:</td><td>%2</td></tr>";
      return QString("<table>" + row.arg(tr("Qt Version"), m_version->qtVersionString()) + row.arg(tr("Location of qmake"), m_version->qmakeFilePath().toUserOutput()) + "</table>");
    }

    return QVariant();
  }

  auto setIcon(const QIcon &icon) -> void
  {
    if (m_icon.cacheKey() == icon.cacheKey())
      return;
    m_icon = icon;
    update();
  }

  auto buildLog() const -> QString
  {
    return m_buildLog;
  }

  auto setBuildLog(const QString &buildLog) -> void
  {
    m_buildLog = buildLog;
  }

  auto setChanged(bool changed) -> void
  {
    if (changed == m_changed)
      return;
    m_changed = changed;
    update();
  }

private:
  QtVersion *m_version = nullptr;
  QIcon m_icon;
  QString m_buildLog;
  bool m_changed = false;
};

// QtOptionsPageWidget

class QtOptionsPageWidget : public Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(QtSupport::Internal::QtOptionsPageWidget)

public:
  QtOptionsPageWidget();
  ~QtOptionsPageWidget();

  static auto linkWithQt() -> void;

private:
  auto apply() -> void final;
  auto updateDescriptionLabel() -> void;
  auto userChangedCurrentVersion() -> void;
  auto updateWidgets() -> void;
  auto setupLinkWithQtButton() -> void;
  auto currentVersion() const -> QtVersion*;
  auto currentItem() const -> QtVersionItem*;
  auto showDebuggingBuildLog(const QtVersionItem *item) -> void;

  const QString m_specifyNameString;
  Ui::QtVersionManager m_ui;
  Ui::QtVersionInfo m_versionUi;
  QTextBrowser *m_infoBrowser;
  QIcon m_invalidVersionIcon;
  QIcon m_warningVersionIcon;
  QIcon m_validVersionIcon;
  QtConfigWidget *m_configurationWidget;
  
  auto updateQtVersions(const QList<int> &, const QList<int> &, const QList<int> &) -> void;
  auto versionChanged(const QModelIndex &current, const QModelIndex &previous) -> void;
  auto addQtDir() -> void;
  auto removeQtDir() -> void;
  auto editPath() -> void;
  auto updateCleanUpButton() -> void;
  auto updateCurrentQtName() -> void;
  auto cleanUpQtVersions() -> void;
  auto toolChainsUpdated() -> void;
  auto setInfoWidgetVisibility() -> void;
  auto infoAnchorClicked(const QUrl &) -> void;

  struct ValidityInfo {
    QString description;
    QString message;
    QString toolTip;
    QIcon icon;
  };

  auto validInformation(const QtVersion *version) -> ValidityInfo;
  auto toolChains(const QtVersion *version) -> QList<ToolChain*>;
  auto defaultToolChainId(const QtVersion *version) -> QByteArray;
  auto isNameUnique(const QtVersion *version) -> bool;
  auto updateVersionItem(QtVersionItem *item) -> void;

  TreeModel<TreeItem, TreeItem, QtVersionItem> *m_model;
  QSortFilterProxyModel *m_filterModel;
  TreeItem *m_autoItem;
  TreeItem *m_manualItem;
};

QtOptionsPageWidget::QtOptionsPageWidget() : m_specifyNameString(tr("<specify a name>")), m_infoBrowser(new QTextBrowser), m_invalidVersionIcon(Utils::Icons::CRITICAL.icon()), m_warningVersionIcon(Utils::Icons::WARNING.icon()), m_configurationWidget(nullptr)
{
  auto versionInfoWidget = new QWidget;
  m_versionUi.setupUi(versionInfoWidget);
  m_versionUi.editPathPushButton->setText(PathChooser::browseButtonLabel());

  m_ui.setupUi(this);

  setupLinkWithQtButton();

  m_infoBrowser->setOpenLinks(false);
  m_infoBrowser->setTextInteractionFlags(Qt::TextBrowserInteraction);
  connect(m_infoBrowser, &QTextBrowser::anchorClicked, this, &QtOptionsPageWidget::infoAnchorClicked);
  m_ui.infoWidget->setWidget(m_infoBrowser);
  connect(m_ui.infoWidget, &DetailsWidget::expanded, this, &QtOptionsPageWidget::setInfoWidgetVisibility);

  m_ui.versionInfoWidget->setWidget(versionInfoWidget);
  m_ui.versionInfoWidget->setState(DetailsWidget::NoSummary);

  m_autoItem = new StaticTreeItem({ProjectExplorer::Constants::msgAutoDetected()}, {ProjectExplorer::Constants::msgAutoDetectedToolTip()});
  m_manualItem = new StaticTreeItem(ProjectExplorer::Constants::msgManual());

  m_model = new TreeModel<TreeItem, TreeItem, QtVersionItem>();
  m_model->setHeader({tr("Name"), tr("qmake Path")});
  m_model->rootItem()->appendChild(m_autoItem);
  m_model->rootItem()->appendChild(m_manualItem);

  m_filterModel = new QSortFilterProxyModel(this);
  m_filterModel->setSourceModel(m_model);
  m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_ui.qtdirList->setModel(m_filterModel);
  m_ui.qtdirList->setSortingEnabled(true);

  m_ui.qtdirList->setFirstColumnSpanned(0, QModelIndex(), true);
  m_ui.qtdirList->setFirstColumnSpanned(1, QModelIndex(), true);

  m_ui.qtdirList->header()->setStretchLastSection(false);
  m_ui.qtdirList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_ui.qtdirList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_ui.qtdirList->setTextElideMode(Qt::ElideMiddle);
  m_ui.qtdirList->sortByColumn(0, Qt::AscendingOrder);

  m_ui.documentationSetting->addItem(tr("Highest Version Only"), int(QtVersionManager::DocumentationSetting::HighestOnly));
  m_ui.documentationSetting->addItem(tr("All"), int(QtVersionManager::DocumentationSetting::All));
  m_ui.documentationSetting->addItem(tr("None"), int(QtVersionManager::DocumentationSetting::None));
  const int selectedIndex = m_ui.documentationSetting->findData(int(QtVersionManager::documentationSetting()));
  if (selectedIndex >= 0)
    m_ui.documentationSetting->setCurrentIndex(selectedIndex);

  const auto additions = transform(QtVersionManager::versions(), &QtVersion::uniqueId);

  updateQtVersions(additions, QList<int>(), QList<int>());

  m_ui.qtdirList->expandAll();

  connect(m_versionUi.nameEdit, &QLineEdit::textEdited, this, &QtOptionsPageWidget::updateCurrentQtName);

  connect(m_versionUi.editPathPushButton, &QAbstractButton::clicked, this, &QtOptionsPageWidget::editPath);

  connect(m_ui.addButton, &QAbstractButton::clicked, this, &QtOptionsPageWidget::addQtDir);
  connect(m_ui.delButton, &QAbstractButton::clicked, this, &QtOptionsPageWidget::removeQtDir);

  connect(m_ui.qtdirList->selectionModel(), &QItemSelectionModel::currentChanged, this, &QtOptionsPageWidget::versionChanged);

  connect(m_ui.cleanUpButton, &QAbstractButton::clicked, this, &QtOptionsPageWidget::cleanUpQtVersions);
  userChangedCurrentVersion();
  updateCleanUpButton();

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &QtOptionsPageWidget::updateQtVersions);

  connect(ToolChainManager::instance(), &ToolChainManager::toolChainsChanged, this, &QtOptionsPageWidget::toolChainsUpdated);

  const auto chooser = new VariableChooser(this);
  chooser->addSupportedWidget(m_versionUi.nameEdit, "Qt:Name");
  chooser->addMacroExpanderProvider([this] {
    const auto version = currentVersion();
    return version ? version->macroExpander() : nullptr;
  });
}

auto QtOptionsPageWidget::currentVersion() const -> QtVersion*
{
  const auto item = currentItem();
  if (!item)
    return nullptr;
  return item->version();
}

auto QtOptionsPageWidget::currentItem() const -> QtVersionItem*
{
  const QModelIndex idx = m_ui.qtdirList->selectionModel()->currentIndex();
  const QModelIndex sourceIdx = m_filterModel->mapToSource(idx);
  return m_model->itemForIndexAtLevel<2>(sourceIdx);
}

auto QtOptionsPageWidget::cleanUpQtVersions() -> void
{
  QVector<QtVersionItem*> toRemove;
  QString text;

  for (const auto child : *m_manualItem) {
    const auto item = static_cast<QtVersionItem*>(child);
    if (item->version() && !item->version()->isValid()) {
      toRemove.append(item);
      if (!text.isEmpty())
        text.append(QLatin1String("</li><li>"));
      text.append(item->version()->displayName());
    }
  }

  if (toRemove.isEmpty())
    return;

  if (QMessageBox::warning(nullptr, tr("Remove Invalid Qt Versions"), tr("Do you want to remove all invalid Qt Versions?<br>" "<ul><li>%1</li></ul><br>" "will be removed.").arg(text), QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    return;

  foreach(QtVersionItem *item, toRemove)
    m_model->destroyItem(item);

  updateCleanUpButton();
}

auto QtOptionsPageWidget::toolChainsUpdated() -> void
{
  m_model->forItemsAtLevel<2>([this](QtVersionItem *item) {
    if (item == currentItem())
      updateDescriptionLabel();
    else
      updateVersionItem(item);
  });
}

auto QtOptionsPageWidget::setInfoWidgetVisibility() -> void
{
  m_ui.versionInfoWidget->setVisible(m_ui.infoWidget->state() == DetailsWidget::Collapsed);
  m_ui.infoWidget->setVisible(true);
}

auto QtOptionsPageWidget::infoAnchorClicked(const QUrl &url) -> void
{
  QDesktopServices::openUrl(url);
}

static auto formatAbiHtmlList(const Abis &abis) -> QString
{
  auto result = QStringLiteral("<ul><li>");
  for (int i = 0, count = abis.size(); i < count; ++i) {
    if (i)
      result += QStringLiteral("</li><li>");
    result += abis.at(i).toString();
  }
  result += QStringLiteral("</li></ul>");
  return result;
}

auto QtOptionsPageWidget::validInformation(const QtVersion *version) -> ValidityInfo
{
  ValidityInfo info;
  info.icon = m_validVersionIcon;

  if (!version)
    return info;

  info.description = tr("Qt version %1 for %2").arg(version->qtVersionString(), version->description());
  if (!version->isValid()) {
    info.icon = m_invalidVersionIcon;
    info.message = version->invalidReason();
    return info;
  }

  // Do we have tool chain issues?
  Abis missingToolChains;
  const auto qtAbis = version->qtAbis();

  for (const auto &abi : qtAbis) {
    const auto abiCompatePred = [&abi](const ToolChain *tc) {
      return contains(tc->supportedAbis(), [&abi](const Abi &sabi) { return sabi.isCompatibleWith(abi); });
    };

    if (!ToolChainManager::toolChain(abiCompatePred))
      missingToolChains.append(abi);
  }

  auto useable = true;
  QStringList warnings;
  if (!isNameUnique(version))
    warnings << tr("Display Name is not unique.");

  if (!missingToolChains.isEmpty()) {
    if (missingToolChains.count() == qtAbis.size()) {
      // Yes, this Qt version can't be used at all!
      info.message = tr("No compiler can produce code for this Qt version." " Please define one or more compilers for: %1").arg(formatAbiHtmlList(qtAbis));
      info.icon = m_invalidVersionIcon;
      useable = false;
    } else {
      // Yes, some ABIs are unsupported
      warnings << tr("Not all possible target environments can be supported due to missing compilers.");
      info.toolTip = tr("The following ABIs are currently not supported: %1").arg(formatAbiHtmlList(missingToolChains));
      info.icon = m_warningVersionIcon;
    }
  }

  if (useable) {
    warnings += version->warningReason();
    if (!warnings.isEmpty()) {
      info.message = warnings.join(QLatin1Char('\n'));
      info.icon = m_warningVersionIcon;
    }
  }

  return info;
}

auto QtOptionsPageWidget::toolChains(const QtVersion *version) -> QList<ToolChain*>
{
  QList<ToolChain*> toolChains;
  if (!version)
    return toolChains;

  QSet<QByteArray> ids;
  foreach(const Abi &a, version->qtAbis()) {
    foreach(ToolChain *tc, ToolChainManager::findToolChains(a)) {
      if (ids.contains(tc->id()))
        continue;
      ids.insert(tc->id());
      toolChains.append(tc);
    }
  }

  return toolChains;
}

auto QtOptionsPageWidget::defaultToolChainId(const QtVersion *version) -> QByteArray
{
  auto possibleToolChains = toolChains(version);
  if (!possibleToolChains.isEmpty())
    return possibleToolChains.first()->id();
  return QByteArray();
}

auto QtOptionsPageWidget::isNameUnique(const QtVersion *version) -> bool
{
  const auto name = version->displayName().trimmed();

  return !m_model->findItemAtLevel<2>([name, version](QtVersionItem *item) {
    const auto v = item->version();
    return v != version && v->displayName().trimmed() == name;
  });
}

auto QtOptionsPageWidget::updateVersionItem(QtVersionItem *item) -> void
{
  if (!item)
    return;
  if (!item->version())
    return;

  const auto info = validInformation(item->version());
  item->update();
  item->setIcon(info.icon);
}

// Non-modal dialog
class BuildLogDialog : public QDialog {
public:
  explicit BuildLogDialog(QWidget *parent = nullptr);
  auto setText(const QString &text) -> void;

private:
  Ui_ShowBuildLog m_ui;
};

BuildLogDialog::BuildLogDialog(QWidget *parent) : QDialog(parent)
{
  m_ui.setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose, true);
}

auto BuildLogDialog::setText(const QString &text) -> void
{
  m_ui.log->setPlainText(text); // Show and scroll to bottom
  m_ui.log->moveCursor(QTextCursor::End);
  m_ui.log->ensureCursorVisible();
}

auto QtOptionsPageWidget::showDebuggingBuildLog(const QtVersionItem *item) -> void
{
  const auto version = item->version();
  if (!version)
    return;
  const auto dialog = new BuildLogDialog(this->window());
  dialog->setWindowTitle(tr("Debugging Helper Build Log for \"%1\"").arg(version->displayName()));
  dialog->setText(item->buildLog());
  dialog->show();
}

auto QtOptionsPageWidget::updateQtVersions(const QList<int> &additions, const QList<int> &removals, const QList<int> &changes) -> void
{
  QList<QtVersionItem*> toRemove;
  auto toAdd = additions;

  // Find existing items to remove/change:
  m_model->forItemsAtLevel<2>([&](QtVersionItem *item) {
    const auto id = item->uniqueId();
    if (removals.contains(id)) {
      toRemove.append(item);
    } else if (changes.contains(id)) {
      toAdd.append(id);
      toRemove.append(item);
    }
  });

  // Remove changed/removed items:
  foreach(QtVersionItem *item, toRemove)
    m_model->destroyItem(item);

  // Add changed/added items:
  foreach(int a, toAdd) {
    const auto version = QtVersionManager::version(a)->clone();
    auto *item = new QtVersionItem(version);

    // Insert in the right place:
    const auto parent = version->isAutodetected() ? m_autoItem : m_manualItem;
    parent->appendChild(item);
  }

  m_model->forItemsAtLevel<2>([this](QtVersionItem *item) { updateVersionItem(item); });
}

QtOptionsPageWidget::~QtOptionsPageWidget()
{
  delete m_configurationWidget;
}

auto QtOptionsPageWidget::addQtDir() -> void
{
  auto qtVersion = FileUtils::getOpenFilePath(this, tr("Select a qmake Executable"), {}, BuildableHelperLibrary::filterForQmakeFileDialog(), 0, QFileDialog::DontResolveSymlinks);
  if (qtVersion.isEmpty())
    return;

  // should add all qt versions here ?
  if (BuildableHelperLibrary::isQtChooser(qtVersion))
    qtVersion = BuildableHelperLibrary::qtChooserToQmakePath(qtVersion.symLinkTarget());

  auto checkAlreadyExists = [qtVersion](TreeItem *parent) {
    for (auto i = 0; i < parent->childCount(); ++i) {
      const auto item = static_cast<QtVersionItem*>(parent->childAt(i));
      if (item->version()->qmakeFilePath() == qtVersion) {
        return std::make_pair(true, item->version()->displayName());
      }
    }
    return std::make_pair(false, QString());
  };

  bool alreadyExists;
  QString otherName;
  std::tie(alreadyExists, otherName) = checkAlreadyExists(m_autoItem);
  if (!alreadyExists)
    std::tie(alreadyExists, otherName) = checkAlreadyExists(m_manualItem);

  if (alreadyExists) {
    // Already exist
    QMessageBox::warning(this, tr("Qt Version Already Known"), tr("This Qt version was already registered as \"%1\".").arg(otherName));
    return;
  }

  QString error;
  const auto version = QtVersionFactory::createQtVersionFromQMakePath(qtVersion, false, QString(), &error);
  if (version) {
    const auto item = new QtVersionItem(version);
    item->setIcon(version->isValid() ? m_validVersionIcon : m_invalidVersionIcon);
    m_manualItem->appendChild(item);
    const auto source = m_model->indexForItem(item);
    m_ui.qtdirList->setCurrentIndex(m_filterModel->mapFromSource(source)); // should update the rest of the ui
    m_versionUi.nameEdit->setFocus();
    m_versionUi.nameEdit->selectAll();
  } else {
    QMessageBox::warning(this, tr("Qmake Not Executable"), tr("The qmake executable %1 could not be added: %2").arg(qtVersion.toUserOutput()).arg(error));
    return;
  }
  updateCleanUpButton();
}

auto QtOptionsPageWidget::removeQtDir() -> void
{
  const auto item = currentItem();
  if (!item)
    return;

  m_model->destroyItem(item);

  updateCleanUpButton();
}

auto QtOptionsPageWidget::editPath() -> void
{
  const auto current = currentVersion();
  const auto qtVersion = FileUtils::getOpenFilePath(this, tr("Select a qmake Executable"), current->qmakeFilePath().absolutePath(), BuildableHelperLibrary::filterForQmakeFileDialog(), nullptr, QFileDialog::DontResolveSymlinks);
  if (qtVersion.isEmpty())
    return;
  const auto version = QtVersionFactory::createQtVersionFromQMakePath(qtVersion);
  if (!version)
    return;
  // Same type? then replace!
  if (current->type() != version->type()) {
    // not the same type, error out
    QMessageBox::critical(this, tr("Incompatible Qt Versions"), tr("The Qt version selected must match the device type."), QMessageBox::Ok);
    delete version;
    return;
  }
  // same type, replace
  version->setId(current->uniqueId());
  if (current->unexpandedDisplayName() != current->defaultUnexpandedDisplayName())
    version->setUnexpandedDisplayName(current->displayName());

  // Update ui
  if (const auto item = currentItem()) {
    item->setVersion(version);
    item->setIcon(version->isValid() ? m_validVersionIcon : m_invalidVersionIcon);
  }
  userChangedCurrentVersion();

  delete current;
}

// To be called if a Qt version was removed or added
auto QtOptionsPageWidget::updateCleanUpButton() -> void
{
  auto hasInvalidVersion = false;
  for (const auto child : *m_manualItem) {
    const auto item = static_cast<QtVersionItem*>(child);
    if (item->version() && !item->version()->isValid()) {
      hasInvalidVersion = true;
      break;
    }
  }

  m_ui.cleanUpButton->setEnabled(hasInvalidVersion);
}

auto QtOptionsPageWidget::userChangedCurrentVersion() -> void
{
  updateWidgets();
  updateDescriptionLabel();
}

auto QtOptionsPageWidget::updateDescriptionLabel() -> void
{
  const auto item = currentItem();
  const QtVersion *version = item ? item->version() : nullptr;
  const auto info = validInformation(version);
  if (info.message.isEmpty()) {
    m_versionUi.errorLabel->setVisible(false);
  } else {
    m_versionUi.errorLabel->setVisible(true);
    m_versionUi.errorLabel->setText(info.message);
    m_versionUi.errorLabel->setToolTip(info.toolTip);
  }
  m_ui.infoWidget->setSummaryText(info.description);
  if (item)
    item->setIcon(info.icon);

  if (version) {
    m_infoBrowser->setHtml(version->toHtml(true));
    setInfoWidgetVisibility();
  } else {
    m_infoBrowser->clear();
    m_ui.versionInfoWidget->setVisible(false);
    m_ui.infoWidget->setVisible(false);
  }
}

auto QtOptionsPageWidget::versionChanged(const QModelIndex &current, const QModelIndex &previous) -> void
{
  Q_UNUSED(current)
  Q_UNUSED(previous)
  userChangedCurrentVersion();
}

auto QtOptionsPageWidget::updateWidgets() -> void
{
  delete m_configurationWidget;
  m_configurationWidget = nullptr;
  const auto version = currentVersion();
  if (version) {
    m_versionUi.nameEdit->setText(version->unexpandedDisplayName());
    m_versionUi.qmakePath->setText(version->qmakeFilePath().toUserOutput());
    m_configurationWidget = version->createConfigurationWidget();
    if (m_configurationWidget) {
      m_versionUi.formLayout->addRow(m_configurationWidget);
      m_configurationWidget->setEnabled(!version->isAutodetected());
      connect(m_configurationWidget, &QtConfigWidget::changed, this, &QtOptionsPageWidget::updateDescriptionLabel);
    }
  } else {
    m_versionUi.nameEdit->clear();
    m_versionUi.qmakePath->clear();
  }

  const auto enabled = version != nullptr;
  const auto isAutodetected = enabled && version->isAutodetected();
  m_ui.delButton->setEnabled(enabled && !isAutodetected);
  m_versionUi.nameEdit->setEnabled(enabled);
  m_versionUi.editPathPushButton->setEnabled(enabled && !isAutodetected);
}

static auto settingsFile(const QString &baseDir) -> QString
{
  return baseDir + (baseDir.isEmpty() ? "" : "/") + Core::Constants::IDE_SETTINGSVARIANT_STR + '/' + Core::Constants::IDE_CASED_ID + ".ini";
}

static auto qtVersionsFile(const QString &baseDir) -> QString
{
  return baseDir + (baseDir.isEmpty() ? "" : "/") + Core::Constants::IDE_SETTINGSVARIANT_STR + '/' + Core::Constants::IDE_ID + '/' + "qtversion.xml";
}

static auto currentlyLinkedQtDir(bool *hasInstallSettings) -> optional<FilePath>
{
  const auto installSettingsFilePath = settingsFile(Core::ICore::resourcePath().toString());
  const auto installSettingsExist = QFile::exists(installSettingsFilePath);
  if (hasInstallSettings)
    *hasInstallSettings = installSettingsExist;
  if (installSettingsExist) {
    const auto value = QSettings(installSettingsFilePath, QSettings::IniFormat).value(kInstallSettingsKey);
    if (value.isValid())
      return FilePath::fromVariant(value);
  }
  return {};
}

static auto linkingPurposeText() -> QString
{
  return QtOptionsPageWidget::tr("Linking with a Qt installation automatically registers Qt versions and kits, and other " "tools that were installed with that Qt installer, in this Qt Creator installation. Other " "Qt Creator installations are not affected.");
}

static auto canLinkWithQt(QString *toolTip) -> bool
{
  auto canLink = true;
  bool installSettingsExist;
  const auto installSettingsValue = currentlyLinkedQtDir(&installSettingsExist);
  QStringList tip;
  tip << linkingPurposeText();
  if (!Core::ICore::resourcePath().isWritableDir()) {
    canLink = false;
    tip << QtOptionsPageWidget::tr("%1's resource directory is not writable.").arg(Core::Constants::IDE_DISPLAY_NAME);
  }
  // guard against redirecting Qt Creator that is part of a Qt installations
  // TODO this fails for pre-releases in the online installer
  // TODO this will fail when make Qt Creator non-required in the Qt installers
  if (installSettingsExist && !installSettingsValue) {
    canLink = false;
    tip << QtOptionsPageWidget::tr("%1 is part of a Qt installation.").arg(Core::Constants::IDE_DISPLAY_NAME);
  }
  const auto link = installSettingsValue ? *installSettingsValue : FilePath();
  if (!link.isEmpty())
    tip << QtOptionsPageWidget::tr("%1 is currently linked to \"%2\".").arg(QString(Core::Constants::IDE_DISPLAY_NAME), link.toUserOutput());
  if (toolTip)
    *toolTip = tip.join("\n\n");
  return canLink;
}

auto QtOptionsPageWidget::setupLinkWithQtButton() -> void
{
  QString tip;
  canLinkWithQt(&tip);
  m_ui.linkWithQtButton->setToolTip(tip);
  connect(m_ui.linkWithQtButton, &QPushButton::clicked, this, &QtOptionsPage::linkWithQt);
}

auto QtOptionsPageWidget::updateCurrentQtName() -> void
{
  const auto item = currentItem();
  if (!item || !item->version())
    return;

  item->setChanged(true);
  item->version()->setUnexpandedDisplayName(m_versionUi.nameEdit->text());

  updateDescriptionLabel();
  m_model->forItemsAtLevel<2>([this](QtVersionItem *item) { updateVersionItem(item); });
}

auto QtOptionsPageWidget::apply() -> void
{
  disconnect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &QtOptionsPageWidget::updateQtVersions);

  QtVersionManager::setDocumentationSetting(QtVersionManager::DocumentationSetting(m_ui.documentationSetting->currentData().toInt()));

  QtVersions versions;
  m_model->forItemsAtLevel<2>([&versions](QtVersionItem *item) {
    item->setChanged(false);
    versions.append(item->version()->clone());
  });
  QtVersionManager::setNewQtVersions(versions);

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &QtOptionsPageWidget::updateQtVersions);
}

// TODO whenever we move the output of sdktool to a different location in the installer,
// this needs to be adapted accordingly
const QStringList kSubdirsToCheck = {"", "Qt Creator.app/Contents/Resources", "Contents/Resources", "Tools/QtCreator/share/qtcreator", "share/qtcreator"};

static auto settingsFilesToCheck() -> QStringList
{
  return transform(kSubdirsToCheck, [](const QString &dir) { return settingsFile(dir); });
}

static auto qtversionFilesToCheck() -> QStringList
{
  return transform(kSubdirsToCheck, [](const QString &dir) { return qtVersionsFile(dir); });
}

static auto settingsDirForQtDir(const QString &qtDir) -> optional<QString>
{
  const auto dirsToCheck = transform(kSubdirsToCheck, [qtDir](const QString &dir) {
    return QString(qtDir + '/' + dir);
  });
  const auto validDir = findOrDefault(dirsToCheck, [](const QString &dir) {
    return QFile::exists(settingsFile(dir)) || QFile::exists(qtVersionsFile(dir));
  });
  if (!validDir.isEmpty())
    return validDir;
  return {};
}

static auto validateQtInstallDir(FancyLineEdit *input, QString *errorString) -> bool
{
  const auto qtDir = input->text();
  if (!settingsDirForQtDir(qtDir)) {
    if (errorString) {
      const auto filesToCheck = settingsFilesToCheck() + qtversionFilesToCheck();
      *errorString = "<html><body>" + QtOptionsPageWidget::tr("Qt installation information was not found in \"%1\". " "Choose a directory that contains one of the files %2").arg(qtDir, "<pre>" + filesToCheck.join('\n') + "</pre>");
    }
    return false;
  }
  return true;
}

static auto defaultQtInstallationPath() -> FilePath
{
  if (HostOsInfo::isWindowsHost())
    return FilePath::fromString({"C:/Qt"});
  return FileUtils::homePath() / "Qt";
}

auto QtOptionsPageWidget::linkWithQt() -> void
{
  const auto title = tr("Choose Qt Installation");
  const auto restartText = tr("The change will take effect after restart.");
  auto askForRestart = false;
  QDialog dialog(Core::ICore::dialogParent());
  dialog.setWindowTitle(title);
  auto layout = new QVBoxLayout;
  dialog.setLayout(layout);
  auto tipLabel = new QLabel(linkingPurposeText());
  tipLabel->setWordWrap(true);
  layout->addWidget(tipLabel);
  auto pathLayout = new QHBoxLayout;
  layout->addLayout(pathLayout);
  auto pathLabel = new QLabel(tr("Qt installation path:"));
  pathLabel->setToolTip(tr("Choose the Qt installation directory, or a directory that contains \"%1\".").arg(settingsFile("")));
  pathLayout->addWidget(pathLabel);
  auto pathInput = new PathChooser;
  pathLayout->addWidget(pathInput);
  pathInput->setExpectedKind(PathChooser::ExistingDirectory);
  pathInput->setPromptDialogTitle(title);
  pathInput->setMacroExpander(nullptr);
  pathInput->setValidationFunction([pathInput](FancyLineEdit *input, QString *errorString) {
    if (pathInput->defaultValidationFunction() && !pathInput->defaultValidationFunction()(input, errorString))
      return false;
    return validateQtInstallDir(input, errorString);
  });
  const auto currentLink = currentlyLinkedQtDir(nullptr);
  pathInput->setFilePath(currentLink ? *currentLink : defaultQtInstallationPath());
  auto buttons = new QDialogButtonBox;
  layout->addStretch(10);
  layout->addWidget(buttons);
  auto linkButton = buttons->addButton(tr("Link with Qt"), QDialogButtonBox::AcceptRole);
  connect(linkButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  auto cancelButton = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
  connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
  auto unlinkButton = buttons->addButton(tr("Remove Link"), QDialogButtonBox::DestructiveRole);
  unlinkButton->setEnabled(currentLink.has_value());
  connect(unlinkButton, &QPushButton::clicked, &dialog, [&dialog, &askForRestart] {
    auto removeSettingsFile = false;
    const auto filePath = settingsFile(Core::ICore::resourcePath().toString());
    {
      QSettings installSettings(filePath, QSettings::IniFormat);
      installSettings.remove(kInstallSettingsKey);
      if (installSettings.allKeys().isEmpty())
        removeSettingsFile = true;
    }
    if (removeSettingsFile)
      QFile::remove(filePath);
    askForRestart = true;
    dialog.reject();
  });
  connect(pathInput, &PathChooser::validChanged, linkButton, &QPushButton::setEnabled);
  linkButton->setEnabled(pathInput->isValid());

  dialog.exec();
  if (dialog.result() == QDialog::Accepted) {
    const auto settingsDir = settingsDirForQtDir(pathInput->rawPath());
    if (QTC_GUARD(settingsDir)) {
      QSettings(settingsFile(Core::ICore::resourcePath().toString()), QSettings::IniFormat).setValue(kInstallSettingsKey, *settingsDir);
      askForRestart = true;
    }
  }
  if (askForRestart) {
    Core::RestartDialog restartDialog(Core::ICore::dialogParent(), restartText);
    restartDialog.exec();
  }
}

// QtOptionsPage

QtOptionsPage::QtOptionsPage()
{
  setId(Constants::QTVERSION_SETTINGS_PAGE_ID);
  setDisplayName(QCoreApplication::translate("QtSupport", "Qt Versions"));
  setCategory(ProjectExplorer::Constants::KITS_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new QtOptionsPageWidget; });
}

auto QtOptionsPage::canLinkWithQt() -> bool
{
  return Internal::canLinkWithQt(nullptr);
}

auto QtOptionsPage::isLinkedWithQt() -> bool
{
  return currentlyLinkedQtDir(nullptr).has_value();
}

auto QtOptionsPage::linkWithQt() -> void
{
  QtOptionsPageWidget::linkWithQt();
}

} // namespace Internal
} // namespace QtSupport
