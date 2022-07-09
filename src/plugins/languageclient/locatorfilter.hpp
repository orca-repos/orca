// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "client.hpp"
#include "languageclient_global.hpp"

#include <core/locator/ilocatorfilter.hpp>
#include <languageserverprotocol/lsptypes.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/workspace.h>

#include <QVector>

namespace Core {
class IEditor;
}

namespace LanguageClient {

class LANGUAGECLIENT_EXPORT DocumentLocatorFilter : public Core::ILocatorFilter {
  Q_OBJECT
public:
  DocumentLocatorFilter();

  auto updateCurrentClient() -> void;
  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry> override;
  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

signals:
  auto symbolsUpToDate(QPrivateSignal) -> void;

protected:
  auto forceUse() -> void { m_forced = true; }

  QPointer<DocumentSymbolCache> m_symbolCache;
  LanguageServerProtocol::DocumentUri m_currentUri;

private:
  auto updateSymbols(const LanguageServerProtocol::DocumentUri &uri, const LanguageServerProtocol::DocumentSymbolsResult &symbols) -> void;
  auto resetSymbols() -> void;

  template <class T>
  auto generateEntries(const QList<T> &list, const QString &filter) -> QList<Core::LocatorFilterEntry>;
  auto generateLocatorEntries(const LanguageServerProtocol::SymbolInformation &info, const QRegularExpression &regexp, const Core::LocatorFilterEntry &parent) -> QList<Core::LocatorFilterEntry>;
  auto generateLocatorEntries(const LanguageServerProtocol::DocumentSymbol &info, const QRegularExpression &regexp, const Core::LocatorFilterEntry &parent) -> QList<Core::LocatorFilterEntry>;
  virtual auto generateLocatorEntry(const LanguageServerProtocol::DocumentSymbol &info, const Core::LocatorFilterEntry &parent) -> Core::LocatorFilterEntry;
  virtual auto generateLocatorEntry(const LanguageServerProtocol::SymbolInformation &info) -> Core::LocatorFilterEntry;

  QMutex m_mutex;
  QMetaObject::Connection m_updateSymbolsConnection;
  QMetaObject::Connection m_resetSymbolsConnection;
  Utils::optional<LanguageServerProtocol::DocumentSymbolsResult> m_currentSymbols;
  bool m_forced = false;
};

class LANGUAGECLIENT_EXPORT WorkspaceLocatorFilter : public Core::ILocatorFilter {
  Q_OBJECT

public:
  WorkspaceLocatorFilter();

  /// request workspace symbols for all clients with enabled locator
  auto prepareSearch(const QString &entry) -> void override;
  /// force request workspace symbols for all given clients
  auto prepareSearch(const QString &entry, const QVector<Client*> &clients) -> void;
  auto matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry> override;
  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

signals:
  auto allRequestsFinished(QPrivateSignal) -> void;

protected:
  explicit WorkspaceLocatorFilter(const QVector<LanguageServerProtocol::SymbolKind> &filter);

  auto setMaxResultCount(qint64 limit) -> void { m_maxResultCount = limit; }

private:
  auto prepareSearch(const QString &entry, const QVector<Client*> &clients, bool force) -> void;
  auto handleResponse(Client *client, const LanguageServerProtocol::WorkspaceSymbolRequest::Response &response) -> void;

  QMutex m_mutex;
  QMap<Client*, LanguageServerProtocol::MessageId> m_pendingRequests;
  QVector<LanguageServerProtocol::SymbolInformation> m_results;
  QVector<LanguageServerProtocol::SymbolKind> m_filterKinds;
  qint64 m_maxResultCount = 0;
};

class LANGUAGECLIENT_EXPORT WorkspaceClassLocatorFilter : public WorkspaceLocatorFilter {
public:
  WorkspaceClassLocatorFilter();
};

class LANGUAGECLIENT_EXPORT WorkspaceMethodLocatorFilter : public WorkspaceLocatorFilter {
public:
  WorkspaceMethodLocatorFilter();
};

} // namespace LanguageClient
