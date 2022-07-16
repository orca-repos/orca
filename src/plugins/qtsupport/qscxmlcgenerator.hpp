// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/extracompiler.hpp>
#include <utils/fileutils.hpp>
#include <utils/temporarydirectory.hpp>

namespace QtSupport {

class QScxmlcGenerator : public ProjectExplorer::ProcessExtraCompiler {
  Q_OBJECT

public:
  QScxmlcGenerator(const ProjectExplorer::Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent = nullptr);

protected:
  auto command() const -> Utils::FilePath override;
  auto arguments() const -> QStringList override;
  auto workingDirectory() const -> Utils::FilePath override;

private:
  auto tmpFile() const -> Utils::FilePath;
  auto handleProcessFinished(Utils::QtcProcess *process) -> ProjectExplorer::FileNameToContentsHash override;
  auto prepareToRun(const QByteArray &sourceContents) -> bool override;
  auto parseIssues(const QByteArray &processStderr) -> ProjectExplorer::Tasks override;

  Utils::TemporaryDirectory m_tmpdir;
  QString m_header;
  QString m_impl;
};

class QScxmlcGeneratorFactory : public ProjectExplorer::ExtraCompilerFactory {
  Q_OBJECT

public:
  QScxmlcGeneratorFactory() = default;

  auto sourceType() const -> ProjectExplorer::FileType override;
  auto sourceTag() const -> QString override;
  auto create(const ProjectExplorer::Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets) -> ProjectExplorer::ExtraCompiler* override;
};

} // QtSupport
