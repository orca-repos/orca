// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangdiagnosticconfigsmodel.hpp"

#include "cppeditorconstants.hpp"
#include "cpptoolsreuse.hpp"

#include <utils/algorithm.hpp>

#include <QCoreApplication>
#include <QUuid>

namespace CppEditor {

ClangDiagnosticConfigsModel::ClangDiagnosticConfigsModel(const ClangDiagnosticConfigs &configs)
{
  m_diagnosticConfigs.append(configs);
}

auto ClangDiagnosticConfigsModel::size() const -> int
{
  return m_diagnosticConfigs.size();
}

auto ClangDiagnosticConfigsModel::at(int index) const -> const ClangDiagnosticConfig&
{
  return m_diagnosticConfigs.at(index);
}

auto ClangDiagnosticConfigsModel::appendOrUpdate(const ClangDiagnosticConfig &config) -> void
{
  const auto index = indexOfConfig(config.id());

  if (index >= 0 && index < m_diagnosticConfigs.size())
    m_diagnosticConfigs.replace(index, config);
  else
    m_diagnosticConfigs.append(config);
}

auto ClangDiagnosticConfigsModel::removeConfigWithId(const Utils::Id &id) -> void
{
  m_diagnosticConfigs.removeOne(configWithId(id));
}

auto ClangDiagnosticConfigsModel::allConfigs() const -> ClangDiagnosticConfigs
{
  return m_diagnosticConfigs;
}

auto ClangDiagnosticConfigsModel::customConfigs() const -> ClangDiagnosticConfigs
{
  return Utils::filtered(allConfigs(), [](const ClangDiagnosticConfig &config) {
    return !config.isReadOnly();
  });
}

auto ClangDiagnosticConfigsModel::hasConfigWithId(const Utils::Id &id) const -> bool
{
  return indexOfConfig(id) != -1;
}

auto ClangDiagnosticConfigsModel::configWithId(const Utils::Id &id) const -> const ClangDiagnosticConfig&
{
  return m_diagnosticConfigs.at(indexOfConfig(id));
}

auto ClangDiagnosticConfigsModel::changedOrRemovedConfigs(const ClangDiagnosticConfigs &oldConfigs, const ClangDiagnosticConfigs &newConfigs) -> QVector<Utils::Id>
{
  ClangDiagnosticConfigsModel newConfigsModel(newConfigs);
  QVector<Utils::Id> changedConfigs;

  for (const auto &old : oldConfigs) {
    const auto i = newConfigsModel.indexOfConfig(old.id());
    if (i == -1)
      changedConfigs.append(old.id()); // Removed
    else if (newConfigsModel.allConfigs().value(i) != old)
      changedConfigs.append(old.id()); // Changed
  }

  return changedConfigs;
}

auto ClangDiagnosticConfigsModel::createCustomConfig(const ClangDiagnosticConfig &baseConfig, const QString &displayName) -> ClangDiagnosticConfig
{
  auto copied = baseConfig;
  copied.setId(Utils::Id::fromString(QUuid::createUuid().toString()));
  copied.setDisplayName(displayName);
  copied.setIsReadOnly(false);

  return copied;
}

auto ClangDiagnosticConfigsModel::globalDiagnosticOptions() -> QStringList
{
  return {
    // Avoid undesired warnings from e.g. Q_OBJECT
    QStringLiteral("-Wno-unknown-pragmas"),
    QStringLiteral("-Wno-unknown-warning-option"),

    // qdoc commands
    QStringLiteral("-Wno-documentation-unknown-command")
  };
}

auto ClangDiagnosticConfigsModel::indexOfConfig(const Utils::Id &id) const -> int
{
  return Utils::indexOf(m_diagnosticConfigs, [&](const ClangDiagnosticConfig &config) {
    return config.id() == id;
  });
}

} // namespace CppEditor
