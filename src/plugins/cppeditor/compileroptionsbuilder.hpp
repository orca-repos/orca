// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "projectpart.hpp"

namespace CppEditor {

enum class UsePrecompiledHeaders : char {
  Yes,
  No
};

enum class UseSystemHeader : char {
  Yes,
  No
};

enum class UseTweakedHeaderPaths : char {
  Yes,
  Tools,
  No
};

enum class UseToolchainMacros : char {
  Yes,
  No
};

enum class UseLanguageDefines : char {
  Yes,
  No
};

enum class UseBuildSystemWarnings : char {
  Yes,
  No
};

CPPEDITOR_EXPORT auto XclangArgs(const QStringList &args) -> QStringList;
CPPEDITOR_EXPORT auto clangArgsForCl(const QStringList &args) -> QStringList;
CPPEDITOR_EXPORT auto createLanguageOptionGcc(ProjectFile::Kind fileKind, bool objcExt) -> QStringList;

class CPPEDITOR_EXPORT CompilerOptionsBuilder {
public:
  CompilerOptionsBuilder(const ProjectPart &projectPart, UseSystemHeader useSystemHeader = UseSystemHeader::No, UseTweakedHeaderPaths useTweakedHeaderPaths = UseTweakedHeaderPaths::No, UseLanguageDefines useLanguageDefines = UseLanguageDefines::No, UseBuildSystemWarnings useBuildSystemWarnings = UseBuildSystemWarnings::No, const QString &clangVersion = {}, const Utils::FilePath &clangIncludeDirectory = {});
  virtual ~CompilerOptionsBuilder();

  auto build(ProjectFile::Kind fileKind, UsePrecompiledHeaders usePrecompiledHeaders) -> QStringList;
  auto options() const -> QStringList { return m_options; }

  // Add options based on project part
  virtual auto addProjectMacros() -> void;
  auto addSyntaxOnly() -> void;
  auto addWordWidth() -> void;
  auto addHeaderPathOptions() -> void;
  auto addPrecompiledHeaderOptions(UsePrecompiledHeaders usePrecompiledHeaders) -> void;
  auto addIncludedFiles(const QStringList &files) -> void;
  auto addMacros(const ProjectExplorer::Macros &macros) -> void;
  auto addTargetTriple() -> void;
  auto addExtraCodeModelFlags() -> void;
  auto addPicIfCompilerFlagsContainsIt() -> void;
  auto addCompilerFlags() -> void;
  auto addMsvcExceptions() -> void;
  auto enableExceptions() -> void;
  auto insertWrappedQtHeaders() -> void;
  auto insertWrappedMingwHeaders() -> void;
  auto addLanguageVersionAndExtensions() -> void;
  auto updateFileLanguage(ProjectFile::Kind fileKind) -> void;
  auto addMsvcCompatibilityVersion() -> void;
  auto undefineCppLanguageFeatureMacrosForMsvc2015() -> void;
  auto addDefineFunctionMacrosMsvc() -> void;
  auto addProjectConfigFileInclude() -> void;
  auto undefineClangVersionMacrosForMsvc() -> void;
  auto addDefineFunctionMacrosQnx() -> void;

  // Add custom options
  auto add(const QString &arg, bool gccOnlyOption = false) -> void;
  auto prepend(const QString &arg) -> void;
  auto add(const QStringList &args, bool gccOnlyOptions = false) -> void;
  virtual auto addExtraOptions() -> void {}
  static auto useToolChainMacros() -> UseToolchainMacros;
  auto reset() -> void;
  auto evaluateCompilerFlags() -> void;
  auto isClStyle() const -> bool;

private:
  auto addIncludeDirOptionForPath(const ProjectExplorer::HeaderPath &path) -> void;
  auto excludeDefineDirective(const ProjectExplorer::Macro &macro) const -> bool;
  auto insertWrappedHeaders(const QStringList &paths) -> void;
  auto wrappedQtHeadersIncludePath() const -> QStringList;
  auto wrappedMingwHeadersIncludePath() const -> QStringList;
  auto msvcVersion() const -> QByteArray;
  auto addIncludeFile(const QString &file) -> void;

  const ProjectPart &m_projectPart;
  const UseSystemHeader m_useSystemHeader;
  const UseTweakedHeaderPaths m_useTweakedHeaderPaths;
  const UseLanguageDefines m_useLanguageDefines;
  const UseBuildSystemWarnings m_useBuildSystemWarnings;
  const QString m_clangVersion;
  const Utils::FilePath m_clangIncludeDirectory;

  struct {
    QStringList flags;
    bool isLanguageVersionSpecified = false;
  } m_compilerFlags;

  QStringList m_options;
  QString m_explicitTarget;
  bool m_clStyle = false;
};

} // namespace CppEditor
