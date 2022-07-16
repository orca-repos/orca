// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditorconstants.hpp"

#include <core/core-options-page-interface.hpp>

#include <QDir>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace CppEditor::Internal {

class CppFileSettings {
public:
  QStringList headerPrefixes;
  QString headerSuffix = "h";
  QStringList headerSearchPaths = {"include", "Include", QDir::toNativeSeparators("../include"), QDir::toNativeSeparators("../Include")};
  QStringList sourcePrefixes;
  QString sourceSuffix = "cpp";
  QStringList sourceSearchPaths = {QDir::toNativeSeparators("../src"), QDir::toNativeSeparators("../Src"), ".."};
  QString licenseTemplatePath;
  bool headerPragmaOnce = false;
  bool lowerCaseFiles = Constants::LOWERCASE_CPPFILES_DEFAULT;

  auto toSettings(QSettings *) const -> void;
  auto fromSettings(QSettings *) -> void;
  auto applySuffixesToMimeDB() -> bool;

  // Convenience to return a license template completely formatted.
  // Currently made public in
  static auto licenseTemplate() -> QString;

  auto equals(const CppFileSettings &rhs) const -> bool;
  auto operator==(const CppFileSettings &s) const -> bool { return equals(s); }
  auto operator!=(const CppFileSettings &s) const -> bool { return !equals(s); }
};

class CppFileSettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  explicit CppFileSettingsPage(CppFileSettings *settings);
};

} // namespace CppEditor::Internal
