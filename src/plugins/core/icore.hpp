// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"
#include "icontext.hpp"

#include <utils/fileutils.hpp>
#include <utils/qtcsettings.hpp>

#include <QList>
#include <QMainWindow>
#include <QObject>
#include <QRect>
#include <QSettings>

#include <functional>

QT_BEGIN_NAMESPACE
class QPrinter;
class QStatusBar;
class QWidget;
QT_END_NAMESPACE

namespace Utils {
class InfoBar;
}

namespace Core {
class Context;
class IWizardFactory;
class SettingsDatabase;

namespace Internal {
class MainWindow;
}

class NewDialog;

class CORE_EXPORT ICore : public QObject {
  Q_OBJECT
  friend class Internal::MainWindow;
  friend class IWizardFactory;

  explicit ICore(Internal::MainWindow *mainwindow);
  ~ICore() override;

public:
  enum class ContextPriority {
    High,
    Low
  };

  static auto instance() -> ICore*;
  static auto isNewItemDialogRunning() -> bool;
  static auto newItemDialog() -> QWidget*;
  static auto showNewItemDialog(const QString &title, const QList<IWizardFactory*> &factories, const Utils::FilePath &default_location = {}, const QVariantMap &extra_variables = {}) -> void;
  static auto showOptionsDialog(Utils::Id page, QWidget *parent = nullptr) -> bool;
  static auto msgShowOptionsDialog() -> QString;
  static auto msgShowOptionsDialogToolTip() -> QString;
  static auto showWarningWithOptions(const QString &title, const QString &text, const QString &details = QString(), Utils::Id settings_id = {}, QWidget *parent = nullptr) -> bool;
  static auto settings(QSettings::Scope scope = QSettings::UserScope) -> Utils::QtcSettings*;
  static auto settingsDatabase() -> SettingsDatabase*;
  static auto printer() -> QPrinter*;
  static auto userInterfaceLanguage() -> QString;
  static auto resourcePath(const QString &rel = {}) -> Utils::FilePath;
  static auto userResourcePath(const QString &rel = {}) -> Utils::FilePath;
  static auto cacheResourcePath(const QString &rel = {}) -> Utils::FilePath;
  static auto installerResourcePath(const QString &rel = {}) -> Utils::FilePath;
  static auto libexecPath(const QString &rel = {}) -> Utils::FilePath;
  static auto crashReportsPath() -> Utils::FilePath;
  static auto ideDisplayName() -> QString;
  static auto versionString() -> QString;
  static auto mainWindow() -> QMainWindow*;
  static auto dialogParent() -> QWidget*;
  static auto infoBar() -> Utils::InfoBar*;
  static auto raiseWindow(const QWidget *widget) -> void;
  static auto currentContextObject() -> IContext*;
  static auto currentContextWidget() -> QWidget*;
  static auto contextObject(QWidget *widget) -> IContext*;
  static auto updateAdditionalContexts(const Context &remove, const Context &add, ContextPriority priority = ContextPriority::Low) -> void;
  static auto addAdditionalContext(const Context &context, ContextPriority priority = ContextPriority::Low) -> void;
  static auto removeAdditionalContext(const Context &context) -> void;
  static auto addContextObject(IContext *context) -> void;
  static auto removeContextObject(IContext *context) -> void;
  static auto registerWindow(QWidget *window, const Context &context) -> void;

  enum OpenFilesFlags {
    None = 0,
    SwitchMode = 1,
    CanContainLineAndColumnNumbers = 2,
    /// Stop loading once the first file fails to load
    StopOnLoadFail = 4,
    SwitchSplitIfAlreadyVisible = 8
  };

  static auto openFiles(const Utils::FilePaths &file_paths, OpenFilesFlags flags = None) -> void;
  static auto addPreCloseListener(const std::function<bool()> &listener) -> void;
  static auto restart() -> void;

  enum SaveSettingsReason {
    InitializationDone,
    SettingsDialogDone,
    ModeChanged,
    MainWindowClosing,
  };

signals:
  auto coreAboutToOpen() -> void;
  auto coreOpened() -> void;
  auto newItemDialogStateChanged() -> void;
  auto saveSettingsRequested(SaveSettingsReason reason) -> void;
  auto coreAboutToClose() -> void;
  auto contextAboutToChange(const QList<Core::IContext*> &context) -> void;
  auto contextChanged(const Core::Context &context) -> void;
  auto systemEnvironmentChanged() -> void;

public:
  /* internal use */
  static auto additionalAboutInformation() -> QStringList;
  static auto appendAboutInformation(const QString &line) -> void;
  static auto systemInformation() -> QString;
  static auto setupScreenShooter(const QString &name, QWidget *w, const QRect &rc = QRect()) -> void;
  static auto pluginPath() -> QString;
  static auto userPluginPath() -> QString;
  static auto clangExecutable(const Utils::FilePath &clang_bin_directory) -> Utils::FilePath;
  static auto clangdExecutable(const Utils::FilePath &clang_bin_directory) -> Utils::FilePath;
  static auto clangTidyExecutable(const Utils::FilePath &clang_bin_directory) -> Utils::FilePath;
  static auto clazyStandaloneExecutable(const Utils::FilePath &clang_bin_directory) -> Utils::FilePath;
  static auto clangIncludeDirectory(const QString &clang_version, const Utils::FilePath &clang_fallback_include_dir) -> Utils::FilePath;
  static auto buildCompatibilityString() -> QString;
  static auto statusBar() -> QStatusBar*;
  static auto saveSettings(SaveSettingsReason reason) -> void;
  static auto setNewDialogFactory(const std::function<NewDialog *(QWidget *)> &new_factory) -> void;

private:
  static auto updateNewItemDialogState() -> void;
};

} // namespace Core
