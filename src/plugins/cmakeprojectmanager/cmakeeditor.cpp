// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeeditor.hpp"

#include "cmakefilecompletionassist.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeindenter.hpp"
#include "cmakeautocompleter.hpp"

#include <core/core-action-container.hpp>
#include <core/core-action-manager.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditoractionhandler.hpp>

#include <QDir>
#include <QTextDocument>

#include <functional>

using namespace Orca::Plugin::Core;
using namespace TextEditor;

namespace CMakeProjectManager {
namespace Internal {

//
// CMakeEditor
//

auto CMakeEditor::contextHelp(const HelpCallback &callback) const -> void
{
  auto pos = position();

  QChar chr;
  do {
    --pos;
    if (pos < 0)
      break;
    chr = characterAt(pos);
    if (chr == QLatin1Char('(')) {
      BaseTextEditor::contextHelp(callback);
      return;
    }
  } while (chr.unicode() != QChar::ParagraphSeparator);

  ++pos;
  chr = characterAt(pos);
  while (chr.isSpace()) {
    ++pos;
    chr = characterAt(pos);
  }
  auto begin = pos;

  do {
    ++pos;
    chr = characterAt(pos);
  } while (chr.isLetterOrNumber() || chr == QLatin1Char('_'));
  auto end = pos;

  while (chr.isSpace()) {
    ++pos;
    chr = characterAt(pos);
  }

  // Not a command
  if (chr != QLatin1Char('(')) {
    BaseTextEditor::contextHelp(callback);
    return;
  }

  const QString id = "command/" + textAt(begin, end - begin).toLower();
  callback({{id, Utils::Text::wordUnderCursor(editorWidget()->textCursor())}, {}, HelpItem::Unknown});
}

//
// CMakeEditorWidget
//

class CMakeEditorWidget final : public TextEditorWidget {
public:
  ~CMakeEditorWidget() final = default;

private:
  auto save(const QString &fileName = QString()) -> bool;
  auto findLinkAt(const QTextCursor &cursor, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget = true, bool inNextSplit = false) -> void override;
  auto contextMenuEvent(QContextMenuEvent *e) -> void override;
};

auto CMakeEditorWidget::contextMenuEvent(QContextMenuEvent *e) -> void
{
  showDefaultContextMenu(e, Constants::M_CONTEXT);
}

static auto mustBeQuotedInFileName(const QChar &c) -> bool
{
  return c.isSpace() || c == '"' || c == '(' || c == ')';
}

static auto isValidFileNameChar(const QString &block, int pos) -> bool
{
  const auto c = block.at(pos);
  return !mustBeQuotedInFileName(c) || (pos > 0 && block.at(pos - 1) == '\\');
}

static auto unescape(const QString &s) -> QString
{
  QString result;
  auto i = 0;
  const int size = s.size();
  while (i < size) {
    const auto c = s.at(i);
    if (c == '\\' && i < size - 1) {
      const auto nc = s.at(i + 1);
      if (mustBeQuotedInFileName(nc)) {
        result += nc;
        i += 2;
        continue;
      }
    }
    result += c;
    ++i;
  }
  return result;
}

auto CMakeEditorWidget::findLinkAt(const QTextCursor &cursor, Utils::ProcessLinkCallback &&processLinkCallback, bool/* resolveTarget*/, bool /*inNextSplit*/) -> void
{
  Utils::Link link;

  auto line = 0;
  auto column = 0;
  convertPosition(cursor.position(), &line, &column);
  const auto positionInBlock = column - 1;

  const auto block = cursor.block().text();

  // check if the current position is commented out
  const int hashPos = block.indexOf(QLatin1Char('#'));
  if (hashPos >= 0 && hashPos < positionInBlock)
    return processLinkCallback(link);

  // find the beginning of a filename
  QString buffer;
  auto beginPos = positionInBlock - 1;
  while (beginPos >= 0) {
    if (isValidFileNameChar(block, beginPos)) {
      buffer.prepend(block.at(beginPos));
      beginPos--;
    } else {
      break;
    }
  }

  // find the end of a filename
  auto endPos = positionInBlock;
  while (endPos < block.count()) {
    if (isValidFileNameChar(block, endPos)) {
      buffer.append(block.at(endPos));
      endPos++;
    } else {
      break;
    }
  }

  if (buffer.isEmpty())
    return processLinkCallback(link);

  QDir dir(textDocument()->filePath().toFileInfo().absolutePath());
  buffer.replace("${CMAKE_CURRENT_SOURCE_DIR}", dir.path());
  buffer.replace("${CMAKE_CURRENT_LIST_DIR}", dir.path());
  // TODO: Resolve more variables

  auto fileName = dir.filePath(unescape(buffer));
  QFileInfo fi(fileName);
  if (fi.exists()) {
    if (fi.isDir()) {
      QDir subDir(fi.absoluteFilePath());
      auto subProject = subDir.filePath(QLatin1String("CMakeLists.txt"));
      if (QFileInfo::exists(subProject))
        fileName = subProject;
      else
        return processLinkCallback(link);
    }
    link.targetFilePath = Utils::FilePath::fromString(fileName);
    link.linkTextStart = cursor.position() - positionInBlock + beginPos + 1;
    link.linkTextEnd = cursor.position() - positionInBlock + endPos;
  }
  processLinkCallback(link);
}

static auto createCMakeDocument() -> TextDocument*
{
  auto doc = new TextDocument;
  doc->setId(Constants::CMAKE_EDITOR_ID);
  doc->setMimeType(QLatin1String(Constants::CMAKE_MIMETYPE));
  return doc;
}

//
// CMakeEditorFactory
//

CMakeEditorFactory::CMakeEditorFactory()
{
  setId(Constants::CMAKE_EDITOR_ID);
  setDisplayName(QCoreApplication::translate("OpenWith::Editors", "CMake Editor"));
  addMimeType(Constants::CMAKE_MIMETYPE);
  addMimeType(Constants::CMAKE_PROJECT_MIMETYPE);

  setEditorCreator([]() { return new CMakeEditor; });
  setEditorWidgetCreator([]() { return new CMakeEditorWidget; });
  setDocumentCreator(createCMakeDocument);
  setIndenterCreator([](QTextDocument *doc) { return new CMakeIndenter(doc); });
  setUseGenericHighlighter(true);
  setCommentDefinition(Utils::CommentDefinition::HashStyle);
  setCodeFoldingSupported(true);

  setCompletionAssistProvider(new CMakeFileCompletionAssistProvider);
  setAutoCompleterCreator([]() { return new CMakeAutoCompleter; });

  setEditorActionHandlers(TextEditorActionHandler::UnCommentSelection | TextEditorActionHandler::JumpToFileUnderCursor | TextEditorActionHandler::Format);

  auto contextMenu = ActionManager::createMenu(Constants::M_CONTEXT);
  contextMenu->addAction(ActionManager::command(TextEditor::Constants::JUMP_TO_FILE_UNDER_CURSOR));
  contextMenu->addSeparator(Context(Constants::CMAKE_EDITOR_ID));
  contextMenu->addAction(ActionManager::command(TextEditor::Constants::UN_COMMENT_SELECTION));
}

} // namespace Internal
} // namespace CMakeProjectManager
