// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QString>

#include <functional>

namespace Utils {
class FilePath;
class MimeType;
} // Utils

namespace ProjectExplorer {

class Project;

class PROJECTEXPLORER_EXPORT ProjectManager {
public:
  static auto canOpenProjectForMimeType(const Utils::MimeType &mt) -> bool;
  static auto openProject(const Utils::MimeType &mt, const Utils::FilePath &fileName) -> Project*;

  template <typename T>
  static auto registerProjectType(const QString &mimeType) -> void
  {
    ProjectManager::registerProjectCreator(mimeType, [](const Utils::FilePath &fileName) {
      return new T(fileName);
    });
  }

private:
  static auto registerProjectCreator(const QString &mimeType, const std::function<Project *(const Utils::FilePath &)> &) -> void;
};

} // namespace ProjectExplorer
