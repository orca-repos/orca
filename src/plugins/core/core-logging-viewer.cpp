// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-logging-viewer.hpp"

#include "core-action-manager.hpp"
#include "core-icons.hpp"
#include "core-interface.hpp"
#include "core-logging-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/basetreeview.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/listmodel.hpp>
#include <utils/utilsicons.hpp>
#include <utils/theme/theme.hpp>

#include <QAction>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeView>

namespace Orca::Plugin::Core {

class LoggingCategoryItem {
public:
  QString name;
  LoggingCategoryEntry entry;

  static auto fromJson(const QJsonObject &object, bool *ok) -> LoggingCategoryItem;
};

auto LoggingCategoryItem::fromJson(const QJsonObject &object, bool *ok) -> LoggingCategoryItem
{
  if (!object.contains("name")) {
    *ok = false;
    return {};
  }

  const auto entry_val = object.value("entry");

  if (entry_val.isUndefined()) {
    *ok = false;
    return {};
  }

  const auto entry_obj = entry_val.toObject();

  if (!entry_obj.contains("level")) {
    *ok = false;
    return {};
  }

  LoggingCategoryEntry entry;
  entry.level = static_cast<QtMsgType>(entry_obj.value("level").toInt());
  entry.enabled = true;

  if (entry_obj.contains("color"))
    entry.color = QColor(entry_obj.value("color").toString());

  LoggingCategoryItem item{object.value("name").toString(), entry};
  *ok = true;
  return item;
}

class LoggingCategoryModel final : public QAbstractListModel {
  Q_OBJECT

public:
  LoggingCategoryModel() = default;
  ~LoggingCategoryModel() override;

  auto append(const QString &category, const LoggingCategoryEntry &entry = {}) -> bool;
  auto update(const QString &category, const LoggingCategoryEntry &entry) -> bool;
  auto columnCount(const QModelIndex &) const -> int override { return 3; }
  auto rowCount(const QModelIndex & = QModelIndex()) const -> int override { return static_cast<int>(m_categories.count()); }
  auto data(const QModelIndex &index, int role) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
  auto reset() -> void;
  auto setFromManager(const LoggingViewManager *manager) -> void;
  auto enabledCategories() const -> QList<LoggingCategoryItem>;
  auto disableAll() -> void;

signals:
  auto categoryChanged(const QString &category, bool enabled) -> void;
  auto colorChanged(const QString &category, const QColor &color) -> void;
  auto logLevelChanged(const QString &category, QtMsgType logLevel) -> void;

private:
  QList<LoggingCategoryItem*> m_categories;
};

LoggingCategoryModel::~LoggingCategoryModel()
{
  reset();
}

auto LoggingCategoryModel::append(const QString &category, const LoggingCategoryEntry &entry) -> bool
{
  // no check?
  beginInsertRows(QModelIndex(), static_cast<int>(m_categories.size()), static_cast<int>(m_categories.size()));
  m_categories.append(new LoggingCategoryItem{category, entry});
  endInsertRows();
  return true;
}

auto LoggingCategoryModel::update(const QString &category, const LoggingCategoryEntry &entry) -> bool
{
  if (m_categories.empty()) // should not happen
    return false;

  auto row = 0;

  for (const auto end = static_cast<int>(m_categories.size()); row < end; ++row) {
    if (m_categories.at(row)->name == category)
      break;
  }

  if (row == m_categories.size()) // should not happen
    return false;

  setData(index(row, 0), Qt::Checked, Qt::CheckStateRole);
  setData(index(row, 1), LoggingViewManager::messageTypeToString(entry.level), Qt::EditRole);
  setData(index(row, 2), entry.color, Qt::DecorationRole);

  return true;
}

auto LoggingCategoryModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  static const auto default_color = Utils::orcaTheme()->palette().text().color();

  if (!index.isValid())
    return {};

  if (role == Qt::DisplayRole) {
    if (index.column() == 0)
      return m_categories.at(index.row())->name;
    if (index.column() == 1) {
      return LoggingViewManager::messageTypeToString(m_categories.at(index.row())->entry.level);
    }
  }

  if (role == Qt::DecorationRole && index.column() == 2) {
    if (const auto color = m_categories.at(index.row())->entry.color; color.isValid())
      return color;
    return default_color;
  }

  if (role == Qt::CheckStateRole && index.column() == 0) {
    const auto entry = m_categories.at(index.row())->entry;
    return entry.enabled ? Qt::Checked : Qt::Unchecked;
  }

  return {};
}

auto LoggingCategoryModel::setData(const QModelIndex &index, const QVariant &value, const int role) -> bool
{
  if (!index.isValid())
    return false;

  if (role == Qt::CheckStateRole && index.column() == 0) {
    const auto item = m_categories.at(index.row());
    if (const auto current = item->entry.enabled ? Qt::Checked : Qt::Unchecked; current != value.toInt()) {
      item->entry.enabled = !item->entry.enabled;
      emit categoryChanged(item->name, item->entry.enabled);
      return true;
    }
  } else if (role == Qt::DecorationRole && index.column() == 2) {
    const auto item = m_categories.at(index.row());
    if (const auto color = value.value<QColor>(); color.isValid() && color != item->entry.color) {
      item->entry.color = color;
      emit colorChanged(item->name, color);
      return true;
    }
  } else if (role == Qt::EditRole && index.column() == 1) {
    const auto item = m_categories.at(index.row());
    item->entry.level = LoggingViewManager::messageTypeFromString(value.toString());
    emit logLevelChanged(item->name, item->entry.level);
    return true;
  }

  return false;
}

auto LoggingCategoryModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  if (!index.isValid())
    return Qt::NoItemFlags;

  // ItemIsEnabled should depend on availability (Qt logging enabled?)
  if (index.column() == 0)
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  if (index.column() == 1)
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

auto LoggingCategoryModel::headerData(const int section, const Qt::Orientation orientation, const int role) const -> QVariant
{
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section >= 0 && section < 3) {
    switch (section) {
    case 0:
      return tr("Category");
    case 1:
      return tr("Type");
    case 2:
      return tr("Color");
    default:
      break;
    }
  }
  return {};
}

auto LoggingCategoryModel::reset() -> void
{
  beginResetModel();
  qDeleteAll(m_categories);
  m_categories.clear();
  endResetModel();
}

auto LoggingCategoryModel::setFromManager(const LoggingViewManager *manager) -> void
{
  beginResetModel();
  qDeleteAll(m_categories);
  m_categories.clear();

  const auto categories = manager->categories();
  auto it = categories.begin();

  for (const auto end = categories.end(); it != end; ++it)
    m_categories.append(new LoggingCategoryItem{it.key(), it.value()});

  endResetModel();
}

auto LoggingCategoryModel::enabledCategories() const -> QList<LoggingCategoryItem>
{
  QList<LoggingCategoryItem> result;

  for (const auto item : m_categories) {
    if (item->entry.enabled)
      result.append({item->name, item->entry});
  }

  return result;
}

auto LoggingCategoryModel::disableAll() -> void
{
  for (auto row = 0, end = static_cast<int>(m_categories.count()); row < end; ++row)
    setData(index(row, 0), Qt::Unchecked, Qt::CheckStateRole);
}

class LoggingLevelDelegate : public QStyledItemDelegate {
public:
  explicit LoggingLevelDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
  ~LoggingLevelDelegate() = default;

protected:
  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* override;
  auto setEditorData(QWidget *editor, const QModelIndex &index) const -> void override;
  auto setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void override;
};

auto LoggingLevelDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &index) const -> QWidget*
{
  if (!index.isValid() || index.column() != 1)
    return nullptr;

  const auto combo = new QComboBox(parent);
  combo->addItems({{"Critical"}, {"Warning"}, {"Debug"}, {"Info"}});

  return combo;
}

auto LoggingLevelDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const -> void
{
  const auto combo = qobject_cast<QComboBox*>(editor);

  if (!combo)
    return;

  if (const auto i = combo->findText(index.data().toString()); i >= 0)
    combo->setCurrentIndex(i);
}

auto LoggingLevelDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void
{
  if (const auto combo = qobject_cast<QComboBox*>(editor))
    model->setData(index, combo->currentText());
}

class LogEntry {
public:
  QString timestamp;
  QString category;
  QString type;
  QString message;

  auto outputLine(const bool print_timestamp, const bool print_type) const -> QString
  {
    QString line;
    if (print_timestamp)
      line.append(timestamp + ' ');
    line.append(category);
    if (print_type)
      line.append('.' + type.toLower());
    line.append(": ");
    line.append(message);
    line.append('\n');
    return line;
  }
};

class LoggingViewManagerWidget : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(LoggingViewManagerWidget)

public:
  explicit LoggingViewManagerWidget(QWidget *parent);

  ~LoggingViewManagerWidget() override
  {
    setEnabled(false);
    delete m_manager;
  }

  static auto colorForCategory(const QString &category) -> QColor;

private:
  auto showLogViewContextMenu(const QPoint &pos) const -> void;
  auto showLogCategoryContextMenu(const QPoint &pos) const -> void;
  auto saveLoggingsToFile() const -> void;
  auto saveEnabledCategoryPreset() const -> void;
  auto loadAndUpdateFromPreset() const -> void;
  LoggingViewManager *m_manager = nullptr;
  static auto setCategoryColor(const QString &category, const QColor &color) -> void;
  // should category model be owned directly by the manager? or is this duplication of
  // categories in manager and widget beneficial?
  LoggingCategoryModel *m_category_model = nullptr;
  Utils::BaseTreeView *m_log_view = nullptr;
  Utils::BaseTreeView *m_category_view = nullptr;
  Utils::ListModel<LogEntry> *m_log_model = nullptr;
  QToolButton *m_timestamps = nullptr;
  QToolButton *m_message_types = nullptr;
  static QHash<QString, QColor> m_category_color;
};

QHash<QString, QColor> LoggingViewManagerWidget::m_category_color;

static auto logEntryDataAccessor(const LogEntry &entry, const int column, const int role) -> QVariant
{
  if (column >= 0 && column <= 3 && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
    switch (column) {
    case 0:
      return entry.timestamp;
    case 1:
      return entry.category;
    case 2:
      return entry.type;
    case 3: {
      if (role == Qt::ToolTipRole)
        return entry.message;
      return entry.message.left(1000);
    }
    default:
      break;
    }
  }

  if (role == Qt::TextAlignmentRole)
    return Qt::AlignTop;

  if (column == 1 && role == Qt::ForegroundRole)
    return LoggingViewManagerWidget::colorForCategory(entry.category);

  return {};
}

LoggingViewManagerWidget::LoggingViewManagerWidget(QWidget *parent) : QDialog(parent), m_manager(new LoggingViewManager)
{
  setWindowTitle(tr("Logging Category Viewer"));
  setModal(false);

  const auto main_layout = new QVBoxLayout;

  const auto buttons_layout = new QHBoxLayout;
  buttons_layout->setSpacing(0);

  // add further buttons..
  const auto save = new QToolButton;
  save->setIcon(Utils::Icons::SAVEFILE.icon());
  save->setToolTip(tr("Save Log"));
  buttons_layout->addWidget(save);

  const auto clean = new QToolButton;
  clean->setIcon(Utils::Icons::CLEAN.icon());
  clean->setToolTip(tr("Clear"));
  buttons_layout->addWidget(clean);

  auto stop = new QToolButton;
  stop->setIcon(Utils::Icons::STOP_SMALL.icon());
  stop->setToolTip(tr("Stop Logging"));
  buttons_layout->addWidget(stop);

  const auto qt_internal = new QToolButton;
  qt_internal->setIcon(ORCALOGO.icon());
  qt_internal->setToolTip(tr("Toggle Qt Internal Logging"));
  qt_internal->setCheckable(true);
  qt_internal->setChecked(false);
  buttons_layout->addWidget(qt_internal);

  auto auto_scroll = new QToolButton;
  auto_scroll->setIcon(Utils::Icons::ARROW_DOWN.icon());
  auto_scroll->setToolTip(tr("Auto Scroll"));
  auto_scroll->setCheckable(true);
  auto_scroll->setChecked(true);
  buttons_layout->addWidget(auto_scroll);

  m_timestamps = new QToolButton;
  auto icon = Utils::Icon({{":/utils/images/stopwatch.png", Utils::Theme::PanelTextColorMid}}, Utils::Icon::Tint);
  m_timestamps->setIcon(icon.icon());
  m_timestamps->setToolTip(tr("Timestamps"));
  m_timestamps->setCheckable(true);
  m_timestamps->setChecked(true);
  buttons_layout->addWidget(m_timestamps);

  m_message_types = new QToolButton;
  icon = Utils::Icon({{":/utils/images/message.png", Utils::Theme::PanelTextColorMid}}, Utils::Icon::Tint);
  m_message_types->setIcon(icon.icon());
  m_message_types->setToolTip(tr("Message Types"));
  m_message_types->setCheckable(true);
  m_message_types->setChecked(false);
  buttons_layout->addWidget(m_message_types);

  buttons_layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding));
  main_layout->addLayout(buttons_layout);

  const auto horizontal = new QHBoxLayout;
  m_log_view = new Utils::BaseTreeView;
  m_log_model = new Utils::ListModel<LogEntry>;
  m_log_model->setHeader({tr("Timestamp"), tr("Category"), tr("Type"), tr("Message")});
  m_log_model->setDataAccessor(&logEntryDataAccessor);
  m_log_view->setModel(m_log_model);
  horizontal->addWidget(m_log_view);
  m_log_view->setUniformRowHeights(true);
  m_log_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_log_view->setFrameStyle(QFrame::Box);
  m_log_view->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_log_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_log_view->setColumnHidden(2, true);
  m_log_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_category_view = new Utils::BaseTreeView;
  m_category_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_category_view->setUniformRowHeights(true);
  m_category_view->setFrameStyle(QFrame::Box);
  m_category_view->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_category_view->setSelectionMode(QAbstractItemView::SingleSelection);
  m_category_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_category_model = new LoggingCategoryModel;
  m_category_model->setFromManager(m_manager);

  auto sort_filter_model = new QSortFilterProxyModel(this);
  sort_filter_model->setSourceModel(m_category_model);
  sort_filter_model->sort(0);

  m_category_view->setModel(sort_filter_model);
  m_category_view->setItemDelegateForColumn(1, new LoggingLevelDelegate(this));

  horizontal->addWidget(m_category_view);
  horizontal->setStretch(0, 5);
  horizontal->setStretch(1, 3);
  main_layout->addLayout(horizontal);

  setLayout(main_layout);
  resize(800, 300);

  connect(m_manager, &LoggingViewManager::receivedLog, this, [this](const QString &timestamp, const QString &type, const QString &category, const QString &msg) {
    if (m_log_model->rowCount() >= 1000000) // limit log to 1000000 items
      m_log_model->destroyItem(m_log_model->itemForIndex(m_log_model->index(0, 0)));
    m_log_model->appendItem(LogEntry{timestamp, type, category, msg});
  }, Qt::QueuedConnection);
  connect(m_log_model, &QAbstractItemModel::rowsInserted, this, [this, auto_scroll] {
    if (auto_scroll->isChecked())
      m_log_view->scrollToBottom();
  }, Qt::QueuedConnection);
  connect(m_manager, &LoggingViewManager::foundNewCategory, m_category_model, &LoggingCategoryModel::append, Qt::QueuedConnection);
  connect(m_manager, &LoggingViewManager::updatedCategory, m_category_model, &LoggingCategoryModel::update, Qt::QueuedConnection);
  connect(m_category_model, &LoggingCategoryModel::categoryChanged, m_manager, &LoggingViewManager::setCategoryEnabled);
  connect(m_category_model, &LoggingCategoryModel::colorChanged, this, &LoggingViewManagerWidget::setCategoryColor);
  connect(m_category_model, &LoggingCategoryModel::logLevelChanged, m_manager, &LoggingViewManager::setLogLevel);
  connect(m_category_view, &Utils::BaseTreeView::activated, this, [this, sort_filter_model](const QModelIndex &index) {
    const auto model_index = sort_filter_model->mapToSource(index);
    const auto value = m_category_model->data(model_index, Qt::DecorationRole);
    if (!value.isValid())
      return;
    const auto original = value.value<QColor>();
    if (!original.isValid())
      return;
    const auto changed = QColorDialog::getColor(original, this);
    if (!changed.isValid())
      return;
    if (original != changed)
      m_category_model->setData(model_index, changed, Qt::DecorationRole);
  });
  connect(save, &QToolButton::clicked, this, &LoggingViewManagerWidget::saveLoggingsToFile);
  connect(m_log_view, &Utils::BaseTreeView::customContextMenuRequested, this, &LoggingViewManagerWidget::showLogViewContextMenu);
  connect(m_category_view, &Utils::BaseTreeView::customContextMenuRequested, this, &LoggingViewManagerWidget::showLogCategoryContextMenu);
  connect(clean, &QToolButton::clicked, m_log_model, &Utils::ListModel<LogEntry>::clear);
  connect(stop, &QToolButton::clicked, this, [this, stop] {
    if (m_manager->isEnabled()) {
      m_manager->setEnabled(false);
      stop->setIcon(Utils::Icons::RUN_SMALL.icon());
      stop->setToolTip(tr("Start Logging"));
    } else {
      m_manager->setEnabled(true);
      stop->setIcon(Utils::Icons::STOP_SMALL.icon());
      stop->setToolTip(tr("Stop Logging"));
    }
  });
  connect(qt_internal, &QToolButton::toggled, m_manager, &LoggingViewManager::setListQtInternal);
  connect(m_timestamps, &QToolButton::toggled, this, [this](const bool checked) {
    m_log_view->setColumnHidden(0, !checked);
  });
  connect(m_message_types, &QToolButton::toggled, this, [this](const bool checked) {
    m_log_view->setColumnHidden(2, !checked);
  });
}

auto LoggingViewManagerWidget::showLogViewContextMenu(const QPoint &pos) const -> void
{
  QMenu m;
  const auto copy = new QAction(tr("Copy Selected Logs"), &m);
  m.addAction(copy);
  const auto copy_all = new QAction(tr("Copy All"), &m);
  m.addAction(copy_all);

  connect(copy, &QAction::triggered, &m, [this] {
    const auto selection_model = m_log_view->selectionModel();
    QString copied;
    const auto use_ts = m_timestamps->isChecked();
    const auto use_ll = m_message_types->isChecked();
    for (auto row = 0, end = m_log_model->rowCount(); row < end; ++row) {
      if (selection_model->isRowSelected(row, QModelIndex()))
        copied.append(m_log_model->dataAt(row).outputLine(use_ts, use_ll));
    }
    QGuiApplication::clipboard()->setText(copied);
  });

  connect(copy_all, &QAction::triggered, &m, [this] {
    QString copied;
    const auto use_ts = m_timestamps->isChecked();
    const auto use_ll = m_message_types->isChecked();
    for (auto row = 0, end = m_log_model->rowCount(); row < end; ++row)
      copied.append(m_log_model->dataAt(row).outputLine(use_ts, use_ll));
    QGuiApplication::clipboard()->setText(copied);
  });

  m.exec(m_log_view->mapToGlobal(pos));
}

auto LoggingViewManagerWidget::showLogCategoryContextMenu(const QPoint &pos) const -> void
{
  QMenu m;
  const auto save_preset = new QAction(tr("Save Enabled as Preset..."), &m);
  m.addAction(save_preset);
  const auto load_preset = new QAction(tr("Update from Preset..."), &m);
  m.addAction(load_preset);
  const auto uncheck_all = new QAction(tr("Uncheck All"), &m);
  m.addAction(uncheck_all);

  connect(save_preset, &QAction::triggered, this, &LoggingViewManagerWidget::saveEnabledCategoryPreset);
  connect(load_preset, &QAction::triggered, this, &LoggingViewManagerWidget::loadAndUpdateFromPreset);
  connect(uncheck_all, &QAction::triggered, m_category_model, &LoggingCategoryModel::disableAll);

  m.exec(m_category_view->mapToGlobal(pos));
}

auto LoggingViewManagerWidget::saveLoggingsToFile() const -> void
{
  // should we just let it continue without temporarily disabling?
  const auto enabled = m_manager->isEnabled();
  Utils::ExecuteOnDestruction exec([this, enabled] { m_manager->setEnabled(enabled); });

  if (enabled)
    m_manager->setEnabled(false);

  const auto fp = Utils::FileUtils::getSaveFilePath(ICore::dialogParent(), tr("Save Logs As"));

  if (fp.isEmpty())
    return;

  const auto use_ts = m_timestamps->isChecked();
  const auto use_ll = m_message_types->isChecked();

  if (QFile file(fp.path()); file.open(QIODevice::WriteOnly)) {
    for (auto row = 0, end = m_log_model->rowCount(); row < end; ++row) {
      if (const auto res = file.write(m_log_model->dataAt(row).outputLine(use_ts, use_ll).toUtf8()); res == -1) {
        QMessageBox::critical(ICore::dialogParent(), tr("Error"), tr("Failed to write logs to \"%1\".").arg(fp.toUserOutput()));
        break;
      }
    }
    file.close();
  } else {
    QMessageBox::critical(ICore::dialogParent(), tr("Error"), tr("Failed to open file \"%1\" for writing logs.").arg(fp.toUserOutput()));
  }
}

auto LoggingViewManagerWidget::saveEnabledCategoryPreset() const -> void
{
  const auto fp = Utils::FileUtils::getSaveFilePath(ICore::dialogParent(), tr("Save Enabled Categories As"));

  if (fp.isEmpty())
    return;

  const auto enabled = m_category_model->enabledCategories();
  // write them to file
  QJsonArray array;

  for (const auto & [name, entry] : enabled) {
    QJsonObject item_obj;
    item_obj.insert("name", name);
    QJsonObject entry_obj;
    entry_obj.insert("level", entry.level);
    if (entry.color.isValid())
      entry_obj.insert("color", entry.color.name(QColor::HexArgb));
    item_obj.insert("entry", entry_obj);
    array.append(item_obj);
  }

  if (const QJsonDocument doc(array); !fp.writeFileContents(doc.toJson(QJsonDocument::Compact)))
    QMessageBox::critical(ICore::dialogParent(), tr("Error"), tr("Failed to write preset file \"%1\".").arg(fp.toUserOutput()));
}

auto LoggingViewManagerWidget::loadAndUpdateFromPreset() const -> void
{
  const auto fp = Utils::FileUtils::getOpenFilePath(ICore::dialogParent(), tr("Load Enabled Categories From"));

  if (fp.isEmpty())
    return;

  // read file, update categories
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(fp.fileContents(), &error);

  if (error.error != QJsonParseError::NoError) {
    QMessageBox::critical(ICore::dialogParent(), tr("Error"), tr("Failed to read preset file \"%1\": %2").arg(fp.toUserOutput()).arg(error.errorString()));
    return;
  }

  auto format_error = false;
  QList<LoggingCategoryItem> preset_items;

  if (doc.isArray()) {
    for (const auto array = doc.array(); const auto value : array) {
      if (!value.isObject()) {
        format_error = true;
        break;
      }
      const auto item_obj = value.toObject();
      auto ok = true;
      auto item = LoggingCategoryItem::fromJson(item_obj, &ok);
      if (!ok) {
        format_error = true;
        break;
      }
      preset_items.append(item);
    }
  } else {
    format_error = true;
  }

  if (format_error) {
    QMessageBox::critical(ICore::dialogParent(), tr("Error"), tr("Unexpected preset file format."));
  }

  for (const auto &[name, entry] : preset_items)
    m_manager->appendOrUpdate(name, entry);
}

auto LoggingViewManagerWidget::colorForCategory(const QString &category) -> QColor
{
  const auto entry = m_category_color.find(category);

  if (entry == m_category_color.end())
    return Utils::orcaTheme()->palette().text().color();

  return entry.value();
}

auto LoggingViewManagerWidget::setCategoryColor(const QString &category, const QColor &color) -> void
{
  if (const auto base_color = Utils::orcaTheme()->palette().text().color(); color != base_color)
    m_category_color.insert(category, color);
  else
    m_category_color.remove(category);
}

auto LoggingViewer::showLoggingView() -> void
{
  ActionManager::command(LOGGER)->action()->setEnabled(false);
  auto widget = new LoggingViewManagerWidget(ICore::mainWindow());

  QObject::connect(widget, &QDialog::finished, widget, [widget] {
    ActionManager::command(LOGGER)->action()->setEnabled(true);
    // explicitly disable manager again
    widget->deleteLater();
  });

  ICore::registerWindow(widget, Context("Qtc.LogViewer"));
  widget->show();
}

} // namespace Orca::Plugin::Core

#include "core-logging-viewer.moc"
