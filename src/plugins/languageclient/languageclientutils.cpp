// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientutils.hpp"

#include "client.hpp"
#include "languageclient_global.hpp"
#include "languageclientmanager.hpp"
#include "languageclientoutline.hpp"
#include "snippet.hpp"

#include <core/editormanager/documentmodel.hpp>
#include <core/icore.hpp>

#include <texteditor/codeassist/textdocumentmanipulatorinterface.hpp>
#include <texteditor/refactoringchanges.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/textutils.hpp>
#include <utils/treeviewcombobox.hpp>
#include <utils/utilsicons.hpp>

#include <QActionGroup>
#include <QFile>
#include <QMenu>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>

using namespace LanguageServerProtocol;
using namespace Utils;
using namespace TextEditor;

namespace LanguageClient {

auto rangeToTextCursor(const Range &range, QTextDocument *doc) -> QTextCursor
{
  QTextCursor cursor(doc);
  cursor.setPosition(range.end().toPositionInDocument(doc));
  cursor.setPosition(range.start().toPositionInDocument(doc), QTextCursor::KeepAnchor);
  return cursor;
}

auto convertRange(const QTextDocument *doc, const Range &range) -> ChangeSet::Range
{
  return ChangeSet::Range(Text::positionInText(doc, range.start().line() + 1, range.start().character() + 1), Text::positionInText(doc, range.end().line() + 1, range.end().character()) + 1);
}

auto editsToChangeSet(const QList<TextEdit> &edits, const QTextDocument *doc) -> ChangeSet
{
  ChangeSet changeSet;
  for (const auto &edit : edits)
    changeSet.replace(convertRange(doc, edit.range()), edit.newText());
  return changeSet;
}

auto applyTextDocumentEdit(const Client *client, const TextDocumentEdit &edit) -> bool
{
  const auto &edits = edit.edits();
  if (edits.isEmpty())
    return true;
  const auto &uri = edit.textDocument().uri();
  const auto &filePath = uri.toFilePath();
  const auto version = edit.textDocument().version();
  if (!version.isNull() && version.value(0) < client->documentVersion(filePath))
    return false;
  return applyTextEdits(uri, edits);
}

auto applyTextEdits(const DocumentUri &uri, const QList<TextEdit> &edits) -> bool
{
  if (edits.isEmpty())
    return true;
  const RefactoringChanges changes;
  RefactoringFilePtr file;
  file = changes.file(uri.toFilePath());
  file->setChangeSet(editsToChangeSet(edits, file->document()));
  return file->apply();
}

auto applyTextEdit(TextDocumentManipulatorInterface &manipulator, const TextEdit &edit, bool newTextIsSnippet) -> void
{
  using namespace Utils::Text;
  const auto range = edit.range();
  const QTextDocument *doc = manipulator.textCursorAt(manipulator.currentPosition()).document();
  const auto start = positionInText(doc, range.start().line() + 1, range.start().character() + 1);
  const auto end = positionInText(doc, range.end().line() + 1, range.end().character() + 1);
  if (newTextIsSnippet) {
    manipulator.replace(start, end - start, {});
    manipulator.insertCodeSnippet(start, edit.newText(), &parseSnippet);
  } else {
    manipulator.replace(start, end - start, edit.newText());
  }
}

auto applyWorkspaceEdit(const Client *client, const WorkspaceEdit &edit) -> bool
{
  auto result = true;
  const auto &documentChanges = edit.documentChanges().value_or(QList<TextDocumentEdit>());
  if (!documentChanges.isEmpty()) {
    for (const auto &documentChange : documentChanges)
      result |= applyTextDocumentEdit(client, documentChange);
  } else {
    const auto &changes = edit.changes().value_or(WorkspaceEdit::Changes());
    for (auto it = changes.cbegin(); it != changes.cend(); ++it)
      result |= applyTextEdits(it.key(), it.value());
    return result;
  }
  return result;
}

auto endOfLineCursor(const QTextCursor &cursor) -> QTextCursor
{
  auto ret = cursor;
  ret.movePosition(QTextCursor::EndOfLine);
  return ret;
}

auto updateCodeActionRefactoringMarker(Client *client, const CodeAction &action, const DocumentUri &uri) -> void
{
  const auto doc = TextDocument::textDocumentForFilePath(uri.toFilePath());
  if (!doc)
    return;
  const auto editors = BaseTextEditor::textEditorsForDocument(doc);
  if (editors.isEmpty())
    return;

  const auto &diagnostics = action.diagnostics().value_or(QList<Diagnostic>());

  RefactorMarkers markers;
  RefactorMarker marker;
  marker.type = client->id();
  if (action.isValid())
    marker.tooltip = action.title();
  if (action.edit().has_value()) {
    auto edit = action.edit().value();
    marker.callback = [client, edit](const TextEditorWidget *) {
      applyWorkspaceEdit(client, edit);
    };
    if (diagnostics.isEmpty()) {
      QList<TextEdit> edits;
      if (const auto documentChanges = edit.documentChanges()) {
        auto changesForUri = Utils::filtered(documentChanges.value(), [uri](const TextDocumentEdit &edit) {
          return edit.textDocument().uri() == uri;
        });
        for (const auto &edit : changesForUri)
          edits << edit.edits();
      } else if (auto localChanges = edit.changes()) {
        edits = localChanges.value()[uri];
      }
      for (const auto &edit : qAsConst(edits)) {
        marker.cursor = endOfLineCursor(edit.range().start().toTextCursor(doc->document()));
        markers << marker;
      }
    }
  } else if (action.command().has_value()) {
    const auto command = action.command().value();
    marker.callback = [command, client = QPointer<Client>(client)](const TextEditorWidget *) {
      if (client)
        client->executeCommand(command);
    };
  } else {
    return;
  }
  for (const auto &diagnostic : diagnostics) {
    marker.cursor = endOfLineCursor(diagnostic.range().start().toTextCursor(doc->document()));
    markers << marker;
  }
  for (const auto editor : editors) {
    if (const auto editorWidget = editor->editorWidget())
      editorWidget->setRefactorMarkers(markers + editorWidget->refactorMarkers());
  }
}

static const char clientExtrasName[] = "__qtcreator_client_extras__";

class ClientExtras : public QObject {
public:
  ClientExtras(QObject *parent) : QObject(parent) { setObjectName(clientExtrasName); };

  QPointer<QAction> m_popupAction;
  QPointer<Client> m_client;
  QPointer<QAction> m_outlineAction;
};

auto updateEditorToolBar(Core::IEditor *editor) -> void
{
  const auto *textEditor = qobject_cast<BaseTextEditor*>(editor);
  if (!textEditor)
    return;
  const auto widget = textEditor->editorWidget();
  if (!widget)
    return;

  const auto document = textEditor->textDocument();
  const auto client = LanguageClientManager::clientForDocument(textEditor->textDocument());

  auto extras = widget->findChild<ClientExtras*>(clientExtrasName, Qt::FindDirectChildrenOnly);
  if (!extras) {
    if (!client)
      return;
    extras = new ClientExtras(widget);
  }
  if (extras->m_popupAction) {
    if (client) {
      extras->m_popupAction->setText(client->name());
    } else {
      widget->toolBar()->removeAction(extras->m_popupAction);
      delete extras->m_popupAction;
    }
  } else if (client) {
    const auto icon = Utils::Icon({{":/languageclient/images/languageclient.png", Utils::Theme::IconsBaseColor}}).icon();
    extras->m_popupAction = widget->toolBar()->addAction(icon, client->name(), [document = QPointer(document)] {
      const auto menu = new QMenu;
      const auto clientsGroup = new QActionGroup(menu);
      clientsGroup->setExclusive(true);
      for (const auto client : LanguageClientManager::clientsSupportingDocument(document)) {
        auto action = clientsGroup->addAction(client->name());
        auto reopen = [action, client = QPointer(client), document] {
          if (!client)
            return;
          LanguageClientManager::openDocumentWithClient(document, client);
          action->setChecked(true);
        };
        action->setCheckable(true);
        action->setChecked(client == LanguageClientManager::clientForDocument(document));
        QObject::connect(action, &QAction::triggered, reopen);
      }
      menu->addActions(clientsGroup->actions());
      if (!clientsGroup->actions().isEmpty())
        menu->addSeparator();
      menu->addAction("Inspect Language Clients", [] {
        LanguageClientManager::showInspector();
      });
      menu->addAction("Manage...", [] {
        Core::ICore::showOptionsDialog(Constants::LANGUAGECLIENT_SETTINGS_PAGE);
      });
      menu->popup(QCursor::pos());
    });
  }

  if (!extras->m_client || extras->m_client != client || !LanguageClientOutlineWidgetFactory::clientSupportsDocumentSymbols(client, document)) {
    if (extras->m_outlineAction) {
      widget->toolBar()->removeAction(extras->m_outlineAction);
      delete extras->m_outlineAction;
    }
    extras->m_client.clear();
  }

  if (!extras->m_client) {
    if (QWidget *comboBox = LanguageClientOutlineWidgetFactory::createComboBox(client, editor)) {
      extras->m_client = client;
      extras->m_outlineAction = widget->insertExtraToolBarWidget(TextEditorWidget::Left, comboBox);
    }
  }
}

auto symbolIcon(int type) -> const QIcon
{
  using namespace Utils::CodeModelIcon;
  static QMap<SymbolKind, QIcon> icons;
  if (type < int(SymbolKind::FirstSymbolKind) || type > int(SymbolKind::LastSymbolKind))
    return {};
  const auto kind = static_cast<SymbolKind>(type);
  if (!icons.contains(kind)) {
    switch (kind) {
    case SymbolKind::File:
      icons[kind] = Utils::Icons::NEWFILE.icon();
      break;
    case SymbolKind::Module:
    case SymbolKind::Namespace:
    case SymbolKind::Package:
      icons[kind] = iconForType(Namespace);
      break;
    case SymbolKind::Class:
      icons[kind] = iconForType(Class);
      break;
    case SymbolKind::Method:
      icons[kind] = iconForType(FuncPublic);
      break;
    case SymbolKind::Property:
      icons[kind] = iconForType(Property);
      break;
    case SymbolKind::Field:
      icons[kind] = iconForType(VarPublic);
      break;
    case SymbolKind::Constructor:
      icons[kind] = iconForType(Class);
      break;
    case SymbolKind::Enum:
      icons[kind] = iconForType(Enum);
      break;
    case SymbolKind::Interface:
      icons[kind] = iconForType(Class);
      break;
    case SymbolKind::Function:
      icons[kind] = iconForType(FuncPublic);
      break;
    case SymbolKind::Variable:
    case SymbolKind::Constant:
    case SymbolKind::String:
    case SymbolKind::Number:
    case SymbolKind::Boolean:
    case SymbolKind::Array:
      icons[kind] = iconForType(VarPublic);
      break;
    case SymbolKind::Object:
      icons[kind] = iconForType(Class);
      break;
    case SymbolKind::Key:
    case SymbolKind::Null:
      icons[kind] = iconForType(Keyword);
      break;
    case SymbolKind::EnumMember:
      icons[kind] = iconForType(Enumerator);
      break;
    case SymbolKind::Struct:
      icons[kind] = iconForType(Struct);
      break;
    case SymbolKind::Event:
    case SymbolKind::Operator:
      icons[kind] = iconForType(FuncPublic);
      break;
    case SymbolKind::TypeParameter:
      icons[kind] = iconForType(VarPublic);
      break;
    }
  }
  return icons[kind];
}

} // namespace LanguageClient
