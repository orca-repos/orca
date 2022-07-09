// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <languageserverprotocol/lsptypes.h>

#include <utils/id.hpp>

#include <QMap>

#include <functional>

namespace TextEditor {
class TextDocument;
class TextMark;
}

namespace LanguageClient {

class Client;

using TextMarkCreator = std::function<TextEditor::TextMark *(const Utils::FilePath &, const LanguageServerProtocol::Diagnostic &, bool)>;
using HideDiagnosticsHandler = std::function<void()>;
using DiagnosticsFilter = std::function<bool(const LanguageServerProtocol::Diagnostic &)>;

class DiagnosticManager {
  Q_DECLARE_TR_FUNCTIONS(LanguageClient::DiagnosticManager)

public:
  explicit DiagnosticManager(Client *client);
  ~DiagnosticManager();

  auto setDiagnostics(const LanguageServerProtocol::DocumentUri &uri, const QList<LanguageServerProtocol::Diagnostic> &diagnostics, const Utils::optional<int> &version) -> void;
  auto showDiagnostics(const LanguageServerProtocol::DocumentUri &uri, int version) -> void;
  auto hideDiagnostics(const Utils::FilePath &filePath) -> void;
  auto clearDiagnostics() -> void;
  auto diagnosticsAt(const LanguageServerProtocol::DocumentUri &uri, const QTextCursor &cursor) const -> QList<LanguageServerProtocol::Diagnostic>;
  auto hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri, const TextEditor::TextDocument *doc, const LanguageServerProtocol::Diagnostic &diag) const -> bool;
  auto setDiagnosticsHandlers(const TextMarkCreator &shownHandler, const HideDiagnosticsHandler &removalHandler, const DiagnosticsFilter &filter) -> void;

private:
  auto createTextMark(const Utils::FilePath &filePath, const LanguageServerProtocol::Diagnostic &diagnostic) const -> TextEditor::TextMark*;

  struct VersionedDiagnostics {
    Utils::optional<int> version;
    QList<LanguageServerProtocol::Diagnostic> diagnostics;
  };

  QMap<LanguageServerProtocol::DocumentUri, VersionedDiagnostics> m_diagnostics;
  QMap<Utils::FilePath, QList<TextEditor::TextMark*>> m_marks;
  TextMarkCreator m_textMarkCreator;
  HideDiagnosticsHandler m_hideHandler;
  DiagnosticsFilter m_filter;
  Client *m_client;
};

} // namespace LanguageClient
