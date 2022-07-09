// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppeditorwidget.hpp"

#include "cppautocompleter.hpp"
#include "cppcanonicalsymbol.hpp"
#include "cppchecksymbols.hpp"
#include "cppcodeformatter.hpp"
#include "cppcodemodelsettings.hpp"
#include "cppcompletionassistprovider.hpp"
#include "doxygengenerator.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditordocument.hpp"
#include "cppeditoroutline.hpp"
#include "cppeditorplugin.hpp"
#include "cppfunctiondecldeflink.hpp"
#include "cpphighlighter.hpp"
#include "cpplocalrenaming.hpp"
#include "cppminimizableinfobars.hpp"
#include "cppmodelmanager.hpp"
#include "cpppreprocessordialog.hpp"
#include "cppsemanticinfo.hpp"
#include "cppselectionchanger.hpp"
#include "cppqtstyleindenter.hpp"
#include "cppquickfixassistant.hpp"
#include "cpptoolsreuse.hpp"
#include "cpptoolssettings.hpp"
#include "cppuseselectionsupdater.hpp"
#include "cppworkingcopy.hpp"
#include "followsymbolinterface.hpp"
#include "refactoringengineinterface.hpp"
#include "symbolfinder.hpp"

#include <clangsupport/sourcelocationscontainer.h>

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/editormanager/documentmodel.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/find/searchresultwindow.hpp>

#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/session.hpp>

#include <texteditor/basefilefind.hpp>
#include <texteditor/behaviorsettings.hpp>
#include <texteditor/codeassist/assistproposalitem.hpp>
#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/genericproposalmodel.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/commentssettings.hpp>
#include <texteditor/completionsettings.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/refactoroverlay.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/textdocumentlayout.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <cplusplus/ASTPath.h>
#include <cplusplus/FastPreprocessor.h>
#include <cplusplus/MatchingText.h>
#include <utils/infobar.hpp>
#include <utils/progressindicator.hpp>
#include <utils/qtcassert.hpp>
#include <utils/textutils.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QMenu>
#include <QPointer>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>

enum {
  UPDATE_FUNCTION_DECL_DEF_LINK_INTERVAL = 200
};

using namespace Core;
using namespace CPlusPlus;
using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace CppEditor {
namespace Internal {

namespace {

auto isStartOfDoxygenComment(const QTextCursor &cursor) -> bool
{
  const auto pos = cursor.position();

  auto document = cursor.document();
  QString comment = QString(document->characterAt(pos - 3)) + document->characterAt(pos - 2) + document->characterAt(pos - 1);

  return comment == QLatin1String("/**") || comment == QLatin1String("/*!") || comment == QLatin1String("///") || comment == QLatin1String("//!");
}

auto doxygenStyle(const QTextCursor &cursor, const QTextDocument *doc) -> DoxygenGenerator::DocumentationStyle
{
  const auto pos = cursor.position();

  QString comment = QString(doc->characterAt(pos - 3)) + doc->characterAt(pos - 2) + doc->characterAt(pos - 1);

  if (comment == QLatin1String("/**"))
    return DoxygenGenerator::JavaStyle;
  else if (comment == QLatin1String("/*!"))
    return DoxygenGenerator::QtStyle;
  else if (comment == QLatin1String("///"))
    return DoxygenGenerator::CppStyleA;
  else
    return DoxygenGenerator::CppStyleB;
}

/// Check if previous line is a CppStyle Doxygen Comment
auto isPreviousLineCppStyleComment(const QTextCursor &cursor) -> bool
{
  const auto &currentBlock = cursor.block();
  if (!currentBlock.isValid())
    return false;

  const auto &actual = currentBlock.previous();
  if (!actual.isValid())
    return false;

  const auto text = actual.text().trimmed();
  return text.startsWith(QLatin1String("///")) || text.startsWith(QLatin1String("//!"));
}

/// Check if next line is a CppStyle Doxygen Comment
auto isNextLineCppStyleComment(const QTextCursor &cursor) -> bool
{
  const auto &currentBlock = cursor.block();
  if (!currentBlock.isValid())
    return false;

  const auto &actual = currentBlock.next();
  if (!actual.isValid())
    return false;

  const auto text = actual.text().trimmed();
  return text.startsWith(QLatin1String("///")) || text.startsWith(QLatin1String("//!"));
}

auto isCppStyleContinuation(const QTextCursor &cursor) -> bool
{
  return isPreviousLineCppStyleComment(cursor) || isNextLineCppStyleComment(cursor);
}

auto lineStartsWithCppDoxygenCommentAndCursorIsAfter(const QTextCursor &cursor, const QTextDocument *doc) -> bool
{
  auto cursorFirstNonBlank(cursor);
  cursorFirstNonBlank.movePosition(QTextCursor::StartOfLine);
  while (doc->characterAt(cursorFirstNonBlank.position()).isSpace() && cursorFirstNonBlank.movePosition(QTextCursor::NextCharacter)) { }

  const auto &block = cursorFirstNonBlank.block();
  const auto text = block.text().trimmed();
  if (text.startsWith(QLatin1String("///")) || text.startsWith(QLatin1String("//!")))
    return (cursor.position() >= cursorFirstNonBlank.position() + 3);

  return false;
}

auto isCursorAfterNonNestedCppStyleComment(const QTextCursor &cursor, TextEditor::TextEditorWidget *editorWidget) -> bool
{
  auto document = editorWidget->document();
  auto cursorBeforeCppComment(cursor);
  while (document->characterAt(cursorBeforeCppComment.position()) != QLatin1Char('/') && cursorBeforeCppComment.movePosition(QTextCursor::PreviousCharacter)) { }

  if (!cursorBeforeCppComment.movePosition(QTextCursor::PreviousCharacter))
    return false;

  if (document->characterAt(cursorBeforeCppComment.position()) != QLatin1Char('/'))
    return false;

  if (!cursorBeforeCppComment.movePosition(QTextCursor::PreviousCharacter))
    return false;

  return !CPlusPlus::MatchingText::isInCommentHelper(cursorBeforeCppComment);
}

auto handleDoxygenCppStyleContinuation(QTextCursor &cursor) -> bool
{
  const auto blockPos = cursor.positionInBlock();
  const auto &text = cursor.block().text();
  auto offset = 0;
  for (; offset < blockPos; ++offset) {
    if (!text.at(offset).isSpace())
      break;
  }

  // If the line does not start with the comment we don't
  // consider it as a continuation. Handles situations like:
  // void d(); ///<enter>
  if (offset + 3 > text.size())
    return false;
  const auto commentMarker = QStringView(text).mid(offset, 3);
  if (commentMarker != QLatin1String("///") && commentMarker != QLatin1String("//!"))
    return false;

  QString newLine(QLatin1Char('\n'));
  newLine.append(text.left(offset)); // indent correctly
  newLine.append(commentMarker.toString());
  newLine.append(QLatin1Char(' '));

  cursor.insertText(newLine);
  return true;
}

auto handleDoxygenContinuation(QTextCursor &cursor, TextEditor::TextEditorWidget *editorWidget, const bool enableDoxygen, const bool leadingAsterisks) -> bool
{
  const QTextDocument *doc = editorWidget->document();

  // It might be a continuation if:
  // a) current line starts with /// or //! and cursor is positioned after the comment
  // b) current line is in the middle of a multi-line Qt or Java style comment

  if (!cursor.atEnd()) {
    if (enableDoxygen && lineStartsWithCppDoxygenCommentAndCursorIsAfter(cursor, doc))
      return handleDoxygenCppStyleContinuation(cursor);

    if (isCursorAfterNonNestedCppStyleComment(cursor, editorWidget))
      return false;
  }

  // We continue the comment if the cursor is after a comment's line asterisk and if
  // there's no asterisk immediately after the cursor (that would already be considered
  // a leading asterisk).
  auto offset = 0;
  const auto blockPos = cursor.positionInBlock();
  const auto &currentLine = cursor.block().text();
  for (; offset < blockPos; ++offset) {
    if (!currentLine.at(offset).isSpace())
      break;
  }

  // In case we don't need to insert leading asteriskses, this code will be run once (right after
  // hitting enter on the line containing '/*'). It will insert a continuation without an
  // asterisk, but with an extra space. After that, the normal indenting will take over and do the
  // Right Thing <TM>.
  if (offset < blockPos && (currentLine.at(offset) == QLatin1Char('*') || (offset < blockPos - 1 && currentLine.at(offset) == QLatin1Char('/') && currentLine.at(offset + 1) == QLatin1Char('*')))) {
    // Ok, so the line started with an '*' or '/*'
    auto followinPos = blockPos;
    // Now search for the first non-whitespace character to align to:
    for (; followinPos < currentLine.length(); ++followinPos) {
      if (!currentLine.at(followinPos).isSpace())
        break;
    }
    if (followinPos == currentLine.length() // a)
      || currentLine.at(followinPos) != QLatin1Char('*')) {
      // b)
      // So either a) the line ended after a '*' and we need to insert a continuation, or
      // b) we found the start of some text and we want to align the continuation to that.
      QString newLine(QLatin1Char('\n'));
      auto c(cursor);
      c.movePosition(QTextCursor::StartOfBlock);
      c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, offset);
      newLine.append(c.selectedText());
      if (currentLine.at(offset) == QLatin1Char('/')) {
        if (leadingAsterisks)
          newLine.append(QLatin1String(" * "));
        else
          newLine.append(QLatin1String("   "));
        offset += 3;
      } else {
        // If '*' is not within a comment, skip.
        auto cursorOnFirstNonWhiteSpace(cursor);
        const auto positionOnFirstNonWhiteSpace = cursor.position() - blockPos + offset;
        cursorOnFirstNonWhiteSpace.setPosition(positionOnFirstNonWhiteSpace);
        if (!CPlusPlus::MatchingText::isInCommentHelper(cursorOnFirstNonWhiteSpace))
          return false;

        // ...otherwise do the continuation
        auto start = offset;
        while (offset < blockPos && currentLine.at(offset) == QLatin1Char('*'))
          ++offset;
        const QChar ch = leadingAsterisks ? QLatin1Char('*') : QLatin1Char(' ');
        newLine.append(QString(offset - start, ch));
      }
      for (; offset < blockPos && currentLine.at(offset) == ' '; ++offset)
        newLine.append(QLatin1Char(' '));
      cursor.insertText(newLine);
      return true;
    }
  }

  return false;
}

static auto trySplitComment(TextEditor::TextEditorWidget *editorWidget, const CPlusPlus::Snapshot &snapshot) -> bool
{
  const auto &settings = CppToolsSettings::instance()->commentsSettings();
  if (!settings.m_enableDoxygen && !settings.m_leadingAsterisks)
    return false;

  if (editorWidget->multiTextCursor().hasMultipleCursors())
    return false;

  auto cursor = editorWidget->textCursor();
  if (!CPlusPlus::MatchingText::isInCommentHelper(cursor))
    return false;

  // We are interested on two particular cases:
  //   1) The cursor is right after a /**, /*!, /// or ///! and the user pressed enter.
  //      If Doxygen is enabled we need to generate an entire comment block.
  //   2) The cursor is already in the middle of a multi-line comment and the user pressed
  //      enter. If leading asterisk(s) is set we need to write a comment continuation
  //      with those.

  if (settings.m_enableDoxygen && cursor.positionInBlock() >= 3) {
    const auto pos = cursor.position();
    if (isStartOfDoxygenComment(cursor)) {
      auto textDocument = editorWidget->document();
      auto style = doxygenStyle(cursor, textDocument);

      // Check if we're already in a CppStyle Doxygen comment => continuation
      // Needs special handling since CppStyle does not have start and end markers
      if ((style == DoxygenGenerator::CppStyleA || style == DoxygenGenerator::CppStyleB) && isCppStyleContinuation(cursor)) {
        return handleDoxygenCppStyleContinuation(cursor);
      }

      DoxygenGenerator doxygen;
      doxygen.setStyle(style);
      doxygen.setAddLeadingAsterisks(settings.m_leadingAsterisks);
      doxygen.setGenerateBrief(settings.m_generateBrief);
      doxygen.setStartComment(false);

      // Move until we reach any possibly meaningful content.
      while (textDocument->characterAt(cursor.position()).isSpace() && cursor.movePosition(QTextCursor::NextCharacter)) { }

      if (!cursor.atEnd()) {
        const auto &comment = doxygen.generate(cursor, snapshot, editorWidget->textDocument()->filePath());
        if (!comment.isEmpty()) {
          cursor.beginEditBlock();
          cursor.setPosition(pos);
          cursor.insertText(comment);
          cursor.setPosition(pos - 3, QTextCursor::KeepAnchor);
          editorWidget->textDocument()->autoIndent(cursor);
          cursor.endEditBlock();
          return true;
        }
        cursor.setPosition(pos);
      }
    }
  } // right after first doxygen comment

  return handleDoxygenContinuation(cursor, editorWidget, settings.m_enableDoxygen, settings.m_leadingAsterisks);
}

} // anonymous namespace

class CppEditorWidgetPrivate {
public:
  CppEditorWidgetPrivate(CppEditorWidget *q);

  auto shouldOfferOutline() const -> bool { return CppModelManager::supportsOutline(m_cppEditorDocument); }

public:
  QPointer<CppModelManager> m_modelManager;

  CppEditorDocument *m_cppEditorDocument;
  CppEditorOutline *m_cppEditorOutline = nullptr;
  QAction *m_outlineAction = nullptr;
  QTimer m_outlineTimer;

  QTimer m_updateFunctionDeclDefLinkTimer;
  SemanticInfo m_lastSemanticInfo;

  FunctionDeclDefLinkFinder *m_declDefLinkFinder;
  QSharedPointer<FunctionDeclDefLink> m_declDefLink;

  QAction *m_parseContextAction = nullptr;
  ParseContextWidget *m_parseContextWidget = nullptr;
  QToolButton *m_preprocessorButton = nullptr;
  MinimizableInfoBars::Actions m_showInfoBarActions;

  CppLocalRenaming m_localRenaming;
  CppUseSelectionsUpdater m_useSelectionsUpdater;
  CppSelectionChanger m_cppSelectionChanger;
  bool inTestMode = false;
};

CppEditorWidgetPrivate::CppEditorWidgetPrivate(CppEditorWidget *q) : m_modelManager(CppModelManager::instance()), m_cppEditorDocument(qobject_cast<CppEditorDocument*>(q->textDocument())), m_declDefLinkFinder(new FunctionDeclDefLinkFinder(q)), m_localRenaming(q), m_useSelectionsUpdater(q), m_cppSelectionChanger() {}
} // namespace Internal

using namespace Internal;

CppEditorWidget::CppEditorWidget() : d(new CppEditorWidgetPrivate(this))
{
  qRegisterMetaType<SemanticInfo>("SemanticInfo");
}

auto CppEditorWidget::finalizeInitialization() -> void
{
  d->m_cppEditorDocument = qobject_cast<CppEditorDocument*>(textDocument());

  setLanguageSettingsId(Constants::CPP_SETTINGS_ID);

  // clang-format off
  // function combo box sorting
  d->m_cppEditorOutline = new CppEditorOutline(this);

  // TODO: Nobody emits this signal... Remove?
  connect(CppEditorPlugin::instance(), &CppEditorPlugin::outlineSortingChanged, outline(), &CppEditorOutline::setSorted);

  connect(d->m_cppEditorDocument, &CppEditorDocument::codeWarningsUpdated, this, &CppEditorWidget::onCodeWarningsUpdated);
  connect(d->m_cppEditorDocument, &CppEditorDocument::ifdefedOutBlocksUpdated, this, &CppEditorWidget::onIfdefedOutBlocksUpdated);
  connect(d->m_cppEditorDocument, &CppEditorDocument::cppDocumentUpdated, this, &CppEditorWidget::onCppDocumentUpdated);
  connect(d->m_cppEditorDocument, &CppEditorDocument::semanticInfoUpdated, this, [this](const SemanticInfo &info) { updateSemanticInfo(info); });

  connect(d->m_declDefLinkFinder, &FunctionDeclDefLinkFinder::foundLink, this, &CppEditorWidget::onFunctionDeclDefLinkFound);

  connect(&d->m_useSelectionsUpdater, &CppUseSelectionsUpdater::selectionsForVariableUnderCursorUpdated, &d->m_localRenaming, &CppLocalRenaming::updateSelectionsForVariableUnderCursor);

  connect(&d->m_useSelectionsUpdater, &CppUseSelectionsUpdater::finished, this, [this](SemanticInfo::LocalUseMap localUses, bool success) {
    if (success) {
      d->m_lastSemanticInfo.localUsesUpdated = true;
      d->m_lastSemanticInfo.localUses = localUses;
    }
  });

  connect(document(), &QTextDocument::contentsChange, &d->m_localRenaming, &CppLocalRenaming::onContentsChangeOfEditorWidgetDocument);
  connect(&d->m_localRenaming, &CppLocalRenaming::finished, [this] {
    cppEditorDocument()->recalculateSemanticInfoDetached();
  });
  connect(&d->m_localRenaming, &CppLocalRenaming::processKeyPressNormally, this, &CppEditorWidget::processKeyNormally);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] {
    if (d->shouldOfferOutline())
      d->m_cppEditorOutline->updateIndex();
  });

  connect(cppEditorDocument(), &CppEditorDocument::preprocessorSettingsChanged, this, [this](bool customSettings) {
    updateWidgetHighlighting(d->m_preprocessorButton, customSettings);
  });

  // set up function declaration - definition link
  d->m_updateFunctionDeclDefLinkTimer.setSingleShot(true);
  d->m_updateFunctionDeclDefLinkTimer.setInterval(UPDATE_FUNCTION_DECL_DEF_LINK_INTERVAL);
  connect(&d->m_updateFunctionDeclDefLinkTimer, &QTimer::timeout, this, &CppEditorWidget::updateFunctionDeclDefLinkNow);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CppEditorWidget::updateFunctionDeclDefLink);
  connect(this, &QPlainTextEdit::textChanged, this, &CppEditorWidget::updateFunctionDeclDefLink);

  // set up the use highlighitng
  connect(this, &CppEditorWidget::cursorPositionChanged, this, [this]() {
    if (!d->m_localRenaming.isActive())
      d->m_useSelectionsUpdater.scheduleUpdate();

    // Notify selection expander about the changed cursor.
    d->m_cppSelectionChanger.onCursorPositionChanged(textCursor());
  });

  // Toolbar: Parse context
  auto &parseContextModel = cppEditorDocument()->parseContextModel();
  d->m_parseContextWidget = new ParseContextWidget(parseContextModel, this);
  d->m_parseContextAction = insertExtraToolBarWidget(TextEditorWidget::Left, d->m_parseContextWidget);
  d->m_parseContextAction->setVisible(false);
  connect(&parseContextModel, &ParseContextModel::updated, this, [this](bool areMultipleAvailable) {
    d->m_parseContextAction->setVisible(areMultipleAvailable);
  });

  // Toolbar: Outline/Overview combo box
  d->m_outlineAction = insertExtraToolBarWidget(TextEditorWidget::Left, d->m_cppEditorOutline->widget());

  // clang-format on
  // Toolbar: '#' Button
  // TODO: Make "Additional Preprocessor Directives" also useful with Clang Code Model.
  if (!d->m_modelManager->isClangCodeModelActive()) {
    d->m_preprocessorButton = new QToolButton(this);
    d->m_preprocessorButton->setText(QLatin1String("#"));
    auto cmd = ActionManager::command(Constants::OPEN_PREPROCESSOR_DIALOG);
    connect(cmd, &Command::keySequenceChanged, this, &CppEditorWidget::updatePreprocessorButtonTooltip);
    updatePreprocessorButtonTooltip();
    connect(d->m_preprocessorButton, &QAbstractButton::clicked, this, &CppEditorWidget::showPreProcessorWidget);

    insertExtraToolBarWidget(TextEditorWidget::Left, d->m_preprocessorButton);
  }

  // Toolbar: Actions to show minimized info bars
  d->m_showInfoBarActions = MinimizableInfoBars::createShowInfoBarActions([this](QWidget *w) {
    return this->insertExtraToolBarWidget(TextEditorWidget::Left, w);
  });
  connect(&cppEditorDocument()->minimizableInfoBars(), &MinimizableInfoBars::showAction, this, &CppEditorWidget::onShowInfoBarAction);

  d->m_outlineTimer.setInterval(5000);
  d->m_outlineTimer.setSingleShot(true);
  connect(&d->m_outlineTimer, &QTimer::timeout, this, [this] {
    d->m_outlineAction->setVisible(d->shouldOfferOutline());
    if (d->m_outlineAction->isVisible()) {
      d->m_cppEditorOutline->update();
      d->m_cppEditorOutline->updateIndex();
    }
  });
  connect(&ClangdSettings::instance(), &ClangdSettings::changed, &d->m_outlineTimer, qOverload<>(&QTimer::start));
  connect(d->m_cppEditorDocument, &CppEditorDocument::changed, &d->m_outlineTimer, qOverload<>(&QTimer::start));
}

auto CppEditorWidget::finalizeInitializationAfterDuplication(TextEditorWidget *other) -> void
{
  QTC_ASSERT(other, return);
  auto cppEditorWidget = qobject_cast<CppEditorWidget*>(other);
  QTC_ASSERT(cppEditorWidget, return);

  if (cppEditorWidget->isSemanticInfoValidExceptLocalUses())
    updateSemanticInfo(cppEditorWidget->semanticInfo());
  if (d->shouldOfferOutline())
    d->m_cppEditorOutline->update();
  const auto selectionKind = CodeWarningsSelection;
  setExtraSelections(selectionKind, cppEditorWidget->extraSelections(selectionKind));

  if (isWidgetHighlighted(cppEditorWidget->d->m_preprocessorButton))
    updateWidgetHighlighting(d->m_preprocessorButton, true);

  d->m_parseContextWidget->syncToModel();
  d->m_parseContextAction->setVisible(d->m_cppEditorDocument->parseContextModel().areMultipleAvailable());
}

auto CppEditorWidget::setProposals(const TextEditor::IAssistProposal *immediateProposal, const TextEditor::IAssistProposal *finalProposal) -> void
{
  QTC_ASSERT(isInTestMode(), return);
  #ifdef WITH_TESTS
    emit proposalsReady(immediateProposal, finalProposal);
  #endif
}

CppEditorWidget::~CppEditorWidget() = default;

auto CppEditorWidget::cppEditorDocument() const -> CppEditorDocument*
{
  return d->m_cppEditorDocument;
}

auto CppEditorWidget::outline() const -> CppEditorOutline*
{
  return d->m_cppEditorOutline;
}

auto CppEditorWidget::paste() -> void
{
  if (d->m_localRenaming.handlePaste())
    return;

  TextEditorWidget::paste();
}

auto CppEditorWidget::cut() -> void
{
  if (d->m_localRenaming.handleCut())
    return;

  TextEditorWidget::cut();
}

auto CppEditorWidget::selectAll() -> void
{
  if (d->m_localRenaming.handleSelectAll())
    return;

  TextEditorWidget::selectAll();
}

auto CppEditorWidget::onCppDocumentUpdated() -> void
{
  if (d->shouldOfferOutline())
    d->m_cppEditorOutline->update();
}

auto CppEditorWidget::onCodeWarningsUpdated(unsigned revision, const QList<QTextEdit::ExtraSelection> selections, const RefactorMarkers &refactorMarkers) -> void
{
  if (revision != documentRevision())
    return;

  setExtraSelections(TextEditorWidget::CodeWarningsSelection, unselectLeadingWhitespace(selections));
  setRefactorMarkers(refactorMarkers + RefactorMarker::filterOutType(this->refactorMarkers(), Constants::CPP_CLANG_FIXIT_AVAILABLE_MARKER_ID));
}

auto CppEditorWidget::onIfdefedOutBlocksUpdated(unsigned revision, const QList<BlockRange> ifdefedOutBlocks) -> void
{
  if (revision != documentRevision())
    return;
  textDocument()->setIfdefedOutBlocks(ifdefedOutBlocks);
}

auto CppEditorWidget::onShowInfoBarAction(const Id &id, bool show) -> void
{
  auto action = d->m_showInfoBarActions.value(id);
  QTC_ASSERT(action, return);
  action->setVisible(show);
}

static auto getDocumentLine(QTextDocument *document, int line) -> QString
{
  if (document)
    return document->findBlockByNumber(line - 1).text();

  return {};
}

static auto getCurrentDocument(const QString &path) -> std::unique_ptr<QTextDocument>
{
  const QTextCodec *defaultCodec = Core::EditorManager::defaultTextCodec();
  QString contents;
  Utils::TextFileFormat format;
  QString error;
  if (Utils::TextFileFormat::readFile(Utils::FilePath::fromString(path), defaultCodec, &contents, &format, &error) != Utils::TextFileFormat::ReadSuccess) {
    qWarning() << "Error reading file " << path << " : " << error;
    return {};
  }

  return std::make_unique<QTextDocument>(contents);
}

static auto onReplaceUsagesClicked(const QString &text, const QList<SearchResultItem> &items, bool preserveCase) -> void
{
  auto modelManager = CppModelManager::instance();
  if (!modelManager)
    return;

  const auto filePaths = TextEditor::BaseFileFind::replaceAll(text, items, preserveCase);
  if (!filePaths.isEmpty()) {
    modelManager->updateSourceFiles(Utils::transform<QSet>(filePaths, &FilePath::toString));
    SearchResultWindow::instance()->hide();
  }
}

static auto getOpenDocument(const QString &path) -> QTextDocument*
{
  const IDocument *document = DocumentModel::documentForFilePath(FilePath::fromString(path));
  if (document)
    return qobject_cast<const TextDocument*>(document)->document();

  return {};
}

static auto addSearchResults(Usages usages, SearchResult &search, const QString &text) -> void
{
  std::sort(usages.begin(), usages.end());

  std::unique_ptr<QTextDocument> currentDocument;
  QString lastPath;

  for (const auto &usage : usages) {
    auto document = getOpenDocument(usage.path);

    if (!document) {
      if (usage.path != lastPath) {
        currentDocument = getCurrentDocument(usage.path);
        lastPath = usage.path;
      }
      document = currentDocument.get();
    }

    const auto lineContent = getDocumentLine(document, usage.line);

    if (!lineContent.isEmpty()) {
      Search::TextRange range{Search::TextPosition(usage.line, usage.column - 1), Search::TextPosition(usage.line, usage.column + text.length() - 1)};
      SearchResultItem item;
      item.setFilePath(FilePath::fromString(usage.path));
      item.setLineText(lineContent);
      item.setMainRange(range);
      item.setUseTextEditorFont(true);
      search.addResult(item);
    }
  }
}

static auto findRenameCallback(CppEditorWidget *widget, const QTextCursor &baseCursor, const Usages &usages, bool rename = false, const QString &replacement = QString()) -> void
{
  auto cursor = Utils::Text::wordStartCursor(baseCursor);
  cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
  const auto text = cursor.selectedText();
  auto mode = SearchResultWindow::SearchOnly;
  if (rename)
    mode = SearchResultWindow::SearchAndReplace;
  auto search = SearchResultWindow::instance()->startNewSearch(QObject::tr("C++ Usages:"), QString(), text, mode, SearchResultWindow::PreserveCaseDisabled, QLatin1String("CppEditor"));
  search->setTextToReplace(replacement);
  search->setSearchAgainSupported(true);
  QObject::connect(search, &SearchResult::replaceButtonClicked, &onReplaceUsagesClicked);
  QObject::connect(search, &SearchResult::searchAgainRequested, [widget, rename, replacement, baseCursor]() {
    rename ? widget->renameUsages(replacement, baseCursor) : widget->findUsages(baseCursor);
  });

  addSearchResults(usages, *search, text);

  search->finishSearch(false);
  QObject::connect(search, &SearchResult::activated, [](const Core::SearchResultItem &item) {
    Core::EditorManager::openEditorAtSearchResult(item);
  });
  search->popup();
}

auto CppEditorWidget::findUsages() -> void
{
  findUsages(textCursor());
}

auto CppEditorWidget::findUsages(QTextCursor cursor) -> void
{
  // 'this' in cursorInEditor is never used (and must never be used) asynchronously.
  const CursorInEditor cursorInEditor{cursor, textDocument()->filePath(), this, textDocument()};
  QPointer<CppEditorWidget> cppEditorWidget = this;
  d->m_modelManager->findUsages(cursorInEditor, [=](const Usages &usages) {
    if (!cppEditorWidget)
      return;
    findRenameCallback(cppEditorWidget.data(), cursor, usages);
  });
}

auto CppEditorWidget::renameUsages(const QString &replacement, QTextCursor cursor) -> void
{
  if (cursor.isNull())
    cursor = textCursor();
  CursorInEditor cursorInEditor{cursor, textDocument()->filePath(), this, textDocument()};
  QPointer<CppEditorWidget> cppEditorWidget = this;
  d->m_modelManager->globalRename(cursorInEditor, [=](const Usages &usages) {
    if (!cppEditorWidget)
      return;
    findRenameCallback(cppEditorWidget.data(), cursor, usages, true, replacement);
  }, replacement);
}

auto CppEditorWidget::selectBlockUp() -> bool
{
  if (!behaviorSettings().m_smartSelectionChanging)
    return TextEditorWidget::selectBlockUp();

  auto cursor = textCursor();
  d->m_cppSelectionChanger.startChangeSelection();
  const bool changed = d->m_cppSelectionChanger.changeSelection(CppSelectionChanger::ExpandSelection, cursor, d->m_lastSemanticInfo.doc);
  if (changed)
    setTextCursor(cursor);
  d->m_cppSelectionChanger.stopChangeSelection();

  return changed;
}

auto CppEditorWidget::selectBlockDown() -> bool
{
  if (!behaviorSettings().m_smartSelectionChanging)
    return TextEditorWidget::selectBlockDown();

  auto cursor = textCursor();
  d->m_cppSelectionChanger.startChangeSelection();
  const bool changed = d->m_cppSelectionChanger.changeSelection(CppSelectionChanger::ShrinkSelection, cursor, d->m_lastSemanticInfo.doc);
  if (changed)
    setTextCursor(cursor);
  d->m_cppSelectionChanger.stopChangeSelection();

  return changed;
}

auto CppEditorWidget::updateWidgetHighlighting(QWidget *widget, bool highlight) -> void
{
  if (!widget)
    return;

  widget->setProperty("highlightWidget", highlight);
  widget->update();
}

auto CppEditorWidget::isWidgetHighlighted(QWidget *widget) -> bool
{
  return widget ? widget->property("highlightWidget").toBool() : false;
}

namespace {

auto fetchProjectParts(CppModelManager *modelManager, const Utils::FilePath &filePath) -> QList<ProjectPart::ConstPtr>
{
  auto projectParts = modelManager->projectPart(filePath);

  if (projectParts.isEmpty())
    projectParts = modelManager->projectPartFromDependencies(filePath);
  if (projectParts.isEmpty())
    projectParts.append(modelManager->fallbackProjectPart());

  return projectParts;
}

auto findProjectPartForCurrentProject(const QList<ProjectPart::ConstPtr> &projectParts, ProjectExplorer::Project *currentProject) -> const ProjectPart*
{
  const auto found = std::find_if(projectParts.cbegin(), projectParts.cend(), [&](const ProjectPart::ConstPtr &projectPart) {
    return projectPart->belongsToProject(currentProject);
  });

  if (found != projectParts.cend())
    return (*found).data();

  return nullptr;
}

} // namespace

auto CppEditorWidget::projectPart() const -> const ProjectPart*
{
  if (!d->m_modelManager)
    return nullptr;

  auto projectParts = fetchProjectParts(d->m_modelManager, textDocument()->filePath());

  return findProjectPartForCurrentProject(projectParts, ProjectExplorer::ProjectTree::currentProject());
}

namespace {

using ClangBackEnd::SourceLocationContainer;
using Utils::Text::selectAt;

auto occurrencesTextCharFormat() -> QTextCharFormat
{
  using TextEditor::TextEditorSettings;

  return TextEditorSettings::fontSettings().toTextCharFormat(TextEditor::C_OCCURRENCES);
}

auto sourceLocationsToExtraSelections(const std::vector<SourceLocationContainer> &sourceLocations, uint selectionLength, CppEditorWidget *cppEditorWidget) -> QList<QTextEdit::ExtraSelection>
{
  const auto textCharFormat = occurrencesTextCharFormat();

  QList<QTextEdit::ExtraSelection> selections;
  selections.reserve(int(sourceLocations.size()));

  auto sourceLocationToExtraSelection = [&](const SourceLocationContainer &sourceLocation) {
    QTextEdit::ExtraSelection selection;

    selection.cursor = selectAt(cppEditorWidget->textCursor(), sourceLocation.line, sourceLocation.column, selectionLength);
    selection.format = textCharFormat;

    return selection;
  };

  std::transform(sourceLocations.begin(), sourceLocations.end(), std::back_inserter(selections), sourceLocationToExtraSelection);

  return selections;
};

}

auto CppEditorWidget::renameSymbolUnderCursor() -> void
{
  using ClangBackEnd::SourceLocationsContainer;

  auto projPart = projectPart();
  if (!projPart)
    return;

  if (d->m_localRenaming.isActive() && d->m_localRenaming.isSameSelection(textCursor().position())) {
    return;
  }
  d->m_useSelectionsUpdater.abortSchedule();

  QPointer<CppEditorWidget> cppEditorWidget = this;

  auto renameSymbols = [=](const QString &symbolName, const SourceLocationsContainer &sourceLocations, int revision) {
    if (cppEditorWidget) {
      viewport()->setCursor(Qt::IBeamCursor);

      if (revision != document()->revision())
        return;
      if (sourceLocations.hasContent()) {
        QList<QTextEdit::ExtraSelection> selections = sourceLocationsToExtraSelections(sourceLocations.sourceLocationContainers(), static_cast<uint>(symbolName.size()), cppEditorWidget);
        setExtraSelections(TextEditor::TextEditorWidget::CodeSemanticsSelection, selections);
        d->m_localRenaming.stop();
        d->m_localRenaming.updateSelectionsForVariableUnderCursor(selections);
      }
      if (!d->m_localRenaming.start())
        cppEditorWidget->renameUsages();
    }
  };

  viewport()->setCursor(Qt::BusyCursor);
  d->m_modelManager->startLocalRenaming(CursorInEditor{textCursor(), textDocument()->filePath(), this, textDocument()}, projPart, std::move(renameSymbols));
}

auto CppEditorWidget::updatePreprocessorButtonTooltip() -> void
{
  if (!d->m_preprocessorButton)
    return;

  auto cmd = ActionManager::command(Constants::OPEN_PREPROCESSOR_DIALOG);
  QTC_ASSERT(cmd, return);
  d->m_preprocessorButton->setToolTip(cmd->action()->toolTip());
}

auto CppEditorWidget::switchDeclarationDefinition(bool inNextSplit) -> void
{
  if (!d->m_modelManager)
    return;

  const CursorInEditor cursor(textCursor(), textDocument()->filePath(), this, textDocument());
  auto callback = [self = QPointer(this), split = inNextSplit != alwaysOpenLinksInNextSplit()](const Link &link) {
    if (self && link.hasValidTarget())
      self->openLink(link, split);
  };
  followSymbolInterface().switchDeclDef(cursor, std::move(callback), d->m_modelManager->snapshot(), d->m_lastSemanticInfo.doc, d->m_modelManager->symbolFinder());
}

auto CppEditorWidget::findLinkAt(const QTextCursor &cursor, ProcessLinkCallback &&processLinkCallback, bool resolveTarget, bool inNextSplit) -> void
{
  if (!d->m_modelManager)
    return processLinkCallback(Utils::Link());

  const auto &filePath = textDocument()->filePath();

  // Let following a "leaf" C++ symbol take us to the designer, if we are in a generated
  // UI header.
  auto c(cursor);
  c.select(QTextCursor::WordUnderCursor);
  ProcessLinkCallback callbackWrapper = [start = c.selectionStart(), end = c.selectionEnd(), doc = QPointer(cursor.document()), callback = std::move(processLinkCallback), filePath](const Link &link) {
    const auto linkPos = doc ? Text::positionInText(doc, link.targetLine, link.targetColumn + 1) : -1;
    if (link.targetFilePath == filePath && linkPos >= start && linkPos < end) {
      const auto fileName = filePath.fileName();
      if (fileName.startsWith("ui_") && fileName.endsWith(".hpp")) {
        const QString uiFileName = fileName.mid(3, fileName.length() - 4) + "ui";
        for (const Project *const project : SessionManager::projects()) {
          const auto nodeMatcher = [uiFileName](Node *n) {
            return n->filePath().fileName() == uiFileName;
          };
          if (const Node *const uiNode = project->rootProjectNode()->findNode(nodeMatcher)) {
            EditorManager::openEditor(uiNode->filePath());
            return;
          }
        }
      }
    }
    callback(link);
  };
  followSymbolInterface().findLink(CursorInEditor{cursor, filePath, this, textDocument()}, std::move(callbackWrapper), resolveTarget, d->m_modelManager->snapshot(), d->m_lastSemanticInfo.doc, d->m_modelManager->symbolFinder(), inNextSplit);
}

auto CppEditorWidget::documentRevision() const -> unsigned
{
  return document()->revision();
}

auto CppEditorWidget::followSymbolInterface() const -> FollowSymbolInterface&
{
  return d->m_modelManager->followSymbolInterface();
}

auto CppEditorWidget::isSemanticInfoValidExceptLocalUses() const -> bool
{
  return d->m_lastSemanticInfo.doc && d->m_lastSemanticInfo.revision == documentRevision() && !d->m_lastSemanticInfo.snapshot.isEmpty();
}

auto CppEditorWidget::isSemanticInfoValid() const -> bool
{
  return isSemanticInfoValidExceptLocalUses() && d->m_lastSemanticInfo.localUsesUpdated;
}

auto CppEditorWidget::isRenaming() const -> bool
{
  return d->m_localRenaming.isActive();
}

auto CppEditorWidget::semanticInfo() const -> SemanticInfo
{
  return d->m_lastSemanticInfo;
}

auto CppEditorWidget::event(QEvent *e) -> bool
{
  switch (e->type()) {
  case QEvent::ShortcutOverride:
    // handle escape manually if a rename is active
    if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape && d->m_localRenaming.isActive()) {
      e->accept();
      return true;
    }
    break;
  default:
    break;
  }

  return TextEditorWidget::event(e);
}

auto CppEditorWidget::processKeyNormally(QKeyEvent *e) -> void
{
  TextEditorWidget::keyPressEvent(e);
}

static auto addRefactoringActions(QMenu *menu, AssistInterface *iface) -> void
{
  if (!iface || !menu)
    return;

  using Processor = QScopedPointer<IAssistProcessor>;
  using Proposal = QScopedPointer<IAssistProposal>;

  const Processor processor(CppEditorPlugin::instance()->quickFixProvider()->createProcessor(iface));
  const Proposal proposal(processor->perform(iface)); // OK, perform() takes ownership of iface.
  if (proposal) {
    auto model = proposal->model().staticCast<GenericProposalModel>();
    for (auto index = 0; index < model->size(); ++index) {
      const auto item = static_cast<AssistProposalItem*>(model->proposalItem(index));
      const auto op = item->data().value<QuickFixOperation::Ptr>();
      const QAction *action = menu->addAction(op->description());
      QObject::connect(action, &QAction::triggered, menu, [op] { op->perform(); });
    }
  }
}

class ProgressIndicatorMenuItem : public QWidgetAction {
  Q_OBJECT public:
  ProgressIndicatorMenuItem(QObject *parent) : QWidgetAction(parent) {}

protected:
  auto createWidget(QWidget *parent = nullptr) -> QWidget* override
  {
    return new Utils::ProgressIndicator(Utils::ProgressIndicatorSize::Small, parent);
  }
};

auto CppEditorWidget::createRefactorMenu(QWidget *parent) const -> QMenu*
{
  auto *menu = new QMenu(tr("&Refactor"), parent);
  menu->addAction(ActionManager::command(TextEditor::Constants::RENAME_SYMBOL)->action());

  // ### enable
  // updateSemanticInfo(m_semanticHighlighter->semanticInfo(currentSource()));

  if (isSemanticInfoValidExceptLocalUses()) {
    d->m_useSelectionsUpdater.abortSchedule();

    const auto runnerInfo = d->m_useSelectionsUpdater.update();
    switch (runnerInfo) {
    case CppUseSelectionsUpdater::RunnerInfo::AlreadyUpToDate:
      addRefactoringActions(menu, createAssistInterface(QuickFix, ExplicitlyInvoked));
      break;
    case CppUseSelectionsUpdater::RunnerInfo::Started: {
      // Update the refactor menu once we get the results.
      auto *progressIndicatorMenuItem = new ProgressIndicatorMenuItem(menu);
      menu->addAction(progressIndicatorMenuItem);

      connect(&d->m_useSelectionsUpdater, &CppUseSelectionsUpdater::finished, menu, [=](SemanticInfo::LocalUseMap, bool success) {
        QTC_CHECK(success);
        menu->removeAction(progressIndicatorMenuItem);
        addRefactoringActions(menu, createAssistInterface(QuickFix, ExplicitlyInvoked));
      });
      break;
    }
    case CppUseSelectionsUpdater::RunnerInfo::FailedToStart:
    case CppUseSelectionsUpdater::RunnerInfo::Invalid: QTC_CHECK(false && "Unexpected CppUseSelectionsUpdater runner result");
    }
  }

  return menu;
}

static auto appendCustomContextMenuActionsAndMenus(QMenu *menu, QMenu *refactorMenu) -> void
{
  auto isRefactoringMenuAdded = false;
  const QMenu *contextMenu = ActionManager::actionContainer(Constants::M_CONTEXT)->menu();
  for (auto action : contextMenu->actions()) {
    menu->addAction(action);
    if (action->objectName() == QLatin1String(Constants::M_REFACTORING_MENU_INSERTION_POINT)) {
      isRefactoringMenuAdded = true;
      menu->addMenu(refactorMenu);
    }
  }

  QTC_CHECK(isRefactoringMenuAdded);
}

auto CppEditorWidget::contextMenuEvent(QContextMenuEvent *e) -> void
{
  const QPointer<QMenu> menu(new QMenu(this));

  appendCustomContextMenuActionsAndMenus(menu, createRefactorMenu(menu));
  appendStandardContextMenuActions(menu);

  menu->exec(e->globalPos());
  if (menu)
    delete menu; // OK, menu was not already deleted by closed editor widget.
}

auto CppEditorWidget::keyPressEvent(QKeyEvent *e) -> void
{
  if (d->m_localRenaming.handleKeyPressEvent(e))
    return;

  if (handleStringSplitting(e))
    return;

  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    if (trySplitComment(this, semanticInfo().snapshot)) {
      e->accept();
      return;
    }
  }

  TextEditorWidget::keyPressEvent(e);
}

auto CppEditorWidget::handleStringSplitting(QKeyEvent *e) const -> bool
{
  if (!TextEditorSettings::completionSettings().m_autoSplitStrings)
    return false;

  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    auto cursor = textCursor();

    const Kind stringKind = CPlusPlus::MatchingText::stringKindAtCursor(cursor);
    if (stringKind >= T_FIRST_STRING_LITERAL && stringKind < T_FIRST_RAW_STRING_LITERAL) {
      cursor.beginEditBlock();
      if (cursor.positionInBlock() > 0 && cursor.block().text().at(cursor.positionInBlock() - 1) == QLatin1Char('\\')) {
        // Already escaped: simply go back to line, but do not indent.
        cursor.insertText(QLatin1String("\n"));
      } else if (e->modifiers() & Qt::ShiftModifier) {
        // With 'shift' modifier, escape the end of line character
        // and start at beginning of next line.
        cursor.insertText(QLatin1String("\\\n"));
      } else {
        // End the current string, and start a new one on the line, properly indented.
        cursor.insertText(QLatin1String("\"\n\""));
        textDocument()->autoIndent(cursor);
      }
      cursor.endEditBlock();
      e->accept();
      return true;
    }
  }

  return false;
}

auto CppEditorWidget::slotCodeStyleSettingsChanged(const QVariant &) -> void
{
  QtStyleCodeFormatter formatter;
  formatter.invalidateCache(document());
}

auto CppEditorWidget::updateSemanticInfo() -> void
{
  updateSemanticInfo(d->m_cppEditorDocument->recalculateSemanticInfo(),
                     /*updateUseSelectionSynchronously=*/ true);
}

auto CppEditorWidget::updateSemanticInfo(const SemanticInfo &semanticInfo, bool updateUseSelectionSynchronously) -> void
{
  if (semanticInfo.revision != documentRevision())
    return;

  d->m_lastSemanticInfo = semanticInfo;

  if (!d->m_localRenaming.isActive()) {
    const auto type = updateUseSelectionSynchronously ? CppUseSelectionsUpdater::CallType::Synchronous : CppUseSelectionsUpdater::CallType::Asynchronous;
    d->m_useSelectionsUpdater.update(type);
  }

  // schedule a check for a decl/def link
  updateFunctionDeclDefLink();
}

auto CppEditorWidget::createAssistInterface(AssistKind kind, AssistReason reason) const -> AssistInterface*
{
  if (kind == Completion || kind == FunctionHint) {
    const auto cap = kind == Completion ? qobject_cast<CppCompletionAssistProvider*>(cppEditorDocument()->completionAssistProvider()) : qobject_cast<CppCompletionAssistProvider*>(cppEditorDocument()->functionHintAssistProvider());
    if (cap) {
      LanguageFeatures features = LanguageFeatures::defaultFeatures();
      if (Document::Ptr doc = d->m_lastSemanticInfo.doc)
        features = doc->languageFeatures();
      features.objCEnabled |= cppEditorDocument()->isObjCEnabled();
      return cap->createAssistInterface(textDocument()->filePath(), this, features, position(), reason);
    } else {
      return TextEditorWidget::createAssistInterface(kind, reason);
    }
  } else if (kind == QuickFix) {
    if (isSemanticInfoValid())
      return new CppQuickFixInterface(const_cast<CppEditorWidget*>(this), reason);
  } else {
    return TextEditorWidget::createAssistInterface(kind, reason);
  }
  return nullptr;
}

auto CppEditorWidget::declDefLink() const -> QSharedPointer<FunctionDeclDefLink>
{
  return d->m_declDefLink;
}

auto CppEditorWidget::updateFunctionDeclDefLink() -> void
{
  const auto pos = textCursor().selectionStart();

  // if there's already a link, abort it if the cursor is outside or the name changed
  // (adding a prefix is an exception since the user might type a return type)
  if (d->m_declDefLink && (pos < d->m_declDefLink->linkSelection.selectionStart() || pos > d->m_declDefLink->linkSelection.selectionEnd() || !d->m_declDefLink->nameSelection.selectedText().trimmed().endsWith(d->m_declDefLink->nameInitial))) {
    abortDeclDefLink();
    return;
  }

  // don't start a new scan if there's one active and the cursor is already in the scanned area
  const auto scannedSelection = d->m_declDefLinkFinder->scannedSelection();
  if (!scannedSelection.isNull() && scannedSelection.selectionStart() <= pos && scannedSelection.selectionEnd() >= pos) {
    return;
  }

  d->m_updateFunctionDeclDefLinkTimer.start();
}

auto CppEditorWidget::updateFunctionDeclDefLinkNow() -> void
{
  auto editor = EditorManager::currentEditor();
  if (!editor || editor->widget() != this)
    return;

  const Snapshot semanticSnapshot = d->m_lastSemanticInfo.snapshot;
  const Document::Ptr semanticDoc = d->m_lastSemanticInfo.doc;

  if (d->m_declDefLink) {
    // update the change marker
    const Utils::ChangeSet changes = d->m_declDefLink->changes(semanticSnapshot);
    if (changes.isEmpty())
      d->m_declDefLink->hideMarker(this);
    else
      d->m_declDefLink->showMarker(this);
    return;
  }

  if (!isSemanticInfoValidExceptLocalUses())
    return;

  auto snapshot = d->m_modelManager->snapshot();
  snapshot.insert(semanticDoc);

  d->m_declDefLinkFinder->startFindLinkAt(textCursor(), semanticDoc, snapshot);
}

auto CppEditorWidget::onFunctionDeclDefLinkFound(QSharedPointer<FunctionDeclDefLink> link) -> void
{
  abortDeclDefLink();
  d->m_declDefLink = link;
  auto targetDocument = DocumentModel::documentForFilePath(d->m_declDefLink->targetFile->filePath());
  if (textDocument() != targetDocument) {
    if (auto textDocument = qobject_cast<BaseTextDocument*>(targetDocument))
      connect(textDocument, &IDocument::contentsChanged, this, &CppEditorWidget::abortDeclDefLink);
  }
}

auto CppEditorWidget::applyDeclDefLinkChanges(bool jumpToMatch) -> void
{
  if (!d->m_declDefLink)
    return;
  d->m_declDefLink->apply(this, jumpToMatch);
  abortDeclDefLink();
  updateFunctionDeclDefLink();
}

auto CppEditorWidget::encourageApply() -> void
{
  if (d->m_localRenaming.encourageApply())
    return;

  TextEditorWidget::encourageApply();
}

auto CppEditorWidget::abortDeclDefLink() -> void
{
  if (!d->m_declDefLink)
    return;

  auto targetDocument = DocumentModel::documentForFilePath(d->m_declDefLink->targetFile->filePath());
  if (textDocument() != targetDocument) {
    if (auto textDocument = qobject_cast<BaseTextDocument*>(targetDocument))
      disconnect(textDocument, &IDocument::contentsChanged, this, &CppEditorWidget::abortDeclDefLink);
  }

  d->m_declDefLink->hideMarker(this);
  d->m_declDefLink.clear();
}

auto CppEditorWidget::showPreProcessorWidget() -> void
{
  const auto filePath = textDocument()->filePath().toString();

  CppPreProcessorDialog dialog(filePath, this);
  if (dialog.exec() == QDialog::Accepted) {
    const auto extraDirectives = dialog.extraPreprocessorDirectives().toUtf8();
    cppEditorDocument()->setExtraPreprocessorDirectives(extraDirectives);
    cppEditorDocument()->scheduleProcessDocument();
  }
}

auto CppEditorWidget::invokeTextEditorWidgetAssist(TextEditor::AssistKind assistKind, TextEditor::IAssistProvider *provider) -> void
{
  invokeAssist(assistKind, provider);
}

auto CppEditorWidget::unselectLeadingWhitespace(const QList<QTextEdit::ExtraSelection> &selections) -> const QList<QTextEdit::ExtraSelection>
{
  QList<QTextEdit::ExtraSelection> filtered;
  for (const auto &sel : selections) {
    QList<QTextEdit::ExtraSelection> splitSelections;
    auto firstNonWhitespacePos = -1;
    auto lastNonWhitespacePos = -1;
    auto split = false;
    const auto firstBlock = sel.cursor.document()->findBlock(sel.cursor.selectionStart());
    auto inIndentation = firstBlock.position() == sel.cursor.selectionStart();
    const auto createSplitSelection = [&] {
      QTextEdit::ExtraSelection newSelection;
      newSelection.cursor = QTextCursor(sel.cursor.document());
      newSelection.cursor.setPosition(firstNonWhitespacePos);
      newSelection.cursor.setPosition(lastNonWhitespacePos + 1, QTextCursor::KeepAnchor);
      newSelection.format = sel.format;
      splitSelections << newSelection;
    };
    for (auto i = sel.cursor.selectionStart(); i < sel.cursor.selectionEnd(); ++i) {
      const auto curChar = sel.cursor.document()->characterAt(i);
      if (!curChar.isSpace()) {
        if (firstNonWhitespacePos == -1)
          firstNonWhitespacePos = i;
        lastNonWhitespacePos = i;
      }
      if (!inIndentation) {
        if (curChar == QChar::ParagraphSeparator)
          inIndentation = true;
        continue;
      }
      if (curChar == QChar::ParagraphSeparator)
        continue;
      if (curChar.isSpace()) {
        if (firstNonWhitespacePos != -1) {
          createSplitSelection();
          firstNonWhitespacePos = -1;
          lastNonWhitespacePos = -1;
        }
        split = true;
        continue;
      }
      inIndentation = false;
    }

    if (!split) {
      filtered << sel;
      continue;
    }

    if (firstNonWhitespacePos != -1)
      createSplitSelection();
    filtered << splitSelections;
  }
  return filtered;
}

auto CppEditorWidget::isInTestMode() const -> bool { return d->inTestMode; }

#ifdef WITH_TESTS
void CppEditorWidget::enableTestMode() { d->inTestMode = true; }
#endif

} // namespace CppEditor

#include "cppeditorwidget.moc"
