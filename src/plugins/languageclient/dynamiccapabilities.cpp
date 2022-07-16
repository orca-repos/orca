// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "dynamiccapabilities.hpp"

using namespace LanguageServerProtocol;

namespace LanguageClient {

auto DynamicCapabilities::registerCapability(const QList<Registration> &registrations) -> void
{
  for (const auto &registration : registrations) {
    const auto &method = registration.method();
    m_capability[method].enable(registration.id(), registration.registerOptions());
    m_methodForId.insert(registration.id(), method);
  }
}

auto DynamicCapabilities::unregisterCapability(const QList<Unregistration> &unregistrations) -> void
{
  for (const auto &unregistration : unregistrations) {
    auto method = unregistration.method();
    if (method.isEmpty())
      method = m_methodForId[unregistration.id()];
    m_capability[method].disable();
    m_methodForId.remove(unregistration.id());
  }
}

auto DynamicCapabilities::isRegistered(const QString &method) const -> Utils::optional<bool>
{
  if (!m_capability.contains(method))
    return Utils::nullopt;
  return m_capability[method].enabled();
}

auto DynamicCapabilities::registeredMethods() const -> QStringList
{
  return m_capability.keys();
}

auto DynamicCapabilities::reset() -> void
{
  m_capability.clear();
  m_methodForId.clear();
}

} // namespace LanguageClient
