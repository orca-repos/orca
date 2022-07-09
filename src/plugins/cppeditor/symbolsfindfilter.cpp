// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "symbolsfindfilter.hpp"

#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"

#include <core/icore.hpp>
#include <core/progressmanager/futureprogress.hpp>
#include <core/progressmanager/progressmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/find/searchresultwindow.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/session.hpp>

#include <utils/algorithm.hpp>
#include <utils/runextensions.hpp>
#include <utils/qtcassert.hpp>

#include <QSet>
#include <QGridLayout>
#include <QLabel>
#include <QButtonGroup>

using namespace Core;
using namespace Utils;

namespace CppEditor::Internal {

constexpr char SETTINGS_GROUP[] = "CppSymbols";
constexpr char SETTINGS_SYMBOLTYPES[] = "SymbolsToSearchFor";
constexpr char SETTINGS_SEARCHSCOPE[] = "SearchScope";

SymbolsFindFilter::SymbolsFindFilter(CppModelManager *manager) : m_manager(manager), m_enabled(true), m_symbolsToSearch(SearchSymbols::AllTypes), m_scope(SymbolSearcher::SearchProjectsOnly)
{
  // for disabling while parser is running
  connect(ProgressManager::instance(), &ProgressManager::taskStarted, this, &SymbolsFindFilter::onTaskStarted);
  connect(ProgressManager::instance(), &ProgressManager::allTasksFinished, this, &SymbolsFindFilter::onAllTasksFinished);
}

auto SymbolsFindFilter::id() const -> QString
{
  return QLatin1String(Constants::SYMBOLS_FIND_FILTER_ID);
}

auto SymbolsFindFilter::displayName() const -> QString
{
  return QString(Constants::SYMBOLS_FIND_FILTER_DISPLAY_NAME);
}

auto SymbolsFindFilter::isEnabled() const -> bool
{
  return m_enabled;
}

auto SymbolsFindFilter::cancel() -> void
{
  auto search = qobject_cast<SearchResult*>(sender());
  QTC_ASSERT(search, return);
  auto watcher = m_watchers.key(search);
  QTC_ASSERT(watcher, return);
  watcher->cancel();
}

auto SymbolsFindFilter::setPaused(bool paused) -> void
{
  auto search = qobject_cast<SearchResult*>(sender());
  QTC_ASSERT(search, return);
  auto watcher = m_watchers.key(search);
  QTC_ASSERT(watcher, return);
  if (!paused || watcher->isRunning()) // guard against pausing when the search is finished
    watcher->setPaused(paused);
}

auto SymbolsFindFilter::findAll(const QString &txt, FindFlags findFlags) -> void
{
  auto window = SearchResultWindow::instance();
  auto search = window->startNewSearch(label(), toolTip(findFlags), txt);
  search->setSearchAgainSupported(true);
  connect(search, &SearchResult::activated, this, &SymbolsFindFilter::openEditor);
  connect(search, &SearchResult::cancelled, this, &SymbolsFindFilter::cancel);
  connect(search, &SearchResult::paused, this, &SymbolsFindFilter::setPaused);
  connect(search, &SearchResult::searchAgainRequested, this, &SymbolsFindFilter::searchAgain);
  connect(this, &IFindFilter::enabledChanged, search, &SearchResult::setSearchAgainEnabled);
  window->popup(IOutputPane::ModeSwitch | IOutputPane::WithFocus);

  SymbolSearcher::Parameters parameters;
  parameters.text = txt;
  parameters.flags = findFlags;
  parameters.types = m_symbolsToSearch;
  parameters.scope = m_scope;
  search->setUserData(QVariant::fromValue(parameters));
  startSearch(search);
}

auto SymbolsFindFilter::startSearch(SearchResult *search) -> void
{
  auto parameters = search->userData().value<SymbolSearcher::Parameters>();
  QSet<QString> projectFileNames;
  if (parameters.scope == SymbolSearcher::SearchProjectsOnly) {
    for (auto project : ProjectExplorer::SessionManager::projects())
      projectFileNames += Utils::transform<QSet>(project->files(ProjectExplorer::Project::AllFiles), &Utils::FilePath::toString);
  }

  auto watcher = new QFutureWatcher<SearchResultItem>;
  m_watchers.insert(watcher, search);
  connect(watcher, &QFutureWatcherBase::finished, this, &SymbolsFindFilter::finish);
  connect(watcher, &QFutureWatcherBase::resultsReadyAt, this, &SymbolsFindFilter::addResults);
  auto symbolSearcher = m_manager->indexingSupport()->createSymbolSearcher(parameters, projectFileNames);
  connect(watcher, &QFutureWatcherBase::finished, symbolSearcher, &QObject::deleteLater);
  watcher->setFuture(Utils::runAsync(m_manager->sharedThreadPool(), &SymbolSearcher::runSearch, symbolSearcher));
  auto progress = ProgressManager::addTask(watcher->future(), tr("Searching for Symbol"), Core::Constants::TASK_SEARCH);
  connect(progress, &FutureProgress::clicked, search, &SearchResult::popup);
}

auto SymbolsFindFilter::addResults(int begin, int end) -> void
{
  auto watcher = static_cast<QFutureWatcher<SearchResultItem>*>(sender());
  SearchResult *search = m_watchers.value(watcher);
  if (!search) {
    // search was removed from search history while the search is running
    watcher->cancel();
    return;
  }
  QList<SearchResultItem> items;
  for (auto i = begin; i < end; ++i)
    items << watcher->resultAt(i);
  search->addResults(items, SearchResult::AddSorted);
}

auto SymbolsFindFilter::finish() -> void
{
  auto watcher = static_cast<QFutureWatcher<SearchResultItem>*>(sender());
  SearchResult *search = m_watchers.value(watcher);
  if (search)
    search->finishSearch(watcher->isCanceled());
  m_watchers.remove(watcher);
  watcher->deleteLater();
}

auto SymbolsFindFilter::openEditor(const SearchResultItem &item) -> void
{
  if (!item.userData().canConvert<IndexItem::Ptr>())
    return;
  auto info = item.userData().value<IndexItem::Ptr>();
  EditorManager::openEditorAt({FilePath::fromString(info->fileName()), info->line(), info->column()}, {}, Core::EditorManager::AllowExternalEditor);
}

auto SymbolsFindFilter::createConfigWidget() -> QWidget*
{
  return new SymbolsFindFilterConfigWidget(this);
}

auto SymbolsFindFilter::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String(SETTINGS_GROUP));
  settings->setValue(QLatin1String(SETTINGS_SYMBOLTYPES), int(m_symbolsToSearch));
  settings->setValue(QLatin1String(SETTINGS_SEARCHSCOPE), int(m_scope));
  settings->endGroup();
}

auto SymbolsFindFilter::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String(SETTINGS_GROUP));
  m_symbolsToSearch = static_cast<SearchSymbols::SymbolTypes>(settings->value(QLatin1String(SETTINGS_SYMBOLTYPES), int(SearchSymbols::AllTypes)).toInt());
  m_scope = static_cast<SearchScope>(settings->value(QLatin1String(SETTINGS_SEARCHSCOPE), int(SymbolSearcher::SearchProjectsOnly)).toInt());
  settings->endGroup();
  emit symbolsToSearchChanged();
}

auto SymbolsFindFilter::onTaskStarted(Id type) -> void
{
  if (type == Constants::TASK_INDEX) {
    m_enabled = false;
    emit enabledChanged(m_enabled);
  }
}

auto SymbolsFindFilter::onAllTasksFinished(Id type) -> void
{
  if (type == Constants::TASK_INDEX) {
    m_enabled = true;
    emit enabledChanged(m_enabled);
  }
}

auto SymbolsFindFilter::searchAgain() -> void
{
  auto search = qobject_cast<SearchResult*>(sender());
  QTC_ASSERT(search, return);
  search->restart();
  startSearch(search);
}

auto SymbolsFindFilter::label() const -> QString
{
  return tr("C++ Symbols:");
}

auto SymbolsFindFilter::toolTip(FindFlags findFlags) const -> QString
{
  QStringList types;
  if (m_symbolsToSearch & SymbolSearcher::Classes)
    types.append(tr("Classes"));
  if (m_symbolsToSearch & SymbolSearcher::Functions)
    types.append(tr("Functions"));
  if (m_symbolsToSearch & SymbolSearcher::Enums)
    types.append(tr("Enums"));
  if (m_symbolsToSearch & SymbolSearcher::Declarations)
    types.append(tr("Declarations"));
  return tr("Scope: %1\nTypes: %2\nFlags: %3").arg(searchScope() == SymbolSearcher::SearchGlobal ? tr("All") : tr("Projects"), types.join(", "), IFindFilter::descriptionForFindFlags(findFlags));
}

// #pragma mark -- SymbolsFindFilterConfigWidget

SymbolsFindFilterConfigWidget::SymbolsFindFilterConfigWidget(SymbolsFindFilter *filter) : m_filter(filter)
{
  connect(m_filter, &SymbolsFindFilter::symbolsToSearchChanged, this, &SymbolsFindFilterConfigWidget::getState);

  auto layout = new QGridLayout(this);
  setLayout(layout);
  layout->setContentsMargins(0, 0, 0, 0);

  auto typeLabel = new QLabel(tr("Types:"));
  layout->addWidget(typeLabel, 0, 0);

  m_typeClasses = new QCheckBox(tr("Classes"));
  layout->addWidget(m_typeClasses, 0, 1);

  m_typeMethods = new QCheckBox(tr("Functions"));
  layout->addWidget(m_typeMethods, 0, 2);

  m_typeEnums = new QCheckBox(tr("Enums"));
  layout->addWidget(m_typeEnums, 1, 1);

  m_typeDeclarations = new QCheckBox(tr("Declarations"));
  layout->addWidget(m_typeDeclarations, 1, 2);

  // hacks to fix layouting:
  typeLabel->setMinimumWidth(80);
  typeLabel->setAlignment(Qt::AlignRight);
  m_typeClasses->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_typeMethods->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  connect(m_typeClasses, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);
  connect(m_typeMethods, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);
  connect(m_typeEnums, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);
  connect(m_typeDeclarations, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);

  m_searchProjectsOnly = new QRadioButton(tr("Projects only"));
  layout->addWidget(m_searchProjectsOnly, 2, 1);

  m_searchGlobal = new QRadioButton(tr("All files"));
  layout->addWidget(m_searchGlobal, 2, 2);

  m_searchGroup = new QButtonGroup(this);
  m_searchGroup->addButton(m_searchProjectsOnly);
  m_searchGroup->addButton(m_searchGlobal);

  connect(m_searchProjectsOnly, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);
  connect(m_searchGlobal, &QAbstractButton::clicked, this, &SymbolsFindFilterConfigWidget::setState);
}

auto SymbolsFindFilterConfigWidget::getState() -> void
{
  auto symbols = m_filter->symbolsToSearch();
  m_typeClasses->setChecked(symbols & SymbolSearcher::Classes);
  m_typeMethods->setChecked(symbols & SymbolSearcher::Functions);
  m_typeEnums->setChecked(symbols & SymbolSearcher::Enums);
  m_typeDeclarations->setChecked(symbols & SymbolSearcher::Declarations);

  auto scope = m_filter->searchScope();
  m_searchProjectsOnly->setChecked(scope == SymbolSearcher::SearchProjectsOnly);
  m_searchGlobal->setChecked(scope == SymbolSearcher::SearchGlobal);
}

auto SymbolsFindFilterConfigWidget::setState() const -> void
{
  SearchSymbols::SymbolTypes symbols;
  if (m_typeClasses->isChecked())
    symbols |= SymbolSearcher::Classes;
  if (m_typeMethods->isChecked())
    symbols |= SymbolSearcher::Functions;
  if (m_typeEnums->isChecked())
    symbols |= SymbolSearcher::Enums;
  if (m_typeDeclarations->isChecked())
    symbols |= SymbolSearcher::Declarations;
  m_filter->setSymbolsToSearch(symbols);

  if (m_searchProjectsOnly->isChecked())
    m_filter->setSearchScope(SymbolSearcher::SearchProjectsOnly);
  else
    m_filter->setSearchScope(SymbolSearcher::SearchGlobal);
}

} // namespace CppEditor::Internal
