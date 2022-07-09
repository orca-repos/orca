// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientsettings.hpp"

#include "client.hpp"
#include "languageclientmanager.hpp"
#include "languageclient_global.hpp"
#include "languageclientinterface.hpp"

#include <core/editormanager/documentmodel.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>

#include <projectexplorer/project.hpp>
#include <projectexplorer/session.hpp>

#include <utils/algorithm.hpp>
#include <utils/utilsicons.hpp>
#include <utils/delegates.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/jsontreeitem.hpp>
#include <utils/stringutils.hpp>
#include <utils/variablechooser.hpp>

#include <QBoxLayout>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QToolButton>
#include <QTreeView>

constexpr char typeIdKey[] = "typeId";
constexpr char nameKey[] = "name";
constexpr char idKey[] = "id";
constexpr char enabledKey[] = "enabled";
constexpr char startupBehaviorKey[] = "startupBehavior";
constexpr char mimeTypeKey[] = "mimeType";
constexpr char filePatternKey[] = "filePattern";
constexpr char initializationOptionsKey[] = "initializationOptions";
constexpr char executableKey[] = "executable";
constexpr char argumentsKey[] = "arguments";
constexpr char settingsGroupKey[] = "LanguageClient";
constexpr char clientsKey[] = "clients";
constexpr char typedClientsKey[] = "typedClients";
constexpr char outlineSortedKey[] = "outlineSorted";
constexpr char mimeType[] = "application/language.client.setting";

namespace LanguageClient {

class LanguageClientSettingsModel : public QAbstractListModel {
public:
  LanguageClientSettingsModel() = default;
  ~LanguageClientSettingsModel() override;

  // QAbstractItemModel interface
  auto rowCount(const QModelIndex &/*parent*/ = QModelIndex()) const -> int final { return m_settings.count(); }
  auto data(const QModelIndex &index, int role) const -> QVariant final;
  auto removeRows(int row, int count = 1, const QModelIndex &parent = QModelIndex()) -> bool final;
  auto insertRows(int row, int count = 1, const QModelIndex &parent = QModelIndex()) -> bool final;
  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool final;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags final;
  auto supportedDropActions() const -> Qt::DropActions override { return Qt::MoveAction; }
  auto mimeTypes() const -> QStringList override { return {mimeType}; }
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool override;
  auto reset(const QList<BaseSettings*> &settings) -> void;
  auto settings() const -> QList<BaseSettings*> { return m_settings; }
  auto insertSettings(BaseSettings *settings) -> int;
  auto enableSetting(const QString &id) -> void;
  auto removed() const -> QList<BaseSettings*> { return m_removed; }
  auto settingForIndex(const QModelIndex &index) const -> BaseSettings*;
  auto indexForSetting(BaseSettings *setting) const -> QModelIndex;

private:
  static constexpr int idRole = Qt::UserRole + 1;
  QList<BaseSettings*> m_settings; // owned
  QList<BaseSettings*> m_removed;
};

class LanguageClientSettingsPageWidget : public QWidget {
public:
  LanguageClientSettingsPageWidget(LanguageClientSettingsModel &settings);
  auto currentChanged(const QModelIndex &index) -> void;
  auto currentRow() const -> int;
  auto resetCurrentSettings(int row) -> void;
  auto applyCurrentSettings() -> void;

private:
  LanguageClientSettingsModel &m_settings;
  QTreeView *m_view = nullptr;

  struct CurrentSettings {
    BaseSettings *setting = nullptr;
    QWidget *widget = nullptr;
  } m_currentSettings;

  auto addItem(const Utils::Id &clientTypeId) -> void;
  auto deleteItem() -> void;
};

class LanguageClientSettingsPage : public Core::IOptionsPage {
  Q_DECLARE_TR_FUNCTIONS(LanguageClientSettingsPage)

public:
  LanguageClientSettingsPage();
  ~LanguageClientSettingsPage() override;

  auto init() -> void;

  // IOptionsPage interface
  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;
  auto settings() const -> QList<BaseSettings*>;
  auto changedSettings() const -> QList<BaseSettings*>;
  auto addSettings(BaseSettings *settings) -> void;
  auto enableSettings(const QString &id) -> void;

private:
  LanguageClientSettingsModel m_model;
  QSet<QString> m_changedSettings;
  QPointer<LanguageClientSettingsPageWidget> m_widget;
};

auto clientTypes() -> QMap<Utils::Id, ClientType>&
{
  static QMap<Utils::Id, ClientType> types;
  return types;
}

LanguageClientSettingsPageWidget::LanguageClientSettingsPageWidget(LanguageClientSettingsModel &settings) : m_settings(settings), m_view(new QTreeView())
{
  const auto mainLayout = new QVBoxLayout();
  const auto layout = new QHBoxLayout();
  m_view->setModel(&m_settings);
  m_view->setHeaderHidden(true);
  m_view->setSelectionMode(QAbstractItemView::SingleSelection);
  m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_view->setDragEnabled(true);
  m_view->viewport()->setAcceptDrops(true);
  m_view->setDropIndicatorShown(true);
  m_view->setDragDropMode(QAbstractItemView::InternalMove);
  connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &LanguageClientSettingsPageWidget::currentChanged);
  const auto buttonLayout = new QVBoxLayout();
  const auto addButton = new QPushButton(LanguageClientSettingsPage::tr("&Add"));
  const auto addMenu = new QMenu;
  addMenu->clear();
  for (const auto &type : clientTypes()) {
    const auto action = new QAction(type.name);
    connect(action, &QAction::triggered, this, [this, id = type.id]() { addItem(id); });
    addMenu->addAction(action);
  }
  addButton->setMenu(addMenu);
  const auto deleteButton = new QPushButton(LanguageClientSettingsPage::tr("&Delete"));
  connect(deleteButton, &QPushButton::pressed, this, &LanguageClientSettingsPageWidget::deleteItem);
  mainLayout->addLayout(layout);
  setLayout(mainLayout);
  layout->addWidget(m_view);
  layout->addLayout(buttonLayout);
  buttonLayout->addWidget(addButton);
  buttonLayout->addWidget(deleteButton);
  buttonLayout->addStretch(10);
}

auto LanguageClientSettingsPageWidget::currentChanged(const QModelIndex &index) -> void
{
  if (m_currentSettings.widget) {
    applyCurrentSettings();
    layout()->removeWidget(m_currentSettings.widget);
    delete m_currentSettings.widget;
  }

  if (index.isValid()) {
    m_currentSettings.setting = m_settings.settingForIndex(index);
    m_currentSettings.widget = m_currentSettings.setting->createSettingsWidget(this);
    layout()->addWidget(m_currentSettings.widget);
  } else {
    m_currentSettings.setting = nullptr;
    m_currentSettings.widget = nullptr;
  }
}

auto LanguageClientSettingsPageWidget::currentRow() const -> int
{
  return m_settings.indexForSetting(m_currentSettings.setting).row();
}

auto LanguageClientSettingsPageWidget::resetCurrentSettings(int row) -> void
{
  if (m_currentSettings.widget) {
    layout()->removeWidget(m_currentSettings.widget);
    delete m_currentSettings.widget;
  }

  m_currentSettings.setting = nullptr;
  m_currentSettings.widget = nullptr;
  m_view->setCurrentIndex(m_settings.index(row));
}

auto LanguageClientSettingsPageWidget::applyCurrentSettings() -> void
{
  if (!m_currentSettings.setting)
    return;

  if (m_currentSettings.setting->applyFromSettingsWidget(m_currentSettings.widget)) {
    const auto index = m_settings.indexForSetting(m_currentSettings.setting);
    emit m_settings.dataChanged(index, index);
  }
}

auto generateSettings(const Utils::Id &clientTypeId) -> BaseSettings*
{
  if (const auto generator = clientTypes().value(clientTypeId).generator) {
    const auto settings = generator();
    settings->m_settingsTypeId = clientTypeId;
    return settings;
  }
  return nullptr;
}

auto LanguageClientSettingsPageWidget::addItem(const Utils::Id &clientTypeId) -> void
{
  const auto newSettings = generateSettings(clientTypeId);
  QTC_ASSERT(newSettings, return);
  m_view->setCurrentIndex(m_settings.index(m_settings.insertSettings(newSettings)));
}

auto LanguageClientSettingsPageWidget::deleteItem() -> void
{
  const auto index = m_view->currentIndex();
  if (!index.isValid())
    return;

  m_settings.removeRows(index.row());
}

LanguageClientSettingsPage::LanguageClientSettingsPage()
{
  setId(Constants::LANGUAGECLIENT_SETTINGS_PAGE);
  setDisplayName(tr("General"));
  setCategory(Constants::LANGUAGECLIENT_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("LanguageClient", Constants::LANGUAGECLIENT_SETTINGS_TR));
  setCategoryIconPath(":/languageclient/images/settingscategory_languageclient.png");
  connect(&m_model, &LanguageClientSettingsModel::dataChanged, [this](const QModelIndex &index) {
    if (const auto setting = m_model.settingForIndex(index))
      m_changedSettings << setting->m_id;
  });
}

LanguageClientSettingsPage::~LanguageClientSettingsPage()
{
  if (m_widget)
    delete m_widget;
}

auto LanguageClientSettingsPage::init() -> void
{
  m_model.reset(LanguageClientSettings::fromSettings(Core::ICore::settings()));
  apply();
  finish();
}

auto LanguageClientSettingsPage::widget() -> QWidget*
{
  if (!m_widget)
    m_widget = new LanguageClientSettingsPageWidget(m_model);
  return m_widget;
}

auto LanguageClientSettingsPage::apply() -> void
{
  if (m_widget)
    m_widget->applyCurrentSettings();
  LanguageClientManager::applySettings();

  for (const auto setting : m_model.removed()) {
    for (const auto client : LanguageClientManager::clientForSetting(setting))
      LanguageClientManager::shutdownClient(client);
  }

  if (m_widget) {
    const auto row = m_widget->currentRow();
    m_model.reset(LanguageClientManager::currentSettings());
    m_widget->resetCurrentSettings(row);
  } else {
    m_model.reset(LanguageClientManager::currentSettings());
  }
}

auto LanguageClientSettingsPage::finish() -> void
{
  m_model.reset(LanguageClientManager::currentSettings());
  m_changedSettings.clear();
}

auto LanguageClientSettingsPage::settings() const -> QList<BaseSettings*>
{
  return m_model.settings();
}

auto LanguageClientSettingsPage::changedSettings() const -> QList<BaseSettings*>
{
  QList<BaseSettings*> result;
  const auto &all = settings();
  for (const auto setting : all) {
    if (m_changedSettings.contains(setting->m_id))
      result << setting;
  }
  return result;
}

auto LanguageClientSettingsPage::addSettings(BaseSettings *settings) -> void
{
  m_model.insertSettings(settings);
  m_changedSettings << settings->m_id;
}

auto LanguageClientSettingsPage::enableSettings(const QString &id) -> void
{
  m_model.enableSetting(id);
}

LanguageClientSettingsModel::~LanguageClientSettingsModel()
{
  qDeleteAll(m_settings);
}

auto LanguageClientSettingsModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto setting = settingForIndex(index);
  if (!setting)
    return QVariant();
  if (role == Qt::DisplayRole)
    return Utils::globalMacroExpander()->expand(setting->m_name);
  else if (role == Qt::CheckStateRole)
    return setting->m_enabled ? Qt::Checked : Qt::Unchecked;
  else if (role == idRole)
    return setting->m_id;
  return QVariant();
}

auto LanguageClientSettingsModel::removeRows(int row, int count, const QModelIndex &parent) -> bool
{
  if (row >= int(m_settings.size()))
    return false;
  const auto end = qMin(row + count - 1, int(m_settings.size()) - 1);
  beginRemoveRows(parent, row, end);
  for (auto i = end; i >= row; --i)
    m_removed << m_settings.takeAt(i);
  endRemoveRows();
  return true;
}

auto LanguageClientSettingsModel::insertRows(int row, int count, const QModelIndex &parent) -> bool
{
  if (row > m_settings.size() || row < 0)
    return false;
  beginInsertRows(parent, row, row + count - 1);
  for (auto i = 0; i < count; ++i)
    m_settings.insert(row + i, new StdIOSettings());
  endInsertRows();
  return true;
}

auto LanguageClientSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role) -> bool
{
  const auto setting = settingForIndex(index);
  if (!setting || role != Qt::CheckStateRole)
    return false;

  if (setting->m_enabled != value.toBool()) {
    setting->m_enabled = !setting->m_enabled;
    emit dataChanged(index, index, {Qt::CheckStateRole});
  }
  return true;
}

auto LanguageClientSettingsModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  const Qt::ItemFlags dragndropFlags = index.isValid() ? Qt::ItemIsDragEnabled : Qt::ItemIsDropEnabled;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | dragndropFlags;
}

auto LanguageClientSettingsModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  QTC_ASSERT(indexes.count() == 1, return nullptr);

  const auto mimeData = new QMimeData;
  QByteArray encodedData;

  QDataStream stream(&encodedData, QIODevice::WriteOnly);

  for (const auto &index : indexes) {
    if (index.isValid())
      stream << data(index, idRole).toString();
  }

  mimeData->setData(mimeType, indexes.first().data(idRole).toString().toUtf8());
  return mimeData;
}

auto LanguageClientSettingsModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool
{
  if (!canDropMimeData(data, action, row, column, parent))
    return false;

  if (action == Qt::IgnoreAction)
    return true;

  const auto id = QString::fromUtf8(data->data(mimeType));
  const auto setting = Utils::findOrDefault(m_settings, [id](const BaseSettings *setting) {
    return setting->m_id == id;
  });
  if (!setting)
    return false;

  if (row == -1)
    row = parent.isValid() ? parent.row() : rowCount(QModelIndex());

  beginInsertRows(parent, row, row);
  m_settings.insert(row, setting->copy());
  endInsertRows();

  return true;
}

auto LanguageClientSettingsModel::reset(const QList<BaseSettings*> &settings) -> void
{
  beginResetModel();
  qDeleteAll(m_settings);
  qDeleteAll(m_removed);
  m_removed.clear();
  m_settings = Utils::transform(settings, [](const BaseSettings *other) { return other->copy(); });
  endResetModel();
}

auto LanguageClientSettingsModel::insertSettings(BaseSettings *settings) -> int
{
  const auto row = rowCount();
  beginInsertRows(QModelIndex(), row, row);
  m_settings.insert(row, settings);
  endInsertRows();
  return row;
}

auto LanguageClientSettingsModel::enableSetting(const QString &id) -> void
{
  const auto setting = Utils::findOrDefault(m_settings, Utils::equal(&BaseSettings::m_id, id));
  if (!setting)
    return;
  setting->m_enabled = true;
  const auto &index = indexForSetting(setting);
  if (index.isValid()) emit dataChanged(index, index, {Qt::CheckStateRole});
}

auto LanguageClientSettingsModel::settingForIndex(const QModelIndex &index) const -> BaseSettings*
{
  if (!index.isValid() || index.row() >= m_settings.size())
    return nullptr;
  return m_settings[index.row()];
}

auto LanguageClientSettingsModel::indexForSetting(BaseSettings *setting) const -> QModelIndex
{
  const int index = m_settings.indexOf(setting);
  return index < 0 ? QModelIndex() : createIndex(index, 0, setting);
}

auto BaseSettings::initializationOptions() const -> QJsonObject
{
  return QJsonDocument::fromJson(Utils::globalMacroExpander()->expand(m_initializationOptions).toUtf8()).object();
}

auto BaseSettings::applyFromSettingsWidget(QWidget *widget) -> bool
{
  auto changed = false;
  if (const auto settingsWidget = qobject_cast<BaseSettingsWidget*>(widget)) {
    if (m_name != settingsWidget->name()) {
      m_name = settingsWidget->name();
      changed = true;
    }
    if (m_languageFilter != settingsWidget->filter()) {
      m_languageFilter = settingsWidget->filter();
      changed = true;
    }
    if (m_startBehavior != settingsWidget->startupBehavior()) {
      m_startBehavior = settingsWidget->startupBehavior();
      changed = true;
    }
    if (m_initializationOptions != settingsWidget->initializationOptions()) {
      m_initializationOptions = settingsWidget->initializationOptions();
      changed = true;
    }
  }
  return changed;
}

auto BaseSettings::createSettingsWidget(QWidget *parent) const -> QWidget*
{
  return new BaseSettingsWidget(this, parent);
}

auto BaseSettings::isValid() const -> bool
{
  return !m_name.isEmpty();
}

auto BaseSettings::createClient() -> Client*
{
  return createClient(nullptr);
}

auto BaseSettings::createClient(ProjectExplorer::Project *project) -> Client*
{
  if (!isValid() || !m_enabled)
    return nullptr;
  const auto interface = createInterfaceWithProject(project);
  QTC_ASSERT(interface, return nullptr);
  auto *client = createClient(interface);
  client->setName(Utils::globalMacroExpander()->expand(m_name));
  client->setSupportedLanguage(m_languageFilter);
  client->setInitializationOptions(initializationOptions());
  client->setActivateDocumentAutomatically(true);
  client->setCurrentProject(project);
  return client;
}

auto BaseSettings::createClient(BaseClientInterface *interface) const -> Client*
{
  return new Client(interface);
}

auto BaseSettings::toMap() const -> QVariantMap
{
  QVariantMap map;
  map.insert(typeIdKey, m_settingsTypeId.toSetting());
  map.insert(nameKey, m_name);
  map.insert(idKey, m_id);
  map.insert(enabledKey, m_enabled);
  map.insert(startupBehaviorKey, m_startBehavior);
  map.insert(mimeTypeKey, m_languageFilter.mimeTypes);
  map.insert(filePatternKey, m_languageFilter.filePattern);
  map.insert(initializationOptionsKey, m_initializationOptions);
  return map;
}

auto BaseSettings::fromMap(const QVariantMap &map) -> void
{
  m_name = map[nameKey].toString();
  m_id = map.value(idKey, QUuid::createUuid().toString()).toString();
  m_enabled = map[enabledKey].toBool();
  m_startBehavior = BaseSettings::StartBehavior(map.value(startupBehaviorKey, BaseSettings::RequiresFile).toInt());
  m_languageFilter.mimeTypes = map[mimeTypeKey].toStringList();
  m_languageFilter.filePattern = map[filePatternKey].toStringList();
  m_languageFilter.filePattern.removeAll(QString()); // remove empty entries
  m_initializationOptions = map[initializationOptionsKey].toString();
}

static auto settingsPage() -> LanguageClientSettingsPage&
{
  static LanguageClientSettingsPage settingsPage;
  return settingsPage;
}

auto LanguageClientSettings::init() -> void
{
  settingsPage().init();
}

auto LanguageClientSettings::fromSettings(QSettings *settingsIn) -> QList<BaseSettings*>
{
  settingsIn->beginGroup(settingsGroupKey);
  QList<BaseSettings*> result;

  for (auto varList : {settingsIn->value(clientsKey).toList(), settingsIn->value(typedClientsKey).toList()}) {
    for (const auto &var : varList) {
      const auto &map = var.toMap();
      auto typeId = Utils::Id::fromSetting(map.value(typeIdKey));
      if (!typeId.isValid())
        typeId = Constants::LANGUAGECLIENT_STDIO_SETTINGS_ID;
      if (const auto settings = generateSettings(typeId)) {
        settings->fromMap(map);
        result << settings;
      }
    }
  }

  settingsIn->endGroup();
  return result;
}

auto LanguageClientSettings::pageSettings() -> QList<BaseSettings*>
{
  return settingsPage().settings();
}

auto LanguageClientSettings::changedSettings() -> QList<BaseSettings*>
{
  return settingsPage().changedSettings();
}

auto LanguageClientSettings::registerClientType(const ClientType &type) -> void
{
  QTC_ASSERT(!clientTypes().contains(type.id), return);
  clientTypes()[type.id] = type;
}

auto LanguageClientSettings::addSettings(BaseSettings *settings) -> void
{
  settingsPage().addSettings(settings);
}

auto LanguageClientSettings::enableSettings(const QString &id) -> void
{
  settingsPage().enableSettings(id);
}

auto LanguageClientSettings::toSettings(QSettings *settings, const QList<BaseSettings*> &languageClientSettings) -> void
{
  settings->beginGroup(settingsGroupKey);
  auto transform = [](const QList<BaseSettings*> &settings) {
    return Utils::transform(settings, [](const BaseSettings *setting) {
      return QVariant(setting->toMap());
    });
  };
  const auto isStdioSetting = Utils::equal(&BaseSettings::m_settingsTypeId, Utils::Id(Constants::LANGUAGECLIENT_STDIO_SETTINGS_ID));
  auto [stdioSettings, typedSettings] = Utils::partition(languageClientSettings, isStdioSetting);
  settings->setValue(clientsKey, transform(stdioSettings));
  settings->setValue(typedClientsKey, transform(typedSettings));
  settings->endGroup();
}

auto LanguageClientSettings::outlineComboBoxIsSorted() -> bool
{
  const auto settings = Core::ICore::settings();
  settings->beginGroup(settingsGroupKey);
  const auto sorted = settings->value(outlineSortedKey).toBool();
  settings->endGroup();
  return sorted;
}

auto LanguageClientSettings::setOutlineComboBoxSorted(bool sorted) -> void
{
  const auto settings = Core::ICore::settings();
  settings->beginGroup(settingsGroupKey);
  settings->setValue(outlineSortedKey, sorted);
  settings->endGroup();
}

auto StdIOSettings::applyFromSettingsWidget(QWidget *widget) -> bool
{
  auto changed = false;
  if (const auto settingsWidget = qobject_cast<StdIOSettingsWidget*>(widget)) {
    changed = BaseSettings::applyFromSettingsWidget(settingsWidget);
    if (m_executable != settingsWidget->executable()) {
      m_executable = settingsWidget->executable();
      changed = true;
    }
    if (m_arguments != settingsWidget->arguments()) {
      m_arguments = settingsWidget->arguments();
      changed = true;
    }
  }
  return changed;
}

auto StdIOSettings::createSettingsWidget(QWidget *parent) const -> QWidget*
{
  return new StdIOSettingsWidget(this, parent);
}

auto StdIOSettings::isValid() const -> bool
{
  return BaseSettings::isValid() && !m_executable.isEmpty();
}

auto StdIOSettings::toMap() const -> QVariantMap
{
  auto map = BaseSettings::toMap();
  map.insert(executableKey, m_executable.toVariant());
  map.insert(argumentsKey, m_arguments);
  return map;
}

auto StdIOSettings::fromMap(const QVariantMap &map) -> void
{
  BaseSettings::fromMap(map);
  m_executable = Utils::FilePath::fromVariant(map[executableKey]);
  m_arguments = map[argumentsKey].toString();
}

auto StdIOSettings::arguments() const -> QString
{
  return Utils::globalMacroExpander()->expand(m_arguments);
}

auto StdIOSettings::command() const -> Utils::CommandLine
{
  return Utils::CommandLine(m_executable, arguments(), Utils::CommandLine::Raw);
}

auto StdIOSettings::createInterfaceWithProject(ProjectExplorer::Project *project) const -> BaseClientInterface*
{
  const auto interface = new StdIOClientInterface;
  interface->setCommandLine(command());
  if (project)
    interface->setWorkingDirectory(project->projectDirectory());
  return interface;
}

class JsonTreeItemDelegate : public QStyledItemDelegate {
public:
  auto displayText(const QVariant &value, const QLocale &) const -> QString override
  {
    auto result = value.toString();
    if (result.size() == 1) {
      switch (result.at(0).toLatin1()) {
      case '\n':
        return QString("\\n");
      case '\t':
        return QString("\\t");
      case '\r':
        return QString("\\r");
      }
    }
    return result;
  }
};

static auto startupBehaviorString(BaseSettings::StartBehavior behavior) -> QString
{
  switch (behavior) {
  case BaseSettings::AlwaysOn:
    return QCoreApplication::translate("LanguageClient::BaseSettings", "Always On");
  case BaseSettings::RequiresFile:
    return QCoreApplication::translate("LanguageClient::BaseSettings", "Requires an Open File");
  case BaseSettings::RequiresProject:
    return QCoreApplication::translate("LanguageClient::BaseSettings", "Start Server per Project");
  default:
    break;
  }
  return {};
}

BaseSettingsWidget::BaseSettingsWidget(const BaseSettings *settings, QWidget *parent) : QWidget(parent), m_name(new QLineEdit(settings->m_name, this)), m_mimeTypes(new QLabel(settings->m_languageFilter.mimeTypes.join(filterSeparator), this)), m_filePattern(new QLineEdit(settings->m_languageFilter.filePattern.join(filterSeparator), this)), m_startupBehavior(new QComboBox), m_initializationOptions(new Utils::FancyLineEdit(this))
{
  auto row = 0;
  auto *mainLayout = new QGridLayout;

  mainLayout->addWidget(new QLabel(tr("Name:")), row, 0);
  mainLayout->addWidget(m_name, row, 1);
  const auto chooser = new Utils::VariableChooser(this);
  chooser->addSupportedWidget(m_name);

  mainLayout->addWidget(new QLabel(tr("Language:")), ++row, 0);
  const auto mimeLayout = new QHBoxLayout;
  mimeLayout->addWidget(m_mimeTypes);
  mimeLayout->addStretch();
  const auto addMimeTypeButton = new QPushButton(tr("Set MIME Types..."), this);
  mimeLayout->addWidget(addMimeTypeButton);
  mainLayout->addLayout(mimeLayout, row, 1);
  m_filePattern->setPlaceholderText(tr("File pattern"));
  mainLayout->addWidget(m_filePattern, ++row, 1);

  mainLayout->addWidget(new QLabel(tr("Startup behavior:")), ++row, 0);
  for (auto behavior = 0; behavior < BaseSettings::LastSentinel; ++behavior)
    m_startupBehavior->addItem(startupBehaviorString(BaseSettings::StartBehavior(behavior)));
  m_startupBehavior->setCurrentIndex(settings->m_startBehavior);
  mainLayout->addWidget(m_startupBehavior, row, 1);

  connect(addMimeTypeButton, &QPushButton::pressed, this, &BaseSettingsWidget::showAddMimeTypeDialog);

  mainLayout->addWidget(new QLabel(tr("Initialization options:")), ++row, 0);
  mainLayout->addWidget(m_initializationOptions, row, 1);
  chooser->addSupportedWidget(m_initializationOptions);
  m_initializationOptions->setValidationFunction([](Utils::FancyLineEdit *edit, QString *errorMessage) {
    const auto value = Utils::globalMacroExpander()->expand(edit->text());

    if (value.isEmpty())
      return true;

    QJsonParseError parseInfo;
    const auto json = QJsonDocument::fromJson(value.toUtf8(), &parseInfo);

    if (json.isNull()) {
      if (errorMessage)
        *errorMessage = tr("Failed to parse JSON at %1: %2").arg(parseInfo.offset).arg(parseInfo.errorString());
      return false;
    }
    return true;
  });
  m_initializationOptions->setText(settings->m_initializationOptions);
  m_initializationOptions->setPlaceholderText(tr("Language server-specific JSON to pass via " "\"initializationOptions\" field of \"initialize\" " "request."));

  setLayout(mainLayout);
}

auto BaseSettingsWidget::name() const -> QString
{
  return m_name->text();
}

auto BaseSettingsWidget::filter() const -> LanguageFilter
{
  return {m_mimeTypes->text().split(filterSeparator, Qt::SkipEmptyParts), m_filePattern->text().split(filterSeparator, Qt::SkipEmptyParts)};
}

auto BaseSettingsWidget::startupBehavior() const -> BaseSettings::StartBehavior
{
  return BaseSettings::StartBehavior(m_startupBehavior->currentIndex());
}

auto BaseSettingsWidget::initializationOptions() const -> QString
{
  return m_initializationOptions->text();
}

class MimeTypeModel : public QStringListModel {
public:
  using QStringListModel::QStringListModel;

  auto data(const QModelIndex &index, int role) const -> QVariant final
  {
    if (index.isValid() && role == Qt::CheckStateRole)
      return m_selectedMimeTypes.contains(index.data().toString()) ? Qt::Checked : Qt::Unchecked;
    return QStringListModel::data(index, role);
  }

  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool final
  {
    if (index.isValid() && role == Qt::CheckStateRole) {
      const auto mimeType = index.data().toString();
      if (value.toInt() == Qt::Checked) {
        if (!m_selectedMimeTypes.contains(mimeType))
          m_selectedMimeTypes.append(index.data().toString());
      } else {
        m_selectedMimeTypes.removeAll(index.data().toString());
      }
      return true;
    }
    return QStringListModel::setData(index, value, role);
  }

  auto flags(const QModelIndex &index) const -> Qt::ItemFlags final
  {
    if (!index.isValid())
      return Qt::NoItemFlags;
    return (QStringListModel::flags(index) & ~(Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled)) | Qt::ItemIsUserCheckable;
  }

  QStringList m_selectedMimeTypes;
};

class MimeTypeDialog : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(MimeTypeDialog)
public:
  explicit MimeTypeDialog(const QStringList &selectedMimeTypes, QWidget *parent = nullptr) : QDialog(parent)
  {
    setWindowTitle(tr("Select MIME Types"));
    const auto mainLayout = new QVBoxLayout;
    const auto filter = new Utils::FancyLineEdit(this);
    filter->setFiltering(true);
    mainLayout->addWidget(filter);
    const auto listView = new QListView(this);
    mainLayout->addWidget(listView);
    const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);
    setLayout(mainLayout);

    filter->setPlaceholderText(tr("Filter"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    const auto proxy = new QSortFilterProxyModel(this);
    m_mimeTypeModel = new MimeTypeModel(Utils::transform(Utils::allMimeTypes(), &Utils::MimeType::name), this);
    m_mimeTypeModel->m_selectedMimeTypes = selectedMimeTypes;
    proxy->setSourceModel(m_mimeTypeModel);
    proxy->sort(0);
    connect(filter, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterWildcard);
    listView->setModel(proxy);

    setModal(true);
  }

  MimeTypeDialog(const MimeTypeDialog &other) = delete;
  MimeTypeDialog(MimeTypeDialog &&other) = delete;

  auto operator=(const MimeTypeDialog &other) -> MimeTypeDialog = delete;
  auto operator=(MimeTypeDialog &&other) -> MimeTypeDialog = delete;

  auto mimeTypes() const -> QStringList
  {
    return m_mimeTypeModel->m_selectedMimeTypes;
  }

private:
  MimeTypeModel *m_mimeTypeModel = nullptr;
};

auto BaseSettingsWidget::showAddMimeTypeDialog() -> void
{
  MimeTypeDialog dialog(m_mimeTypes->text().split(filterSeparator, Qt::SkipEmptyParts), Core::ICore::dialogParent());
  if (dialog.exec() == QDialog::Rejected)
    return;
  m_mimeTypes->setText(dialog.mimeTypes().join(filterSeparator));
}

StdIOSettingsWidget::StdIOSettingsWidget(const StdIOSettings *settings, QWidget *parent) : BaseSettingsWidget(settings, parent), m_executable(new Utils::PathChooser(this)), m_arguments(new QLineEdit(settings->m_arguments, this))
{
  const auto mainLayout = qobject_cast<QGridLayout*>(layout());
  QTC_ASSERT(mainLayout, return);
  const auto baseRows = mainLayout->rowCount();
  mainLayout->addWidget(new QLabel(tr("Executable:")), baseRows, 0);
  mainLayout->addWidget(m_executable, baseRows, 1);
  mainLayout->addWidget(new QLabel(tr("Arguments:")), baseRows + 1, 0);
  m_executable->setExpectedKind(Utils::PathChooser::ExistingCommand);
  m_executable->setFilePath(settings->m_executable);
  mainLayout->addWidget(m_arguments, baseRows + 1, 1);

  const auto chooser = new Utils::VariableChooser(this);
  chooser->addSupportedWidget(m_arguments);
}

auto StdIOSettingsWidget::executable() const -> Utils::FilePath
{
  return m_executable->filePath();
}

auto StdIOSettingsWidget::arguments() const -> QString
{
  return m_arguments->text();
}

auto LanguageFilter::isSupported(const Utils::FilePath &filePath, const QString &mimeType) const -> bool
{
  if (mimeTypes.contains(mimeType))
    return true;
  if (filePattern.isEmpty() && filePath.isEmpty())
    return mimeTypes.isEmpty();
  const QRegularExpression::PatternOptions options = Utils::HostOsInfo::fileNameCaseSensitivity() == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
  const auto regexps = Utils::transform(filePattern, [&options](const QString &pattern) {
    return QRegularExpression(QRegularExpression::wildcardToRegularExpression(pattern), options);
  });
  return Utils::anyOf(regexps, [filePath](const QRegularExpression &reg) {
    return reg.match(filePath.toString()).hasMatch() || reg.match(filePath.fileName()).hasMatch();
  });
}

auto LanguageFilter::isSupported(const Core::IDocument *document) const -> bool
{
  return isSupported(document->filePath(), document->mimeType());
}

auto LanguageFilter::operator==(const LanguageFilter &other) const -> bool
{
  return this->filePattern == other.filePattern && this->mimeTypes == other.mimeTypes;
}

auto LanguageFilter::operator!=(const LanguageFilter &other) const -> bool
{
  return this->filePattern != other.filePattern || this->mimeTypes != other.mimeTypes;
}

} // namespace LanguageClient
