// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "clangdiagnosticconfig.hpp"

#include <QVector>

namespace CppEditor {

class CPPEDITOR_EXPORT ClangDiagnosticConfigsModel {
public:
  ClangDiagnosticConfigsModel() = default;
  explicit ClangDiagnosticConfigsModel(const ClangDiagnosticConfigs &configs);

  auto size() const -> int;
  auto at(int index) const -> const ClangDiagnosticConfig&;
  auto appendOrUpdate(const ClangDiagnosticConfig &config) -> void;
  auto removeConfigWithId(const Utils::Id &id) -> void;
  auto allConfigs() const -> ClangDiagnosticConfigs;
  auto customConfigs() const -> ClangDiagnosticConfigs;
  auto hasConfigWithId(const Utils::Id &id) const -> bool;
  auto configWithId(const Utils::Id &id) const -> const ClangDiagnosticConfig&;
  auto indexOfConfig(const Utils::Id &id) const -> int;
  static auto changedOrRemovedConfigs(const ClangDiagnosticConfigs &oldConfigs, const ClangDiagnosticConfigs &newConfigs) -> QVector<Utils::Id>;
  static auto createCustomConfig(const ClangDiagnosticConfig &baseConfig, const QString &displayName) -> ClangDiagnosticConfig;
  static auto globalDiagnosticOptions() -> QStringList;

private:
  ClangDiagnosticConfigs m_diagnosticConfigs;
};

} // namespace CppEditor
