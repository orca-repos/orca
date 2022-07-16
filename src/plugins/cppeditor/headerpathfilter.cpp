// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "headerpathfilter.hpp"

#ifndef UNIT_TESTS
#include <core/core-interface.hpp>
#endif

#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <QRegularExpression>

#include <utils/algorithm.hpp>

using namespace ProjectExplorer;
using namespace Utils;

namespace CppEditor::Internal {

auto HeaderPathFilter::process() -> void
{
  const auto &headerPaths = projectPart.headerPaths;

  addPreIncludesPath();

  for (const auto &headerPath : headerPaths)
    filterHeaderPath(headerPath);

  if (useTweakedHeaderPaths != UseTweakedHeaderPaths::No)
    tweakHeaderPaths();
}

auto HeaderPathFilter::isProjectHeaderPath(const QString &path) const -> bool
{
  return path.startsWith(projectDirectory) || path.startsWith(buildDirectory);
}

auto HeaderPathFilter::removeGccInternalIncludePaths() -> void
{
  if (projectPart.toolchainType != ProjectExplorer::Constants::GCC_TOOLCHAIN_TYPEID && projectPart.toolchainType != ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID) {
    return;
  }

  if (projectPart.toolChainInstallDir.isEmpty())
    return;

  const auto gccInstallDir = projectPart.toolChainInstallDir;
  auto isGccInternalInclude = [gccInstallDir](const HeaderPath &headerPath) {
    const auto filePath = Utils::FilePath::fromString(headerPath.path);
    return filePath == gccInstallDir.pathAppended("include") || filePath == gccInstallDir.pathAppended("include-fixed");
  };

  Utils::erase(builtInHeaderPaths, isGccInternalInclude);
}

auto HeaderPathFilter::filterHeaderPath(const ProjectExplorer::HeaderPath &headerPath) -> void
{
  if (headerPath.path.isEmpty())
    return;

  switch (headerPath.type) {
  case HeaderPathType::BuiltIn:
    builtInHeaderPaths.push_back(headerPath);
    break;
  case HeaderPathType::System:
  case HeaderPathType::Framework:
    systemHeaderPaths.push_back(headerPath);
    break;
  case HeaderPathType::User:
    if (isProjectHeaderPath(headerPath.path))
      userHeaderPaths.push_back(headerPath);
    else
      systemHeaderPaths.push_back(headerPath);
    break;
  }
}

namespace {

auto clangIncludeDirectory(const QString &clangVersion, const FilePath &clangFallbackIncludeDir) -> FilePath
{
  #ifndef UNIT_TESTS
  return Orca::Plugin::Core::ICore::clangIncludeDirectory(clangVersion, clangFallbackIncludeDir);
  #else
    Q_UNUSED(clangVersion)
    Q_UNUSED(clangFallbackIncludeDir)
    return {CLANG_INCLUDE_DIR};
  #endif
}

auto resourceIterator(HeaderPaths &headerPaths) -> HeaderPaths::iterator
{
  // include/c++, include/g++, libc++\include and libc++abi\include
  static const QString cppIncludes = R"((.*/include/.*(g\+\+|c\+\+).*))" R"(|(.*libc\+\+/include))" R"(|(.*libc\+\+abi/include))" R"(|(/usr/local/include))";
  static const QRegularExpression includeRegExp("\\A(" + cppIncludes + ")\\z");

  return std::stable_partition(headerPaths.begin(), headerPaths.end(), [&](const HeaderPath &headerPath) {
    return includeRegExp.match(headerPath.path).hasMatch();
  });
}

auto isClangSystemHeaderPath(const HeaderPath &headerPath) -> bool
{
  // Always exclude clang system includes (including intrinsics) which do not come with libclang
  // that Qt Creator uses for code model.
  // For example GCC on macOS uses system clang include path which makes clang code model
  // include incorrect system headers.
  static const QRegularExpression clangIncludeDir(R"(\A.*/lib\d*/clang/\d+\.\d+(\.\d+)?/include\z)");
  return clangIncludeDir.match(headerPath.path).hasMatch();
}

auto removeClangSystemHeaderPaths(HeaderPaths &headerPaths) -> void
{
  auto newEnd = std::remove_if(headerPaths.begin(), headerPaths.end(), isClangSystemHeaderPath);
  headerPaths.erase(newEnd, headerPaths.end());
}

} // namespace

auto HeaderPathFilter::tweakHeaderPaths() -> void
{
  removeClangSystemHeaderPaths(builtInHeaderPaths);
  removeGccInternalIncludePaths();

  auto split = resourceIterator(builtInHeaderPaths);

  if (!clangVersion.isEmpty()) {
    const auto clangIncludePath = clangIncludeDirectory(clangVersion, clangFallbackIncludeDirectory);
    builtInHeaderPaths.insert(split, HeaderPath::makeBuiltIn(clangIncludePath));
  }
}

auto HeaderPathFilter::addPreIncludesPath() -> void
{
  if (!projectDirectory.isEmpty()) {
    const auto rootProjectDirectory = Utils::FilePath::fromString(projectDirectory).pathAppended(".pre_includes");
    systemHeaderPaths.push_back(ProjectExplorer::HeaderPath::makeSystem(rootProjectDirectory));
  }
}

auto HeaderPathFilter::ensurePathWithSlashEnding(const QString &path) -> QString
{
  auto pathWithSlashEnding = path;
  if (!pathWithSlashEnding.isEmpty() && *pathWithSlashEnding.rbegin() != '/')
    pathWithSlashEnding.push_back('/');

  return pathWithSlashEnding;
}

} // namespace CppEditor::Internal
