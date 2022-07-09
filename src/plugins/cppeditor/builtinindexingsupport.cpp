// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "builtinindexingsupport.hpp"

#include "builtineditordocumentparser.hpp"
#include "cppchecksymbols.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditorplugin.hpp"
#include "cppmodelmanager.hpp"
#include "cppprojectfile.hpp"
#include "cppsourceprocessor.hpp"
#include "cpptoolsreuse.hpp"
#include "searchsymbols.hpp"

#include <core/icore.hpp>
#include <core/find/searchresultwindow.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <cplusplus/LookupContext.h>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/stringutils.hpp>
#include <utils/temporarydirectory.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>

namespace CppEditor::Internal {

static const bool FindErrorsIndexing = qgetenv("QTC_FIND_ERRORS_INDEXING") == "1";
static Q_LOGGING_CATEGORY(indexerLog, "qtc.cppeditor.indexer", QtWarningMsg)

namespace {

class ParseParams {
public:
  ProjectExplorer::HeaderPaths headerPaths;
  WorkingCopy workingCopy;
  QSet<QString> sourceFiles;
  int indexerFileSizeLimitInMb = -1;
};

class WriteTaskFileForDiagnostics {
  Q_DISABLE_COPY(WriteTaskFileForDiagnostics)

public:
  WriteTaskFileForDiagnostics()
  {
    const QString fileName = Utils::TemporaryDirectory::masterDirectoryPath() + "/qtc_findErrorsIndexing.diagnostics." + QDateTime::currentDateTime().toString("yyMMdd_HHmm") + ".tasks";

    m_file.setFileName(fileName);
    Q_ASSERT(m_file.open(QIODevice::WriteOnly | QIODevice::Text));
    m_out.setDevice(&m_file);

    qDebug("FindErrorsIndexing: Task file for diagnostics is \"%s\".", qPrintable(m_file.fileName()));
  }

  ~WriteTaskFileForDiagnostics()
  {
    qDebug("FindErrorsIndexing: %d diagnostic messages written to \"%s\".", m_processedDiagnostics, qPrintable(m_file.fileName()));
  }

  auto process(const CPlusPlus::Document::Ptr document) -> void
  {
    using namespace CPlusPlus;
    const QString fileName = document->fileName();

    foreach(const Document::DiagnosticMessage &message, document->diagnosticMessages()) {
      ++m_processedDiagnostics;

      QString type;
      switch (message.level()) {
      case Document::DiagnosticMessage::Warning:
        type = QLatin1String("warn");
        break;
      case Document::DiagnosticMessage::Error:
      case Document::DiagnosticMessage::Fatal:
        type = QLatin1String("err");
        break;
      default:
        break;
      }

      // format: file\tline\ttype\tdescription
      m_out << fileName << "\t" << message.line() << "\t" << type << "\t" << message.text() << "\n";
    }
  }

private:
  QFile m_file;
  QTextStream m_out;
  int m_processedDiagnostics = 0;
};

auto classifyFiles(const QSet<QString> &files, QStringList *headers, QStringList *sources) -> void
{
  foreach(const QString &file, files) {
    if (ProjectFile::isSource(ProjectFile::classify(file)))
      sources->append(file);
    else
      headers->append(file);
  }
}

auto indexFindErrors(QFutureInterface<void> &indexingFuture, const ParseParams params) -> void
{
  QStringList sources, headers;
  classifyFiles(params.sourceFiles, &headers, &sources);
  sources.sort();
  headers.sort();
  auto files = sources + headers;

  WriteTaskFileForDiagnostics taskFileWriter;
  QElapsedTimer timer;
  timer.start();

  for (int i = 0, end = files.size(); i < end; ++i) {
    if (indexingFuture.isCanceled())
      break;

    const auto file = files.at(i);
    qDebug("FindErrorsIndexing: \"%s\"", qPrintable(file));

    // Parse the file as precisely as possible
    BuiltinEditorDocumentParser parser(file);
    parser.setReleaseSourceAndAST(false);
    parser.update({CppModelManager::instance()->workingCopy(), nullptr, Utils::Language::Cxx, false});
    CPlusPlus::Document::Ptr document = parser.document();
    QTC_ASSERT(document, return);

    // Write diagnostic messages
    taskFileWriter.process(document);

    // Look up symbols
    CPlusPlus::LookupContext context(document, parser.snapshot());
    CheckSymbols::go(document, context, QList<CheckSymbols::Result>()).waitForFinished();

    document->releaseSourceAndAST();

    indexingFuture.setProgressValue(i + 1);
  }

  const auto elapsedTime = Utils::formatElapsedTime(timer.elapsed());
  qDebug("FindErrorsIndexing: %s", qPrintable(elapsedTime));
}

auto index(QFutureInterface<void> &indexingFuture, const ParseParams params) -> void
{
  QScopedPointer<CppSourceProcessor> sourceProcessor(CppModelManager::createSourceProcessor());
  sourceProcessor->setFileSizeLimitInMb(params.indexerFileSizeLimitInMb);
  sourceProcessor->setHeaderPaths(params.headerPaths);
  sourceProcessor->setWorkingCopy(params.workingCopy);

  QStringList sources;
  QStringList headers;
  classifyFiles(params.sourceFiles, &headers, &sources);

  foreach(const QString &file, params.sourceFiles)
    sourceProcessor->removeFromCache(file);

  const int sourceCount = sources.size();
  auto files = sources + headers;

  sourceProcessor->setTodo(Utils::toSet(files));

  const auto conf = CppModelManager::configurationFileName();
  auto processingHeaders = false;

  auto cmm = CppModelManager::instance();
  const auto fallbackHeaderPaths = cmm->headerPaths();
  const CPlusPlus::LanguageFeatures defaultFeatures = CPlusPlus::LanguageFeatures::defaultFeatures();

  qCDebug(indexerLog) << "About to index" << files.size() << "files.";
  for (auto i = 0; i < files.size(); ++i) {
    if (indexingFuture.isCanceled())
      break;

    const auto fileName = files.at(i);
    const auto parts = cmm->projectPart(fileName);
    const CPlusPlus::LanguageFeatures languageFeatures = parts.isEmpty() ? defaultFeatures : parts.first()->languageFeatures;
    sourceProcessor->setLanguageFeatures(languageFeatures);

    const auto isSourceFile = i < sourceCount;
    if (isSourceFile) {
      (void)sourceProcessor->run(conf);
    } else if (!processingHeaders) {
      (void)sourceProcessor->run(conf);

      processingHeaders = true;
    }

    qCDebug(indexerLog) << "  Indexing" << i + 1 << "of" << files.size() << ":" << fileName;
    auto headerPaths = parts.isEmpty() ? fallbackHeaderPaths : parts.first()->headerPaths;
    sourceProcessor->setHeaderPaths(headerPaths);
    sourceProcessor->run(fileName);

    indexingFuture.setProgressValue(files.size() - sourceProcessor->todo().size());

    if (isSourceFile)
      sourceProcessor->resetEnvironment();
  }
  qCDebug(indexerLog) << "Indexing finished.";
}

auto parse(QFutureInterface<void> &indexingFuture, const ParseParams params) -> void
{
  const auto &files = params.sourceFiles;
  if (files.isEmpty())
    return;

  indexingFuture.setProgressRange(0, files.size());

  if (FindErrorsIndexing)
    indexFindErrors(indexingFuture, params);
  else
    index(indexingFuture, params);

  indexingFuture.setProgressValue(files.size());
  CppModelManager::instance()->finishedRefreshingSourceFiles(files);
}

class BuiltinSymbolSearcher : public SymbolSearcher {
public:
  BuiltinSymbolSearcher(const CPlusPlus::Snapshot &snapshot, const Parameters &parameters, const QSet<QString> &fileNames) : m_snapshot(snapshot), m_parameters(parameters), m_fileNames(fileNames) {}

  ~BuiltinSymbolSearcher() override = default;

  auto runSearch(QFutureInterface<Core::SearchResultItem> &future) -> void override
  {
    future.setProgressRange(0, m_snapshot.size());
    future.setProgressValue(0);
    auto progress = 0;

    SearchSymbols search;
    search.setSymbolsToSearchFor(m_parameters.types);
    CPlusPlus::Snapshot::const_iterator it = m_snapshot.begin();

    auto findString = (m_parameters.flags & Core::FindRegularExpression ? m_parameters.text : QRegularExpression::escape(m_parameters.text));
    if (m_parameters.flags & Core::FindWholeWords)
      findString = QString::fromLatin1("\\b%1\\b").arg(findString);
    QRegularExpression matcher(findString, (m_parameters.flags & Core::FindCaseSensitively ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption));
    matcher.optimize();
    while (it != m_snapshot.end()) {
      if (future.isPaused())
        future.waitForResume();
      if (future.isCanceled())
        break;
      if (m_fileNames.isEmpty() || m_fileNames.contains(it.value()->fileName())) {
        QVector<Core::SearchResultItem> resultItems;
        auto filter = [&](const IndexItem::Ptr &info) -> IndexItem::VisitorResult {
          if (matcher.match(info->symbolName()).hasMatch()) {
            auto text = info->symbolName();
            auto scope = info->symbolScope();
            if (info->type() == IndexItem::Function) {
              QString name;
              info->unqualifiedNameAndScope(info->symbolName(), &name, &scope);
              text = name + info->symbolType();
            } else if (info->type() == IndexItem::Declaration) {
              text = info->representDeclaration();
            }

            Core::SearchResultItem item;
            item.setPath(scope.split(QLatin1String("::"), Qt::SkipEmptyParts));
            item.setLineText(text);
            item.setIcon(info->icon());
            item.setUserData(QVariant::fromValue(info));
            resultItems << item;
          }

          return IndexItem::Recurse;
        };
        search(it.value())->visitAllChildren(filter);
        if (!resultItems.isEmpty())
          future.reportResults(resultItems);
      }
      ++it;
      ++progress;
      future.setProgressValue(progress);
    }
    if (future.isPaused())
      future.waitForResume();
  }

private:
  const CPlusPlus::Snapshot m_snapshot;
  const Parameters m_parameters;
  const QSet<QString> m_fileNames;
};

} // anonymous namespace

BuiltinIndexingSupport::BuiltinIndexingSupport()
{
  m_synchronizer.setCancelOnWait(true);
}

BuiltinIndexingSupport::~BuiltinIndexingSupport() = default;

auto BuiltinIndexingSupport::refreshSourceFiles(const QSet<QString> &sourceFiles, CppModelManager::ProgressNotificationMode mode) -> QFuture<void>
{
  auto mgr = CppModelManager::instance();

  ParseParams params;
  params.indexerFileSizeLimitInMb = indexerFileSizeLimitInMb();
  params.headerPaths = mgr->headerPaths();
  params.workingCopy = mgr->workingCopy();
  params.sourceFiles = sourceFiles;

  auto result = Utils::runAsync(mgr->sharedThreadPool(), parse, params);
  m_synchronizer.addFuture(result);

  if (mode == CppModelManager::ForcedProgressNotification || sourceFiles.count() > 1) {
    Core::ProgressManager::addTask(result, QCoreApplication::translate("CppEditor::Internal::BuiltinIndexingSupport", "Parsing C/C++ Files"), CppEditor::Constants::TASK_INDEX);
  }

  return result;
}

auto BuiltinIndexingSupport::createSymbolSearcher(const SymbolSearcher::Parameters &parameters, const QSet<QString> &fileNames) -> SymbolSearcher*
{
  return new BuiltinSymbolSearcher(CppModelManager::instance()->snapshot(), parameters, fileNames);
}

auto BuiltinIndexingSupport::isFindErrorsIndexingActive() -> bool
{
  return FindErrorsIndexing;
}

} // namespace CppEditor::Internal
