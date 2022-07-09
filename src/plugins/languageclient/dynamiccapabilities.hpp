// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <languageserverprotocol/client.h>

namespace LanguageClient {

class DynamicCapability {
public:
  DynamicCapability() = default;

  auto enable(const QString &id, const QJsonValue &options) -> void
  {
    QTC_CHECK(!m_enabled);
    m_enabled = true;
    m_id = id;
    m_options = options;
  }

  auto disable() -> void
  {
    m_enabled = true;
    m_id.clear();
    m_options = QJsonValue();
  }

  auto enabled() const -> bool { return m_enabled; }
  auto options() const -> QJsonValue { return m_options; }

private:
  bool m_enabled = false;
  QString m_id;
  QJsonValue m_options;

};

class DynamicCapabilities {
public:
  DynamicCapabilities() = default;

  auto registerCapability(const QList<LanguageServerProtocol::Registration> &registrations) -> void;
  auto unregisterCapability(const QList<LanguageServerProtocol::Unregistration> &unregistrations) -> void;
  auto isRegistered(const QString &method) const -> Utils::optional<bool>;
  auto option(const QString &method) const -> QJsonValue { return m_capability.value(method).options(); }
  auto registeredMethods() const -> QStringList;
  auto reset() -> void;

private:
  QHash<QString, DynamicCapability> m_capability;
  QHash<QString, QString> m_methodForId;
};

} // namespace LanguageClient
