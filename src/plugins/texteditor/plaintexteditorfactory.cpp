// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "plaintexteditorfactory.hpp"
#include "basehoverhandler.hpp"
#include "textdocument.hpp"
#include "texteditor.hpp"
#include "texteditoractionhandler.hpp"
#include "texteditorconstants.hpp"

#include <core/core-constants.hpp>

#include <utils/infobar.hpp>
#include <utils/qtcassert.hpp>

#include <QCoreApplication>

namespace TextEditor {

static PlainTextEditorFactory *m_instance = nullptr;

class PlainTextEditorWidget : public TextEditorWidget {
public:
  PlainTextEditorWidget() = default;

  auto finalizeInitialization() -> void override
  {
    textDocument()->setMimeType(QLatin1String(Constants::C_TEXTEDITOR_MIMETYPE_TEXT));
  }
};

PlainTextEditorFactory::PlainTextEditorFactory()
{
  QTC_CHECK(!m_instance);
  m_instance = this;
  setId(Orca::Plugin::Core::K_DEFAULT_TEXT_EDITOR_ID);
  setDisplayName(QCoreApplication::translate("OpenWith::Editors", Orca::Plugin::Core::K_DEFAULT_TEXT_EDITOR_DISPLAY_NAME));
  addMimeType(QLatin1String(Constants::C_TEXTEDITOR_MIMETYPE_TEXT));
  addMimeType(QLatin1String("text/css")); // for some reason freedesktop thinks css is text/x-csrc
  addHoverHandler(new BaseHoverHandler);

  setDocumentCreator([]() { return new TextDocument(Orca::Plugin::Core::K_DEFAULT_TEXT_EDITOR_ID); });
  setEditorWidgetCreator([]() { return new PlainTextEditorWidget; });
  setUseGenericHighlighter(true);

  setEditorActionHandlers(TextEditorActionHandler::Format | TextEditorActionHandler::UnCommentSelection | TextEditorActionHandler::UnCollapseAll | TextEditorActionHandler::FollowSymbolUnderCursor);
}

auto PlainTextEditorFactory::instance() -> PlainTextEditorFactory*
{
  return m_instance;
}

auto PlainTextEditorFactory::createPlainTextEditor() -> BaseTextEditor*
{
  return qobject_cast<BaseTextEditor*>(m_instance->createEditor());
}

} // namespace TextEditor
