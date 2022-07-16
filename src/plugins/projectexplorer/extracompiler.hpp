// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectnodes.hpp"
#include "project.hpp"
#include "task.hpp"

#include <core/core-editor-interface.hpp>
#include <utils/fileutils.hpp>
#include <utils/environment.hpp>

#include <QByteArray>
#include <QHash>
#include <QList>

#include <functional>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QThreadPool);
QT_BEGIN_NAMESPACE
template <typename T>
class QFutureInterface;
template <typename T>
class QFutureWatcher;
QT_END_NAMESPACE

namespace Utils {
class QtcProcess;
}

namespace ProjectExplorer {

class ExtraCompilerPrivate;
using FileNameToContentsHash = QHash<Utils::FilePath, QByteArray>;

class PROJECTEXPLORER_EXPORT ExtraCompiler : public QObject {
  Q_OBJECT

public:
  ExtraCompiler(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent = nullptr);
  ~ExtraCompiler() override;

  auto project() const -> const Project*;
  auto source() const -> Utils::FilePath;

  // You can set the contents from the outside. This is done if the file has been (re)created by
  // the regular build process.
  auto setContent(const Utils::FilePath &file, const QByteArray &content) -> void;
  auto content(const Utils::FilePath &file) const -> QByteArray;
  auto targets() const -> Utils::FilePaths;
  auto forEachTarget(std::function<void(const Utils::FilePath &)> func) -> void;
  auto setCompileTime(const QDateTime &time) -> void;
  auto compileTime() const -> QDateTime;
  static auto extraCompilerThreadPool() -> QThreadPool*;
  virtual auto run() -> QFuture<FileNameToContentsHash> = 0;
  auto isDirty() const -> bool;

signals:
  auto contentsChanged(const Utils::FilePath &file) -> void;

protected:
  auto buildEnvironment() const -> Utils::Environment;
  auto setCompileIssues(const Tasks &issues) -> void;

private:
  auto onTargetsBuilt(Project *project) -> void;
  auto onEditorChanged(Orca::Plugin::Core::IEditor *editor) -> void;
  auto onEditorAboutToClose(Orca::Plugin::Core::IEditor *editor) -> void;
  auto setDirty() -> void;
  // This method may not block!
  virtual auto run(const QByteArray &sourceContent) -> void = 0;

  const std::unique_ptr<ExtraCompilerPrivate> d;
};

class PROJECTEXPLORER_EXPORT ProcessExtraCompiler : public ExtraCompiler {
  Q_OBJECT

public:
  ProcessExtraCompiler(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent = nullptr);
  ~ProcessExtraCompiler() override;

protected:
  // This will run a process in a thread, if
  //  * command() does not return an empty file name
  //  * command() is exectuable
  //  * prepareToRun returns true
  //  * The process is not yet running
  auto run(const QByteArray &sourceContents) -> void override;
  auto run() -> QFuture<FileNameToContentsHash> override;

  // Information about the process to run:
  virtual auto workingDirectory() const -> Utils::FilePath;
  virtual auto command() const -> Utils::FilePath = 0;
  virtual auto arguments() const -> QStringList;
  virtual auto prepareToRun(const QByteArray &sourceContents) -> bool;
  virtual auto handleProcessFinished(Utils::QtcProcess *process) -> FileNameToContentsHash = 0;
  virtual auto parseIssues(const QByteArray &stdErr) -> Tasks;

private:
  using ContentProvider = std::function<QByteArray()>;
  auto runImpl(const ContentProvider &sourceContents) -> QFuture<FileNameToContentsHash>;
  auto runInThread(QFutureInterface<FileNameToContentsHash> &futureInterface, const Utils::FilePath &cmd, const Utils::FilePath &workDir, const QStringList &args, const ContentProvider &provider, const Utils::Environment &env) -> void;
  auto cleanUp() -> void;

  QFutureWatcher<FileNameToContentsHash> *m_watcher = nullptr;
};

class PROJECTEXPLORER_EXPORT ExtraCompilerFactory : public QObject {
  Q_OBJECT

public:
  explicit ExtraCompilerFactory(QObject *parent = nullptr);
  ~ExtraCompilerFactory() override;

  virtual auto sourceType() const -> FileType = 0;
  virtual auto sourceTag() const -> QString = 0;
  virtual auto create(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets) -> ExtraCompiler* = 0;
  static auto extraCompilerFactories() -> QList<ExtraCompilerFactory*>;
};

} // namespace ProjectExplorer
