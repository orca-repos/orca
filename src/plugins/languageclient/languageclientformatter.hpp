// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <languageserverprotocol/icontent.h>
#include <languageserverprotocol/languagefeatures.h>

#include <texteditor/formatter.hpp>

namespace TextEditor {
class TextDocument;
}

namespace LanguageClient {

class Client;

class LanguageClientFormatter : public TextEditor::Formatter {
public:
  LanguageClientFormatter(TextEditor::TextDocument *document, Client *client);
  ~LanguageClientFormatter() override;

  auto format(const QTextCursor &cursor, const TextEditor::TabSettings &tabSettings) -> QFutureWatcher<Utils::ChangeSet>* override;

private:
  auto cancelCurrentRequest() -> void;
  auto handleResponse(const LanguageServerProtocol::DocumentRangeFormattingRequest::Response &response) -> void;

  Client *m_client = nullptr; // not owned
  QMetaObject::Connection m_cancelConnection;
  TextEditor::TextDocument *m_document; // not owned
  bool m_ignoreCancel = false;
  QFutureInterface<Utils::ChangeSet> m_progress;
  Utils::optional<LanguageServerProtocol::MessageId> m_currentRequest;
};

} // namespace LanguageClient
