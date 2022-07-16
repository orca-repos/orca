// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentaspect.hpp"

#include <utils/aspects.hpp>

#include <QPointer>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QPlainTextEdit;
class QToolButton;
QT_END_NAMESPACE namespace Utils {
class ExpandButton;
}

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT TerminalAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  TerminalAspect();

  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  auto useTerminal() const -> bool;
  auto setUseTerminalHint(bool useTerminal) -> void;
  auto isUserSet() const -> bool;

private:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;
  auto calculateUseTerminal() -> void;

  bool m_useTerminalHint = false;
  bool m_useTerminal = false;
  bool m_userSet = false;
  QPointer<QCheckBox> m_checkBox; // Owned by RunConfigWidget
};

class PROJECTEXPLORER_EXPORT WorkingDirectoryAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  WorkingDirectoryAspect();

  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  auto acquaintSiblings(const Utils::AspectContainer &) -> void override;
  auto workingDirectory() const -> Utils::FilePath;
  auto defaultWorkingDirectory() const -> Utils::FilePath;
  auto unexpandedWorkingDirectory() const -> Utils::FilePath;
  auto setDefaultWorkingDirectory(const Utils::FilePath &defaultWorkingDirectory) -> void;
  auto setMacroExpander(Utils::MacroExpander *macroExpander) -> void;
  auto pathChooser() const -> Utils::PathChooser*;

private:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;
  auto resetPath() -> void;

  EnvironmentAspect *m_envAspect = nullptr;
  Utils::FilePath m_workingDirectory;
  Utils::FilePath m_defaultWorkingDirectory;
  QPointer<Utils::PathChooser> m_chooser;
  QPointer<QToolButton> m_resetButton;
  Utils::MacroExpander *m_macroExpander = nullptr;
};

class PROJECTEXPLORER_EXPORT ArgumentsAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  ArgumentsAspect();

  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  auto arguments(const Utils::MacroExpander *expander) const -> QString;
  auto unexpandedArguments() const -> QString;
  auto setArguments(const QString &arguments) -> void;
  auto setLabelText(const QString &labelText) -> void;
  auto setResetter(const std::function<QString()> &resetter) -> void;
  auto resetArguments() -> void;

private:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;
  auto setupChooser() -> QWidget*;

  QString m_arguments;
  QString m_labelText;
  QPointer<Utils::FancyLineEdit> m_chooser;
  QPointer<QPlainTextEdit> m_multiLineChooser;
  QPointer<Utils::ExpandButton> m_multiLineButton;
  QPointer<QToolButton> m_resetButton;
  bool m_multiLine = false;
  mutable bool m_currentlyExpanding = false;
  std::function<QString()> m_resetter;
};

class PROJECTEXPLORER_EXPORT UseLibraryPathsAspect : public Utils::BoolAspect {
  Q_OBJECT

public:
  UseLibraryPathsAspect();
};

class PROJECTEXPLORER_EXPORT UseDyldSuffixAspect : public Utils::BoolAspect {
  Q_OBJECT

public:
  UseDyldSuffixAspect();
};

class PROJECTEXPLORER_EXPORT RunAsRootAspect : public Utils::BoolAspect {
  Q_OBJECT

public:
  RunAsRootAspect();
};

class PROJECTEXPLORER_EXPORT ExecutableAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  ExecutableAspect();
  ~ExecutableAspect() override;

  auto executable() const -> Utils::FilePath;
  auto setExecutable(const Utils::FilePath &executable) -> void;
  auto setSettingsKey(const QString &key) -> void;
  auto makeOverridable(const QString &overridingKey, const QString &useOverridableKey) -> void;
  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  auto setLabelText(const QString &labelText) -> void;
  auto setPlaceHolderText(const QString &placeHolderText) -> void;
  auto setExecutablePathStyle(Utils::OsType osType) -> void;
  auto setHistoryCompleter(const QString &historyCompleterKey) -> void;
  auto setExpectedKind(const Utils::PathChooser::Kind expectedKind) -> void;
  auto setEnvironmentChange(const Utils::EnvironmentChange &change) -> void;
  auto setDisplayStyle(Utils::StringAspect::DisplayStyle style) -> void;

protected:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;

private:
  auto executableText() const -> QString;

  Utils::StringAspect m_executable;
  Utils::StringAspect *m_alternativeExecutable = nullptr;
};

class PROJECTEXPLORER_EXPORT SymbolFileAspect : public Utils::StringAspect {
  Q_OBJECT

public:
  SymbolFileAspect() = default;
};

} // namespace ProjectExplorer
