// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientformatter.hpp"

#include "client.hpp"
#include "languageclientutils.hpp"

#include <texteditor/tabsettings.hpp>
#include <texteditor/textdocument.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QTextDocument>

using namespace LanguageServerProtocol;
using namespace Utils;

namespace LanguageClient {

LanguageClientFormatter::LanguageClientFormatter(TextEditor::TextDocument *document, Client *client) : m_client(client), m_document(document)
{
  m_cancelConnection = QObject::connect(document->document(), &QTextDocument::contentsChanged, [this]() {
    if (m_ignoreCancel)
      m_ignoreCancel = false;
    else
      cancelCurrentRequest();
  });
}

LanguageClientFormatter::~LanguageClientFormatter()
{
  QObject::disconnect(m_cancelConnection);
  cancelCurrentRequest();
}

static auto formattingOptions(const TextEditor::TabSettings &settings) -> const FormattingOptions
{
  FormattingOptions options;
  options.setTabSize(settings.m_tabSize);
  options.setInsertSpace(settings.m_tabPolicy == TextEditor::TabSettings::SpacesOnlyTabPolicy);
  return options;
}

auto LanguageClientFormatter::format(const QTextCursor &cursor, const TextEditor::TabSettings &tabSettings) -> QFutureWatcher<ChangeSet>*
{
  cancelCurrentRequest();
  m_progress = QFutureInterface<ChangeSet>();

  const auto &filePath = m_document->filePath();
  const auto dynamicCapabilities = m_client->dynamicCapabilities();
  const QString method(DocumentRangeFormattingRequest::methodName);
  if (const auto registered = dynamicCapabilities.isRegistered(method)) {
    if (!registered.value())
      return nullptr;
    const TextDocumentRegistrationOptions option(dynamicCapabilities.option(method).toObject());
    if (option.isValid() && !option.filterApplies(filePath, Utils::mimeTypeForName(m_document->mimeType()))) {
      return nullptr;
    }
  } else {
    const auto &provider = m_client->capabilities().documentRangeFormattingProvider();
    if (!provider.has_value())
      return nullptr;
    if (Utils::holds_alternative<bool>(*provider) && !Utils::get<bool>(*provider))
      return nullptr;
  }
  DocumentRangeFormattingParams params;
  const auto uri = DocumentUri::fromFilePath(filePath);
  params.setTextDocument(TextDocumentIdentifier(uri));
  params.setOptions(formattingOptions(tabSettings));
  if (!cursor.hasSelection()) {
    auto c = cursor;
    c.select(QTextCursor::LineUnderCursor);
    params.setRange(Range(c));
  } else {
    params.setRange(Range(cursor));
  }
  DocumentRangeFormattingRequest request(params);
  request.setResponseCallback([this](const DocumentRangeFormattingRequest::Response &response) {
    handleResponse(response);
  });
  m_currentRequest = request.id();
  m_client->sendContent(request);
  // ignore first contents changed, because this function is called inside a begin/endEdit block
  m_ignoreCancel = true;
  m_progress.reportStarted();
  const auto watcher = new QFutureWatcher<ChangeSet>();
  QObject::connect(watcher, &QFutureWatcher<Text::Replacements>::canceled, [this]() {
    cancelCurrentRequest();
  });
  watcher->setFuture(m_progress.future());
  return watcher;
}

auto LanguageClientFormatter::cancelCurrentRequest() -> void
{
  if (m_currentRequest.has_value()) {
    m_progress.reportCanceled();
    m_progress.reportFinished();
    m_client->cancelRequest(*m_currentRequest);
    m_ignoreCancel = false;
    m_currentRequest = nullopt;
  }
}

auto LanguageClientFormatter::handleResponse(const DocumentRangeFormattingRequest::Response &response) -> void
{
  m_currentRequest = nullopt;
  if (const auto &error = response.error())
    m_client->log(*error);
  ChangeSet changeSet;
  if (const auto result = response.result()) {
    if (!result->isNull())
      changeSet = editsToChangeSet(result->toList(), m_document->document());
  }
  m_progress.reportResult(changeSet);
  m_progress.reportFinished();
}

} // namespace LanguageClient
