// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/extracompiler.hpp>
#include <utils/fileutils.hpp>

namespace QtSupport {

class UicGenerator : public ProjectExplorer::ProcessExtraCompiler {
  Q_OBJECT

public:
  UicGenerator(const ProjectExplorer::Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent = nullptr);

protected:
  auto command() const -> Utils::FilePath override;
  auto arguments() const -> QStringList override;
  auto handleProcessFinished(Utils::QtcProcess *process) -> ProjectExplorer::FileNameToContentsHash override;
};

class UicGeneratorFactory : public ProjectExplorer::ExtraCompilerFactory {
  Q_OBJECT

public:
  UicGeneratorFactory() = default;

  auto sourceType() const -> ProjectExplorer::FileType override;
  auto sourceTag() const -> QString override;
  auto create(const ProjectExplorer::Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets) -> ProjectExplorer::ExtraCompiler* override;
};

} // QtSupport
