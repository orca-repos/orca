// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "basefilefind.hpp"
#include "textdocument.hpp"

#include <texteditor/refactoringchanges.hpp>
#include <texteditor/texteditor.hpp>

#include <core/dialogs/readonlyfilesdialog.hpp>
#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/find/ifindsupport.hpp>
#include <core/icore.hpp>
#include <core/progressmanager/futureprogress.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <aggregation/aggregate.hpp>

#include <utils/algorithm.hpp>
#include <utils/fadingindicator.hpp>
#include <utils/filesearch.hpp>
#include <utils/futuresynchronizer.hpp>

#include <QSettings>
#include <QHash>
#include <QPair>
#include <QStringListModel>
#include <QFutureWatcher>
#include <QPointer>
#include <QComboBox>
#include <QLabel>

using namespace Utils;
using namespace Core;

namespace TextEditor {
namespace Internal {
namespace {

class InternalEngine : public SearchEngine {
public:
  InternalEngine() : m_widget(new QWidget) {}
  ~InternalEngine() override { delete m_widget; }

  auto title() const -> QString override { return tr("Internal"); }
  auto toolTip() const -> QString override { return {}; }
  auto widget() const -> QWidget* override { return m_widget; }
  auto parameters() const -> QVariant override { return {}; }
  auto readSettings(QSettings * /*settings*/) -> void override {}
  auto writeSettings(QSettings * /*settings*/) const -> void override {}

  auto executeSearch(const FileFindParameters &parameters, BaseFileFind *baseFileFind) -> QFuture<FileSearchResultList> override
  {
    const auto func = parameters.flags & FindRegularExpression ? findInFilesRegExp : findInFiles;

    return func(parameters.text, baseFileFind->files(parameters.nameFilters, parameters.exclusionFilters, parameters.additionalParameters), textDocumentFlagsForFindFlags(parameters.flags), TextDocument::openedTextDocumentContents());
  }

  auto openEditor(const SearchResultItem &/*item*/, const FileFindParameters &/*parameters*/) -> IEditor* override
  {
    return nullptr;
  }

private:
  QWidget *m_widget;
};

} // namespace

class SearchEnginePrivate {
public:
  bool isEnabled = true;
};

class CountingLabel : public QLabel {
public:
  CountingLabel();
  auto updateCount(int count) -> void;
};

class BaseFileFindPrivate {
public:
  BaseFileFindPrivate() { m_futureSynchronizer.setCancelOnWait(true); }

  QPointer<IFindSupport> m_currentFindSupport;

  FutureSynchronizer m_futureSynchronizer;
  QLabel *m_resultLabel = nullptr;
  // models in native path format
  QStringListModel m_filterStrings;
  QStringListModel m_exclusionStrings;
  // current filter in portable path format
  QString m_filterSetting;
  QString m_exclusionSetting;
  QPointer<QComboBox> m_filterCombo;
  QPointer<QComboBox> m_exclusionCombo;
  QVector<SearchEngine*> m_searchEngines;
  InternalEngine m_internalSearchEngine;
  int m_currentSearchEngineIndex = -1;
};

} // namespace Internal

static auto syncComboWithSettings(QComboBox *combo, const QString &setting) -> void
{
  if (!combo)
    return;
  const auto &nativeSettings = QDir::toNativeSeparators(setting);
  const auto index = combo->findText(nativeSettings);
  if (index < 0)
    combo->setEditText(nativeSettings);
  else
    combo->setCurrentIndex(index);
}

static auto updateComboEntries(QComboBox *combo, bool onTop) -> void
{
  const auto index = combo->findText(combo->currentText());
  if (index < 0) {
    if (onTop)
      combo->insertItem(0, combo->currentText());
    else
      combo->addItem(combo->currentText());
    combo->setCurrentIndex(combo->findText(combo->currentText()));
  }
}

using namespace Internal;

SearchEngine::SearchEngine(QObject *parent) : QObject(parent), d(new SearchEnginePrivate) {}

SearchEngine::~SearchEngine()
{
  delete d;
}

auto SearchEngine::isEnabled() const -> bool
{
  return d->isEnabled;
}

auto SearchEngine::setEnabled(bool enabled) -> void
{
  if (enabled == d->isEnabled)
    return;
  d->isEnabled = enabled;
  emit enabledChanged(d->isEnabled);
}

BaseFileFind::BaseFileFind() : d(new BaseFileFindPrivate)
{
  addSearchEngine(&d->m_internalSearchEngine);
}

BaseFileFind::~BaseFileFind()
{
  delete d;
}

auto BaseFileFind::isEnabled() const -> bool
{
  return true;
}

auto BaseFileFind::fileNameFilters() const -> QStringList
{
  if (d->m_filterCombo)
    return splitFilterUiText(d->m_filterCombo->currentText());
  return {};
}

auto BaseFileFind::fileExclusionFilters() const -> QStringList
{
  if (d->m_exclusionCombo)
    return splitFilterUiText(d->m_exclusionCombo->currentText());
  return {};
}

auto BaseFileFind::currentSearchEngine() const -> SearchEngine*
{
  if (d->m_searchEngines.isEmpty() || d->m_currentSearchEngineIndex == -1)
    return nullptr;
  return d->m_searchEngines[d->m_currentSearchEngineIndex];
}

auto BaseFileFind::searchEngines() const -> QVector<SearchEngine*>
{
  return d->m_searchEngines;
}

auto BaseFileFind::setCurrentSearchEngine(int index) -> void
{
  if (d->m_currentSearchEngineIndex == index)
    return;
  d->m_currentSearchEngineIndex = index;
  emit currentSearchEngineChanged();
}

static auto displayText(const QString &line) -> QString
{
  auto result = line;
  const auto end = result.end();
  for (auto it = result.begin(); it != end; ++it) {
    if (!it->isSpace() && !it->isPrint())
      *it = QChar('?');
  }
  return result;
}

static auto displayResult(QFutureWatcher<FileSearchResultList> *watcher, SearchResult *search, int index) -> void
{
  const auto results = watcher->resultAt(index);
  QList<SearchResultItem> items;
  for (const auto &result : results) {
    SearchResultItem item;
    item.setFilePath(FilePath::fromString(result.fileName));
    item.setMainRange(result.lineNumber, result.matchStart, result.matchLength);
    item.setLineText(displayText(result.matchingLine));
    item.setUseTextEditorFont(true);
    item.setUserData(result.regexpCapturedTexts);
    items << item;
  }
  search->addResults(items, SearchResult::AddOrdered);
}

auto BaseFileFind::runNewSearch(const QString &txt, FindFlags findFlags, SearchResultWindow::SearchMode searchMode) -> void
{
  d->m_currentFindSupport = nullptr;
  if (d->m_filterCombo)
    updateComboEntries(d->m_filterCombo, true);
  if (d->m_exclusionCombo)
    updateComboEntries(d->m_exclusionCombo, true);
  const auto tooltip = toolTip();

  SearchResult *search = SearchResultWindow::instance()->startNewSearch(label(), tooltip.arg(descriptionForFindFlags(findFlags)), txt, searchMode, SearchResultWindow::PreserveCaseEnabled, QString::fromLatin1("TextEditor"));
  search->setTextToReplace(txt);
  search->setSearchAgainSupported(true);
  FileFindParameters parameters;
  parameters.text = txt;
  parameters.flags = findFlags;
  parameters.nameFilters = fileNameFilters();
  parameters.exclusionFilters = fileExclusionFilters();
  parameters.additionalParameters = additionalParameters();
  parameters.searchEngineParameters = currentSearchEngine()->parameters();
  parameters.searchEngineIndex = d->m_currentSearchEngineIndex;
  search->setUserData(QVariant::fromValue(parameters));
  connect(search, &SearchResult::activated, this, [this, search](const SearchResultItem &item) {
    openEditor(search, item);
  });
  if (searchMode == SearchResultWindow::SearchAndReplace)
    connect(search, &SearchResult::replaceButtonClicked, this, &BaseFileFind::doReplace);
  connect(search, &SearchResult::visibilityChanged, this, &BaseFileFind::hideHighlightAll);
  connect(search, &SearchResult::searchAgainRequested, this, [this, search] {
    searchAgain(search);
  });
  connect(this, &BaseFileFind::enabledChanged, search, &SearchResult::requestEnabledCheck);
  connect(search, &SearchResult::requestEnabledCheck, this, [this, search] {
    recheckEnabled(search);
  });

  runSearch(search);
}

auto BaseFileFind::runSearch(SearchResult *search) -> void
{
  const FileFindParameters parameters = search->userData().value<FileFindParameters>();
  SearchResultWindow::instance()->popup(IOutputPane::Flags(IOutputPane::ModeSwitch | IOutputPane::WithFocus));
  auto watcher = new QFutureWatcher<FileSearchResultList>();
  watcher->setPendingResultsLimit(1);
  // search is deleted if it is removed from search panel
  connect(search, &QObject::destroyed, watcher, &QFutureWatcherBase::cancel);
  connect(search, &SearchResult::cancelled, watcher, &QFutureWatcherBase::cancel);
  connect(search, &SearchResult::paused, watcher, [watcher](bool paused) {
    if (!paused || watcher->isRunning()) // guard against pausing when the search is finished
      watcher->setPaused(paused);
  });
  connect(watcher, &QFutureWatcherBase::resultReadyAt, search, [watcher, search](int index) {
    displayResult(watcher, search, index);
  });
  // auto-delete:
  connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  connect(watcher, &QFutureWatcherBase::finished, search, [watcher, search]() {
    search->finishSearch(watcher->isCanceled());
  });
  auto future = executeSearch(parameters);
  watcher->setFuture(future);
  d->m_futureSynchronizer.addFuture(future);
  FutureProgress *progress = ProgressManager::addTask(future, tr("Searching"), Constants::TASK_SEARCH);
  connect(search, &SearchResult::countChanged, progress, [progress](int c) {
    progress->setSubtitle(tr("%n found.", nullptr, c));
  });
  progress->setSubtitleVisibleInStatusBar(true);
  connect(progress, &FutureProgress::clicked, search, &SearchResult::popup);
}

auto BaseFileFind::findAll(const QString &txt, FindFlags findFlags) -> void
{
  runNewSearch(txt, findFlags, SearchResultWindow::SearchOnly);
}

auto BaseFileFind::replaceAll(const QString &txt, FindFlags findFlags) -> void
{
  runNewSearch(txt, findFlags, SearchResultWindow::SearchAndReplace);
}

auto BaseFileFind::addSearchEngine(SearchEngine *searchEngine) -> void
{
  d->m_searchEngines.push_back(searchEngine);
  if (d->m_searchEngines.size() == 1) // empty before, make sure we have a current engine
    setCurrentSearchEngine(0);
}

auto BaseFileFind::doReplace(const QString &text, const QList<SearchResultItem> &items, bool preserveCase) -> void
{
  const FilePaths files = replaceAll(text, items, preserveCase);
  if (!files.isEmpty()) {
    showText(ICore::dialogParent(), tr("%n occurrences replaced.", nullptr, items.size()), FadingIndicator::SmallText);
    DocumentManager::notifyFilesChangedInternally(files);
    SearchResultWindow::instance()->hide();
  }
}

static auto createCombo(QAbstractItemModel *model) -> QComboBox*
{
  const auto combo = new QComboBox;
  combo->setEditable(true);
  combo->setModel(model);
  combo->setMaxCount(10);
  combo->setMinimumContentsLength(10);
  combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  combo->setInsertPolicy(QComboBox::InsertAtBottom);
  combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return combo;
}

static auto createLabel(const QString &text) -> QLabel*
{
  const auto filePatternLabel = new QLabel(text);
  filePatternLabel->setMinimumWidth(80);
  filePatternLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  filePatternLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  return filePatternLabel;
}

auto BaseFileFind::createPatternWidgets() -> QList<QPair<QWidget*, QWidget*>>
{
  auto filterLabel = createLabel(msgFilePatternLabel());
  d->m_filterCombo = createCombo(&d->m_filterStrings);
  d->m_filterCombo->setToolTip(msgFilePatternToolTip());
  filterLabel->setBuddy(d->m_filterCombo);
  syncComboWithSettings(d->m_filterCombo, d->m_filterSetting);
  auto exclusionLabel = createLabel(msgExclusionPatternLabel());
  d->m_exclusionCombo = createCombo(&d->m_exclusionStrings);
  d->m_exclusionCombo->setToolTip(msgFilePatternToolTip());
  exclusionLabel->setBuddy(d->m_exclusionCombo);
  syncComboWithSettings(d->m_exclusionCombo, d->m_exclusionSetting);
  return {qMakePair(filterLabel, d->m_filterCombo), qMakePair(exclusionLabel, d->m_exclusionCombo)};
}

auto BaseFileFind::writeCommonSettings(QSettings *settings) -> void
{
  const auto fromNativeSeparators = [](const QStringList &files) -> QStringList {
    return transform(files, &QDir::fromNativeSeparators);
  };

  settings->setValue("filters", fromNativeSeparators(d->m_filterStrings.stringList()));
  if (d->m_filterCombo)
    settings->setValue("currentFilter", QDir::fromNativeSeparators(d->m_filterCombo->currentText()));
  settings->setValue("exclusionFilters", fromNativeSeparators(d->m_exclusionStrings.stringList()));
  if (d->m_exclusionCombo)
    settings->setValue("currentExclusionFilter", QDir::fromNativeSeparators(d->m_exclusionCombo->currentText()));

  for (const SearchEngine *searchEngine : qAsConst(d->m_searchEngines))
    searchEngine->writeSettings(settings);
  settings->setValue("currentSearchEngineIndex", d->m_currentSearchEngineIndex);
}

auto BaseFileFind::readCommonSettings(QSettings *settings, const QString &defaultFilter, const QString &defaultExclusionFilter) -> void
{
  const auto toNativeSeparators = [](const QStringList &files) -> QStringList {
    return transform(files, &QDir::toNativeSeparators);
  };

  const auto filterSetting = settings->value("filters").toStringList();
  const auto filters = filterSetting.isEmpty() ? QStringList(defaultFilter) : filterSetting;
  const auto currentFilter = settings->value("currentFilter");
  d->m_filterSetting = currentFilter.isValid() ? currentFilter.toString() : filters.first();
  d->m_filterStrings.setStringList(toNativeSeparators(filters));
  if (d->m_filterCombo)
    syncComboWithSettings(d->m_filterCombo, d->m_filterSetting);

  auto exclusionFilters = settings->value("exclusionFilters").toStringList();
  if (!exclusionFilters.contains(defaultExclusionFilter))
    exclusionFilters << defaultExclusionFilter;
  const auto currentExclusionFilter = settings->value("currentExclusionFilter");
  d->m_exclusionSetting = currentExclusionFilter.isValid() ? currentExclusionFilter.toString() : exclusionFilters.first();
  d->m_exclusionStrings.setStringList(toNativeSeparators(exclusionFilters));
  if (d->m_exclusionCombo)
    syncComboWithSettings(d->m_exclusionCombo, d->m_exclusionSetting);

  for (auto searchEngine : qAsConst(d->m_searchEngines))
    searchEngine->readSettings(settings);
  const auto currentSearchEngineIndex = settings->value("currentSearchEngineIndex", 0).toInt();
  syncSearchEngineCombo(currentSearchEngineIndex);
}

auto BaseFileFind::openEditor(SearchResult *result, const SearchResultItem &item) -> void
{
  const FileFindParameters parameters = result->userData().value<FileFindParameters>();
  auto openedEditor = d->m_searchEngines[parameters.searchEngineIndex]->openEditor(item, parameters);
  if (!openedEditor)
    EditorManager::openEditorAtSearchResult(item, Id(), EditorManager::DoNotSwitchToDesignMode);
  if (d->m_currentFindSupport)
    d->m_currentFindSupport->clearHighlights();
  d->m_currentFindSupport = nullptr;
  if (!openedEditor)
    return;
  // highlight results
  if (auto findSupport = Aggregation::query<IFindSupport>(openedEditor->widget())) {
    d->m_currentFindSupport = findSupport;
    d->m_currentFindSupport->highlightAll(parameters.text, parameters.flags);
  }
}

auto BaseFileFind::hideHighlightAll(bool visible) -> void
{
  if (!visible && d->m_currentFindSupport)
    d->m_currentFindSupport->clearHighlights();
}

auto BaseFileFind::searchAgain(SearchResult *search) -> void
{
  search->restart();
  runSearch(search);
}

auto BaseFileFind::recheckEnabled(SearchResult *search) -> void
{
  if (!search)
    return;
  search->setSearchAgainEnabled(isEnabled());
}

auto BaseFileFind::replaceAll(const QString &text, const QList<SearchResultItem> &items, bool preserveCase) -> FilePaths
{
  if (items.isEmpty())
    return {};

  const RefactoringChanges refactoring;

  QHash<FilePath, QList<SearchResultItem>> changes;
  for (const auto &item : items)
    changes[FilePath::fromUserInput(item.path().first())].append(item);

  // Checking for files without write permissions
  QSet<FilePath> roFiles;
  for (auto it = changes.cbegin(), end = changes.cend(); it != end; ++it) {
    if (!it.key().isWritableFile())
      roFiles.insert(it.key());
  }

  // Query the user for permissions
  if (!roFiles.isEmpty()) {
    ReadOnlyFilesDialog roDialog(toList(roFiles), ICore::dialogParent());
    roDialog.setShowFailWarning(true, tr("Aborting replace."));
    if (roDialog.exec() == ReadOnlyFilesDialog::RO_Cancel)
      return {};
  }

  for (auto it = changes.cbegin(), end = changes.cend(); it != end; ++it) {
    const auto filePath = it.key();
    const auto changeItems = it.value();

    ChangeSet changeSet;
    const auto file = refactoring.file(filePath);
    QSet<QPair<int, int>> processed;
    for (const auto &item : changeItems) {
      const QPair<int, int> &p = qMakePair(item.mainRange().begin.line, item.mainRange().begin.column);
      if (processed.contains(p))
        continue;
      processed.insert(p);

      QString replacement;
      if (item.userData().canConvert<QStringList>() && !item.userData().toStringList().isEmpty()) {
        replacement = expandRegExpReplacement(text, item.userData().toStringList());
      } else if (preserveCase) {
        const QString originalText = item.mainRange().length(item.lineText()) == 0 ? item.lineText() : item.mainRange().mid(item.lineText());
        replacement = matchCaseReplacement(originalText, text);
      } else {
        replacement = text;
      }

      const auto start = file->position(item.mainRange().begin.line, item.mainRange().begin.column + 1);
      const auto end = file->position(item.mainRange().end.line, item.mainRange().end.column + 1);
      changeSet.replace(start, end, replacement);
    }
    file->setChangeSet(changeSet);
    file->apply();
  }

  return changes.keys();
}

auto BaseFileFind::getAdditionalParameters(SearchResult *search) -> QVariant
{
  return search->userData().value<FileFindParameters>().additionalParameters;
}

auto BaseFileFind::executeSearch(const FileFindParameters &parameters) -> QFuture<FileSearchResultList>
{
  return d->m_searchEngines[parameters.searchEngineIndex]->executeSearch(parameters, this);
}

} // namespace TextEditor
