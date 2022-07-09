// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/workspace.h>

#include <texteditor/refactoroverlay.hpp>
#include <utils/changeset.hpp>

namespace Core {
class IEditor;
}

namespace TextEditor {
class TextDocument;
class TextDocumentManipulatorInterface;
} // namespace TextEditor

namespace LanguageClient {

class Client;

enum class Schedule {
  Now,
  Delayed
};

auto editsToChangeSet(const QList<LanguageServerProtocol::TextEdit> &edits, const QTextDocument *doc) -> Utils::ChangeSet;
LANGUAGECLIENT_EXPORT auto applyWorkspaceEdit(const Client *client, const LanguageServerProtocol::WorkspaceEdit &edit) -> bool;
LANGUAGECLIENT_EXPORT auto applyTextDocumentEdit(const Client *client, const LanguageServerProtocol::TextDocumentEdit &edit) -> bool;
LANGUAGECLIENT_EXPORT auto applyTextEdits(const LanguageServerProtocol::DocumentUri &uri, const QList<LanguageServerProtocol::TextEdit> &edits) -> bool;
LANGUAGECLIENT_EXPORT auto applyTextEdit(TextEditor::TextDocumentManipulatorInterface &manipulator, const LanguageServerProtocol::TextEdit &edit, bool newTextIsSnippet = false) -> void;
LANGUAGECLIENT_EXPORT auto updateCodeActionRefactoringMarker(Client *client, const LanguageServerProtocol::CodeAction &action, const LanguageServerProtocol::DocumentUri &uri) -> void;
auto updateEditorToolBar(Core::IEditor *editor) -> void;
LANGUAGECLIENT_EXPORT auto symbolIcon(int type) -> const QIcon;

} // namespace LanguageClient
