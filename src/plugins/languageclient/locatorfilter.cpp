// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "locatorfilter.hpp"

#include "languageclient_global.hpp"
#include "languageclientmanager.hpp"
#include "languageclientutils.hpp"

#include <core/editormanager/editormanager.hpp>
#include <languageserverprotocol/servercapabilities.h>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/fuzzymatcher.hpp>
#include <utils/linecolumn.hpp>

#include <QFutureWatcher>
#include <QRegularExpression>

using namespace LanguageServerProtocol;

namespace LanguageClient {

DocumentLocatorFilter::DocumentLocatorFilter()
{
  setId(Constants::LANGUAGECLIENT_DOCUMENT_FILTER_ID);
  setDisplayName(Constants::LANGUAGECLIENT_DOCUMENT_FILTER_DISPLAY_NAME);
  setDescription(tr("Matches all symbols from the current document, based on a language server."));
  setDefaultShortcutString(".");
  setDefaultIncludedByDefault(false);
  setPriority(ILocatorFilter::Low);
  connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, this, &DocumentLocatorFilter::updateCurrentClient);
}

auto DocumentLocatorFilter::updateCurrentClient() -> void
{
  resetSymbols();
  disconnect(m_resetSymbolsConnection);

  const auto document = TextEditor::TextDocument::currentTextDocument();
  if (const auto client = LanguageClientManager::clientForDocument(document); client && (client->locatorsEnabled() || m_forced)) {
    setEnabled(!m_forced);
    if (m_symbolCache != client->documentSymbolCache()) {
      disconnect(m_updateSymbolsConnection);
      m_symbolCache = client->documentSymbolCache();
      m_updateSymbolsConnection = connect(m_symbolCache, &DocumentSymbolCache::gotSymbols, this, &DocumentLocatorFilter::updateSymbols);
    }
    m_resetSymbolsConnection = connect(document, &Core::IDocument::contentsChanged, this, &DocumentLocatorFilter::resetSymbols);
    m_currentUri = DocumentUri::fromFilePath(document->filePath());
  } else {
    disconnect(m_updateSymbolsConnection);
    m_symbolCache.clear();
    m_currentUri.clear();
    setEnabled(false);
  }
}

auto DocumentLocatorFilter::updateSymbols(const DocumentUri &uri, const DocumentSymbolsResult &symbols) -> void
{
  if (uri != m_currentUri)
    return;
  QMutexLocker locker(&m_mutex);
  m_currentSymbols = symbols;
  emit symbolsUpToDate({});
}

auto DocumentLocatorFilter::resetSymbols() -> void
{
  QMutexLocker locker(&m_mutex);
  m_currentSymbols.reset();
}

static auto generateLocatorEntry(const SymbolInformation &info, Core::ILocatorFilter *filter) -> Core::LocatorFilterEntry
{
  Core::LocatorFilterEntry entry;
  entry.filter = filter;
  entry.display_name = info.name();
  if (const auto container = info.containerName())
    entry.extra_info = container.value_or(QString());
  entry.display_icon = symbolIcon(info.kind());
  entry.internal_data = QVariant::fromValue(info.location().toLink());
  return entry;
}

auto DocumentLocatorFilter::generateLocatorEntry(const SymbolInformation &info) -> Core::LocatorFilterEntry
{
  return LanguageClient::generateLocatorEntry(info, this);
}

auto DocumentLocatorFilter::generateLocatorEntries(const SymbolInformation &info, const QRegularExpression &regexp, const Core::LocatorFilterEntry &parent) -> QList<Core::LocatorFilterEntry>
{
  Q_UNUSED(parent)
  if (regexp.match(info.name()).hasMatch())
    return {generateLocatorEntry(info)};
  return {};
}

auto DocumentLocatorFilter::generateLocatorEntry(const DocumentSymbol &info, const Core::LocatorFilterEntry &parent) -> Core::LocatorFilterEntry
{
  Q_UNUSED(parent)
  Core::LocatorFilterEntry entry;
  entry.filter = this;
  entry.display_name = info.name();
  if (const auto detail = info.detail())
    entry.extra_info = detail.value_or(QString());
  entry.display_icon = symbolIcon(info.kind());
  const auto &pos = info.range().start();
  entry.internal_data = QVariant::fromValue(Utils::LineColumn(pos.line(), pos.character()));
  return entry;
}

auto DocumentLocatorFilter::generateLocatorEntries(const DocumentSymbol &info, const QRegularExpression &regexp, const Core::LocatorFilterEntry &parent) -> QList<Core::LocatorFilterEntry>
{
  QList<Core::LocatorFilterEntry> entries;
  const auto children = info.children().value_or(QList<DocumentSymbol>());
  const auto hasMatch = regexp.match(info.name()).hasMatch();
  Core::LocatorFilterEntry entry;
  if (hasMatch || !children.isEmpty())
    entry = generateLocatorEntry(info, parent);
  if (hasMatch)
    entries << entry;
  for (const auto &child : children)
    entries << generateLocatorEntries(child, regexp, entry);
  return entries;
}

template <class T>
auto DocumentLocatorFilter::generateEntries(const QList<T> &list, const QString &filter) -> QList<Core::LocatorFilterEntry>
{
  QList<Core::LocatorFilterEntry> entries;
  const auto caseSensitivity = ILocatorFilter::caseSensitivity(filter) == Qt::CaseSensitive ? FuzzyMatcher::CaseSensitivity::CaseSensitive : FuzzyMatcher::CaseSensitivity::CaseInsensitive;
  const auto regexp = FuzzyMatcher::createRegExp(filter, caseSensitivity);
  if (!regexp.isValid())
    return entries;

  for (const T &item : list)
    entries << generateLocatorEntries(item, regexp, {});
  return entries;
}

auto DocumentLocatorFilter::prepareSearch(const QString &/*entry*/) -> void
{
  QMutexLocker locker(&m_mutex);
  if (m_symbolCache && !m_currentSymbols.has_value()) {
    locker.unlock();
    m_symbolCache->requestSymbols(m_currentUri, Schedule::Delayed);
  }
}

auto DocumentLocatorFilter::matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry>
{
  QMutexLocker locker(&m_mutex);
  if (!m_symbolCache)
    return {};
  if (!m_currentSymbols.has_value()) {
    QEventLoop loop;
    connect(this, &DocumentLocatorFilter::symbolsUpToDate, &loop, [&]() { loop.exit(1); });
    QFutureWatcher<Core::LocatorFilterEntry> watcher;
    connect(&watcher, &QFutureWatcher<Core::LocatorFilterEntry>::canceled, &loop, &QEventLoop::quit);
    watcher.setFuture(future.future());
    locker.unlock();
    if (!loop.exec())
      return {};
    locker.relock();
  }

  QTC_ASSERT(m_currentSymbols.has_value(), return {});

  if (const auto list = Utils::get_if<QList<DocumentSymbol>>(&m_currentSymbols.value()))
    return generateEntries(*list, entry);
  else if (const auto list = Utils::get_if<QList<SymbolInformation>>(&m_currentSymbols.value()))
    return generateEntries(*list, entry);

  return {};
}

auto DocumentLocatorFilter::accept(const Core::LocatorFilterEntry &selection, QString * /*newText*/, int * /*selectionStart*/, int * /*selectionLength*/) const -> void
{
  if (selection.internal_data.canConvert<Utils::LineColumn>()) {
    const auto lineColumn = qvariant_cast<Utils::LineColumn>(selection.internal_data);
    const Utils::Link link(m_currentUri.toFilePath(), lineColumn.line + 1, lineColumn.column);
    Core::EditorManager::openEditorAt(link, {}, Core::EditorManager::AllowExternalEditor);
  } else if (selection.internal_data.canConvert<Utils::Link>()) {
    Core::EditorManager::openEditorAt(qvariant_cast<Utils::Link>(selection.internal_data), {}, Core::EditorManager::AllowExternalEditor);
  }
}

WorkspaceLocatorFilter::WorkspaceLocatorFilter() : WorkspaceLocatorFilter(QVector<SymbolKind>()) {}

WorkspaceLocatorFilter::WorkspaceLocatorFilter(const QVector<SymbolKind> &filter) : m_filterKinds(filter)
{
  setId(Constants::LANGUAGECLIENT_WORKSPACE_FILTER_ID);
  setDisplayName(Constants::LANGUAGECLIENT_WORKSPACE_FILTER_DISPLAY_NAME);
  setDefaultShortcutString(":");
  setDefaultIncludedByDefault(false);
  setPriority(ILocatorFilter::Low);
}

auto WorkspaceLocatorFilter::prepareSearch(const QString &entry) -> void
{
  prepareSearch(entry, LanguageClientManager::clients(), false);
}

auto WorkspaceLocatorFilter::prepareSearch(const QString &entry, const QVector<Client*> &clients) -> void
{
  prepareSearch(entry, clients, true);
}

auto WorkspaceLocatorFilter::prepareSearch(const QString &entry, const QVector<Client*> &clients, bool force) -> void
{
  m_pendingRequests.clear();
  m_results.clear();

  WorkspaceSymbolParams params;
  params.setQuery(entry);
  if (m_maxResultCount > 0)
    params.setLimit(m_maxResultCount);

  QMutexLocker locker(&m_mutex);
  for (auto client : qAsConst(clients)) {
    if (!client->reachable())
      continue;
    if (!(force || client->locatorsEnabled()))
      continue;
    auto capability = client->capabilities().workspaceSymbolProvider();
    if (!capability.has_value())
      continue;
    if (Utils::holds_alternative<bool>(*capability) && !Utils::get<bool>(*capability))
      continue;
    WorkspaceSymbolRequest request(params);
    request.setResponseCallback([this, client](const WorkspaceSymbolRequest::Response &response) {
      handleResponse(client, response);
    });
    m_pendingRequests[client] = request.id();
    client->sendContent(request);
  }
}

auto WorkspaceLocatorFilter::matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString & /*entry*/) -> QList<Core::LocatorFilterEntry>
{
  QMutexLocker locker(&m_mutex);
  if (!m_pendingRequests.isEmpty()) {
    QEventLoop loop;
    connect(this, &WorkspaceLocatorFilter::allRequestsFinished, &loop, [&]() { loop.exit(1); });
    QFutureWatcher<Core::LocatorFilterEntry> watcher;
    connect(&watcher, &QFutureWatcher<Core::LocatorFilterEntry>::canceled, &loop, &QEventLoop::quit);
    watcher.setFuture(future.future());
    locker.unlock();
    if (!loop.exec())
      return {};

    locker.relock();
  }

  if (!m_filterKinds.isEmpty()) {
    m_results = Utils::filtered(m_results, [&](const SymbolInformation &info) {
      return m_filterKinds.contains(SymbolKind(info.kind()));
    });
  }
  return Utils::transform(m_results, [this](const SymbolInformation &info) {
    return generateLocatorEntry(info, this);
  }).toList();
}

auto WorkspaceLocatorFilter::accept(const Core::LocatorFilterEntry &selection, QString * /*newText*/, int * /*selectionStart*/, int * /*selectionLength*/) const -> void
{
  if (selection.internal_data.canConvert<Utils::Link>())
    Core::EditorManager::openEditorAt(qvariant_cast<Utils::Link>(selection.internal_data), {}, Core::EditorManager::AllowExternalEditor);
}

auto WorkspaceLocatorFilter::handleResponse(Client *client, const WorkspaceSymbolRequest::Response &response) -> void
{
  QMutexLocker locker(&m_mutex);
  m_pendingRequests.remove(client);
  const auto result = response.result().value_or(LanguageClientArray<SymbolInformation>());
  if (!result.isNull())
    m_results.append(result.toList().toVector());
  if (m_pendingRequests.isEmpty()) emit allRequestsFinished(QPrivateSignal());
}

WorkspaceClassLocatorFilter::WorkspaceClassLocatorFilter() : WorkspaceLocatorFilter({SymbolKind::Class, SymbolKind::Struct})
{
  setId(Constants::LANGUAGECLIENT_WORKSPACE_CLASS_FILTER_ID);
  setDisplayName(Constants::LANGUAGECLIENT_WORKSPACE_CLASS_FILTER_DISPLAY_NAME);
  setDefaultShortcutString("c");
}

WorkspaceMethodLocatorFilter::WorkspaceMethodLocatorFilter() : WorkspaceLocatorFilter({SymbolKind::Method, SymbolKind::Function, SymbolKind::Constructor})
{
  setId(Constants::LANGUAGECLIENT_WORKSPACE_METHOD_FILTER_ID);
  setDisplayName(Constants::LANGUAGECLIENT_WORKSPACE_METHOD_FILTER_DISPLAY_NAME);
  setDefaultShortcutString("m");
}

} // namespace LanguageClient
