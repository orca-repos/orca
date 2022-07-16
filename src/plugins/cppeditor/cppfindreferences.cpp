// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppfindreferences.hpp"

#include "cppcodemodelsettings.hpp"
#include "cppeditorconstants.hpp"
#include "cppfilesettingspage.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"
#include "cppworkingcopy.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-interface.hpp>
#include <core/core-future-progress.hpp>
#include <core/core-progress-manager.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/session.hpp>
#include <texteditor/basefilefind.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/textfileformat.hpp>

#include <cplusplus/Overview.h>
#include <QtConcurrentMap>
#include <QCheckBox>
#include <QDir>
#include <QFutureWatcher>
#include <QVBoxLayout>

#include <functional>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CppEditor {

namespace {
static auto isAllLowerCase(const QString &text) -> bool { return text.toLower() == text; }
}

auto colorStyleForUsageType(CPlusPlus::Usage::Type type) -> SearchResultColor::Style
{
  switch (type) {
  case CPlusPlus::Usage::Type::Read:
    return SearchResultColor::Style::Alt1;
  case CPlusPlus::Usage::Type::Initialization:
  case CPlusPlus::Usage::Type::Write:
  case CPlusPlus::Usage::Type::WritableRef:
    return SearchResultColor::Style::Alt2;
  case CPlusPlus::Usage::Type::Declaration:
  case CPlusPlus::Usage::Type::Other:
    return SearchResultColor::Style::Default;
  }
  return SearchResultColor::Style::Default; // For dumb compilers.
}

auto renameFilesForSymbol(const QString &oldSymbolName, const QString &newSymbolName, const QVector<Node*> &files) -> void
{
  Internal::CppFileSettings settings;
  settings.fromSettings(Orca::Plugin::Core::ICore::settings());

  const auto newPaths = Utils::transform<QList>(files, [&oldSymbolName, newSymbolName, &settings](const Node *node) -> QString {
    const auto fi = node->filePath().toFileInfo();
    const auto oldBaseName = fi.baseName();
    auto newBaseName = newSymbolName;

    // 1) new symbol lowercase: new base name lowercase
    if (isAllLowerCase(newSymbolName)) {
      newBaseName = newSymbolName;

      // 2) old base name mixed case: new base name is verbatim symbol name
    } else if (!isAllLowerCase(oldBaseName)) {
      newBaseName = newSymbolName;

      // 3) old base name lowercase, old symbol mixed case: new base name lowercase
    } else if (!isAllLowerCase(oldSymbolName)) {
      newBaseName = newSymbolName.toLower();

      // 4) old base name lowercase, old symbol lowercase, new symbol mixed case:
      //    use the preferences setting for new base name case
    } else if (settings.lowerCaseFiles) {
      newBaseName = newSymbolName.toLower();
    }

    if (newBaseName == oldBaseName)
      return QString();

    return fi.absolutePath() + "/" + newBaseName + '.' + fi.completeSuffix();
  });

  for (auto i = 0; i < files.size(); ++i) {
    if (!newPaths.at(i).isEmpty()) {
      auto node = files.at(i);
      ProjectExplorerPlugin::renameFile(node, newPaths.at(i));
    }
  }
}

auto CppSearchResultFilter::createWidget() -> QWidget*
{
  const auto widget = new QWidget;
  const auto layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  const auto readsCheckBox = new QCheckBox(Internal::CppFindReferences::tr("Reads"));
  readsCheckBox->setChecked(m_showReads);
  const auto writesCheckBox = new QCheckBox(Internal::CppFindReferences::tr("Writes"));
  writesCheckBox->setChecked(m_showWrites);
  const auto declsCheckBox = new QCheckBox(Internal::CppFindReferences::tr("Declarations"));
  declsCheckBox->setChecked(m_showDecls);
  const auto otherCheckBox = new QCheckBox(Internal::CppFindReferences::tr("Other"));
  otherCheckBox->setChecked(m_showOther);
  layout->addWidget(readsCheckBox);
  layout->addWidget(writesCheckBox);
  layout->addWidget(declsCheckBox);
  layout->addWidget(otherCheckBox);
  connect(readsCheckBox, &QCheckBox::toggled, this, [this](bool checked) { setValue(m_showReads, checked); });
  connect(writesCheckBox, &QCheckBox::toggled, this, [this](bool checked) { setValue(m_showWrites, checked); });
  connect(declsCheckBox, &QCheckBox::toggled, this, [this](bool checked) { setValue(m_showDecls, checked); });
  connect(otherCheckBox, &QCheckBox::toggled, this, [this](bool checked) { setValue(m_showOther, checked); });
  return widget;
}

auto CppSearchResultFilter::matches(const SearchResultItem &item) const -> bool
{
  switch (static_cast<CPlusPlus::Usage::Type>(item.userData().toInt())) {
  case CPlusPlus::Usage::Type::Read:
    return m_showReads;
  case CPlusPlus::Usage::Type::Write:
  case CPlusPlus::Usage::Type::WritableRef:
  case CPlusPlus::Usage::Type::Initialization:
    return m_showWrites;
  case CPlusPlus::Usage::Type::Declaration:
    return m_showDecls;
  case CPlusPlus::Usage::Type::Other:
    return m_showOther;
  }
  return false;
}

auto CppSearchResultFilter::setValue(bool &member, bool value) -> void
{
  member = value;
  emit filterChanged();
}

namespace Internal {

static auto getSource(const Utils::FilePath &fileName, const WorkingCopy &workingCopy) -> QByteArray
{
  if (workingCopy.contains(fileName)) {
    return workingCopy.source(fileName);
  } else {
    QString fileContents;
    Utils::TextFileFormat format;
    QString error;
    auto defaultCodec = EditorManager::defaultTextCodec();
    auto result = Utils::TextFileFormat::readFile(fileName, defaultCodec, &fileContents, &format, &error);
    if (result != Utils::TextFileFormat::ReadSuccess)
      qWarning() << "Could not read " << fileName << ". Error: " << error;

    return fileContents.toUtf8();
  }
}

static auto typeId(CPlusPlus::Symbol *symbol) -> QByteArray
{
  if (symbol->asEnum()) {
    return QByteArray("e");
  } else if (symbol->asFunction()) {
    return QByteArray("f");
  } else if (symbol->asNamespace()) {
    return QByteArray("n");
  } else if (symbol->asTemplate()) {
    return QByteArray("t");
  } else if (symbol->asNamespaceAlias()) {
    return QByteArray("na");
  } else if (symbol->asClass()) {
    return QByteArray("c");
  } else if (symbol->asBlock()) {
    return QByteArray("b");
  } else if (symbol->asUsingNamespaceDirective()) {
    return QByteArray("u");
  } else if (symbol->asUsingDeclaration()) {
    return QByteArray("ud");
  } else if (symbol->asDeclaration()) {
    QByteArray temp("d,");
    CPlusPlus::Overview pretty;
    temp.append(pretty.prettyType(symbol->type()).toUtf8());
    return temp;
  } else if (symbol->asArgument()) {
    return QByteArray("a");
  } else if (symbol->asTypenameArgument()) {
    return QByteArray("ta");
  } else if (symbol->asBaseClass()) {
    return QByteArray("bc");
  } else if (symbol->asForwardClassDeclaration()) {
    return QByteArray("fcd");
  } else if (symbol->asQtPropertyDeclaration()) {
    return QByteArray("qpd");
  } else if (symbol->asQtEnum()) {
    return QByteArray("qe");
  } else if (symbol->asObjCBaseClass()) {
    return QByteArray("ocbc");
  } else if (symbol->asObjCBaseProtocol()) {
    return QByteArray("ocbp");
  } else if (symbol->asObjCClass()) {
    return QByteArray("occ");
  } else if (symbol->asObjCForwardClassDeclaration()) {
    return QByteArray("ocfd");
  } else if (symbol->asObjCProtocol()) {
    return QByteArray("ocp");
  } else if (symbol->asObjCForwardProtocolDeclaration()) {
    return QByteArray("ocfpd");
  } else if (symbol->asObjCMethod()) {
    return QByteArray("ocm");
  } else if (symbol->asObjCPropertyDeclaration()) {
    return QByteArray("ocpd");
  }
  return QByteArray("unknown");
}

static auto idForSymbol(CPlusPlus::Symbol *symbol) -> QByteArray
{
  auto uid(typeId(symbol));
  if (const CPlusPlus::Identifier *id = symbol->identifier()) {
    uid.append("|");
    uid.append(QByteArray(id->chars(), id->size()));
  } else if (CPlusPlus::Scope *scope = symbol->enclosingScope()) {
    // add the index of this symbol within its enclosing scope
    // (counting symbols without identifier of the same type)
    auto count = 0;
    CPlusPlus::Scope::iterator it = scope->memberBegin();
    while (it != scope->memberEnd() && *it != symbol) {
      CPlusPlus::Symbol *val = *it;
      ++it;
      if (val->identifier() || typeId(val) != uid)
        continue;
      ++count;
    }
    uid.append(QString::number(count).toLocal8Bit());
  }
  return uid;
}

static auto fullIdForSymbol(CPlusPlus::Symbol *symbol) -> QList<QByteArray>
{
  QList<QByteArray> uid;
  auto current = symbol;
  do {
    uid.prepend(idForSymbol(current));
    current = current->enclosingScope();
  } while (current);
  return uid;
}

namespace {

class ProcessFile {
  const WorkingCopy workingCopy;
  const CPlusPlus::Snapshot snapshot;
  CPlusPlus::Document::Ptr symbolDocument;
  CPlusPlus::Symbol *symbol;
  QFutureInterface<CPlusPlus::Usage> *future;
  const bool categorize;

public:
  // needed by QtConcurrent
  using argument_type = const Utils::FilePath&;
  using result_type = QList<CPlusPlus::Usage>;

  ProcessFile(const WorkingCopy &workingCopy, const CPlusPlus::Snapshot snapshot, CPlusPlus::Document::Ptr symbolDocument, CPlusPlus::Symbol *symbol, QFutureInterface<CPlusPlus::Usage> *future, bool categorize) : workingCopy(workingCopy), snapshot(snapshot), symbolDocument(symbolDocument), symbol(symbol), future(future), categorize(categorize) { }

  auto operator()(const Utils::FilePath &fileName) -> QList<CPlusPlus::Usage>
  {
    QList<CPlusPlus::Usage> usages;
    if (future->isPaused())
      future->waitForResume();
    if (future->isCanceled())
      return usages;
    const CPlusPlus::Identifier *symbolId = symbol->identifier();

    if (CPlusPlus::Document::Ptr previousDoc = snapshot.document(fileName)) {
      CPlusPlus::Control *control = previousDoc->control();
      if (!control->findIdentifier(symbolId->chars(), symbolId->size()))
        return usages; // skip this document, it's not using symbolId.
    }
    CPlusPlus::Document::Ptr doc;
    const auto unpreprocessedSource = getSource(fileName, workingCopy);

    if (symbolDocument && fileName == Utils::FilePath::fromString(symbolDocument->fileName())) {
      doc = symbolDocument;
    } else {
      doc = snapshot.preprocessedDocument(unpreprocessedSource, fileName);
      doc->tokenize();
    }

    CPlusPlus::Control *control = doc->control();
    if (control->findIdentifier(symbolId->chars(), symbolId->size()) != nullptr) {
      if (doc != symbolDocument)
        doc->check();

      CPlusPlus::FindUsages process(unpreprocessedSource, doc, snapshot, categorize);
      process(symbol);

      usages = process.usages();
    }

    if (future->isPaused())
      future->waitForResume();
    return usages;
  }
};

class UpdateUI {
  QFutureInterface<CPlusPlus::Usage> *future;

public:
  explicit UpdateUI(QFutureInterface<CPlusPlus::Usage> *future): future(future) {}

  auto operator()(QList<CPlusPlus::Usage> &, const QList<CPlusPlus::Usage> &usages) -> void
  {
    foreach(const CPlusPlus::Usage &u, usages)
      future->reportResult(u);

    future->setProgressValue(future->progressValue() + 1);
  }
};

} // end of anonymous namespace

CppFindReferences::CppFindReferences(CppModelManager *modelManager) : QObject(modelManager), m_modelManager(modelManager) {}
CppFindReferences::~CppFindReferences() = default;

auto CppFindReferences::references(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) const -> QList<int>
{
  QList<int> references;

  CPlusPlus::FindUsages findUsages(context);
  findUsages(symbol);
  references = findUsages.references();

  return references;
}

static auto find_helper(QFutureInterface<CPlusPlus::Usage> &future, const WorkingCopy workingCopy, const CPlusPlus::LookupContext &context, CPlusPlus::Symbol *symbol, bool categorize) -> void
{
  const CPlusPlus::Identifier *symbolId = symbol->identifier();
  QTC_ASSERT(symbolId != nullptr, return);

  const CPlusPlus::Snapshot snapshot = context.snapshot();

  const auto sourceFile = Utils::FilePath::fromUtf8(symbol->fileName(), symbol->fileNameLength());
  Utils::FilePaths files{sourceFile};

  if (symbol->isClass() || symbol->isForwardClassDeclaration() || (symbol->enclosingScope() && !symbol->isStatic() && symbol->enclosingScope()->isNamespace())) {
    const CPlusPlus::Snapshot snapshotFromContext = context.snapshot();
    for (auto i = snapshotFromContext.begin(), ei = snapshotFromContext.end(); i != ei; ++i) {
      if (i.key() == sourceFile)
        continue;

      const CPlusPlus::Control *control = i.value()->control();

      if (control->findIdentifier(symbolId->chars(), symbolId->size()))
        files.append(i.key());
    }
  } else {
    files += snapshot.filesDependingOn(sourceFile);
  }
  files = Utils::filteredUnique(files);

  future.setProgressRange(0, files.size());

  ProcessFile process(workingCopy, snapshot, context.thisDocument(), symbol, &future, categorize);
  UpdateUI reduce(&future);
  // This thread waits for blockingMappedReduced to finish, so reduce the pool's used thread count
  // so the blockingMappedReduced can use one more thread, and increase it again afterwards.
  QThreadPool::globalInstance()->releaseThread();
  QtConcurrent::blockingMappedReduced<QList<CPlusPlus::Usage>>(files, process, reduce);
  QThreadPool::globalInstance()->reserveThread();
  future.setProgressValue(files.size());
}

auto CppFindReferences::findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) -> void
{
  findUsages(symbol, context, QString(), false);
}

auto CppFindReferences::findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, const QString &replacement, bool replace) -> void
{
  CPlusPlus::Overview overview;
  SearchResult *search = SearchResultWindow::instance()->startNewSearch(tr("C++ Usages:"), QString(), overview.prettyName(CPlusPlus::LookupContext::fullyQualifiedName(symbol)), replace ? SearchResultWindow::SearchAndReplace : SearchResultWindow::SearchOnly, SearchResultWindow::PreserveCaseDisabled, QLatin1String("CppEditor"));
  search->setTextToReplace(replacement);
  if (codeModelSettings()->categorizeFindReferences())
    search->setFilter(new CppSearchResultFilter);
  auto renameFilesCheckBox = new QCheckBox();
  renameFilesCheckBox->setVisible(false);
  search->setAdditionalReplaceWidget(renameFilesCheckBox);
  connect(search, &SearchResult::replaceButtonClicked, this, &CppFindReferences::onReplaceButtonClicked);
  search->setSearchAgainSupported(true);
  connect(search, &SearchResult::searchAgainRequested, this, &CppFindReferences::searchAgain);
  CppFindReferencesParameters parameters;
  parameters.symbolId = fullIdForSymbol(symbol);
  parameters.symbolFileName = QByteArray(symbol->fileName());
  parameters.categorize = codeModelSettings()->categorizeFindReferences();

  if (symbol->isClass() || symbol->isForwardClassDeclaration()) {
    CPlusPlus::Overview overview;
    parameters.prettySymbolName = overview.prettyName(CPlusPlus::LookupContext::path(symbol).constLast());
  }

  search->setUserData(QVariant::fromValue(parameters));
  findAll_helper(search, symbol, context, codeModelSettings()->categorizeFindReferences());
}

auto CppFindReferences::renameUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, const QString &replacement) -> void
{
  if (const CPlusPlus::Identifier *id = symbol->identifier()) {
    const QString textToReplace = replacement.isEmpty() ? QString::fromUtf8(id->chars(), id->size()) : replacement;
    findUsages(symbol, context, textToReplace, true);
  }
}

auto CppFindReferences::findAll_helper(SearchResult *search, CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, bool categorize) -> void
{
  if (!(symbol && symbol->identifier())) {
    search->finishSearch(false);
    return;
  }
  connect(search, &SearchResult::activated, [](const SearchResultItem &item) {
    Orca::Plugin::Core::EditorManager::openEditorAtSearchResult(item);
  });

  SearchResultWindow::instance()->popup(IOutputPane::ModeSwitch | IOutputPane::WithFocus);
  const auto workingCopy = m_modelManager->workingCopy();
  QFuture<CPlusPlus::Usage> result;
  result = Utils::runAsync(m_modelManager->sharedThreadPool(), find_helper, workingCopy, context, symbol, categorize);
  createWatcher(result, search);

  FutureProgress *progress = ProgressManager::addTask(result, tr("Searching for Usages"), CppEditor::Constants::TASK_SEARCH);

  connect(progress, &FutureProgress::clicked, search, &SearchResult::popup);
}

auto CppFindReferences::onReplaceButtonClicked(const QString &text, const QList<SearchResultItem> &items, bool preserveCase) -> void
{
  const auto filePaths = TextEditor::BaseFileFind::replaceAll(text, items, preserveCase);
  if (!filePaths.isEmpty()) {
    m_modelManager->updateSourceFiles(Utils::transform<QSet>(filePaths, &Utils::FilePath::toString));
    SearchResultWindow::instance()->hide();
  }

  auto search = qobject_cast<SearchResult*>(sender());
  QTC_ASSERT(search, return);

  auto parameters = search->userData().value<CppFindReferencesParameters>();
  if (parameters.filesToRename.isEmpty())
    return;

  auto renameFilesCheckBox = qobject_cast<QCheckBox*>(search->additionalReplaceWidget());
  if (!renameFilesCheckBox || !renameFilesCheckBox->isChecked())
    return;

  renameFilesForSymbol(parameters.prettySymbolName, text, parameters.filesToRename);
}

auto CppFindReferences::searchAgain() -> void
{
  auto search = qobject_cast<SearchResult*>(sender());
  auto parameters = search->userData().value<CppFindReferencesParameters>();
  parameters.filesToRename.clear();
  CPlusPlus::Snapshot snapshot = CppModelManager::instance()->snapshot();
  search->restart();
  CPlusPlus::LookupContext context;
  CPlusPlus::Symbol *symbol = findSymbol(parameters, snapshot, &context);
  if (!symbol) {
    search->finishSearch(false);
    return;
  }
  findAll_helper(search, symbol, context, parameters.categorize);
}

namespace {
class UidSymbolFinder : public CPlusPlus::SymbolVisitor {
public:
  explicit UidSymbolFinder(const QList<QByteArray> &uid) : m_uid(uid) { }
  auto result() const -> CPlusPlus::Symbol* { return m_result; }

  auto preVisit(CPlusPlus::Symbol *symbol) -> bool override
  {
    if (m_result)
      return false;
    auto index = m_index;
    if (symbol->asScope())
      ++m_index;
    if (index >= m_uid.size())
      return false;
    if (idForSymbol(symbol) != m_uid.at(index))
      return false;
    if (index == m_uid.size() - 1) {
      // symbol found
      m_result = symbol;
      return false;
    }
    return true;
  }

  auto postVisit(CPlusPlus::Symbol *symbol) -> void override
  {
    if (symbol->asScope())
      --m_index;
  }

private:
  QList<QByteArray> m_uid;
  int m_index = 0;
  CPlusPlus::Symbol *m_result = nullptr;
};
}

auto CppFindReferences::findSymbol(const CppFindReferencesParameters &parameters, const CPlusPlus::Snapshot &snapshot, CPlusPlus::LookupContext *context) -> CPlusPlus::Symbol*
{
  QTC_ASSERT(context, return nullptr);
  QString symbolFile = QLatin1String(parameters.symbolFileName);
  if (!snapshot.contains(symbolFile))
    return nullptr;

  CPlusPlus::Document::Ptr newSymbolDocument = snapshot.document(symbolFile);
  // document is not parsed and has no bindings yet, do it
  QByteArray source = getSource(Utils::FilePath::fromString(newSymbolDocument->fileName()), m_modelManager->workingCopy());
  CPlusPlus::Document::Ptr doc = snapshot.preprocessedDocument(source, FilePath::fromString(newSymbolDocument->fileName()));
  doc->check();

  // find matching symbol in new document and return the new parameters
  UidSymbolFinder finder(parameters.symbolId);
  finder.accept(doc->globalNamespace());
  if (finder.result()) {
    *context = CPlusPlus::LookupContext(doc, snapshot);
    return finder.result();
  }
  return nullptr;
}

static auto displayResults(SearchResult *search, QFutureWatcher<CPlusPlus::Usage> *watcher, int first, int last) -> void
{
  auto parameters = search->userData().value<CppFindReferencesParameters>();

  for (auto index = first; index != last; ++index) {
    const CPlusPlus::Usage result = watcher->future().resultAt(index);
    SearchResultItem item;
    item.setFilePath(result.path);
    item.setMainRange(result.line, result.col, result.len);
    item.setLineText(result.lineText);
    item.setUserData(int(result.type));
    item.setStyle(colorStyleForUsageType(result.type));
    item.setUseTextEditorFont(true);
    if (search->supportsReplace())
      item.setSelectForReplacement(SessionManager::projectForFile(result.path));
    search->addResult(item);

    if (parameters.prettySymbolName.isEmpty())
      continue;

    if (Utils::contains(parameters.filesToRename, Utils::equal(&Node::filePath, result.path)))
      continue;

    Node *node = ProjectTree::nodeForFile(result.path);
    if (!node) // Not part of any project
      continue;

    const QFileInfo fi = node->filePath().toFileInfo();
    if (fi.baseName().compare(parameters.prettySymbolName, Qt::CaseInsensitive) == 0)
      parameters.filesToRename.append(node);
  }

  search->setUserData(QVariant::fromValue(parameters));
}

static auto searchFinished(SearchResult *search, QFutureWatcher<CPlusPlus::Usage> *watcher) -> void
{
  search->finishSearch(watcher->isCanceled());

  auto parameters = search->userData().value<CppFindReferencesParameters>();
  if (!parameters.filesToRename.isEmpty()) {
    const auto filesToRename = Utils::transform<QList>(parameters.filesToRename, [](const Node *node) {
      return node->filePath().toUserOutput();
    });

    auto renameCheckBox = qobject_cast<QCheckBox*>(search->additionalReplaceWidget());
    if (renameCheckBox) {
      renameCheckBox->setText(CppFindReferences::tr("Re&name %n files", nullptr, filesToRename.size()));
      renameCheckBox->setToolTip(CppFindReferences::tr("Files:\n%1").arg(filesToRename.join('\n')));
      renameCheckBox->setVisible(true);
    }
  }

  watcher->deleteLater();
}

namespace {

class FindMacroUsesInFile {
  const WorkingCopy workingCopy;
  const CPlusPlus::Snapshot snapshot;
  const CPlusPlus::Macro &macro;
  QFutureInterface<CPlusPlus::Usage> *future;

public:
  // needed by QtConcurrent
  using argument_type = const Utils::FilePath&;
  using result_type = QList<CPlusPlus::Usage>;

  FindMacroUsesInFile(const WorkingCopy &workingCopy, const CPlusPlus::Snapshot snapshot, const CPlusPlus::Macro &macro, QFutureInterface<CPlusPlus::Usage> *future) : workingCopy(workingCopy), snapshot(snapshot), macro(macro), future(future) { }

  auto operator()(const Utils::FilePath &fileName) -> QList<CPlusPlus::Usage>
  {
    QList<CPlusPlus::Usage> usages;
    CPlusPlus::Document::Ptr doc = snapshot.document(fileName);
    QByteArray source;

  restart_search: if (future->isPaused())
      future->waitForResume();
    if (future->isCanceled())
      return usages;

    usages.clear();
    foreach(const CPlusPlus::Document::MacroUse &use, doc->macroUses()) {
      const CPlusPlus::Macro &useMacro = use.macro();

      if (useMacro.fileName() == macro.fileName()) {
        // Check if this is a match, but possibly against an outdated document.
        if (source.isEmpty())
          source = getSource(fileName, workingCopy);

        if (macro.fileRevision() > useMacro.fileRevision()) {
          // yes, it is outdated, so re-preprocess and start from scratch for this file.
          doc = snapshot.preprocessedDocument(source, fileName);
          usages.clear();
          goto restart_search;
        }

        if (macro.name() == useMacro.name()) {
          unsigned column;
          const QString &lineSource = matchingLine(use.bytesBegin(), source, &column);
          usages.append(CPlusPlus::Usage(fileName, lineSource, CPlusPlus::Usage::Type::Other, use.beginLine(), column, useMacro.nameToQString().size()));
        }
      }
    }

    if (future->isPaused())
      future->waitForResume();
    return usages;
  }

  static auto matchingLine(unsigned bytesOffsetOfUseStart, const QByteArray &utf8Source, unsigned *columnOfUseStart = nullptr) -> QString
  {
    int lineBegin = utf8Source.lastIndexOf('\n', bytesOffsetOfUseStart) + 1;
    int lineEnd = utf8Source.indexOf('\n', bytesOffsetOfUseStart);
    if (lineEnd == -1)
      lineEnd = utf8Source.length();

    if (columnOfUseStart) {
      *columnOfUseStart = 0;
      auto startOfUse = utf8Source.constData() + bytesOffsetOfUseStart;
      QTC_ASSERT(startOfUse < utf8Source.constData() + lineEnd, return QString());
      auto currentSourceByte = utf8Source.constData() + lineBegin;
      unsigned char yychar = *currentSourceByte;
      while (currentSourceByte != startOfUse)
        CPlusPlus::Lexer::yyinp_utf8(currentSourceByte, yychar, *columnOfUseStart);
    }

    const auto matchingLine = utf8Source.mid(lineBegin, lineEnd - lineBegin);
    return QString::fromUtf8(matchingLine, matchingLine.size());
  }
};

} // end of anonymous namespace

static auto findMacroUses_helper(QFutureInterface<CPlusPlus::Usage> &future, const WorkingCopy workingCopy, const CPlusPlus::Snapshot snapshot, const CPlusPlus::Macro macro) -> void
{
  const auto sourceFile = Utils::FilePath::fromString(macro.fileName());
  Utils::FilePaths files{sourceFile};
  files = Utils::filteredUnique(files + snapshot.filesDependingOn(sourceFile));

  future.setProgressRange(0, files.size());
  FindMacroUsesInFile process(workingCopy, snapshot, macro, &future);
  UpdateUI reduce(&future);
  // This thread waits for blockingMappedReduced to finish, so reduce the pool's used thread count
  // so the blockingMappedReduced can use one more thread, and increase it again afterwards.
  QThreadPool::globalInstance()->releaseThread();
  QtConcurrent::blockingMappedReduced<QList<CPlusPlus::Usage>>(files, process, reduce);
  QThreadPool::globalInstance()->reserveThread();
  future.setProgressValue(files.size());
}

auto CppFindReferences::findMacroUses(const CPlusPlus::Macro &macro) -> void
{
  findMacroUses(macro, QString(), false);
}

auto CppFindReferences::findMacroUses(const CPlusPlus::Macro &macro, const QString &replacement, bool replace) -> void
{
  auto search = SearchResultWindow::instance()->startNewSearch(tr("C++ Macro Usages:"), QString(), macro.nameToQString(), replace ? SearchResultWindow::SearchAndReplace : SearchResultWindow::SearchOnly, SearchResultWindow::PreserveCaseDisabled, QLatin1String("CppEditor"));

  search->setTextToReplace(replacement);
  auto renameFilesCheckBox = new QCheckBox();
  renameFilesCheckBox->setVisible(false);
  search->setAdditionalReplaceWidget(renameFilesCheckBox);
  connect(search, &SearchResult::replaceButtonClicked, this, &CppFindReferences::onReplaceButtonClicked);

  SearchResultWindow::instance()->popup(IOutputPane::ModeSwitch | IOutputPane::WithFocus);

  connect(search, &SearchResult::activated, [](const Orca::Plugin::Core::SearchResultItem &item) {
    Orca::Plugin::Core::EditorManager::openEditorAtSearchResult(item);
  });

  const CPlusPlus::Snapshot snapshot = m_modelManager->snapshot();
  const auto workingCopy = m_modelManager->workingCopy();

  // add the macro definition itself
  {
    const auto &source = getSource(Utils::FilePath::fromString(macro.fileName()), workingCopy);
    unsigned column;
    const auto line = FindMacroUsesInFile::matchingLine(macro.bytesOffset(), source, &column);
    SearchResultItem item;
    const auto filePath = Utils::FilePath::fromString(macro.fileName());
    item.setFilePath(filePath);
    item.setLineText(line);
    item.setMainRange(macro.line(), column, macro.nameToQString().length());
    item.setUseTextEditorFont(true);
    if (search->supportsReplace())
      item.setSelectForReplacement(SessionManager::projectForFile(filePath));
    search->addResult(item);
  }

  QFuture<CPlusPlus::Usage> result;
  result = Utils::runAsync(m_modelManager->sharedThreadPool(), findMacroUses_helper, workingCopy, snapshot, macro);
  createWatcher(result, search);

  FutureProgress *progress = ProgressManager::addTask(result, tr("Searching for Usages"), CppEditor::Constants::TASK_SEARCH);
  connect(progress, &FutureProgress::clicked, search, &SearchResult::popup);
}

auto CppFindReferences::renameMacroUses(const CPlusPlus::Macro &macro, const QString &replacement) -> void
{
  const QString textToReplace = replacement.isEmpty() ? macro.nameToQString() : replacement;
  findMacroUses(macro, textToReplace, true);
}

auto CppFindReferences::createWatcher(const QFuture<CPlusPlus::Usage> &future, SearchResult *search) -> void
{
  auto watcher = new QFutureWatcher<CPlusPlus::Usage>();
  // auto-delete:
  connect(watcher, &QFutureWatcherBase::finished, watcher, [search, watcher]() {
    searchFinished(search, watcher);
  });

  connect(watcher, &QFutureWatcherBase::resultsReadyAt, search, [search, watcher](int first, int last) {
    displayResults(search, watcher, first, last);
  });
  connect(watcher, &QFutureWatcherBase::finished, search, [search, watcher]() {
    search->finishSearch(watcher->isCanceled());
  });
  connect(search, &SearchResult::cancelled, watcher, [watcher]() { watcher->cancel(); });
  connect(search, &SearchResult::paused, watcher, [watcher](bool paused) {
    if (!paused || watcher->isRunning()) // guard against pausing when the search is finished
      watcher->setPaused(paused);
  });
  watcher->setPendingResultsLimit(1);
  watcher->setFuture(future);
}

} // namespace Internal
} // namespace CppEditor
