// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "texteditorplugin.hpp"

#include "findincurrentfile.hpp"
#include "findinfiles.hpp"
#include "findinopenfiles.hpp"
#include "fontsettings.hpp"
#include "highlighter.hpp"
#include "linenumberfilter.hpp"
#include "outlinefactory.hpp"
#include "plaintexteditorfactory.hpp"
#include "textdocument.hpp"
#include "texteditor.hpp"
#include "texteditorsettings.hpp"

#include <texteditor/snippets/snippetprovider.hpp>
#include <texteditor/icodestylepreferences.hpp>
#include <texteditor/tabsettings.hpp>

#include <core/core-action-container.hpp>
#include <core/core-action-manager.hpp>
#include <core/core-command.hpp>
#include <core/core-diff-service.hpp>
#include <core/core-external-tool-manager.hpp>
#include <core/core-folder-navigation-widget.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <utils/fancylineedit.hpp>
#include <utils/qtcassert.hpp>
#include <utils/macroexpander.hpp>

#include <QAction>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace TextEditor {
namespace Internal {

static constexpr char kCurrentDocumentSelection[] = "CurrentDocument:Selection";
static constexpr char kCurrentDocumentRow[] = "CurrentDocument:Row";
static constexpr char kCurrentDocumentColumn[] = "CurrentDocument:Column";
static constexpr char kCurrentDocumentRowCount[] = "CurrentDocument:RowCount";
static constexpr char kCurrentDocumentColumnCount[] = "CurrentDocument:ColumnCount";
static constexpr char kCurrentDocumentFontSize[] = "CurrentDocument:FontSize";
static constexpr char kCurrentDocumentWordUnderCursor[] = "CurrentDocument:WordUnderCursor";

class TextEditorPluginPrivate : public QObject {
public:
  auto extensionsInitialized() -> void;
  auto updateSearchResultsFont(const FontSettings &) -> void;
  auto updateSearchResultsTabWidth(const TabSettings &tabSettings) -> void;
  auto updateCurrentSelection(const QString &text) -> void;

  auto createStandardContextMenu() -> void;

  TextEditorSettings settings;
  LineNumberFilter lineNumberFilter; // Goto line functionality for quick open
  OutlineFactory outlineFactory;

  FindInFiles findInFilesFilter;
  FindInCurrentFile findInCurrentFileFilter;
  FindInOpenFiles findInOpenFilesFilter;

  PlainTextEditorFactory plainTextEditorFactory;
};

static TextEditorPlugin *m_instance = nullptr;

TextEditorPlugin::TextEditorPlugin()
{
  QTC_ASSERT(!m_instance, return);
  m_instance = this;
}

TextEditorPlugin::~TextEditorPlugin()
{
  delete d;
  d = nullptr;
  m_instance = nullptr;
}

auto TextEditorPlugin::instance() -> TextEditorPlugin*
{
  return m_instance;
}

auto TextEditorPlugin::initialize(const QStringList &arguments, QString *errorMessage) -> bool
{
  Q_UNUSED(arguments)
  Q_UNUSED(errorMessage)

  d = new TextEditorPluginPrivate;

  Context context(Constants::C_TEXTEDITOR);

  // Add shortcut for invoking automatic completion
  auto completionAction = new QAction(tr("Trigger Completion"), this);
  Command *command = ActionManager::registerAction(completionAction, Constants::COMPLETE_THIS, context);
  command->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Space") : tr("Ctrl+Space")));
  connect(completionAction, &QAction::triggered, []() {
    if (const auto editor = BaseTextEditor::currentTextEditor())
      editor->editorWidget()->invokeAssist(Completion);
  });
  connect(command, &Command::keySequenceChanged, [command] {
    FancyLineEdit::setCompletionShortcut(command->keySequence());
  });
  FancyLineEdit::setCompletionShortcut(command->keySequence());

  // Add shortcut for invoking function hint completion
  auto functionHintAction = new QAction(tr("Display Function Hint"), this);
  command = ActionManager::registerAction(functionHintAction, Constants::FUNCTION_HINT, context);
  command->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Shift+D") : tr("Ctrl+Shift+D")));
  connect(functionHintAction, &QAction::triggered, []() {
    if (const auto editor = BaseTextEditor::currentTextEditor())
      editor->editorWidget()->invokeAssist(FunctionHint);
  });

  // Add shortcut for invoking quick fix options
  auto quickFixAction = new QAction(tr("Trigger Refactoring Action"), this);
  Command *quickFixCommand = ActionManager::registerAction(quickFixAction, Constants::QUICKFIX_THIS, context);
  quickFixCommand->setDefaultKeySequence(QKeySequence(tr("Alt+Return")));
  connect(quickFixAction, &QAction::triggered, []() {
    if (const auto editor = BaseTextEditor::currentTextEditor())
      editor->editorWidget()->invokeAssist(QuickFix);
  });

  auto showContextMenuAction = new QAction(tr("Show Context Menu"), this);
  ActionManager::registerAction(showContextMenuAction, Constants::SHOWCONTEXTMENU, context);
  connect(showContextMenuAction, &QAction::triggered, []() {
    if (const auto editor = BaseTextEditor::currentTextEditor())
      editor->editorWidget()->showContextMenu();
  });

  // Add text snippet provider.
  SnippetProvider::registerGroup(Constants::TEXT_SNIPPET_GROUP_ID, tr("Text", "SnippetProvider"));

  d->createStandardContextMenu();

  return true;
}

auto TextEditorPluginPrivate::extensionsInitialized() -> void
{
  connect(FolderNavigationWidgetFactory::instance(), &FolderNavigationWidgetFactory::aboutToShowContextMenu, this, [](QMenu *menu, const FilePath &filePath, bool isDir) {
    if (!isDir && DiffService::instance()) {
      menu->addAction(TextDocument::createDiffAgainstCurrentFileAction(menu, [filePath]() { return filePath; }));
    }
  });

  connect(&settings, &TextEditorSettings::fontSettingsChanged, this, &TextEditorPluginPrivate::updateSearchResultsFont);

  updateSearchResultsFont(TextEditorSettings::fontSettings());

  connect(TextEditorSettings::codeStyle(), &ICodeStylePreferences::currentTabSettingsChanged, this, &TextEditorPluginPrivate::updateSearchResultsTabWidth);

  updateSearchResultsTabWidth(TextEditorSettings::codeStyle()->currentTabSettings());

  connect(ExternalToolManager::instance(), &ExternalToolManager::replaceSelectionRequested, this, &TextEditorPluginPrivate::updateCurrentSelection);
}

auto TextEditorPlugin::extensionsInitialized() -> void
{
  d->extensionsInitialized();

  const auto expander = globalMacroExpander();

  expander->registerVariable(kCurrentDocumentSelection, tr("Selected text within the current document."), []() -> QString {
    QString value;
    if (const auto editor = BaseTextEditor::currentTextEditor()) {
      value = editor->selectedText();
      value.replace(QChar::ParagraphSeparator, QLatin1String("\n"));
    }
    return value;
  });

  expander->registerIntVariable(kCurrentDocumentRow, tr("Line number of the text cursor position in current document (starts with 1)."), []() -> int {
    const auto editor = BaseTextEditor::currentTextEditor();
    return editor ? editor->currentLine() : 0;
  });

  expander->registerIntVariable(kCurrentDocumentColumn, tr("Column number of the text cursor position in current document (starts with 0)."), []() -> int {
    const auto editor = BaseTextEditor::currentTextEditor();
    return editor ? editor->currentColumn() : 0;
  });

  expander->registerIntVariable(kCurrentDocumentRowCount, tr("Number of lines visible in current document."), []() -> int {
    const auto editor = BaseTextEditor::currentTextEditor();
    return editor ? editor->rowCount() : 0;
  });

  expander->registerIntVariable(kCurrentDocumentColumnCount, tr("Number of columns visible in current document."), []() -> int {
    const auto editor = BaseTextEditor::currentTextEditor();
    return editor ? editor->columnCount() : 0;
  });

  expander->registerIntVariable(kCurrentDocumentFontSize, tr("Current document's font size in points."), []() -> int {
    auto editor = BaseTextEditor::currentTextEditor();
    return editor ? editor->widget()->font().pointSize() : 0;
  });

  expander->registerVariable(kCurrentDocumentWordUnderCursor, tr("Word under the current document's text cursor."), []() {
    const auto editor = BaseTextEditor::currentTextEditor();
    if (!editor)
      return QString();
    return Text::wordUnderCursor(editor->editorWidget()->textCursor());
  });
}

auto TextEditorPlugin::lineNumberFilter() -> LineNumberFilter*
{
  return &m_instance->d->lineNumberFilter;
}

auto TextEditorPlugin::aboutToShutdown() -> ShutdownFlag
{
  Highlighter::handleShutdown();
  return SynchronousShutdown;
}

auto TextEditorPluginPrivate::updateSearchResultsFont(const FontSettings &settings) -> void
{
  if (auto window = SearchResultWindow::instance()) {
    const auto textFormat = settings.formatFor(C_TEXT);
    const auto defaultResultFormat = settings.formatFor(C_SEARCH_RESULT);
    const auto alt1ResultFormat = settings.formatFor(C_SEARCH_RESULT_ALT1);
    const auto alt2ResultFormat = settings.formatFor(C_SEARCH_RESULT_ALT2);
    window->setTextEditorFont(QFont(settings.family(), settings.fontSize() * settings.fontZoom() / 100), {std::make_pair(SearchResultColor::Style::Default, SearchResultColor(textFormat.background(), textFormat.foreground(), defaultResultFormat.background(), defaultResultFormat.foreground())), std::make_pair(SearchResultColor::Style::Alt1, SearchResultColor(textFormat.background(), textFormat.foreground(), alt1ResultFormat.background(), alt1ResultFormat.foreground())), std::make_pair(SearchResultColor::Style::Alt2, SearchResultColor(textFormat.background(), textFormat.foreground(), alt2ResultFormat.background(), alt2ResultFormat.foreground()))});
  }
}

auto TextEditorPluginPrivate::updateSearchResultsTabWidth(const TabSettings &tabSettings) -> void
{
  if (auto window = SearchResultWindow::instance())
    window->setTabWidth(tabSettings.m_tabSize);
}

auto TextEditorPluginPrivate::updateCurrentSelection(const QString &text) -> void
{
  if (const auto editor = BaseTextEditor::currentTextEditor()) {
    const auto pos = editor->position();
    auto anchor = editor->position(AnchorPosition);
    if (anchor < 0) // no selection
      anchor = pos;
    auto selectionLength = pos - anchor;
    const auto selectionInTextDirection = selectionLength >= 0;
    if (!selectionInTextDirection)
      selectionLength = -selectionLength;
    const auto start = qMin(pos, anchor);
    editor->setCursorPosition(start);
    editor->replace(selectionLength, text);
    const auto replacementEnd = editor->position();
    editor->setCursorPosition(selectionInTextDirection ? start : replacementEnd);
    editor->select(selectionInTextDirection ? replacementEnd : start);
  }
}

auto TextEditorPluginPrivate::createStandardContextMenu() -> void
{
  ActionContainer *contextMenu = ActionManager::createMenu(Constants::M_STANDARDCONTEXTMENU);
  contextMenu->appendGroup(Constants::G_UNDOREDO);
  contextMenu->appendGroup(Constants::G_COPYPASTE);
  contextMenu->appendGroup(Constants::G_SELECT);
  contextMenu->appendGroup(Constants::G_BOM);

  const auto add = [contextMenu](const Id &id, const Id &group) {
    Command *cmd = ActionManager::command(id);
    if (cmd)
      contextMenu->addAction(cmd, group);
  };

  add(Orca::Plugin::Core::UNDO, Constants::G_UNDOREDO);
  add(Orca::Plugin::Core::REDO, Constants::G_UNDOREDO);
  contextMenu->addSeparator(Constants::G_COPYPASTE);
  add(Orca::Plugin::Core::CUT, Constants::G_COPYPASTE);
  add(Orca::Plugin::Core::COPY, Constants::G_COPYPASTE);
  add(Orca::Plugin::Core::PASTE, Constants::G_COPYPASTE);
  add(Constants::CIRCULAR_PASTE, Constants::G_COPYPASTE);
  contextMenu->addSeparator(Constants::G_SELECT);
  add(Orca::Plugin::Core::SELECTALL, Constants::G_SELECT);
  contextMenu->addSeparator(Constants::G_BOM);
  add(Constants::SWITCH_UTF8BOM, Constants::G_BOM);
}

} // namespace Internal
} // namespace TextEditor
