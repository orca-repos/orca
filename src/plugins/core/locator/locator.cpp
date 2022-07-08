// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "locator.hpp"
#include "directoryfilter.hpp"
#include "executefilter.hpp"
#include "externaltoolsfilter.hpp"
#include "filesystemfilter.hpp"
#include "javascriptfilter.hpp"
#include "locatorconstants.hpp"
#include "locatorfiltersfilter.hpp"
#include "locatormanager.hpp"
#include "locatorsettingspage.hpp"
#include "locatorwidget.hpp"
#include "opendocumentsfilter.hpp"
#include "spotlightlocatorfilter.hpp"
#include "urllocatorfilter.hpp"

#include <core/coreplugin.hpp>
#include <core/coreconstants.hpp>
#include <core/icore.hpp>
#include <core/settingsdatabase.hpp>
#include <core/statusbarmanager.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/actioncontainer.hpp>
#include <core/editormanager/editormanager_p.hpp>
#include <core/menubarfilter.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/mapreduce.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <utility>

using namespace Utils;

namespace Core {
namespace Internal {

static Locator *m_instance = nullptr;

constexpr char k_directory_filter_prefix[] = "directory";
constexpr char k_url_filter_prefix[] = "url";

class LocatorData {
public:
  LocatorData();

  LocatorManager m_locator_manager;
  LocatorSettingsPage m_locator_settings_page;
  JavaScriptFilter m_java_script_filter;
  OpenDocumentsFilter m_open_documents_filter;
  FileSystemFilter m_file_system_filter;
  ExecuteFilter m_execute_filter;
  ExternalToolsFilter m_external_tools_filter;
  LocatorFiltersFilter m_locators_filters_filter;
  MenuBarFilter m_menubar_filter;
  UrlLocatorFilter m_url_filter{UrlLocatorFilter::tr("Web Search"), "RemoteHelpFilter"};
  UrlLocatorFilter m_bug_filter{UrlLocatorFilter::tr("Qt Project Bugs"), "QtProjectBugs"};
  SpotlightLocatorFilter m_spotlight_locator_filter;
};

LocatorData::LocatorData()
{
  m_url_filter.setDefaultShortcutString("r");
  m_url_filter.addDefaultUrl("https://www.bing.com/search?q=%1");
  m_url_filter.addDefaultUrl("https://www.google.com/search?q=%1");
  m_url_filter.addDefaultUrl("https://search.yahoo.com/search?p=%1");
  m_url_filter.addDefaultUrl("https://stackoverflow.com/search?q=%1");
  m_url_filter.addDefaultUrl("http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=%1");
  m_url_filter.addDefaultUrl("https://en.wikipedia.org/w/index.php?search=%1");
  m_bug_filter.setDefaultShortcutString("bug");
  m_bug_filter.addDefaultUrl("https://bugreports.qt.io/secure/QuickSearch.jspa?searchString=%1");
}

Locator::Locator()
{
  m_instance = this;
  m_refresh_timer.setSingleShot(false);
  connect(&m_refresh_timer, &QTimer::timeout, this, [this] { refresh(filters()); });
}

Locator::~Locator()
{
  delete m_locator_data;
  qDeleteAll(m_custom_filters);
}

auto Locator::instance() -> Locator*
{
  return m_instance;
}

auto Locator::initialize() -> void
{
  m_locator_data = new LocatorData;

  const auto action = new QAction(Icons::ZOOM.icon(), tr("Locate..."), this);
  const auto cmd = ActionManager::registerAction(action, Constants::locate);

  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+K")));
  connect(action, &QAction::triggered, this, [] {
    LocatorManager::show(QString());
  });

  const auto mtools = ActionManager::actionContainer(Constants::M_TOOLS);
  mtools->addAction(cmd);

  const auto locator_widget = LocatorManager::createLocatorInputWidget(ICore::mainWindow());
  locator_widget->setObjectName("LocatorInput"); // used for UI introduction

  StatusBarManager::addStatusBarWidget(locator_widget, StatusBarManager::First, Context("LocatorWidget"));
  connect(ICore::instance(), &ICore::saveSettingsRequested, this, &Locator::saveSettings);
}

auto Locator::extensionsInitialized() -> void
{
  m_filters = ILocatorFilter::allLocatorFilters();
  sort(m_filters, [](const ILocatorFilter *first, const ILocatorFilter *second) -> bool {
    if (first->priority() != second->priority())
      return first->priority() < second->priority();
    return first->id().alphabeticallyBefore(second->id());
  });
  setFilters(m_filters);

  const auto open_command = ActionManager::command(Constants::OPEN);
  const auto locate_command = ActionManager::command(Constants::locate);

  connect(open_command, &Command::keySequenceChanged, this, &Locator::updateEditorManagerPlaceholderText);
  connect(locate_command, &Command::keySequenceChanged, this, &Locator::updateEditorManagerPlaceholderText);
  updateEditorManagerPlaceholderText();
}

auto Locator::delayedInitialize() -> bool
{
  loadSettings();
  return true;
}

auto Locator::aboutToShutdown(const std::function<void()> &emit_asynchronous_shutdown_finished) -> ExtensionSystem::IPlugin::ShutdownFlag
{
  m_shutting_down = true;
  m_refresh_timer.stop();

  if (m_refresh_task.isRunning()) {
    m_refresh_task.cancel();
    m_refresh_task.waitForFinished();
  }

  return LocatorWidget::aboutToShutdown(emit_asynchronous_shutdown_finished);
}

auto Locator::loadSettings() -> void
{
  const auto settings = ICore::settingsDatabase();
  // check if we have to read old settings
  // TOOD remove a few versions after 4.15
  const auto settings_group = settings->contains("Locator") ? QString("Locator") : QString("QuickOpen");
  settings->beginGroup(settings_group);
  m_refresh_timer.setInterval(settings->value("RefreshInterval", 60).toInt() * 60000);

  for (const auto filter : qAsConst(m_filters)) {
    if (settings->contains(filter->id().toString())) {
      if (const auto state = settings->value(filter->id().toString()).toByteArray(); !state.isEmpty())
        filter->restoreState(state);
    }
  }

  settings->beginGroup("CustomFilters");
  QList<ILocatorFilter*> custom_filters;
  const auto keys = settings->childKeys();
  auto count = 0;
  const Id directory_base_id(Constants::custom_directory_filter_baseid);
  const Id url_base_id(Constants::custom_url_filter_baseid);

  for (const auto &key : keys) {
    ++count;
    ILocatorFilter *filter;
    if (key.startsWith(k_directory_filter_prefix)) {
      filter = new DirectoryFilter(directory_base_id.withSuffix(count));
    } else {
      const auto url_filter = new UrlLocatorFilter(url_base_id.withSuffix(count));
      url_filter->setIsCustomFilter(true);
      filter = url_filter;
    }
    filter->restoreState(settings->value(key).toByteArray());
    custom_filters.append(filter);
  }

  setCustomFilters(custom_filters);
  settings->endGroup();
  settings->endGroup();

  if (m_refresh_timer.interval() > 0)
    m_refresh_timer.start();

  m_settings_initialized = true;
  setFilters(m_filters + custom_filters);
}

auto Locator::updateFilterActions() -> void
{
  auto action_copy = m_filter_action_map;
  m_filter_action_map.clear();

  // register new actions, update existent
  for (auto filter : qAsConst(m_filters)) {
    if (filter->shortcutString().isEmpty() || filter->isHidden())
      continue;

    auto filter_id = filter->id();
    const auto action_id = filter->actionId();
    QAction *action = nullptr;

    if (!action_copy.contains(filter_id)) {
      // register new action
      action = new QAction(filter->displayName(), this);
      const auto cmd = ActionManager::registerAction(action, action_id);
      cmd->setAttribute(Command::CA_UpdateText);
      connect(action, &QAction::triggered, this, [filter] {
        LocatorManager::showFilter(filter);
      });
    } else {
      action = action_copy.take(filter_id);
      action->setText(filter->displayName());
    }
    action->setToolTip(filter->description());
    m_filter_action_map.insert(filter_id, action);
  }

  // unregister actions that are deleted now
  const auto end = action_copy.end();

  for (auto it = action_copy.begin(); it != end; ++it) {
    ActionManager::unregisterAction(it.value(), it.key().withPrefix("Locator."));
    delete it.value();
  }
}

auto Locator::updateEditorManagerPlaceholderText() -> void
{
  const auto open_command = ActionManager::command(Constants::OPEN);
  const auto locate_command = ActionManager::command(Constants::locate);
  const auto placeholder_text = tr("<html><body style=\"color:#909090; font-size:14px\">" "<div align='center'>" "<div style=\"font-size:20px\">Open a document</div>" "<table><tr><td>" "<hr/>" "<div style=\"margin-top: 5px\">&bull; File > Open File or Project (%1)</div>" "<div style=\"margin-top: 5px\">&bull; File > Recent Files</div>" "<div style=\"margin-top: 5px\">&bull; Tools > Locate (%2) and</div>" "<div style=\"margin-left: 1em\">- type to open file from any open project</div>" "%4" "%5" "<div style=\"margin-left: 1em\">- type <code>%3&lt;space&gt;&lt;filename&gt;</code> to open file from file system</div>" "<div style=\"margin-left: 1em\">- select one of the other filters for jumping to a location</div>" "<div style=\"margin-top: 5px\">&bull; Drag and drop files here</div>" "</td></tr></table>" "</div>" "</body></html>").arg(open_command->keySequence().toString(QKeySequence::NativeText)).arg(locate_command->keySequence().toString(QKeySequence::NativeText)).arg(m_locator_data->m_file_system_filter.shortcutString());

  QString classes;

  // not nice, but anyhow
  if (const auto classes_filter = findOrDefault(m_filters, equal(&ILocatorFilter::id, Id("Classes"))))
    classes = tr("<div style=\"margin-left: 1em\">- type <code>%1&lt;space&gt;&lt;pattern&gt;</code>" " to jump to a class definition</div>").arg(classes_filter->shortcutString());

  QString methods;

  // not nice, but anyhow
  if (const auto methods_filter = findOrDefault(m_filters, equal(&ILocatorFilter::id, Id("Methods"))))
    methods = tr("<div style=\"margin-left: 1em\">- type <code>%1&lt;space&gt;&lt;pattern&gt;</code>" " to jump to a function definition</div>").arg(methods_filter->shortcutString());

  EditorManagerPrivate::setPlaceholderText(placeholder_text.arg(classes, methods));
}

auto Locator::saveSettings() const -> void
{
  if (!m_settings_initialized)
    return;

  const auto s = ICore::settingsDatabase();
  s->beginTransaction();
  s->beginGroup("Locator");
  s->remove(QString());
  s->setValue("RefreshInterval", refreshInterval());

  for (auto filter : m_filters) {
    if (!m_custom_filters.contains(filter) && filter->id().isValid()) {
      const auto state = filter->saveState();
      s->setValueWithDefault(filter->id().toString(), state);
    }
  }

  s->beginGroup("CustomFilters");
  auto i = 0;

  for (const auto filter : m_custom_filters) {
    auto prefix = filter->id().name().startsWith(Constants::custom_directory_filter_baseid) ? k_directory_filter_prefix : k_url_filter_prefix;
    const auto state = filter->saveState();
    s->setValueWithDefault(prefix + QString::number(i), state);
    ++i;
  }

  s->endGroup();
  s->endGroup();
  s->endTransaction();
}

/*!
    Return all filters, including the ones created by the user.
*/
auto Locator::filters() -> QList<ILocatorFilter*>
{
  return m_instance->m_filters;
}

/*!
    This returns a subset of all the filters, that contains only the filters that
    have been created by the user at some point (maybe in a previous session).
 */
auto Locator::customFilters() -> QList<ILocatorFilter*>
{
  return m_custom_filters;
}

auto Locator::setFilters(QList<ILocatorFilter*> f) -> void
{
  m_filters = std::move(f);
  updateFilterActions();
  updateEditorManagerPlaceholderText(); // possibly some shortcut changed
  emit filtersChanged();
}

auto Locator::setCustomFilters(QList<ILocatorFilter*> filters) -> void
{
  m_custom_filters = std::move(filters);
}

auto Locator::refreshInterval() const -> int
{
  return m_refresh_timer.interval() / 60000;
}

auto Locator::setRefreshInterval(const int interval) -> void
{
  if (interval < 1) {
    m_refresh_timer.stop();
    m_refresh_timer.setInterval(0);
    return;
  }

  m_refresh_timer.setInterval(interval * 60000);
  m_refresh_timer.start();
}

auto Locator::refresh(QList<ILocatorFilter*> filters) -> void
{
  if (m_shutting_down)
    return;

  if (m_refresh_task.isRunning()) {
    m_refresh_task.cancel();
    m_refresh_task.waitForFinished();
    // this is not ideal because some of the previous filters might have finished, but we
    // currently cannot find out which part of a map-reduce has finished
    filters = filteredUnique(m_refreshing_filters + filters);
  }

  m_refreshing_filters = filters;
  m_refresh_task = map(filters, &ILocatorFilter::refresh, MapReduceOption::Unordered);

  ProgressManager::addTask(m_refresh_task, tr("Updating Locator Caches"), Constants::task_index);
  onFinished(m_refresh_task, this, [this](const QFuture<void> &future) {
    if (!future.isCanceled()) {
      saveSettings();
      m_refreshing_filters.clear();
      m_refresh_task = QFuture<void>();
    }
  });
}

} // namespace Internal
} // namespace Core
