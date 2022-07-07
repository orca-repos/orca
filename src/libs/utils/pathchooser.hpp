// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fancylineedit.hpp"
#include "fileutils.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QAbstractButton;
class QLineEdit;
QT_END_NAMESPACE


namespace Utils {

class CommandLine;
class FancyLineEdit;
class MacroExpander;
class Environment;
class EnvironmentChange;
class PathChooserPrivate;

class ORCA_UTILS_EXPORT PathChooser : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged DESIGNABLE true)
  Q_PROPERTY(QString promptDialogTitle READ promptDialogTitle WRITE setPromptDialogTitle DESIGNABLE true)
  Q_PROPERTY(QString promptDialogFilter READ promptDialogFilter WRITE setPromptDialogFilter DESIGNABLE true)
  Q_PROPERTY(Kind expectedKind READ expectedKind WRITE setExpectedKind DESIGNABLE true)
  Q_PROPERTY(Utils::FilePath baseDirectory READ baseDirectory WRITE setBaseDirectory DESIGNABLE true)
  Q_PROPERTY(QStringList commandVersionArguments READ commandVersionArguments WRITE setCommandVersionArguments)
  Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly DESIGNABLE true)
  // Designer does not know this type, so force designable to false:
  Q_PROPERTY(Utils::FilePath filePath READ filePath WRITE setFilePath DESIGNABLE false)public:

  static auto browseButtonLabel() -> QString;

  explicit PathChooser(QWidget *parent = nullptr);
  ~PathChooser() override;

  enum Kind {
    ExistingDirectory,
    Directory,
    // A directory, doesn't need to exist
    File,
    SaveFile,
    ExistingCommand,
    // A command that must exist at the time of selection
    Command,
    // A command that may or may not exist at the time of selection (e.g. result of a build)
    Any
  };

  Q_ENUM(Kind)

  // Default is <Directory>
  auto setExpectedKind(Kind expected) -> void;
  auto expectedKind() const -> Kind;
  auto setPromptDialogTitle(const QString &title) -> void;
  auto promptDialogTitle() const -> QString;
  auto setPromptDialogFilter(const QString &filter) -> void;
  auto promptDialogFilter() const -> QString;
  auto setInitialBrowsePathBackup(const FilePath &path) -> void;
  auto isValid() const -> bool;
  auto errorMessage() const -> QString;
  auto filePath() const -> FilePath;         // Close to what's in the line edit.
  auto absoluteFilePath() const -> FilePath; // Relative paths resolved wrt the specified base dir.
  auto rawPath() const -> QString;      // The raw unexpanded input.
  auto rawFilePath() const -> FilePath; // The raw unexpanded input as FilePath.

  static auto expandedDirectory(const QString &input, const Environment &env, const QString &baseDir) -> QString;

  auto baseDirectory() const -> FilePath;
  auto setBaseDirectory(const FilePath &base) -> void;
  auto setEnvironmentChange(const EnvironmentChange &change) -> void;

  /** Returns the suggested label title when used in a form layout. */
  static auto label() -> QString;
  auto defaultValidationFunction() const -> FancyLineEdit::ValidationFunction;
  auto setValidationFunction(const FancyLineEdit::ValidationFunction &fn) -> void;

  /** Return the home directory, which needs some fixing under Windows. */
  static auto homePath() -> FilePath;
  auto addButton(const QString &text, QObject *context, const std::function<void()> &callback) -> void;
  auto insertButton(int index, const QString &text, QObject *context, const std::function<void()> &callback) -> void;
  auto buttonAtIndex(int index) const -> QAbstractButton*;
  auto lineEdit() const -> FancyLineEdit*;

  // For PathChoosers of 'Command' type, this property specifies the arguments
  // required to obtain the tool version (commonly, '--version'). Setting them
  // causes the version to be displayed as a tooltip.
  auto commandVersionArguments() const -> QStringList;
  auto setCommandVersionArguments(const QStringList &arguments) -> void;

  // Utility to run a tool and return its stdout.
  static auto toolVersion(const Utils::CommandLine &cmd) -> QString;
  // Install a tooltip on lineedits used for binaries showing the version.
  static auto installLineEditVersionToolTip(QLineEdit *le, const QStringList &arguments) -> void;
  // Enable a history completer with a history of entries.
  auto setHistoryCompleter(const QString &historyKey, bool restoreLastItemFromHistory = false) -> void;
  // Sets a macro expander that is used when producing path and fileName.
  // By default, the global expander is used.
  // nullptr can be passed to disable macro expansion.
  auto setMacroExpander(MacroExpander *macroExpander) -> void;
  auto isReadOnly() const -> bool;
  auto setReadOnly(bool b) -> void;
  auto triggerChanged() -> void;

  // global handler for adding context menus to ALL pathchooser
  // used by the coreplugin to add "Open in Terminal" and "Open in Explorer" context menu actions
  using AboutToShowContextMenuHandler = std::function<void (PathChooser *, QMenu *)>;
  static auto setAboutToShowContextMenuHandler(AboutToShowContextMenuHandler handler) -> void;

  auto setOpenTerminalHandler(const std::function<void()> &openTerminal) -> void;
  auto openTerminalHandler() const -> std::function<void()>;

  // Deprecated. Use filePath().toString() or better suitable conversions.
  auto path() const -> QString { return filePath().toString(); }

  // this sets the placeHolderText to defaultValue and enables to use this as
  // input value during validation if the real value is empty
  // setting an empty QString will disable this and clear the placeHolderText
  auto setDefaultValue(const QString &defaultValue) -> void;
private:
  auto validatePath(FancyLineEdit *edit, QString *errorMessage) const -> bool;
  // Returns overridden title or the one from <title>
  auto makeDialogTitle(const QString &title) -> QString;
  auto slotBrowse() -> void;
  auto contextMenuRequested(const QPoint &pos) -> void;

signals:
  auto validChanged(bool validState) -> void;
  auto rawPathChanged(const QString &text) -> void;
  auto pathChanged(const QString &path) -> void;
  auto filePathChanged(const FilePath &path) -> void;
  auto editingFinished() -> void;
  auto beforeBrowsing() -> void;
  auto browsingFinished() -> void;
  auto returnPressed() -> void;

public slots:
  auto setPath(const QString &) -> void;
  auto setFilePath(const FilePath &) -> void;

private:
  PathChooserPrivate *d = nullptr;
  static AboutToShowContextMenuHandler s_aboutToShowContextMenuHandler;
};

} // namespace Utils
