// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include "cmaketool.hpp"

#include <utils/fileutils.hpp>
#include <utils/id.hpp>

#include <QObject>

#include <memory>

namespace CMakeProjectManager {

class CMAKE_EXPORT CMakeToolManager : public QObject {
  Q_OBJECT

public:
  CMakeToolManager();
  ~CMakeToolManager();

  static auto instance() -> CMakeToolManager*;
  static auto cmakeTools() -> QList<CMakeTool*>;
  static auto registerCMakeTool(std::unique_ptr<CMakeTool> &&tool) -> bool;
  static auto deregisterCMakeTool(const Utils::Id &id) -> void;
  static auto defaultCMakeTool() -> CMakeTool*;
  static auto setDefaultCMakeTool(const Utils::Id &id) -> void;
  static auto findByCommand(const Utils::FilePath &command) -> CMakeTool*;
  static auto findById(const Utils::Id &id) -> CMakeTool*;
  static auto notifyAboutUpdate(CMakeTool *) -> void;
  static auto restoreCMakeTools() -> void;
  static auto updateDocumentation() -> void;

public slots:
  auto autoDetectCMakeForDevice(const Utils::FilePaths &searchPaths, const QString &detectionSource, QString *logMessage) -> void;
  auto registerCMakeByPath(const Utils::FilePath &cmakePath, const QString &detectionSource) -> void;
  auto removeDetectedCMake(const QString &detectionSource, QString *logMessage) -> void;
  auto listDetectedCMake(const QString &detectionSource, QString *logMessage) -> void;

signals:
  auto cmakeAdded(const Utils::Id &id) -> void;
  auto cmakeRemoved(const Utils::Id &id) -> void;
  auto cmakeUpdated(const Utils::Id &id) -> void;
  auto cmakeToolsChanged() -> void;
  auto cmakeToolsLoaded() -> void;
  auto defaultCMakeChanged() -> void;

private:
  static auto saveCMakeTools() -> void;
  static auto ensureDefaultCMakeToolIsValid() -> void;

  static CMakeToolManager *m_instance;
};

} // namespace CMakeProjectManager

Q_DECLARE_METATYPE(QString *)
