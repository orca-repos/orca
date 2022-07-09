// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "diagnosticmanager.hpp"

#include "client.hpp"

#include <core/editormanager/documentmodel.hpp>
#include <projectexplorer/project.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/textmark.hpp>
#include <texteditor/textstyles.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QTextEdit>

using namespace LanguageServerProtocol;
using namespace Utils;
using namespace TextEditor;

namespace LanguageClient {

class TextMark : public TextEditor::TextMark {
public:
  TextMark(const FilePath &fileName, const Diagnostic &diag, const Id &clientId) : TextEditor::TextMark(fileName, diag.range().start().line() + 1, clientId), m_diagnostic(diag)
  {
    const auto isError = diag.severity().value_or(DiagnosticSeverity::Hint) == DiagnosticSeverity::Error;

    setLineAnnotation(diag.message());
    setToolTip(diag.message());
    setColor(isError ? Theme::CodeModel_Error_TextMarkColor : Theme::CodeModel_Warning_TextMarkColor);
    setIcon(isError ? Icons::CODEMODEL_ERROR.icon() : Icons::CODEMODEL_WARNING.icon());
  }

  auto diagnostic() const -> const Diagnostic& { return m_diagnostic; }

private:
  const Diagnostic m_diagnostic;
};

DiagnosticManager::DiagnosticManager(Client *client) : m_client(client)
{
  m_textMarkCreator = [this](const FilePath &filePath, const Diagnostic &diagnostic, bool /*isProjectFile*/) {
    return createTextMark(filePath, diagnostic);
  };
}

DiagnosticManager::~DiagnosticManager()
{
  clearDiagnostics();
}

auto DiagnosticManager::setDiagnostics(const LanguageServerProtocol::DocumentUri &uri, const QList<LanguageServerProtocol::Diagnostic> &diagnostics, const Utils::optional<int> &version) -> void
{
  hideDiagnostics(uri.toFilePath());
  const auto filteredDiags = m_filter ? Utils::filtered(diagnostics, m_filter) : diagnostics;
  m_diagnostics[uri] = {version, filteredDiags};
}

auto DiagnosticManager::hideDiagnostics(const Utils::FilePath &filePath) -> void
{
  if (m_hideHandler)
    m_hideHandler();
  if (const auto doc = TextDocument::textDocumentForFilePath(filePath)) {
    for (const auto editor : BaseTextEditor::textEditorsForDocument(doc))
      editor->editorWidget()->setExtraSelections(TextEditorWidget::CodeWarningsSelection, {});
  }
  qDeleteAll(m_marks.take(filePath));
}

static auto toDiagnosticsSelections(const Diagnostic &diagnostic, QTextDocument *textDocument) -> QTextEdit::ExtraSelection
{
  QTextCursor cursor(textDocument);
  cursor.setPosition(diagnostic.range().start().toPositionInDocument(textDocument));
  cursor.setPosition(diagnostic.range().end().toPositionInDocument(textDocument), QTextCursor::KeepAnchor);

  const auto &fontSettings = TextEditorSettings::fontSettings();
  const auto severity = diagnostic.severity().value_or(DiagnosticSeverity::Warning);
  const auto style = severity == DiagnosticSeverity::Error ? C_ERROR : C_WARNING;

  return QTextEdit::ExtraSelection{cursor, fontSettings.toTextCharFormat(style)};
}

auto DiagnosticManager::showDiagnostics(const DocumentUri &uri, int version) -> void
{
  const auto &filePath = uri.toFilePath();
  if (const auto doc = TextDocument::textDocumentForFilePath(filePath)) {
    QList<QTextEdit::ExtraSelection> extraSelections;
    const auto &versionedDiagnostics = m_diagnostics.value(uri);
    if (versionedDiagnostics.version.value_or(version) == version && !versionedDiagnostics.diagnostics.isEmpty()) {
      auto &marks = m_marks[filePath];
      const auto isProjectFile = m_client->project() && m_client->project()->isKnownFile(filePath);
      for (const auto &diagnostic : versionedDiagnostics.diagnostics) {
        extraSelections << toDiagnosticsSelections(diagnostic, doc->document());
        marks.append(m_textMarkCreator(filePath, diagnostic, isProjectFile));
      }
    }

    for (const auto editor : BaseTextEditor::textEditorsForDocument(doc)) {
      editor->editorWidget()->setExtraSelections(TextEditorWidget::CodeWarningsSelection, extraSelections);
    }
  }
}

auto DiagnosticManager::createTextMark(const FilePath &filePath, const Diagnostic &diagnostic) const -> TextEditor::TextMark*
{
  static const auto icon = QIcon::fromTheme("edit-copy", Utils::Icons::COPY.icon());
  static const auto tooltip = tr("Copy to Clipboard");
  auto action = new QAction();
  action->setIcon(icon);
  action->setToolTip(tooltip);
  QObject::connect(action, &QAction::triggered, [text = diagnostic.message()]() {
    QApplication::clipboard()->setText(text);
  });
  const auto mark = new TextMark(filePath, diagnostic, m_client->id());
  mark->setActions({action});
  return mark;
}

auto DiagnosticManager::clearDiagnostics() -> void
{
  for (const auto &uri : m_diagnostics.keys())
    hideDiagnostics(uri.toFilePath());
  m_diagnostics.clear();
  if (!QTC_GUARD(m_marks.isEmpty())) {
    for (const auto &marks : qAsConst(m_marks))
      qDeleteAll(marks);
    m_marks.clear();
  }
}

auto DiagnosticManager::diagnosticsAt(const DocumentUri &uri, const QTextCursor &cursor) const -> QList<Diagnostic>
{
  const auto documentRevision = m_client->documentVersion(uri.toFilePath());
  const auto it = m_diagnostics.find(uri);
  if (it == m_diagnostics.end())
    return {};
  if (documentRevision != it->version.value_or(documentRevision))
    return {};
  return Utils::filtered(it->diagnostics, [range = Range(cursor)](const Diagnostic &diagnostic) {
    return diagnostic.range().overlaps(range);
  });
}

auto DiagnosticManager::hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri, const TextDocument *doc, const LanguageServerProtocol::Diagnostic &diag) const -> bool
{
  if (!doc)
    return false;
  const auto it = m_diagnostics.find(uri);
  if (it == m_diagnostics.end())
    return {};
  const auto revision = m_client->documentVersion(uri.toFilePath());
  if (revision != it->version.value_or(revision))
    return false;
  return it->diagnostics.contains(diag);
}

auto DiagnosticManager::setDiagnosticsHandlers(const TextMarkCreator &textMarkCreator, const HideDiagnosticsHandler &removalHandler, const DiagnosticsFilter &filter) -> void
{
  m_textMarkCreator = textMarkCreator;
  m_hideHandler = removalHandler;
  m_filter = filter;
}

} // namespace LanguageClient
