// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectconfiguration.hpp"

#include "buildconfiguration.hpp"
#include "projectexplorer_export.hpp"

#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>

#include <QFutureInterface>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

namespace Utils {
class Environment;
class FilePath;
class MacroExpander;
class OutputFormatter;
} // Utils

namespace ProjectExplorer {

class BuildConfiguration;
class BuildStepFactory;
class BuildStepList;
class BuildSystem;
class DeployConfiguration;
class Task;

class PROJECTEXPLORER_EXPORT BuildStep : public ProjectConfiguration {
  Q_OBJECT

protected:
  friend class BuildStepFactory;
  explicit BuildStep(BuildStepList *bsl, Utils::Id id);

public:
  ~BuildStep() override;

  virtual auto init() -> bool = 0;
  auto run() -> void;
  auto cancel() -> void;
  auto fromMap(const QVariantMap &map) -> bool override;
  auto toMap() const -> QVariantMap override;
  auto enabled() const -> bool;
  auto setEnabled(bool b) -> void;
  auto stepList() const -> BuildStepList*;
  auto buildConfiguration() const -> BuildConfiguration*;
  auto deployConfiguration() const -> DeployConfiguration*;
  auto projectConfiguration() const -> ProjectConfiguration*;
  auto buildSystem() const -> BuildSystem*;
  auto buildEnvironment() const -> Utils::Environment;
  auto buildDirectory() const -> Utils::FilePath;
  auto buildType() const -> BuildConfiguration::BuildType;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto fallbackWorkingDirectory() const -> QString;
  virtual auto setupOutputFormatter(Utils::OutputFormatter *formatter) -> void;

  enum class OutputFormat {
    Stdout,
    Stderr,
    // These are for forwarded output from external tools
    NormalMessage,
    ErrorMessage // These are for messages from Creator itself
  };

  enum OutputNewlineSetting {
    DoAppendNewline,
    DontAppendNewline
  };

  static auto reportRunResult(QFutureInterface<bool> &fi, bool success) -> void;
  auto widgetExpandedByDefault() const -> bool;
  auto setWidgetExpandedByDefault(bool widgetExpandedByDefault) -> void;
  auto hasUserExpansionState() const -> bool { return m_wasExpanded.has_value(); }
  auto wasUserExpanded() const -> bool { return m_wasExpanded.value_or(false); }
  auto setUserExpanded(bool expanded) -> void { m_wasExpanded = expanded; }
  auto isImmutable() const -> bool { return m_immutable; }
  auto setImmutable(bool immutable) -> void { m_immutable = immutable; }
  virtual auto data(Utils::Id id) const -> QVariant;
  auto setSummaryUpdater(const std::function<QString ()> &summaryUpdater) -> void;
  auto addMacroExpander() -> void;
  auto summaryText() const -> QString;
  auto setSummaryText(const QString &summaryText) -> void;
  auto doCreateConfigWidget() -> QWidget*;

signals:
  auto updateSummary() -> void;

  /// Adds a \p task to the Issues pane.
  /// Do note that for linking compile output with tasks, you should first emit the output
  /// and then emit the task. \p linkedOutput lines will be linked. And the last \p skipLines will
  /// be skipped.
  auto addTask(const Task &task, int linkedOutputLines = 0, int skipLines = 0) -> void;
  /// Adds \p string to the compile output view, formatted in \p format
  auto addOutput(const QString &string, OutputFormat format, OutputNewlineSetting newlineSetting = DoAppendNewline) -> void;
  auto enabledChanged() -> void;
  auto progress(int percentage, const QString &message) -> void;
  auto finished(bool result) -> void;

protected:
  virtual auto createConfigWidget() -> QWidget*;
  auto runInThread(const std::function<bool()> &syncImpl) -> void;
  auto cancelChecker() const -> std::function<bool()>;
  auto isCanceled() const -> bool;

private:
  using ProjectConfiguration::parent;

  virtual auto doRun() -> void = 0;
  virtual auto doCancel() -> void;

  std::atomic_bool m_cancelFlag;
  bool m_enabled = true;
  bool m_immutable = false;
  bool m_widgetExpandedByDefault = true;
  bool m_runInGuiThread = true;
  bool m_addMacroExpander = false;
  Utils::optional<bool> m_wasExpanded;
  std::function<QString()> m_summaryUpdater;
  QString m_summaryText;
};

class PROJECTEXPLORER_EXPORT BuildStepInfo {
public:
  enum Flags {
    Uncreatable = 1 << 0,
    Unclonable = 1 << 1,
    UniqueStep = 1 << 8 // Can't be used twice in a BuildStepList
  };

  using BuildStepCreator = std::function<BuildStep *(BuildStepList *)>;

  Utils::Id id;
  QString displayName;
  Flags flags = Flags();
  BuildStepCreator creator;
};

class PROJECTEXPLORER_EXPORT BuildStepFactory {
public:
  BuildStepFactory();
  BuildStepFactory(const BuildStepFactory &) = delete;
  virtual ~BuildStepFactory();

  auto operator=(const BuildStepFactory &) -> BuildStepFactory& = delete;

  static auto allBuildStepFactories() -> const QList<BuildStepFactory*>;
  auto stepInfo() const -> BuildStepInfo;
  auto stepId() const -> Utils::Id;
  auto create(BuildStepList *parent) -> BuildStep*;
  auto restore(BuildStepList *parent, const QVariantMap &map) -> BuildStep*;
  auto canHandle(BuildStepList *bsl) const -> bool;

protected:
  using BuildStepCreator = std::function<BuildStep *(BuildStepList *)>;

  template <class BuildStepType>
  auto registerStep(Utils::Id id) -> void
  {
    QTC_CHECK(!m_info.creator);
    m_info.id = id;
    m_info.creator = [id](BuildStepList *bsl) { return new BuildStepType(bsl, id); };
  }

  auto setSupportedStepList(Utils::Id id) -> void;
  auto setSupportedStepLists(const QList<Utils::Id> &ids) -> void;
  auto setSupportedConfiguration(Utils::Id id) -> void;
  auto setSupportedProjectType(Utils::Id id) -> void;
  auto setSupportedDeviceType(Utils::Id id) -> void;
  auto setSupportedDeviceTypes(const QList<Utils::Id> &ids) -> void;
  auto setRepeatable(bool on) -> void { m_isRepeatable = on; }
  auto setDisplayName(const QString &displayName) -> void;
  auto setFlags(BuildStepInfo::Flags flags) -> void;

private:
  BuildStepInfo m_info;
  Utils::Id m_supportedProjectType;
  QList<Utils::Id> m_supportedDeviceTypes;
  QList<Utils::Id> m_supportedStepLists;
  Utils::Id m_supportedConfiguration;
  bool m_isRepeatable = true;
};

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::BuildStep::OutputFormat)
Q_DECLARE_METATYPE(ProjectExplorer::BuildStep::OutputNewlineSetting)
