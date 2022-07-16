// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include "cmakeconfigitem.hpp"

#include <projectexplorer/kitmanager.hpp>

namespace CMakeProjectManager {
class CMakeTool;

class CMAKE_EXPORT CMakeKitAspect : public ProjectExplorer::KitAspect {
  Q_OBJECT

public:
  CMakeKitAspect();

  static auto id() -> Utils::Id;

  static auto cmakeToolId(const ProjectExplorer::Kit *k) -> Utils::Id;
  static auto cmakeTool(const ProjectExplorer::Kit *k) -> CMakeTool*;
  static auto setCMakeTool(ProjectExplorer::Kit *k, const Utils::Id id) -> void;

  // KitAspect interface
  auto validate(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks final;
  auto setup(ProjectExplorer::Kit *k) -> void final;
  auto fix(ProjectExplorer::Kit *k) -> void final;
  auto toUserOutput(const ProjectExplorer::Kit *k) const -> ItemList final;
  auto createConfigWidget(ProjectExplorer::Kit *k) const -> ProjectExplorer::KitAspectWidget* final;
  auto addToMacroExpander(ProjectExplorer::Kit *k, Utils::MacroExpander *expander) const -> void final;
  auto availableFeatures(const ProjectExplorer::Kit *k) const -> QSet<Utils::Id> final;
  static auto msgUnsupportedVersion(const QByteArray &versionString) -> QString;
};

class CMAKE_EXPORT CMakeGeneratorKitAspect : public ProjectExplorer::KitAspect {
  Q_OBJECT

public:
  CMakeGeneratorKitAspect();

  static auto generator(const ProjectExplorer::Kit *k) -> QString;
  static auto extraGenerator(const ProjectExplorer::Kit *k) -> QString;
  static auto platform(const ProjectExplorer::Kit *k) -> QString;
  static auto toolset(const ProjectExplorer::Kit *k) -> QString;
  static auto setGenerator(ProjectExplorer::Kit *k, const QString &generator) -> void;
  static auto setExtraGenerator(ProjectExplorer::Kit *k, const QString &extraGenerator) -> void;
  static auto setPlatform(ProjectExplorer::Kit *k, const QString &platform) -> void;
  static auto setToolset(ProjectExplorer::Kit *k, const QString &toolset) -> void;
  static auto set(ProjectExplorer::Kit *k, const QString &generator, const QString &extraGenerator, const QString &platform, const QString &toolset) -> void;
  static auto generatorArguments(const ProjectExplorer::Kit *k) -> QStringList;
  static auto generatorCMakeConfig(const ProjectExplorer::Kit *k) -> CMakeConfig;
  static auto isMultiConfigGenerator(const ProjectExplorer::Kit *k) -> bool;

  // KitAspect interface
  auto validate(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks final;
  auto setup(ProjectExplorer::Kit *k) -> void final;
  auto fix(ProjectExplorer::Kit *k) -> void final;
  auto upgrade(ProjectExplorer::Kit *k) -> void final;
  auto toUserOutput(const ProjectExplorer::Kit *k) const -> ItemList final;
  auto createConfigWidget(ProjectExplorer::Kit *k) const -> ProjectExplorer::KitAspectWidget* final;
  auto addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const -> void final;

private:
  auto defaultValue(const ProjectExplorer::Kit *k) const -> QVariant;
};

class CMAKE_EXPORT CMakeConfigurationKitAspect : public ProjectExplorer::KitAspect {
  Q_OBJECT

public:
  CMakeConfigurationKitAspect();

  static auto configuration(const ProjectExplorer::Kit *k) -> CMakeConfig;
  static auto setConfiguration(ProjectExplorer::Kit *k, const CMakeConfig &config) -> void;
  static auto additionalConfiguration(const ProjectExplorer::Kit *k) -> QString;
  static auto setAdditionalConfiguration(ProjectExplorer::Kit *k, const QString &config) -> void;
  static auto toStringList(const ProjectExplorer::Kit *k) -> QStringList;
  static auto fromStringList(ProjectExplorer::Kit *k, const QStringList &in) -> void;
  static auto toArgumentsList(const ProjectExplorer::Kit *k) -> QStringList;
  static auto defaultConfiguration(const ProjectExplorer::Kit *k) -> CMakeConfig;

  // KitAspect interface
  auto validate(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks final;
  auto setup(ProjectExplorer::Kit *k) -> void final;
  auto fix(ProjectExplorer::Kit *k) -> void final;
  auto toUserOutput(const ProjectExplorer::Kit *k) const -> ItemList final;
  auto createConfigWidget(ProjectExplorer::Kit *k) const -> ProjectExplorer::KitAspectWidget* final;

private:
  auto defaultValue(const ProjectExplorer::Kit *k) const -> QVariant;
};

} // namespace CMakeProjectManager
