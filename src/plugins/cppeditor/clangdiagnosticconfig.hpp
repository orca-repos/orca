// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <utils/id.hpp>

#include <QHash>
#include <QMap>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace CppEditor {

// TODO: Split this class as needed for ClangCodeModel and ClangTools
class CPPEDITOR_EXPORT ClangDiagnosticConfig {
public:
  // Clang-Tidy
  enum class TidyMode {
    // Disabled, // Used by Qt Creator 4.10 and below.
    UseCustomChecks = 1,
    UseConfigFile,
    UseDefaultChecks,
  };
  
  // Clazy
  enum class ClazyMode {
    UseDefaultChecks,
    UseCustomChecks,
  };

  using TidyCheckOptions = QMap<QString, QString>;

  auto id() const -> Utils::Id;
  auto setId(const Utils::Id &id) -> void;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &displayName) -> void;
  auto isReadOnly() const -> bool;
  auto setIsReadOnly(bool isReadOnly) -> void;
  auto clangOptions() const -> QStringList;
  auto setClangOptions(const QStringList &options) -> void;
  auto useBuildSystemWarnings() const -> bool;
  auto setUseBuildSystemWarnings(bool useBuildSystemWarnings) -> void;
  auto clangTidyMode() const -> TidyMode;
  auto setClangTidyMode(TidyMode mode) -> void;
  auto clangTidyChecks() const -> QString;
  auto clangTidyChecksAsJson() const -> QString;
  auto setClangTidyChecks(const QString &checks) -> void;
  auto isClangTidyEnabled() const -> bool;
  auto setTidyCheckOptions(const QString &check, const TidyCheckOptions &options) -> void;
  auto tidyCheckOptions(const QString &check) const -> TidyCheckOptions;
  auto setTidyChecksOptionsFromSettings(const QVariant &options) -> void;
  auto tidyChecksOptionsForSettings() const -> QVariant;
  auto clazyMode() const -> ClazyMode;
  auto setClazyMode(const ClazyMode &clazyMode) -> void;
  auto clazyChecks() const -> QString;
  auto setClazyChecks(const QString &checks) -> void;
  auto isClazyEnabled() const -> bool;
  auto operator==(const ClangDiagnosticConfig &other) const -> bool;
  auto operator!=(const ClangDiagnosticConfig &other) const -> bool;

private:
  Utils::Id m_id;
  QString m_displayName;
  QStringList m_clangOptions;
  TidyMode m_clangTidyMode = TidyMode::UseDefaultChecks;
  QString m_clangTidyChecks;
  QHash<QString, TidyCheckOptions> m_tidyChecksOptions;
  QString m_clazyChecks;
  ClazyMode m_clazyMode = ClazyMode::UseDefaultChecks;
  bool m_isReadOnly = false;
  bool m_useBuildSystemWarnings = false;
};

using ClangDiagnosticConfigs = QVector<ClangDiagnosticConfig>;

CPPEDITOR_EXPORT auto diagnosticConfigsFromSettings(QSettings *s) -> ClangDiagnosticConfigs;
CPPEDITOR_EXPORT auto diagnosticConfigsToSettings(QSettings *s, const ClangDiagnosticConfigs &configs) -> void;

} // namespace CppEditor
