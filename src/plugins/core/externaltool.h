// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/id.h>

#include <QObject>
#include <QProcess>
#include <QSharedPointer>
#include <QTextCodec>
#include <QMetaType>

namespace Utils {
class QtcProcess;
}

namespace Core {
namespace Internal {

class ExternalTool : public QObject {
  Q_OBJECT

public:
  enum OutputHandling {
    Ignore,
    ShowInPane,
    ReplaceSelection
  };

  ExternalTool();
  explicit ExternalTool(const ExternalTool *other);
  ~ExternalTool() override;

  auto id() const -> QString;
  auto description() const -> QString;
  auto displayName() const -> QString;
  auto displayCategory() const -> QString;
  auto order() const -> int;
  auto outputHandling() const -> OutputHandling;
  auto errorHandling() const -> OutputHandling;
  auto modifiesCurrentDocument() const -> bool;
  auto executables() const -> Utils::FilePaths;
  auto arguments() const -> QString;
  auto input() const -> QString;
  auto workingDirectory() const -> Utils::FilePath;
  auto baseEnvironmentProviderId() const -> Utils::Id;
  auto baseEnvironment() const -> Utils::Environment;
  auto environmentUserChanges() const -> Utils::EnvironmentItems;
  auto setFileName(const Utils::FilePath &fileName) -> void;
  auto setPreset(QSharedPointer<ExternalTool> preset) -> void;
  auto fileName() const -> Utils::FilePath;
  // all tools that are preset (changed or unchanged) have the original value here:
  auto preset() const -> QSharedPointer<ExternalTool>;
  static auto createFromXml(const QByteArray &xml, QString *errorMessage = nullptr, const QString &locale = QString()) -> ExternalTool*;
  static auto createFromFile(const Utils::FilePath &fileName, QString *errorMessage = nullptr, const QString &locale = QString()) -> ExternalTool*;
  auto save(QString *errorMessage = nullptr) const -> bool;
  auto operator==(const ExternalTool &other) const -> bool;
  auto operator!=(const ExternalTool &other) const -> bool { return !((*this) == other); }
  auto operator=(const ExternalTool &other) -> ExternalTool&;
  auto setId(const QString &id) -> void;
  auto setDisplayCategory(const QString &category) -> void;
  auto setDisplayName(const QString &name) -> void;
  auto setDescription(const QString &description) -> void;
  auto setOutputHandling(OutputHandling handling) -> void;
  auto setErrorHandling(OutputHandling handling) -> void;
  auto setModifiesCurrentDocument(bool modifies) -> void;
  auto setExecutables(const Utils::FilePaths &executables) -> void;
  auto setArguments(const QString &arguments) -> void;
  auto setInput(const QString &input) -> void;
  auto setWorkingDirectory(const Utils::FilePath &workingDirectory) -> void;
  auto setBaseEnvironmentProviderId(Utils::Id id) -> void;
  auto setEnvironmentUserChanges(const Utils::EnvironmentItems &items) -> void;

private:
  QString m_id;
  QString m_description;
  QString m_displayName;
  QString m_displayCategory;
  int m_order = -1;
  Utils::FilePaths m_executables;
  QString m_arguments;
  QString m_input;
  Utils::FilePath m_workingDirectory;
  Utils::Id m_baseEnvironmentProviderId;
  Utils::EnvironmentItems m_environment;
  OutputHandling m_outputHandling = ShowInPane;
  OutputHandling m_errorHandling = ShowInPane;
  bool m_modifiesCurrentDocument = false;
  Utils::FilePath m_filePath;
  Utils::FilePath m_presetFileName;
  QSharedPointer<ExternalTool> m_presetTool;
};

class ExternalToolRunner : public QObject {
  Q_OBJECT

public:
  ExternalToolRunner(const ExternalTool *tool);
  ~ExternalToolRunner() override;

  auto hasError() const -> bool;
  auto errorString() const -> QString;

private:
  auto finished() -> void;
  auto error(QProcess::ProcessError error) -> void;
  auto readStandardOutput() -> void;
  auto readStandardError() -> void;
  auto run() -> void;
  auto resolve() -> bool;

  const ExternalTool *m_tool; // is a copy of the tool that was passed in
  Utils::FilePath m_resolvedExecutable;
  QString m_resolvedArguments;
  QString m_resolvedInput;
  Utils::FilePath m_resolvedWorkingDirectory;
  Utils::Environment m_resolvedEnvironment;
  Utils::QtcProcess *m_process;
  QTextCodec *m_outputCodec;
  QTextCodec::ConverterState m_outputCodecState;
  QTextCodec::ConverterState m_errorCodecState;
  QString m_processOutput;
  Utils::FilePath m_expectedFilePath;
  bool m_hasError;
  QString m_errorString;
};

} // Internal
} // Core

Q_DECLARE_METATYPE(Core::Internal::ExternalTool *)
