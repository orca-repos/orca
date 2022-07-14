// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-mime-type-settings.hpp"
#include "ui_core-mime-type-settings-page.h"

#include "core-constants.hpp"
#include "core-editor-factory-interface.hpp"
#include "core-editor-factory-private-interface.hpp"
#include "core-interface.hpp"
#include "core-mime-type-magic-dialog.hpp"

#include <utils/algorithm.hpp>
#include <utils/headerviewstretcher.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QAbstractTableModel>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QMessageBox>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

static constexpr char g_k_modified_mime_types_file[] = "mimetypes/modifiedmimetypes.xml";
static constexpr char g_mime_info_tag_c[] = "mime-info";
static constexpr char g_mime_type_tag_c[] = "mime-type";
static constexpr char g_mime_type_attribute_c[] = "type";
static constexpr char g_pattern_attribute_c[] = "pattern";
static constexpr char g_match_tag_c[] = "match";
static constexpr char g_match_value_attribute_c[] = "value";
static constexpr char g_match_type_attribute_c[] = "type";
static constexpr char g_match_offset_attribute_c[] = "offset";
static constexpr char g_priority_attribute_c[] = "priority";
static constexpr char g_match_mask_attribute_c[] = "mask";

namespace Orca::Plugin::Core {

class MimeEditorDelegate final : public QStyledItemDelegate {
public:
  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* final;
  auto setEditorData(QWidget *editor, const QModelIndex &index) const -> void final;
  auto setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void final;
};

class UserMimeType {
public:
  auto isValid() const -> bool { return !name.isEmpty(); }
  QString name;
  QStringList glob_patterns;
  QMap<int, QList<Utils::Internal::MimeMagicRule>> rules;
};

// MimeTypeSettingsModel
class MimeTypeSettingsModel final : public QAbstractTableModel {
  Q_OBJECT

public:
  enum class Role {
    DefaultHandler = Qt::UserRole
  };

  explicit MimeTypeSettingsModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
  auto data(const QModelIndex &model_index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool final;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags final;
  auto load() -> void;
  auto handlersForMimeType(const Utils::MimeType &mime_type) const -> QList<EditorType*>;
  auto defaultHandlerForMimeType(const Utils::MimeType &mime_type) const -> EditorType*;
  auto resetUserDefaults() -> void;

  QList<Utils::MimeType> m_mime_types;
  mutable QHash<Utils::MimeType, QList<EditorType*>> m_handlers_by_mime_type;
  QHash<Utils::MimeType, EditorType*> m_user_default;
};

auto MimeTypeSettingsModel::rowCount(const QModelIndex &) const -> int
{
  return static_cast<int>(m_mime_types.size());
}

auto MimeTypeSettingsModel::columnCount(const QModelIndex &) const -> int
{
  return 2;
}

auto MimeTypeSettingsModel::headerData(const int section, const Qt::Orientation orientation, const int role) const -> QVariant
{
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return {};

  if (section == 0)
    return tr("MIME Type");

  return tr("Handler");
}

auto MimeTypeSettingsModel::data(const QModelIndex &model_index, const int role) const -> QVariant
{
  if (!model_index.isValid())
    return {};

  const auto column = model_index.column();

  if (role == Qt::DisplayRole) {
    const auto &type = m_mime_types.at(model_index.row());
    if (column == 0) {
      return type.name();
    }
    const auto default_handler = defaultHandlerForMimeType(type);
    return default_handler ? default_handler->displayName() : QString();
  }

  if (role == Qt::EditRole) {
    return QVariant::fromValue(handlersForMimeType(m_mime_types.at(model_index.row())));
  }

  if (role == static_cast<int>(Role::DefaultHandler)) {
    return QVariant::fromValue(defaultHandlerForMimeType(m_mime_types.at(model_index.row())));
  }

  if (role == Qt::FontRole) {
    if (column == 1) {
      if (const auto &type = m_mime_types.at(model_index.row()); m_user_default.contains(type)) {
        auto font = QGuiApplication::font();
        font.setItalic(true);
        return font;
      }
    }
    return {};
  }

  return {};
}

auto MimeTypeSettingsModel::setData(const QModelIndex &index, const QVariant &value, const int role) -> bool
{
  if (role != static_cast<int>(Role::DefaultHandler) || index.column() != 1)
    return false;

  const auto factory = value.value<EditorType*>();
  QTC_ASSERT(factory, return false);
  const auto row = index.row();
  QTC_ASSERT(row >= 0 && row < m_mime_types.size(), return false);
  const auto mime_type = m_mime_types.at(row);
  const auto handlers = handlersForMimeType(mime_type);
  QTC_ASSERT(handlers.contains(factory), return false);

  if (handlers.first() == factory) // selection is the default anyhow
    m_user_default.remove(mime_type);
  else
    m_user_default.insert(mime_type, factory);

  emit dataChanged(index, index);
  return true;
}

auto MimeTypeSettingsModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  if (index.column() == 0 || handlersForMimeType(m_mime_types.at(index.row())).size() < 2)
    return QAbstractTableModel::flags(index);
  return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

auto MimeTypeSettingsModel::load() -> void
{
  beginResetModel();
  m_mime_types = Utils::allMimeTypes();
  m_user_default = userPreferredEditorTypes();

  Utils::sort(m_mime_types, [](const Utils::MimeType &a, const Utils::MimeType &b) {
    return a.name().compare(b.name(), Qt::CaseInsensitive) < 0;
  });

  m_handlers_by_mime_type.clear();
  endResetModel();
}

auto MimeTypeSettingsModel::handlersForMimeType(const Utils::MimeType &mime_type) const -> QList<EditorType*>
{
  if (!m_handlers_by_mime_type.contains(mime_type))
    m_handlers_by_mime_type.insert(mime_type, EditorType::defaultEditorTypes(mime_type));
  return m_handlers_by_mime_type.value(mime_type);
}

auto MimeTypeSettingsModel::defaultHandlerForMimeType(const Utils::MimeType &mime_type) const -> EditorType*
{
  if (m_user_default.contains(mime_type))
    return m_user_default.value(mime_type);

  const auto handlers = handlersForMimeType(mime_type);
  return handlers.isEmpty() ? nullptr : handlers.first();
}

auto MimeTypeSettingsModel::resetUserDefaults() -> void
{
  beginResetModel();
  m_user_default.clear();
  endResetModel();
}

// MimeTypeSettingsPrivate
class MimeTypeSettingsPrivate : public QObject {
  Q_OBJECT

public:
  MimeTypeSettingsPrivate();
  ~MimeTypeSettingsPrivate() override;

  auto configureUi(QWidget *w) -> void;

private:
  auto addMagicHeaderRow(const MagicData &data) const -> void;
  auto editMagicHeaderRowData(int row, const MagicData &data) const -> void;

  auto setFilterPattern(const QString &pattern) -> void;
  auto syncData(const QModelIndex &current, const QModelIndex &previous) -> void;
  auto updatePatternEditAndMagicButtons() const -> void;
  auto handlePatternEdited() -> void;
  auto addMagicHeader() -> void;
  auto removeMagicHeader() -> void;
  auto editMagicHeader() -> void;
  auto resetMimeTypes() -> void;

  auto ensurePendingMimeType(const Utils::MimeType &mime_type) -> void;
  static auto writeUserModifiedMimeTypes() -> void;

public:
  using UserMimeTypeHash = QHash<QString, UserMimeType>; // name -> mime type

  static const QChar k_semi_colon;
  static auto readUserModifiedMimeTypes() -> UserMimeTypeHash;
  static auto applyUserModifiedMimeTypes(const UserMimeTypeHash &mime_types) -> void;

  static UserMimeTypeHash m_user_modified_mime_types; // these are already in mime database
  MimeTypeSettingsModel *m_model;
  QSortFilterProxyModel *m_filter_model;
  UserMimeTypeHash m_pending_modified_mime_types; // currently edited in the options page
  QString m_filter_pattern;
  Ui::MimeTypeSettingsPage m_ui{};
  QPointer<QWidget> m_widget;
  MimeEditorDelegate m_delegate;
};

const QChar MimeTypeSettingsPrivate::k_semi_colon(QLatin1Char(';'));

MimeTypeSettingsPrivate::UserMimeTypeHash MimeTypeSettingsPrivate::m_user_modified_mime_types = UserMimeTypeHash();

MimeTypeSettingsPrivate::MimeTypeSettingsPrivate() : m_model(new MimeTypeSettingsModel(this)), m_filter_model(new QSortFilterProxyModel(this))
{
  m_filter_model->setSourceModel(m_model);
  m_filter_model->setFilterKeyColumn(-1);
  connect(ICore::instance(), &ICore::saveSettingsRequested, this, &MimeTypeSettingsPrivate::writeUserModifiedMimeTypes);
}

MimeTypeSettingsPrivate::~MimeTypeSettingsPrivate() = default;

auto MimeTypeSettingsPrivate::configureUi(QWidget *w) -> void
{
  m_ui.setupUi(w);
  m_ui.filterLineEdit->setText(m_filter_pattern);
  m_model->load();
  connect(m_ui.filterLineEdit, &QLineEdit::textChanged, this, &MimeTypeSettingsPrivate::setFilterPattern);
  m_ui.mimeTypesTreeView->setModel(m_filter_model);
  m_ui.mimeTypesTreeView->setItemDelegate(&m_delegate);

  new Utils::HeaderViewStretcher(m_ui.mimeTypesTreeView->header(), 1);

  connect(m_ui.mimeTypesTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MimeTypeSettingsPrivate::syncData);
  connect(m_ui.mimeTypesTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MimeTypeSettingsPrivate::updatePatternEditAndMagicButtons);
  connect(m_ui.patternsLineEdit, &QLineEdit::textEdited, this, &MimeTypeSettingsPrivate::handlePatternEdited);
  connect(m_ui.addMagicButton, &QPushButton::clicked, this, &MimeTypeSettingsPrivate::addMagicHeader);
  // TODO
  connect(m_ui.removeMagicButton, &QPushButton::clicked, this, &MimeTypeSettingsPrivate::removeMagicHeader);
  connect(m_ui.editMagicButton, &QPushButton::clicked, this, &MimeTypeSettingsPrivate::editMagicHeader);
  connect(m_ui.resetButton, &QPushButton::clicked, this, &MimeTypeSettingsPrivate::resetMimeTypes);
  connect(m_ui.resetHandlersButton, &QPushButton::clicked, m_model, &MimeTypeSettingsModel::resetUserDefaults);
  connect(m_ui.magicHeadersTreeWidget, &QTreeWidget::itemSelectionChanged, this, &MimeTypeSettingsPrivate::updatePatternEditAndMagicButtons);

  updatePatternEditAndMagicButtons();
}

auto MimeTypeSettingsPrivate::syncData(const QModelIndex &current, const QModelIndex &previous) -> void
{
  Q_UNUSED(previous)
  m_ui.patternsLineEdit->clear();
  m_ui.magicHeadersTreeWidget->clear();

  if (current.isValid()) {
    const auto &current_mime_type = m_model->m_mime_types.at(m_filter_model->mapToSource(current).row());
    const auto modified_type = m_pending_modified_mime_types.value(current_mime_type.name());
    m_ui.patternsLineEdit->setText(modified_type.isValid() ? modified_type.glob_patterns.join(k_semi_colon) : current_mime_type.globPatterns().join(k_semi_colon));
    const auto rules = modified_type.isValid() ? modified_type.rules : magicRulesForMimeType(current_mime_type);

    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
      const auto priority = it.key();
      for(const auto &rule: it.value()) {
        addMagicHeaderRow(MagicData(rule, priority));
      }
    }
  }
}

auto MimeTypeSettingsPrivate::updatePatternEditAndMagicButtons() const -> void
{
  const auto &mime_type_index = m_ui.mimeTypesTreeView->currentIndex();
  const auto mime_type_valid = mime_type_index.isValid();

  m_ui.patternsLineEdit->setEnabled(mime_type_valid);
  m_ui.addMagicButton->setEnabled(mime_type_valid);

  const auto &magic_index = m_ui.magicHeadersTreeWidget->currentIndex();
  const auto magic_valid = magic_index.isValid();

  m_ui.removeMagicButton->setEnabled(magic_valid);
  m_ui.editMagicButton->setEnabled(magic_valid);
}

auto MimeTypeSettingsPrivate::handlePatternEdited() -> void
{
  const auto &model_index = m_ui.mimeTypesTreeView->currentIndex();
  QTC_ASSERT(model_index.isValid(), return);
  const auto index = m_filter_model->mapToSource(model_index).row();
  const auto &mt = m_model->m_mime_types.at(index);
  ensurePendingMimeType(mt);
  m_pending_modified_mime_types[mt.name()].glob_patterns = m_ui.patternsLineEdit->text().split(k_semi_colon, Qt::SkipEmptyParts);
}

auto MimeTypeSettingsPrivate::addMagicHeaderRow(const MagicData &data) const -> void
{
  const auto row = m_ui.magicHeadersTreeWidget->topLevelItemCount();
  editMagicHeaderRowData(row, data);
}

auto MimeTypeSettingsPrivate::editMagicHeaderRowData(const int row, const MagicData &data) const -> void
{
  const auto item = new QTreeWidgetItem;
  item->setText(0, QString::fromUtf8(data.m_rule.value()));
  item->setText(1, QString::fromLatin1(Utils::Internal::MimeMagicRule::typeName(data.m_rule.type())));
  item->setText(2, QString::fromLatin1("%1:%2").arg(data.m_rule.startPos()).arg(data.m_rule.endPos()));
  item->setText(3, QString::number(data.m_priority));
  item->setData(0, Qt::UserRole, QVariant::fromValue(data));
  m_ui.magicHeadersTreeWidget->takeTopLevelItem(row);
  m_ui.magicHeadersTreeWidget->insertTopLevelItem(row, item);
  m_ui.magicHeadersTreeWidget->setCurrentItem(item);
}

auto MimeTypeSettingsPrivate::addMagicHeader() -> void
{
  const auto &mime_type_index = m_ui.mimeTypesTreeView->currentIndex();
  QTC_ASSERT(mime_type_index.isValid(), return);
  const auto index = m_filter_model->mapToSource(mime_type_index).row();
  const auto &mt = m_model->m_mime_types.at(index);

  if (MimeTypeMagicDialog dlg; dlg.exec()) {
    const auto &data = dlg.magicData();
    ensurePendingMimeType(mt);
    m_pending_modified_mime_types[mt.name()].rules[data.m_priority].append(data.m_rule);
    addMagicHeaderRow(data);
  }
}

auto MimeTypeSettingsPrivate::removeMagicHeader() -> void
{
  const auto &mime_type_index = m_ui.mimeTypesTreeView->currentIndex();
  QTC_ASSERT(mime_type_index.isValid(), return);
  const auto &magic_index = m_ui.magicHeadersTreeWidget->currentIndex();
  QTC_ASSERT(magic_index.isValid(), return);
  const auto index = m_filter_model->mapToSource(mime_type_index).row();
  const auto &mt = m_model->m_mime_types.at(index);
  const auto item = m_ui.magicHeadersTreeWidget->topLevelItem(magic_index.row());
  QTC_ASSERT(item, return);
  const auto data = item->data(0, Qt::UserRole).value<MagicData>();

  ensurePendingMimeType(mt);
  m_pending_modified_mime_types[mt.name()].rules[data.m_priority].removeOne(data.m_rule);
  syncData(mime_type_index, mime_type_index);
}

auto MimeTypeSettingsPrivate::editMagicHeader() -> void
{
  const auto &mime_type_index = m_ui.mimeTypesTreeView->currentIndex();
  QTC_ASSERT(mime_type_index.isValid(), return);
  const auto &magic_index = m_ui.magicHeadersTreeWidget->currentIndex();
  QTC_ASSERT(magic_index.isValid(), return);
  const auto index = m_filter_model->mapToSource(mime_type_index).row();
  const auto &mt = m_model->m_mime_types.at(index);
  const auto item = m_ui.magicHeadersTreeWidget->topLevelItem(magic_index.row());
  QTC_ASSERT(item, return);
  const auto old_data = item->data(0, Qt::UserRole).value<MagicData>();

  MimeTypeMagicDialog dlg;
  dlg.setMagicData(old_data);
  if (dlg.exec()) {
    if (dlg.magicData() != old_data) {
      ensurePendingMimeType(mt);
      const auto &dialog_data = dlg.magicData();
      const auto rule_index = static_cast<int>(m_pending_modified_mime_types[mt.name()].rules[old_data.m_priority].indexOf(old_data.m_rule));
      if (old_data.m_priority != dialog_data.m_priority) {
        m_pending_modified_mime_types[mt.name()].rules[old_data.m_priority].removeAt(rule_index);
        m_pending_modified_mime_types[mt.name()].rules[dialog_data.m_priority].append(dialog_data.m_rule);
      } else {
        m_pending_modified_mime_types[mt.name()].rules[old_data.m_priority][rule_index] = dialog_data.m_rule;
      }
      editMagicHeaderRowData(magic_index.row(), dialog_data);
    }
  }
}

auto MimeTypeSettingsPrivate::resetMimeTypes() -> void
{
  m_pending_modified_mime_types.clear();
  m_user_modified_mime_types.clear(); // settings file will be removed with next settings-save
  QMessageBox::information(ICore::dialogParent(), tr("Reset MIME Types"), tr("Changes will take effect after restart."));
}

auto MimeTypeSettingsPrivate::setFilterPattern(const QString &pattern) -> void
{
  m_filter_pattern = pattern;
  m_filter_model->setFilterWildcard(pattern);
}

auto MimeTypeSettingsPrivate::ensurePendingMimeType(const Utils::MimeType &mime_type) -> void
{
  if (!m_pending_modified_mime_types.contains(mime_type.name())) {
    // get a copy of the mime type into pending modified types
    UserMimeType user_mt;
    user_mt.name = mime_type.name();
    user_mt.glob_patterns = mime_type.globPatterns();
    user_mt.rules = magicRulesForMimeType(mime_type);
    m_pending_modified_mime_types.insert(user_mt.name, user_mt);
  }
}

auto MimeTypeSettingsPrivate::writeUserModifiedMimeTypes() -> void
{
  if (static auto modified_mime_types_file = ICore::userResourcePath(g_k_modified_mime_types_file); QFile::exists(modified_mime_types_file.toString()) || QDir().mkpath(modified_mime_types_file.parentDir().toString())) {
    QFile file(modified_mime_types_file.toString());
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
      // Notice this file only represents user modifications. It is writen in a
      // convienient way for synchronization, which is similar to but not exactly the
      // same format we use for the embedded mime type files.
      QXmlStreamWriter writer(&file);
      writer.setAutoFormatting(true);
      writer.writeStartDocument();
      writer.writeStartElement(QLatin1String(g_mime_info_tag_c));

      for(const auto &mt: m_user_modified_mime_types) {
        writer.writeStartElement(QLatin1String(g_mime_type_tag_c));
        writer.writeAttribute(QLatin1String(g_mime_type_attribute_c), mt.name);
        writer.writeAttribute(QLatin1String(g_pattern_attribute_c), mt.glob_patterns.join(k_semi_colon));
        for (auto prio_it = mt.rules.constBegin(); prio_it != mt.rules.constEnd(); ++prio_it) {
          const auto priority_string = QString::number(prio_it.key());
          for(const auto &rule: prio_it.value()) {
            writer.writeStartElement(QLatin1String(g_match_tag_c));
            writer.writeAttribute(QLatin1String(g_match_value_attribute_c), QString::fromUtf8(rule.value()));
            writer.writeAttribute(QLatin1String(g_match_type_attribute_c), QString::fromUtf8(Utils::Internal::MimeMagicRule::typeName(rule.type())));
            writer.writeAttribute(QLatin1String(g_match_offset_attribute_c), QString::fromLatin1("%1:%2").arg(rule.startPos()).arg(rule.endPos()));
            writer.writeAttribute(QLatin1String(g_priority_attribute_c), priority_string);
            writer.writeAttribute(QLatin1String(g_match_mask_attribute_c), QString::fromLatin1(MagicData::normalizedMask(rule)));
            writer.writeEndElement();
          }
        }
        writer.writeEndElement();
      }
      writer.writeEndElement();
      writer.writeEndDocument();
      file.close();
    }
  }
}

static auto rangeFromString(const QString &offset) -> QPair<int, int>
{
  const auto list = offset.split(QLatin1Char(':'));
  QPair<int, int> range;
  QTC_ASSERT(!list.empty(), return range);
  range.first = list.at(0).toInt();

  if (list.size() > 1)
    range.second = list.at(1).toInt();
  else
    range.second = range.first;

  return range;
}

auto MimeTypeSettingsPrivate::readUserModifiedMimeTypes() -> UserMimeTypeHash
{
  static auto modified_mime_types_path = ICore::userResourcePath(g_k_modified_mime_types_file);
  UserMimeTypeHash user_mime_types;
  QFile file(modified_mime_types_path.toString());

  if (file.open(QFile::ReadOnly)) {
    QXmlStreamReader reader(&file);
    QXmlStreamAttributes atts;
    while (!reader.atEnd()) {
      UserMimeType mt;
      switch (reader.readNext()) {
      case QXmlStreamReader::StartElement:
        atts = reader.attributes();
        if (reader.name() == QLatin1String(g_mime_type_tag_c)) {
          mt.name = atts.value(QLatin1String(g_mime_type_attribute_c)).toString();
          mt.glob_patterns = atts.value(QLatin1String(g_pattern_attribute_c)).toString().split(k_semi_colon, Qt::SkipEmptyParts);
        } else if (reader.name() == QLatin1String(g_match_tag_c)) {
          auto value = atts.value(QLatin1String(g_match_value_attribute_c)).toUtf8();
          auto type_name = atts.value(QLatin1String(g_match_type_attribute_c)).toUtf8();
          const auto range_string = atts.value(QLatin1String(g_match_offset_attribute_c)).toString();
          const auto [fst, snd] = rangeFromString(range_string);
          auto priority = atts.value(QLatin1String(g_priority_attribute_c)).toString().toInt();
          auto mask = atts.value(QLatin1String(g_match_mask_attribute_c)).toLatin1();
          QString error_message;
          if (Utils::Internal::MimeMagicRule rule(Utils::Internal::MimeMagicRule::type(type_name), value, fst, snd, mask, &error_message); rule.isValid()) {
            mt.rules[priority].append(rule);
          } else {
            qWarning("Error reading magic rule in custom mime type %s: %s", qPrintable(mt.name), qPrintable(error_message));
          }
        }
        break;
      case QXmlStreamReader::EndElement:
        if (reader.name() == QLatin1String(g_mime_type_tag_c)) {
          user_mime_types.insert(mt.name, mt);
          mt.name.clear();
          mt.glob_patterns.clear();
          mt.rules.clear();
        }
        break;
      default:
        break;
      }
    }
    if (reader.hasError())
      qWarning() << modified_mime_types_path << reader.errorString() << reader.lineNumber() << reader.columnNumber();
    file.close();
  }
  return user_mime_types;
}

auto MimeTypeSettingsPrivate::applyUserModifiedMimeTypes(const UserMimeTypeHash &mime_types) -> void
{
  // register in mime data base, and remember for later
  for (auto it = mime_types.constBegin(); it != mime_types.constEnd(); ++it) {
    auto mt = Utils::mimeTypeForName(it.key());

    if (!mt.isValid()) // loaded from settings
      continue;

    m_user_modified_mime_types.insert(it.key(), it.value());
    setGlobPatternsForMimeType(mt, it.value().glob_patterns);
    setMagicRulesForMimeType(mt, it.value().rules);
  }
}

// MimeTypeSettingsPage

MimeTypeSettings::MimeTypeSettings() : d(new MimeTypeSettingsPrivate)
{
  setId(SETTINGS_ID_MIMETYPES);
  setDisplayName(tr("MIME Types"));
  setCategory(SETTINGS_CATEGORY_CORE);
}

MimeTypeSettings::~MimeTypeSettings()
{
  delete d;
}

auto MimeTypeSettings::widget() -> QWidget*
{
  if (!d->m_widget) {
    d->m_widget = new QWidget;
    d->configureUi(d->m_widget);
  }
  return d->m_widget;
}

auto MimeTypeSettings::apply() -> void
{
  MimeTypeSettingsPrivate::applyUserModifiedMimeTypes(d->m_pending_modified_mime_types);
  setUserPreferredEditorTypes(d->m_model->m_user_default);
  d->m_pending_modified_mime_types.clear();
  d->m_model->load();
}

auto MimeTypeSettings::finish() -> void
{
  d->m_pending_modified_mime_types.clear();
  delete d->m_widget;
}

auto MimeTypeSettings::restoreSettings() -> void
{
  const auto mimetypes = MimeTypeSettingsPrivate::readUserModifiedMimeTypes();
  MimeTypeSettingsPrivate::applyUserModifiedMimeTypes(mimetypes);
}

auto MimeEditorDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget*
{
  Q_UNUSED(option)
  Q_UNUSED(index)
  return new QComboBox(parent);
}

auto MimeEditorDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const -> void
{
  const auto box = dynamic_cast<QComboBox*>(editor);
  const auto factories = index.model()->data(index, Qt::EditRole).value<QList<EditorType*>>();

  for (auto factory : factories)
    box->addItem(factory->displayName(), QVariant::fromValue(factory));

  if (const int current_index = factories.indexOf(index.model()->data(index, static_cast<int>(MimeTypeSettingsModel::Role::DefaultHandler)).value<EditorType*>()); QTC_GUARD(current_index != -1))
    box->setCurrentIndex(current_index);
}

auto MimeEditorDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void
{
  const auto box = dynamic_cast<QComboBox*>(editor);
  model->setData(index, box->currentData(Qt::UserRole), static_cast<int>(MimeTypeSettingsModel::Role::DefaultHandler));
}

} // namespace Orca::Plugin::Core

#include "core-mime-type-settings.moc"
