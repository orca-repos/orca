// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpptoolsreuse.hpp"

#include "cppautocompleter.hpp"
#include "cppcodemodelsettings.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditorplugin.hpp"
#include "cpphighlighter.hpp"
#include "cppqtstyleindenter.hpp"
#include "cpprefactoringchanges.hpp"
#include "projectinfo.hpp"

#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/idocument.hpp>
#include <core/messagemanager.hpp>
#include <projectexplorer/session.hpp>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/textdocument.hpp>

#include <cplusplus/BackwardsScanner.h>
#include <cplusplus/LookupContext.h>
#include <cplusplus/Overview.h>
#include <cplusplus/SimpleLexer.h>
#include <utils/algorithm.hpp>
#include <utils/porting.hpp>
#include <utils/textutils.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include <QTextCursor>
#include <QTextDocument>

using namespace CPlusPlus;

namespace CppEditor {

static auto skipChars(QTextCursor *tc, QTextCursor::MoveOperation op, int offset, std::function<bool(const QChar &)> skip) -> int
{
  const QTextDocument *doc = tc->document();
  if (!doc)
    return 0;
  auto ch = doc->characterAt(tc->position() + offset);
  if (ch.isNull())
    return 0;
  auto count = 0;
  while (skip(ch)) {
    if (tc->movePosition(op))
      ++count;
    else
      break;
    ch = doc->characterAt(tc->position() + offset);
  }
  return count;
}

static auto skipCharsForward(QTextCursor *tc, std::function<bool(const QChar &)> skip) -> int
{
  return skipChars(tc, QTextCursor::NextCharacter, 0, skip);
}

static auto skipCharsBackward(QTextCursor *tc, std::function<bool(const QChar &)> skip) -> int
{
  return skipChars(tc, QTextCursor::PreviousCharacter, -1, skip);
}

auto identifierWordsUnderCursor(const QTextCursor &tc) -> QStringList
{
  const QTextDocument *document = tc.document();
  if (!document)
    return {};
  const auto isSpace = [](const QChar &c) { return c.isSpace(); };
  const auto isColon = [](const QChar &c) { return c == ':'; };
  const auto isValidIdentifierCharAt = [document](const QTextCursor &tc) {
    return isValidIdentifierChar(document->characterAt(tc.position()));
  };
  // move to the end
  auto endCursor(tc);
  do {
    moveCursorToEndOfIdentifier(&endCursor);
    // possibly skip ::
    auto temp(endCursor);
    skipCharsForward(&temp, isSpace);
    const auto colons = skipCharsForward(&temp, isColon);
    skipCharsForward(&temp, isSpace);
    if (colons == 2 && isValidIdentifierCharAt(temp))
      endCursor = temp;
  } while (isValidIdentifierCharAt(endCursor));

  QStringList results;
  auto startCursor(endCursor);
  do {
    moveCursorToStartOfIdentifier(&startCursor);
    if (startCursor.position() == endCursor.position())
      break;
    auto temp(endCursor);
    temp.setPosition(startCursor.position(), QTextCursor::KeepAnchor);
    results.append(temp.selectedText().remove(QRegularExpression("\\s")));
    // possibly skip ::
    temp = startCursor;
    skipCharsBackward(&temp, isSpace);
    const auto colons = skipCharsBackward(&temp, isColon);
    skipCharsBackward(&temp, isSpace);
    if (colons == 2 && isValidIdentifierChar(document->characterAt(temp.position() - 1))) {
      startCursor = temp;
    }
  } while (!isValidIdentifierCharAt(startCursor));
  return results;
}

auto moveCursorToEndOfIdentifier(QTextCursor *tc) -> void
{
  skipCharsForward(tc, isValidIdentifierChar);
}

auto moveCursorToStartOfIdentifier(QTextCursor *tc) -> void
{
  skipCharsBackward(tc, isValidIdentifierChar);
}

static auto isOwnershipRAIIName(const QString &name) -> bool
{
  static QSet<QString> knownNames;
  if (knownNames.isEmpty()) {
    // Qt
    knownNames.insert(QLatin1String("QScopedPointer"));
    knownNames.insert(QLatin1String("QScopedArrayPointer"));
    knownNames.insert(QLatin1String("QMutexLocker"));
    knownNames.insert(QLatin1String("QReadLocker"));
    knownNames.insert(QLatin1String("QWriteLocker"));
    // Standard C++
    knownNames.insert(QLatin1String("auto_ptr"));
    knownNames.insert(QLatin1String("unique_ptr"));
    // Boost
    knownNames.insert(QLatin1String("scoped_ptr"));
    knownNames.insert(QLatin1String("scoped_array"));
  }

  return knownNames.contains(name);
}

auto isOwnershipRAIIType(Symbol *symbol, const LookupContext &context) -> bool
{
  if (!symbol)
    return false;

  // This is not a "real" comparison of types. What we do is to resolve the symbol
  // in question and then try to match its name with already known ones.
  if (symbol->isDeclaration()) {
    Declaration *declaration = symbol->asDeclaration();
    const NamedType *namedType = declaration->type()->asNamedType();
    if (namedType) {
      ClassOrNamespace *clazz = context.lookupType(namedType->name(), declaration->enclosingScope());
      if (clazz && !clazz->symbols().isEmpty()) {
        Overview overview;
        Symbol *symbol = clazz->symbols().at(0);
        return isOwnershipRAIIName(overview.prettyName(symbol->name()));
      }
    }
  }

  return false;
}

auto isValidAsciiIdentifierChar(const QChar &ch) -> bool
{
  return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

auto isValidFirstIdentifierChar(const QChar &ch) -> bool
{
  return ch.isLetter() || ch == QLatin1Char('_') || ch.isHighSurrogate() || ch.isLowSurrogate();
}

auto isValidIdentifierChar(const QChar &ch) -> bool
{
  return isValidFirstIdentifierChar(ch) || ch.isNumber();
}

auto isValidIdentifier(const QString &s) -> bool
{
  const int length = s.length();
  for (auto i = 0; i < length; ++i) {
    const auto &c = s.at(i);
    if (i == 0) {
      if (!isValidFirstIdentifierChar(c))
        return false;
    } else {
      if (!isValidIdentifierChar(c))
        return false;
    }
  }
  return true;
}

auto isQtKeyword(QStringView text) -> bool
{
  switch (text.length()) {
  case 4:
    switch (text.at(0).toLatin1()) {
    case 'e':
      if (text == QLatin1String("emit"))
        return true;
      break;
    case 'S':
      if (text == QLatin1String("SLOT"))
        return true;
      break;
    }
    break;

  case 5:
    if (text.at(0) == QLatin1Char('s') && text == QLatin1String("slots"))
      return true;
    break;

  case 6:
    if (text.at(0) == QLatin1Char('S') && text == QLatin1String("SIGNAL"))
      return true;
    break;

  case 7:
    switch (text.at(0).toLatin1()) {
    case 's':
      if (text == QLatin1String("signals"))
        return true;
      break;
    case 'f':
      if (text == QLatin1String("foreach") || text == QLatin1String("forever"))
        return true;
      break;
    }
    break;

  default:
    break;
  }
  return false;
}

auto switchHeaderSource() -> void
{
  const Core::IDocument *currentDocument = Core::EditorManager::currentDocument();
  QTC_ASSERT(currentDocument, return);
  const auto otherFile = Utils::FilePath::fromString(correspondingHeaderOrSource(currentDocument->filePath().toString()));
  if (!otherFile.isEmpty())
    Core::EditorManager::openEditor(otherFile);
}

auto identifierUnderCursor(QTextCursor *cursor) -> QString
{
  cursor->movePosition(QTextCursor::StartOfWord);
  cursor->movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
  return cursor->selectedText();
}

auto findCanonicalMacro(const QTextCursor &cursor, Document::Ptr document) -> const Macro*
{
  QTC_ASSERT(document, return nullptr);

  int line, column;
  Utils::Text::convertPosition(cursor.document(), cursor.position(), &line, &column);

  if (const Macro *macro = document->findMacroDefinitionAt(line)) {
    auto macroCursor = cursor;
    const auto name = identifierUnderCursor(&macroCursor).toUtf8();
    if (macro->name() == name)
      return macro;
  } else if (const Document::MacroUse *use = document->findMacroUseAt(cursor.position())) {
    return &use->macro();
  }

  return nullptr;
}

auto isInCommentOrString(const TextEditor::AssistInterface *interface, CPlusPlus::LanguageFeatures features) -> bool
{
  QTextCursor tc(interface->textDocument());
  tc.setPosition(interface->position());

  SimpleLexer tokenize;
  features.qtMocRunEnabled = true;
  tokenize.setLanguageFeatures(features);
  tokenize.setSkipComments(false);
  const Tokens &tokens = tokenize(tc.block().text(), BackwardsScanner::previousBlockState(tc.block()));
  const int tokenIdx = SimpleLexer::tokenBefore(tokens, qMax(0, tc.positionInBlock() - 1));
  const Token tk = (tokenIdx == -1) ? Token() : tokens.at(tokenIdx);

  if (tk.isComment())
    return true;
  if (!tk.isLiteral())
    return false;
  if (tokens.size() == 3 && tokens.at(0).kind() == T_POUND && tokens.at(1).kind() == T_IDENTIFIER) {
    const auto &line = tc.block().text();
    const Token &idToken = tokens.at(1);
    QStringView identifier = Utils::midView(line, idToken.utf16charsBegin(), idToken.utf16chars());
    if (identifier == QLatin1String("include") || identifier == QLatin1String("include_next") || (features.objCEnabled && identifier == QLatin1String("import"))) {
      return false;
    }
  }
  return true;
}

auto codeModelSettings() -> CppCodeModelSettings*
{
  return Internal::CppEditorPlugin::instance()->codeModelSettings();
}

auto indexerFileSizeLimitInMb() -> int
{
  const CppCodeModelSettings *settings = codeModelSettings();
  QTC_ASSERT(settings, return -1);

  if (settings->skipIndexingBigFiles())
    return settings->indexerFileSizeLimitInMb();

  return -1;
}

auto fileSizeExceedsLimit(const QFileInfo &fileInfo, int sizeLimitInMb) -> bool
{
  if (sizeLimitInMb <= 0)
    return false;

  const auto fileSizeInMB = fileInfo.size() / (1000 * 1000);
  if (fileSizeInMB > sizeLimitInMb) {
    const auto absoluteFilePath = fileInfo.absoluteFilePath();
    const auto msg = QCoreApplication::translate("CppIndexer", "C++ Indexer: Skipping file \"%1\" because it is too big.").arg(absoluteFilePath);

    QMetaObject::invokeMethod(Core::MessageManager::instance(), [msg]() { Core::MessageManager::writeSilently(msg); });

    return true;
  }

  return false;
}

auto getPchUsage() -> UsePrecompiledHeaders
{
  const CppCodeModelSettings *cms = codeModelSettings();
  if (cms->pchUsage() == CppCodeModelSettings::PchUse_None)
    return UsePrecompiledHeaders::No;
  return UsePrecompiledHeaders::Yes;
}

static auto addBuiltinConfigs(ClangDiagnosticConfigsModel &model) -> void
{
  ClangDiagnosticConfig config;

  // Questionable constructs
  config = ClangDiagnosticConfig();
  config.setId(Constants::CPP_CLANG_DIAG_CONFIG_QUESTIONABLE);
  config.setDisplayName(QCoreApplication::translate("ClangDiagnosticConfigsModel", "Checks for questionable constructs"));
  config.setIsReadOnly(true);
  config.setClangOptions({"-Wall", "-Wextra",});
  config.setClazyMode(ClangDiagnosticConfig::ClazyMode::UseCustomChecks);
  config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseCustomChecks);
  model.appendOrUpdate(config);

  // Warning flags from build system
  config = ClangDiagnosticConfig();
  config.setId(Constants::CPP_CLANG_DIAG_CONFIG_BUILDSYSTEM);
  config.setDisplayName(QCoreApplication::translate("ClangDiagnosticConfigsModel", "Build-system warnings"));
  config.setIsReadOnly(true);
  config.setClazyMode(ClangDiagnosticConfig::ClazyMode::UseCustomChecks);
  config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseCustomChecks);
  config.setUseBuildSystemWarnings(true);
  model.appendOrUpdate(config);
}

auto diagnosticConfigsModel(const ClangDiagnosticConfigs &customConfigs) -> ClangDiagnosticConfigsModel
{
  ClangDiagnosticConfigsModel model;
  addBuiltinConfigs(model);
  for (const auto &config : customConfigs)
    model.appendOrUpdate(config);
  return model;
}

auto diagnosticConfigsModel() -> ClangDiagnosticConfigsModel
{
  return diagnosticConfigsModel(codeModelSettings()->clangCustomDiagnosticConfigs());
}

NSVisitor::NSVisitor(const CppRefactoringFile *file, const QStringList &namespaces, int symbolPos) : ASTVisitor(file->cppDocument()->translationUnit()), m_file(file), m_remainingNamespaces(namespaces), m_symbolPos(symbolPos) {}

auto NSVisitor::preVisit(AST *ast) -> bool
{
  if (!m_firstToken)
    m_firstToken = ast;
  if (m_file->startOf(ast) >= m_symbolPos)
    m_done = true;
  return !m_done;
}

auto NSVisitor::visit(NamespaceAST *ns) -> bool
{
  if (!m_firstNamespace)
    m_firstNamespace = ns;
  if (m_remainingNamespaces.isEmpty()) {
    m_done = true;
    return false;
  }

  QString name;
  const Identifier *const id = translationUnit()->identifier(ns->identifier_token);
  if (id)
    name = QString::fromUtf8(id->chars(), id->size());
  if (name != m_remainingNamespaces.first())
    return false;

  if (!ns->linkage_body) {
    m_done = true;
    return false;
  }

  m_enclosingNamespace = ns;
  m_remainingNamespaces.removeFirst();
  return !m_remainingNamespaces.isEmpty();
}

auto NSVisitor::postVisit(AST *ast) -> void
{
  if (ast == m_enclosingNamespace)
    m_done = true;
}

/**
 * @brief The NSCheckerVisitor class checks which namespaces are missing for a given list
 * of enclosing namespaces at a given position
 */
NSCheckerVisitor::NSCheckerVisitor(const CppRefactoringFile *file, const QStringList &namespaces, int symbolPos) : ASTVisitor(file->cppDocument()->translationUnit()), m_file(file), m_remainingNamespaces(namespaces), m_symbolPos(symbolPos) {}

auto NSCheckerVisitor::preVisit(AST *ast) -> bool
{
  if (m_file->startOf(ast) >= m_symbolPos)
    m_done = true;
  return !m_done;
}

auto NSCheckerVisitor::postVisit(AST *ast) -> void
{
  if (!m_done && m_file->endOf(ast) > m_symbolPos)
    m_done = true;
}

auto NSCheckerVisitor::visit(NamespaceAST *ns) -> bool
{
  if (m_remainingNamespaces.isEmpty())
    return false;

  QString name = getName(ns);
  if (name != m_remainingNamespaces.first())
    return false;

  m_enteredNamespaces.push_back(ns);
  m_remainingNamespaces.removeFirst();
  // if we reached the searched namespace we don't have to search deeper
  return !m_remainingNamespaces.isEmpty();
}

auto NSCheckerVisitor::visit(UsingDirectiveAST *usingNS) -> bool
{
  // example: we search foo::bar and get 'using namespace foo;using namespace foo::bar;'
  const QString fullName = Overview{}.prettyName(usingNS->name->name);
  const QStringList namespaces = fullName.split("::");
  if (namespaces.length() > m_remainingNamespaces.length())
    return false;

  // from other using namespace statements
  const auto curList = m_usingsPerNamespace.find(currentNamespace());
  const bool isCurListValid = curList != m_usingsPerNamespace.end();

  const bool startEqual = std::equal(namespaces.cbegin(), namespaces.cend(), m_remainingNamespaces.cbegin());
  if (startEqual) {
    if (isCurListValid) {
      if (namespaces.length() > curList->second.length()) {
        // eg. we already have 'using namespace foo;' and
        // now get 'using namespace foo::bar;'
        curList->second = namespaces;
      }
      // the other case: first 'using namespace foo::bar;' and now 'using namespace foo;'
    } else
      m_usingsPerNamespace.emplace(currentNamespace(), namespaces);
  } else if (isCurListValid) {
    // ex: we have already 'using namespace foo;' and get 'using namespace bar;' now
    QStringList newlist = curList->second;
    newlist.append(namespaces);
    if (newlist.length() <= m_remainingNamespaces.length()) {
      const bool startEqual = std::equal(newlist.cbegin(), newlist.cend(), m_remainingNamespaces.cbegin());
      if (startEqual)
        curList->second.append(namespaces);
    }
  }
  return false;
}

auto NSCheckerVisitor::endVisit(NamespaceAST *ns) -> void
{
  // if the symbolPos was in the namespace and the
  // namespace has no children, m_done should be true
  postVisit(ns);
  if (!m_done && currentNamespace() == ns) {
    // we were not succesfull in this namespace, so undo all changes
    m_remainingNamespaces.push_front(getName(currentNamespace()));
    m_usingsPerNamespace.erase(currentNamespace());
    m_enteredNamespaces.pop_back();
  }
}

auto NSCheckerVisitor::endVisit(TranslationUnitAST *) -> void
{
  // the last node, create the final result
  // we must handle like the following: We search for foo::bar and have:
  // using namespace foo::bar;
  // namespace foo {
  //    // cursor/symbolPos here
  // }
  if (m_remainingNamespaces.empty()) {
    // we are already finished
    return;
  }
  // find the longest combination of normal namespaces + using statements
  auto longestNamespaceList = 0;
  auto enteredNamespaceCount = 0;
  // check 'using namespace ...;' statements in the global scope
  const auto namespaces = m_usingsPerNamespace.find(nullptr);
  if (namespaces != m_usingsPerNamespace.end())
    longestNamespaceList = namespaces->second.length();

  for (auto ns : m_enteredNamespaces) {
    ++enteredNamespaceCount;
    const auto namespaces = m_usingsPerNamespace.find(ns);
    auto newListLength = enteredNamespaceCount;
    if (namespaces != m_usingsPerNamespace.end())
      newListLength += namespaces->second.length();
    longestNamespaceList = std::max(newListLength, longestNamespaceList);
  }
  m_remainingNamespaces.erase(m_remainingNamespaces.begin(), m_remainingNamespaces.begin() + longestNamespaceList - m_enteredNamespaces.size());
}

auto NSCheckerVisitor::getName(NamespaceAST *ns) -> QString
{
  const Identifier *const id = translationUnit()->identifier(ns->identifier_token);
  if (id)
    return QString::fromUtf8(id->chars(), id->size());
  return {};
}

auto NSCheckerVisitor::currentNamespace() -> NamespaceAST*
{
  return m_enteredNamespaces.empty() ? nullptr : m_enteredNamespaces.back();
}

auto projectForProjectPart(const ProjectPart &part) -> ProjectExplorer::Project*
{
  return ProjectExplorer::SessionManager::projectWithProjectFilePath(part.topLevelProject);
}

auto projectForProjectInfo(const ProjectInfo &info) -> ProjectExplorer::Project*
{
  return ProjectExplorer::SessionManager::projectWithProjectFilePath(info.projectFilePath());
}

namespace Internal {

auto decorateCppEditor(TextEditor::TextEditorWidget *editor) -> void
{
  editor->textDocument()->setSyntaxHighlighter(new CppHighlighter);
  editor->textDocument()->setIndenter(new CppQtStyleIndenter(editor->textDocument()->document()));
  editor->setAutoCompleter(new CppAutoCompleter);
}

} // namespace Internal
} // CppEditor
