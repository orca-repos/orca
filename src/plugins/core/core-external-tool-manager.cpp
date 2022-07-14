// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-external-tool-manager.hpp"

#include "core-action-container.hpp"
#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-constants.hpp"
#include "core-context-interface.hpp"
#include "core-external-tool.hpp"
#include "core-interface.hpp"
#include "core-message-manager.hpp"

#include <utils/qtcassert.hpp>

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QMenu>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr char k_special_uncategorized_setting[] = "SpecialEmptyCategoryForUncategorizedTools";

struct ExternalToolManagerPrivate {
  QMap<QString, ExternalTool*> m_tools;
  QMap<QString, QList<ExternalTool*>> m_category_map;
  QMap<QString, QAction*> m_actions;
  QMap<QString, ActionContainer*> m_containers;
  QAction *m_configure_separator{};
  QAction *m_configure_action{};
};

static ExternalToolManager *m_instance = nullptr;
static ExternalToolManagerPrivate *d = nullptr;

static auto writeSettings() -> void;
static auto readSettings(const QMap<QString, ExternalTool*> &tools, QMap<QString, QList<ExternalTool*>> *category_map) -> void;
static auto parseDirectory(const QString &directory, QMap<QString, QMultiMap<int, ExternalTool*>> *category_menus, QMap<QString, ExternalTool*> *tools, bool is_preset = false) -> void;

ExternalToolManager::ExternalToolManager() : QObject(ICore::instance())
{
  m_instance = this;

  d = new ExternalToolManagerPrivate;
  d->m_configure_separator = new QAction(this);
  d->m_configure_separator->setSeparator(true);
  d->m_configure_action = new QAction(ICore::msgShowOptionsDialog(), this);

  connect(d->m_configure_action, &QAction::triggered, this, [] {
    ICore::showOptionsDialog(SETTINGS_ID_TOOLS);
  });

  // add the external tools menu
  const auto mexternaltools = ActionManager::createMenu(Id(M_TOOLS_EXTERNAL));
  mexternaltools->menu()->setTitle(tr("&External"));

  const auto mtools = ActionManager::actionContainer(M_TOOLS);
  mtools->addMenu(mexternaltools, G_DEFAULT_THREE);

  QMap<QString, QMultiMap<int, ExternalTool*>> category_priority_map;
  QMap<QString, ExternalTool*> tools;

  parseDirectory(ICore::userResourcePath("externaltools").toString(), &category_priority_map, &tools);
  parseDirectory(ICore::resourcePath("externaltools").toString(), &category_priority_map, &tools, true);

  QMap<QString, QList<ExternalTool*>> category_map;

  for (auto it = category_priority_map.cbegin(), end = category_priority_map.cend(); it != end; ++it)
    category_map.insert(it.key(), it.value().values());

  // read renamed categories and custom order
  readSettings(tools, &category_map);
  setToolsByCategory(category_map);
}

ExternalToolManager::~ExternalToolManager()
{
  writeSettings();
  qDeleteAll(d->m_tools);  // TODO kill running tools
  delete d;
}

auto ExternalToolManager::instance() -> ExternalToolManager*
{
  return m_instance;
}

static auto parseDirectory(const QString &directory, QMap<QString, QMultiMap<int, ExternalTool*>> *category_menus, QMap<QString, ExternalTool*> *tools, const bool is_preset) -> void
{
  QTC_ASSERT(category_menus, return);
  QTC_ASSERT(tools, return);

  for(const QDir dir(directory, QLatin1String("*.xml"), QDir::Unsorted, QDir::Files | QDir::Readable); const auto &info: dir.entryInfoList()) {
    const auto &file_name = info.absoluteFilePath();
    QString error;
    auto tool = ExternalTool::createFromFile(FilePath::fromString(file_name), &error, ICore::userInterfaceLanguage());

    if (!tool) {
      qWarning() << ExternalTool::tr("Error while parsing external tool %1: %2").arg(file_name, error);
      continue;
    }

    if (tools->contains(tool->id())) {
      if (is_preset) {
        // preset that was changed
        const auto other = tools->value(tool->id());
        other->setPreset(QSharedPointer<ExternalTool>(tool));
      } else {
        qWarning() << ExternalToolManager::tr("Error: External tool in %1 has duplicate id").arg(file_name);
        delete tool;
      }
      continue;
    }

    if (is_preset) {
      // preset that wasn't changed --> save original values
      tool->setPreset(QSharedPointer<ExternalTool>(new ExternalTool(tool)));
    }

    tools->insert(tool->id(), tool);
    (*category_menus)[tool->displayCategory()].insert(tool->order(), tool);
  }
}

auto ExternalToolManager::toolsByCategory() -> QMap<QString, QList<ExternalTool*>>
{
  return d->m_category_map;
}

auto ExternalToolManager::toolsById() -> QMap<QString, ExternalTool*>
{
  return d->m_tools;
}

auto ExternalToolManager::setToolsByCategory(const QMap<QString, QList<ExternalTool*>> &tools) -> void
{
  // clear menu
  const auto mexternaltools = ActionManager::actionContainer(Id(M_TOOLS_EXTERNAL));
  mexternaltools->clear();

  // delete old tools and create list of new ones
  QMap<QString, ExternalTool*> new_tools;
  QMap<QString, QAction*> new_actions;

  for (auto it = tools.cbegin(), end = tools.cend(); it != end; ++it) {
    for(auto tool: it.value()) {
      const auto id = tool->id();
      if (d->m_tools.value(id) == tool) {
        new_actions.insert(id, d->m_actions.value(id));
        // remove from list to prevent deletion
        d->m_tools.remove(id);
        d->m_actions.remove(id);
      }
      new_tools.insert(id, tool);
    }
  }

  qDeleteAll(d->m_tools);

  const Id external_tools_prefix = "Tools.External.";
  for (auto remaining_actions = d->m_actions.cbegin(), end = d->m_actions.cend(); remaining_actions != end; ++remaining_actions) {
    ActionManager::unregisterAction(remaining_actions.value(), external_tools_prefix.withSuffix(remaining_actions.key()));
    delete remaining_actions.value();
  }

  // assign the new stuff
  d->m_actions.clear();
  d->m_tools = new_tools;
  d->m_actions = new_actions;
  d->m_category_map = tools;

  // create menu structure and remove no-longer used containers
  // add all the category menus, QMap is nicely sorted
  QMap<QString, ActionContainer*> new_containers;

  for (auto it = tools.cbegin(), end = tools.cend(); it != end; ++it) {
    ActionContainer *container = nullptr;
    if (const auto &container_name = it.key(); container_name.isEmpty()) {
      // no displayCategory, so put into external tools menu directly
      container = mexternaltools;
    } else {
      if (d->m_containers.contains(container_name))
        container = d->m_containers.take(container_name); // remove to avoid deletion below
      else
        container = ActionManager::createMenu(Id("Tools.External.Category.").withSuffix(container_name));

      new_containers.insert(container_name, container);
      mexternaltools->addMenu(container, G_DEFAULT_ONE);
      container->menu()->setTitle(container_name);
    }
    for(auto tool: it.value()) {
      const auto &tool_id = tool->id();
      // tool action and command
      QAction *action = nullptr;
      Command *command = nullptr;
      if (d->m_actions.contains(tool_id)) {
        action = d->m_actions.value(tool_id);
        command = ActionManager::command(external_tools_prefix.withSuffix(tool_id));
      } else {
        action = new QAction(tool->displayName(), m_instance);
        d->m_actions.insert(tool_id, action);
        connect(action, &QAction::triggered, tool, [tool] {
          if (const auto runner = new ExternalToolRunner(tool); runner->hasError())
            MessageManager::writeFlashing(runner->errorString());
        });
        command = ActionManager::registerAction(action, external_tools_prefix.withSuffix(tool_id));
        command->setAttribute(Command::CA_UpdateText);
      }
      action->setText(tool->displayName());
      action->setToolTip(tool->description());
      action->setWhatsThis(tool->description());
      container->addAction(command, G_DEFAULT_TWO);
    }
  }

  // delete the unused containers
  qDeleteAll(d->m_containers);
  // remember the new containers
  d->m_containers = new_containers;

  // (re)add the configure menu item
  mexternaltools->menu()->addAction(d->m_configure_separator);
  mexternaltools->menu()->addAction(d->m_configure_action);
}

static auto readSettings(const QMap<QString, ExternalTool*> &tools, QMap<QString, QList<ExternalTool*>> *category_map) -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(QLatin1String("ExternalTools"));

  if (category_map) {
    settings->beginGroup(QLatin1String("OverrideCategories"));
    for(const auto &settings_category: settings->childGroups()) {
      auto display_category = settings_category;
      if (display_category == QLatin1String(k_special_uncategorized_setting))
        display_category = QLatin1String("");
      const auto count = settings->beginReadArray(settings_category);
      for (auto i = 0; i < count; ++i) {
        settings->setArrayIndex(i);
        if (const auto &tool_id = settings->value(QLatin1String("Tool")).toString(); tools.contains(tool_id)) {
          auto tool = tools.value(tool_id);
          // remove from old category
          (*category_map)[tool->displayCategory()].removeAll(tool);
          if (category_map->value(tool->displayCategory()).isEmpty())
            category_map->remove(tool->displayCategory());
          // add to new category
          (*category_map)[display_category].append(tool);
        }
      }
      settings->endArray();
    }
    settings->endGroup();
  }
  settings->endGroup();
}

static auto writeSettings() -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(QLatin1String("ExternalTools"));
  settings->remove(QLatin1String(""));
  settings->beginGroup(QLatin1String("OverrideCategories"));

  for (auto it = d->m_category_map.cbegin(), end = d->m_category_map.cend(); it != end; ++it) {
    auto category = it.key();
    if (category.isEmpty())
      category = QLatin1String(k_special_uncategorized_setting);
    settings->beginWriteArray(category, static_cast<int>(it.value().count()));
    auto i = 0;
    for(const auto tool: it.value()) {
      settings->setArrayIndex(i);
      settings->setValue(QLatin1String("Tool"), tool->id());
      ++i;
    }
    settings->endArray();
  }
  settings->endGroup();
  settings->endGroup();
}

auto ExternalToolManager::emitReplaceSelectionRequested(const QString &output) -> void
{
  emit m_instance->replaceSelectionRequested(output);
}

} // namespace Orca::Plugin::Core
