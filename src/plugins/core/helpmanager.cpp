// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "helpmanager_implementation.hpp"
#include "coreplugin.hpp"

#include <extensionsystem/pluginspec.hpp>
#include <utils/qtcassert.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QUrl>

namespace Core {
namespace HelpManager {

// makes sure that plugins can connect to HelpManager signals even if the Help plugin is not loaded
Q_GLOBAL_STATIC(Signals, m_signals)

static Implementation *m_instance = nullptr;

static auto checkInstance() -> bool
{
  const auto plugin = Internal::CorePlugin::instance();
  // HelpManager API can only be used after the actual implementation has been created by the
  // Help plugin, so check that the plugins have all been created. That is the case
  // when the Core plugin is initialized.
  QTC_CHECK(plugin && plugin->pluginSpec() && plugin->pluginSpec()->state() >= ExtensionSystem::PluginSpec::Initialized);
  return m_instance != nullptr;
}

auto Signals::instance() -> Signals*
{
  return m_signals;
}

Implementation::Implementation()
{
  QTC_CHECK(!m_instance);
  m_instance = this;
}

Implementation::~Implementation()
{
  m_instance = nullptr;
}

auto documentationPath() -> QString
{
  return QDir::cleanPath(QCoreApplication::applicationDirPath() + '/' + RELATIVE_DOC_PATH);
}

auto registerDocumentation(const QStringList &file_names) -> void
{
  if (checkInstance())
    m_instance->registerDocumentation(file_names);
}

auto unregisterDocumentation(const QStringList &file_names) -> void
{
  if (checkInstance())
    m_instance->unregisterDocumentation(file_names);
}

auto linksForIdentifier(const QString &id) -> QMultiMap<QString, QUrl>
{
  return checkInstance() ? m_instance->linksForIdentifier(id) : QMultiMap<QString, QUrl>();
}

auto linksForKeyword(const QString &keyword) -> QMultiMap<QString, QUrl>
{
  return checkInstance() ? m_instance->linksForKeyword(keyword) : QMultiMap<QString, QUrl>();
}

auto fileData(const QUrl &url) -> QByteArray
{
  return checkInstance() ? m_instance->fileData(url) : QByteArray();
}

auto showHelpUrl(const QUrl &url, const HelpViewerLocation location) -> void
{
  if (checkInstance())
    m_instance->showHelpUrl(url, location);
}

auto showHelpUrl(const QString &url, const HelpViewerLocation location) -> void
{
  showHelpUrl(QUrl(url), location);
}

} // HelpManager
} // Core
