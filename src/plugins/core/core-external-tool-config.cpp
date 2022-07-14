// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-external-tool-config.hpp"
#include "ui_core-external-tool-config.h"

#include "core-constants.hpp"
#include "core-external-tool-manager.hpp"
#include "core-external-tool.hpp"
#include "core-interface.hpp"
#include "core-options-page-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/environmentdialog.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/variablechooser.hpp>

#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QRandomGenerator>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr Qt::ItemFlags toolsmenu_item_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
constexpr Qt::ItemFlags category_item_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
constexpr Qt::ItemFlags tool_item_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;

class ExternalToolModel final : public QAbstractItemModel {
  Q_DECLARE_TR_FUNCTIONS(Core::ExternalToolConfig)
  Q_DISABLE_COPY_MOVE(ExternalToolModel)

public:
  ExternalToolModel() = default;
  ~ExternalToolModel() override;

  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &model_index, int role = Qt::DisplayRole) const -> QVariant override;
  auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto flags(const QModelIndex &model_index) const -> Qt::ItemFlags override;
  auto setData(const QModelIndex &model_index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool override;
  auto mimeTypes() const -> QStringList override;
  auto setTools(const QMap<QString, QList<ExternalTool*>> &tools) -> void;
  auto tools() const -> QMap<QString, QList<ExternalTool*>> { return m_tools; }
  static auto toolForIndex(const QModelIndex &model_index) -> ExternalTool*;
  auto categoryForIndex(const QModelIndex &model_index, bool *found) const -> QString;
  auto revertTool(const QModelIndex &model_index) -> void;
  auto addCategory() -> QModelIndex;
  auto addTool(const QModelIndex &at_index) -> QModelIndex;
  auto removeTool(const QModelIndex &model_index) -> void;
  auto supportedDropActions() const -> Qt::DropActions override { return Qt::MoveAction; }

private:
  static auto data(const ExternalTool *tool, int role = Qt::DisplayRole) -> QVariant;
  static auto data(const QString &category, int role = Qt::DisplayRole) -> QVariant;

  QMap<QString, QList<ExternalTool*>> m_tools;
};

ExternalToolModel::~ExternalToolModel()
{
  for (auto &tool_in_category : m_tools)
    qDeleteAll(tool_in_category);
}

auto ExternalToolModel::columnCount(const QModelIndex &parent) const -> int
{
  bool category_found;
  categoryForIndex(parent, &category_found);
  if (!parent.isValid() || toolForIndex(parent) || category_found)
    return 1;
  return 0;
}

auto ExternalToolModel::data(const QModelIndex &model_index, int role) const -> QVariant
{
  if (const auto tool = toolForIndex(model_index))
    return data(tool, role);

  bool found;
  const auto category = categoryForIndex(model_index, &found);

  if (found)
    return data(category, role);

  return {};
}

auto ExternalToolModel::data(const ExternalTool *tool, const int role) -> QVariant
{
  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    return tool->displayName();
  default:
    break;
  }
  return {};
}

auto ExternalToolModel::data(const QString &category, const int role) -> QVariant
{
  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    return category.isEmpty() ? tr("Uncategorized") : category;
  case Qt::ToolTipRole:
    return category.isEmpty() ? tr("Tools that will appear directly under the External Tools menu.") : QVariant();
  default:
    break;
  }
  return {};
}

auto ExternalToolModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  if (indexes.isEmpty())
    return nullptr;

  const auto &model_index = indexes.first();
  const auto tool = toolForIndex(model_index);

  QTC_ASSERT(tool, return nullptr);
  bool found;
  const auto category = categoryForIndex(model_index.parent(), &found);

  QTC_ASSERT(found, return nullptr);
  const auto md = new QMimeData();

  QByteArray ba;
  QDataStream stream(&ba, QIODevice::WriteOnly);
  stream << category << m_tools.value(category).indexOf(tool);
  md->setData("application/core-external-tool-config", ba);

  return md;
}

auto ExternalToolModel::dropMimeData(const QMimeData *data, const Qt::DropAction action, int row, const int column, const QModelIndex &parent) -> bool
{
  Q_UNUSED(column)
  if (action != Qt::MoveAction || !data)
    return false;

  bool found;
  const auto to_category = categoryForIndex(parent, &found);

  QTC_ASSERT(found, return false);

  auto ba = data->data("application/core-external-tool-config");
  if (ba.isEmpty())
    return false;

  QDataStream stream(&ba, QIODevice::ReadOnly);
  QString category;
  auto pos = -1;
  stream >> category;
  stream >> pos;
  auto &items = m_tools[category];
  QTC_ASSERT(pos >= 0 && pos < items.count(), return false);

  beginRemoveRows(index(static_cast<int>(m_tools.keys().indexOf(category)), 0), pos, pos);
  const auto tool = items.takeAt(pos);
  endRemoveRows();

  if (row < 0)
    row = static_cast<int>(m_tools.value(to_category).count());

  beginInsertRows(index(static_cast<int>(m_tools.keys().indexOf(to_category)), 0), row, row);
  m_tools[to_category].insert(row, tool);
  endInsertRows();

  return true;
}

auto ExternalToolModel::mimeTypes() const -> QStringList
{
  return QStringList("application/core-external-tool-config");
}

auto ExternalToolModel::index(const int row, const int column, const QModelIndex &parent) const -> QModelIndex
{
  if (column == 0 && parent.isValid()) {
    bool found;
    const auto category = categoryForIndex(parent, &found);
    if (found) {
      if (const auto items = m_tools.value(category); row < items.count())
        return createIndex(row, 0, items.at(row));
    }
  } else if (column == 0 && row < m_tools.size()) {
    return createIndex(row, 0);
  }
  return {};
}

auto ExternalToolModel::parent(const QModelIndex &child) const -> QModelIndex
{
  if (const auto tool = toolForIndex(child)) {
    auto category_index = 0;
    for (const auto &tools_in_category : m_tools) {
      if (tools_in_category.contains(tool))
        return index(category_index, 0);
      ++category_index;
    }
  }
  return {};
}

auto ExternalToolModel::rowCount(const QModelIndex &parent) const -> int
{
  if (!parent.isValid())
    return static_cast<int>(m_tools.size());

  if (toolForIndex(parent))
    return 0;

  bool found;
  const auto category = categoryForIndex(parent, &found);

  if (found)
    return static_cast<int>(m_tools.value(category).count());

  return 0;
}

auto ExternalToolModel::flags(const QModelIndex &model_index) const -> Qt::ItemFlags
{
  if (toolForIndex(model_index))
    return tool_item_flags;

  bool found;
  const auto category = categoryForIndex(model_index, &found);

  if (found) {
    if (category.isEmpty())
      return toolsmenu_item_flags;
    return category_item_flags;
  }
  return {};
}

auto ExternalToolModel::setData(const QModelIndex &model_index, const QVariant &value, const int role) -> bool
{
  if (role != Qt::EditRole)
    return false;

  const auto string = value.toString();

  if (const auto tool = toolForIndex(model_index)) {
    if (string.isEmpty() || tool->displayName() == string)
      return false;
    // rename tool
    tool->setDisplayName(string);
    emit dataChanged(model_index, model_index);
    return true;
  }

  bool found;
  const auto category = categoryForIndex(model_index, &found);

  if (found) {
    if (string.isEmpty() || m_tools.contains(string))
      return false;

    // rename category
    auto categories = m_tools.keys();
    const auto previous_index = categories.indexOf(category);
    categories.removeAt(previous_index);
    categories.append(string);
    Utils::sort(categories);

    const auto new_index = categories.indexOf(string);
    if (new_index != previous_index) {
      // we have same parent so we have to do special stuff for beginMoveRows...
      const auto begin_move_rows_special_index = previous_index < new_index ? new_index + 1 : new_index;
      beginMoveRows(QModelIndex(), static_cast<int>(previous_index), static_cast<int>(previous_index), QModelIndex(), static_cast<int>(begin_move_rows_special_index));
    }

    const auto items = m_tools.take(category);
    m_tools.insert(string, items);

    if (new_index != previous_index)
      endMoveRows();

    return true;
  }
  return false;
}

auto ExternalToolModel::setTools(const QMap<QString, QList<ExternalTool*>> &tools) -> void
{
  beginResetModel();
  m_tools = tools;
  endResetModel();
}

auto ExternalToolModel::toolForIndex(const QModelIndex &model_index) -> ExternalTool*
{
  return static_cast<ExternalTool*>(model_index.internalPointer());
}

auto ExternalToolModel::categoryForIndex(const QModelIndex &model_index, bool *found) const -> QString
{
  if (model_index.isValid() && !model_index.parent().isValid() && model_index.column() == 0 && model_index.row() >= 0) {
    if (const auto &keys = m_tools.keys(); model_index.row() < keys.count()) {
      if (found)
        *found = true;
      return keys.at(model_index.row());
    }
  }

  if (found)
    *found = false;

  return {};
}

auto ExternalToolModel::revertTool(const QModelIndex &model_index) -> void
{
  const auto tool = toolForIndex(model_index);

  QTC_ASSERT(tool, return);
  QTC_ASSERT(tool->preset() && !tool->preset()->fileName().isEmpty(), return);

  const auto reset_tool = new ExternalTool(tool->preset().data());
  reset_tool->setPreset(tool->preset());

  *tool = *reset_tool;
  delete reset_tool;

  emit dataChanged(model_index, model_index);
}

auto ExternalToolModel::addCategory() -> QModelIndex
{
  const auto &category_base = tr("New Category");
  auto category = category_base;
  auto count = 0;

  while (m_tools.contains(category)) {
    ++count;
    category = category_base + QString::number(count);
  }

  auto categories = m_tools.keys();
  categories.append(category);
  Utils::sort(categories);

  const auto pos = static_cast<int>(categories.indexOf(category));
  beginInsertRows(QModelIndex(), pos, pos);
  m_tools.insert(category, QList<ExternalTool*>());
  endInsertRows();

  return index(pos, 0);
}

auto ExternalToolModel::addTool(const QModelIndex &at_index) -> QModelIndex
{
  bool found;
  auto category = categoryForIndex(at_index, &found);

  if (!found)
    category = categoryForIndex(at_index.parent(), &found);

  const auto tool = new ExternalTool;
  tool->setDisplayCategory(category);
  tool->setDisplayName(tr("New Tool"));
  tool->setDescription(tr("This tool prints a line of useful text"));

  //: Sample external tool text
  const auto text = tr("Useful text");
  if constexpr (HostOsInfo::isWindowsHost()) {
    tool->setExecutables({"cmd"});
    tool->setArguments("/c echo " + text);
  } else {
    tool->setExecutables({"echo"});
    tool->setArguments(text);
  }

  int pos;
  QModelIndex parent;
  if (at_index.parent().isValid()) {
    pos = at_index.row() + 1;
    parent = at_index.parent();
  } else {
    pos = static_cast<int>(m_tools.value(category).count());
    parent = at_index;
  }

  beginInsertRows(parent, pos, pos);
  m_tools[category].insert(pos, tool);
  endInsertRows();

  return index(pos, 0, parent);
}

auto ExternalToolModel::removeTool(const QModelIndex &model_index) -> void
{
  const auto tool = toolForIndex(model_index);
  QTC_ASSERT(tool, return);
  QTC_ASSERT(!tool->preset(), return);

  // remove the tool and the tree item
  auto category_index = 0;
  for (auto &items : m_tools) {
    if (const auto pos = static_cast<int>(items.indexOf(tool)); pos != -1) {
      beginRemoveRows(index(category_index, 0), pos, pos);
      items.removeAt(pos);
      endRemoveRows();
      break;
    }
    ++category_index;
  }
  delete tool;
}

static auto fillBaseEnvironmentComboBox(QComboBox *box) -> void
{
  box->clear();
  box->addItem(ExternalTool::tr("System Environment"), QByteArray());
  for (const auto & [id, displayName, environment] : EnvironmentProvider::providers())
    box->addItem(displayName, Id::fromName(id).toSetting());
}

class ExternalToolConfig final : public IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(Core::ExternalToolConfig)

public:
  ExternalToolConfig();

  auto setTools(const QMap<QString, QList<ExternalTool*>> &tools) -> void;
  auto tools() const -> QMap<QString, QList<ExternalTool*>> { return m_model.tools(); }
  auto apply() -> void override;

private:
  auto handleCurrentChanged(const QModelIndex &now, const QModelIndex &previous) -> void;
  auto showInfoForItem(const QModelIndex &index) -> void;
  auto updateItem(const QModelIndex &index) const -> void;
  auto revertCurrentItem() -> void;
  auto updateButtons(const QModelIndex &index) const -> void;
  auto updateCurrentItem() const -> void;
  auto addTool() -> void;
  auto removeTool() -> void;
  auto addCategory() -> void;
  auto updateEffectiveArguments() const -> void;
  auto editEnvironmentChanges() -> void;
  auto updateEnvironmentLabel() const -> void;

  Ui::ExternalToolConfig m_ui{};
  EnvironmentItems m_environment;
  ExternalToolModel m_model;
};

ExternalToolConfig::ExternalToolConfig()
{
  m_ui.setupUi(this);
  m_ui.executable->setExpectedKind(PathChooser::ExistingCommand);
  m_ui.scrollArea->viewport()->setAutoFillBackground(false);
  m_ui.scrollAreaWidgetContents->setAutoFillBackground(false);
  m_ui.toolTree->setModel(&m_model);
  m_ui.toolTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  connect(m_ui.toolTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &ExternalToolConfig::handleCurrentChanged);

  const auto chooser = new VariableChooser(this);
  chooser->addSupportedWidget(m_ui.executable->lineEdit());
  chooser->addSupportedWidget(m_ui.arguments);
  chooser->addSupportedWidget(m_ui.workingDirectory->lineEdit());
  chooser->addSupportedWidget(m_ui.inputText);

  fillBaseEnvironmentComboBox(m_ui.baseEnvironment);

  connect(m_ui.description, &QLineEdit::editingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.executable, &PathChooser::editingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.executable, &PathChooser::browsingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.arguments, &QLineEdit::editingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.arguments, &QLineEdit::editingFinished, this, &ExternalToolConfig::updateEffectiveArguments);
  connect(m_ui.workingDirectory, &PathChooser::editingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.workingDirectory, &PathChooser::browsingFinished, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.environmentButton, &QAbstractButton::clicked, this, &ExternalToolConfig::editEnvironmentChanges);
  connect(m_ui.outputBehavior, QOverload<int>::of(&QComboBox::activated), this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.errorOutputBehavior, QOverload<int>::of(&QComboBox::activated), this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.modifiesDocumentCheckbox, &QAbstractButton::clicked, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.inputText, &QPlainTextEdit::textChanged, this, &ExternalToolConfig::updateCurrentItem);
  connect(m_ui.revertButton, &QAbstractButton::clicked, this, &ExternalToolConfig::revertCurrentItem);
  connect(m_ui.removeButton, &QAbstractButton::clicked, this, &ExternalToolConfig::removeTool);

  const auto menu = new QMenu(m_ui.addButton);
  m_ui.addButton->setMenu(menu);

  const auto add_tool = new QAction(tr("Add Tool"), this);
  menu->addAction(add_tool);
  connect(add_tool, &QAction::triggered, this, &ExternalToolConfig::addTool);

  const auto add_category = new QAction(tr("Add Category"), this);
  menu->addAction(add_category);
  connect(add_category, &QAction::triggered, this, &ExternalToolConfig::addCategory);

  showInfoForItem(QModelIndex());
  setTools(ExternalToolManager::toolsByCategory());
}

auto ExternalToolConfig::setTools(const QMap<QString, QList<ExternalTool*>> &tools) -> void
{
  QMap<QString, QList<ExternalTool*>> tools_copy;

  for (auto it = tools.cbegin(), end = tools.cend(); it != end; ++it) {
    QList<ExternalTool*> item_copy;
    for (const auto tool : it.value())
      item_copy.append(new ExternalTool(tool));
    tools_copy.insert(it.key(), item_copy);
  }

  if (!tools_copy.contains(QString()))
    tools_copy.insert(QString(), QList<ExternalTool*>());

  m_model.setTools(tools_copy);
  m_ui.toolTree->expandAll();
}

auto ExternalToolConfig::handleCurrentChanged(const QModelIndex &now, const QModelIndex &previous) -> void
{
  updateItem(previous);
  showInfoForItem(now);
}

auto ExternalToolConfig::updateButtons(const QModelIndex &index) const -> void
{
  const ExternalTool *tool = ExternalToolModel::toolForIndex(index);
  if (!tool) {
    m_ui.removeButton->setEnabled(false);
    m_ui.revertButton->setEnabled(false);
    return;
  }
  if (!tool->preset()) {
    m_ui.removeButton->setEnabled(true);
    m_ui.revertButton->setEnabled(false);
  } else {
    m_ui.removeButton->setEnabled(false);
    m_ui.revertButton->setEnabled(*tool != *tool->preset());
  }
}

auto ExternalToolConfig::updateCurrentItem() const -> void
{
  const auto index = m_ui.toolTree->selectionModel()->currentIndex();
  updateItem(index);
  updateButtons(index);
}

auto ExternalToolConfig::updateItem(const QModelIndex &index) const -> void
{
  const auto tool = ExternalToolModel::toolForIndex(index);

  if (!tool)
    return;

  tool->setDescription(m_ui.description->text());
  auto executables = tool->executables();

  if (!executables.empty())
    executables[0] = m_ui.executable->rawFilePath();
  else
    executables << m_ui.executable->rawFilePath();

  tool->setExecutables(executables);
  tool->setArguments(m_ui.arguments->text());
  tool->setWorkingDirectory(m_ui.workingDirectory->rawFilePath());
  tool->setBaseEnvironmentProviderId(Id::fromSetting(m_ui.baseEnvironment->currentData()));
  tool->setEnvironmentUserChanges(m_environment);
  tool->setOutputHandling(static_cast<ExternalTool::OutputHandling>(m_ui.outputBehavior->currentIndex()));
  tool->setErrorHandling(static_cast<ExternalTool::OutputHandling>(m_ui.errorOutputBehavior->currentIndex()));
  tool->setModifiesCurrentDocument(m_ui.modifiesDocumentCheckbox->checkState());
  tool->setInput(m_ui.inputText->toPlainText());
}

auto ExternalToolConfig::showInfoForItem(const QModelIndex &index) -> void
{
  updateButtons(index);
  const ExternalTool *tool = ExternalToolModel::toolForIndex(index);

  if (!tool) {
    m_ui.description->clear();
    m_ui.executable->setFilePath({});
    m_ui.arguments->clear();
    m_ui.workingDirectory->setFilePath({});
    m_ui.inputText->clear();
    m_ui.infoWidget->setEnabled(false);
    m_environment.clear();
    return;
  }

  m_ui.infoWidget->setEnabled(true);
  m_ui.description->setText(tool->description());
  m_ui.executable->setFilePath(tool->executables().isEmpty() ? FilePath() : tool->executables().constFirst());
  m_ui.arguments->setText(tool->arguments());
  m_ui.workingDirectory->setFilePath(tool->workingDirectory());
  m_ui.outputBehavior->setCurrentIndex(tool->outputHandling());
  m_ui.errorOutputBehavior->setCurrentIndex(tool->errorHandling());
  m_ui.modifiesDocumentCheckbox->setChecked(tool->modifiesCurrentDocument());

  const auto base_environment_index = m_ui.baseEnvironment->findData(tool->baseEnvironmentProviderId().toSetting());

  m_ui.baseEnvironment->setCurrentIndex(std::max(0, base_environment_index));
  m_environment = tool->environmentUserChanges();

  QSignalBlocker blocker(m_ui.inputText);

  m_ui.inputText->setPlainText(tool->input());
  m_ui.description->setCursorPosition(0);
  m_ui.arguments->setCursorPosition(0);

  updateEnvironmentLabel();
  updateEffectiveArguments();
}

static auto getUserFilePath(const QString &proposal_file_name) -> FilePath
{
  if (const auto resource_dir(ICore::userResourcePath().toDir()); !resource_dir.exists(QLatin1String("externaltools")))
    resource_dir.mkpath(QLatin1String("externaltools"));

  const QFileInfo fi(proposal_file_name);
  const QString &suffix = QLatin1Char('.') + fi.completeSuffix();
  const auto new_file_path = ICore::userResourcePath("externaltools") / fi.baseName();
  auto count = 0;
  auto try_path = new_file_path + suffix;

  while (try_path.exists()) {
    if (++count > 15)
      return {};
    const auto number = static_cast<int>(QRandomGenerator::global()->generate() % 1000);
    try_path = new_file_path + QString::number(number) + suffix;
  }
  return try_path;
}

static auto idFromDisplayName(const QString &display_name) -> QString
{
  auto id = display_name;
  id.remove(QRegularExpression("&(?!&)"));

  auto c = id.data();
  while (!c->isNull()) {
    if (!c->isLetterOrNumber())
      *c = QLatin1Char('_');
    ++c;
  }

  return id;
}

static auto findUnusedId(const QString &proposal, const QMap<QString, QList<ExternalTool*>> &tools) -> QString
{
  auto number = 0;
  QString result;
  auto found = false;
  do {
    result = proposal + (number > 0 ? QString::number(number) : QString::fromLatin1(""));
    ++number;
    found = false;
    for (auto it = tools.cbegin(), end = tools.cend(); it != end; ++it) {
      const auto tools = it.value();
       for (const ExternalTool *tool : tools) {
        if (tool->id() == result) {
          found = true;
          break;
        }
      }
    }
  } while (found);
  return result;
}

auto ExternalToolConfig::apply() -> void
{
  const auto index = m_ui.toolTree->selectionModel()->currentIndex();
  updateItem(index);
  updateButtons(index);

  auto original_tools = ExternalToolManager::toolsById();
  const auto new_tools_map = tools();
  QMap<QString, QList<ExternalTool*>> result_map;

  for (auto it = new_tools_map.cbegin(), end = new_tools_map.cend(); it != end; ++it) {
    QList<ExternalTool*> items;
    for (const auto &tools = it.value(); const auto tool : tools) {
      ExternalTool *tool_to_add = nullptr;
      if (const auto original_tool = original_tools.take(tool->id())) {
        // check if it has different category and is custom tool
        if (tool->displayCategory() != it.key() && !tool->preset())
          tool->setDisplayCategory(it.key());
        // check if the tool has changed
        if (*original_tool == *tool) {
          tool_to_add = original_tool;
        } else {
          // case 1: tool is changed preset
          if (tool->preset() && *tool != *tool->preset()) {
            // check if we need to choose a new file name
            if (tool->preset()->fileName() == tool->fileName()) {
              const auto &file_name = tool->preset()->fileName().fileName();
              const auto &new_file_path = getUserFilePath(file_name);
              // TODO error handling if newFilePath.isEmpty() (i.e. failed to find a unused name)
              tool->setFileName(new_file_path);
            }
            // TODO error handling
            tool->save();
            // case 2: tool is previously changed preset but now same as preset
          } else if (tool->preset() && *tool == *tool->preset()) {
            // check if we need to delete the changed description
            if (original_tool->fileName() != tool->preset()->fileName() && original_tool->fileName().exists()) {
              // TODO error handling
              original_tool->fileName().removeFile();
            }
            tool->setFileName(tool->preset()->fileName());
            // no need to save, it's the same as the preset
            // case 3: tool is custom tool
          } else {
            // TODO error handling
            tool->save();
          }

          // 'tool' is deleted by config page, 'originalTool' is deleted by setToolsByCategory
          tool_to_add = new ExternalTool(tool);
        }
      } else {
        // new tool. 'tool' is deleted by config page
        auto id = idFromDisplayName(tool->displayName());
        id = findUnusedId(id, new_tools_map);
        tool->setId(id);
        // TODO error handling if newFilePath.isEmpty() (i.e. failed to find a unused name)
        tool->setFileName(getUserFilePath(id + QLatin1String(".xml")));
        // TODO error handling
        tool->save();
        tool_to_add = new ExternalTool(tool);
      }
      items.append(tool_to_add);
    }
    if (!items.isEmpty())
      result_map.insert(it.key(), items);
  }

  // Remove tools that have been deleted from the settings (and are no preset)
  for (const ExternalTool *tool : qAsConst(original_tools)) {
    QTC_ASSERT(!tool->preset(), continue);
    // TODO error handling
    tool->fileName().removeFile();
  }

  ExternalToolManager::setToolsByCategory(result_map);
}

auto ExternalToolConfig::revertCurrentItem() -> void
{
  const auto index = m_ui.toolTree->selectionModel()->currentIndex();
  m_model.revertTool(index);
  showInfoForItem(index);
}

auto ExternalToolConfig::addTool() -> void
{
  auto current_index = m_ui.toolTree->selectionModel()->currentIndex();

  if (!current_index.isValid()) // default to Uncategorized
    current_index = m_model.index(0, 0);

  const auto index = m_model.addTool(current_index);

  m_ui.toolTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Clear);
  m_ui.toolTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);
  m_ui.toolTree->edit(index);
}

auto ExternalToolConfig::removeTool() -> void
{
  const auto current_index = m_ui.toolTree->selectionModel()->currentIndex();
  m_ui.toolTree->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
  m_model.removeTool(current_index);
}

auto ExternalToolConfig::addCategory() -> void
{
  const auto index = m_model.addCategory();
  m_ui.toolTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Clear);
  m_ui.toolTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);
  m_ui.toolTree->edit(index);
}

auto ExternalToolConfig::updateEffectiveArguments() const -> void
{
  m_ui.arguments->setToolTip(Utils::globalMacroExpander()->expandProcessArgs(m_ui.arguments->text()));
}

auto ExternalToolConfig::editEnvironmentChanges() -> void
{
  const auto placeholder_text = HostOsInfo::isWindowsHost() ? tr("PATH=C:\\dev\\bin;${PATH}") : tr("PATH=/opt/bin:${PATH}");
  if (const auto new_items = EnvironmentDialog::getEnvironmentItems(m_ui.environmentLabel, m_environment, placeholder_text)) {
    m_environment = *new_items;
    updateEnvironmentLabel();
  }
}

auto ExternalToolConfig::updateEnvironmentLabel() const -> void
{
  auto short_summary = EnvironmentItem::toStringList(m_environment).join("; ");
  const QFontMetrics fm(m_ui.environmentLabel->font());
  short_summary = fm.elidedText(short_summary, Qt::ElideRight, m_ui.environmentLabel->width());
  m_ui.environmentLabel->setText(short_summary.isEmpty() ? tr("No changes to apply.") : short_summary);
}

// ToolSettingsPage

ToolSettings::ToolSettings()
{
  setId(SETTINGS_ID_TOOLS);
  setDisplayName(ExternalToolConfig::tr("External Tools"));
  setCategory(SETTINGS_CATEGORY_CORE);
  setWidgetCreator([] { return new ExternalToolConfig; });
}

} // Orca::Plugin::Core
