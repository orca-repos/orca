// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "foldernavigationwidget.hpp"

#include <utils/environment.hpp>

#include <extensionsystem/iplugin.hpp>

#include <qglobal.h>

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

namespace Utils {
class PathChooser;
}

namespace Core {

class FolderNavigationWidgetFactory;

namespace Internal {

class EditMode;
class MainWindow;
class Locator;

class CorePlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "core.json")

public:
  CorePlugin();
  ~CorePlugin() override;

  static auto instance() -> CorePlugin*;
  auto initialize(const QStringList &arguments, QString *error_message = nullptr) -> bool override;
  auto extensionsInitialized() -> void override;
  auto delayedInitialize() -> bool override;
  auto aboutToShutdown() -> ShutdownFlag override;
  auto remoteCommand(const QStringList & /* options */, const QString &working_directory, const QStringList &args) -> QObject* override;
  static auto startupSystemEnvironment() -> Utils::Environment;
  static auto environmentChanges() -> Utils::EnvironmentItems;
  static auto setEnvironmentChanges(const Utils::EnvironmentItems &changes) -> void;
  static auto msgCrashpadInformation() -> QString;

public slots:
  auto fileOpenRequest(const QString &) -> void;

  #if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)
private slots:
  void testVcsManager_data();
  void testVcsManager();
  void test_basefilefilter();
  void test_basefilefilter_data();
  void testOutputFormatter();
  #endif

private:
  static auto addToPathChooserContextMenu(Utils::PathChooser *path_chooser, QMenu *menu) -> void;
  static auto setupSystemEnvironment() -> void;
  auto checkSettings() -> void;
  static auto warnAboutCrashReporing() -> void;

  MainWindow *m_main_window = nullptr;
  EditMode *m_edit_mode = nullptr;
  Locator *m_locator = nullptr;
  FolderNavigationWidgetFactory *m_folder_navigation_widget_factory = nullptr;
  Utils::Environment m_startup_system_environment;
  Utils::EnvironmentItems m_environment_changes;
};

} // namespace Internal
} // namespace Core
