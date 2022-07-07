// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "icontext.hpp"
#include "icore.hpp"

#include <utils/appmainwindow.hpp>
#include <utils/dropsupport.hpp>

#include <QColor>

#include <functional>
#include <unordered_map>

QT_BEGIN_NAMESPACE
class QPrinter;
class QToolButton;
QT_END_NAMESPACE

namespace Utils {
class InfoBar;
}

namespace Core {
class EditorManager;
class ExternalToolManager;
class IDocument;
class JsExpander;
class MessageManager;
class ModeManager;
class ProgressManager;
class NavigationWidget;
enum class Side;
class RightPaneWidget;
class SettingsDatabase;
class VcsManager;

namespace Internal {
class FancyTabWidget;
class GeneralSettings;
class ProgressManagerPrivate;
class ShortcutSettings;
class ToolSettings;
class MimeTypeSettings;
class VersionDialog;
class WindowSupport;
class SystemEditor;
class SystemSettings;

class MainWindow final : public Utils::AppMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow() override;

  auto init() const -> void;
  auto extensionsInitialized() -> void;
  auto aboutToShutdown() -> void;
  auto contextObject(QWidget *widget) const -> IContext*;
  auto addContextObject(IContext *context) -> void;
  auto removeContextObject(IContext *context) -> void;
  static auto openFiles(const Utils::FilePaths &file_paths, ICore::OpenFilesFlags flags, const QString &working_directory = QString()) -> IDocument*;
  auto settingsDatabase() const -> SettingsDatabase* { return m_settings_database; }
  auto printer() const -> QPrinter*;
  auto currentContextObject() const -> IContext*;
  auto statusBar() const -> QStatusBar*;
  auto infoBar() const -> Utils::InfoBar*;
  auto updateAdditionalContexts(const Context &remove, const Context &add, ICore::ContextPriority priority) -> void;
  auto askConfirmationBeforeExit() const -> bool;
  auto setAskConfirmationBeforeExit(bool ask) -> void;
  auto setOverrideColor(const QColor &color) -> void;
  auto additionalAboutInformation() const -> QStringList;
  auto appendAboutInformation(const QString &line) -> void;
  auto addPreCloseListener(const std::function<bool()> &listener) -> void;
  auto saveSettings() const -> void;
  auto restart() -> void;

public slots:
  static auto openFileWith() -> void;
  auto exit() -> void;

protected:
  auto closeEvent(QCloseEvent *event) -> void override;

private:
  static auto openFile() -> void;
  auto aboutToShowRecentFiles() -> void;
  static auto setFocusToEditor() -> void;
  auto aboutOrca() -> void;
  auto aboutPlugins() -> void;
  auto contact() -> void;
  auto updateFocusWidget(const QWidget *old, QWidget *now) -> void;
  auto navigationWidget(Side side) const -> NavigationWidget*;
  auto setSidebarVisible(bool visible, Side side) const -> void;
  auto destroyVersionDialog() -> void;
  auto openDroppedFiles(const QList<Utils::DropSupport::FileSpec> &files) -> void;
  auto restoreWindowState() -> void;
  auto updateContextObject(const QList<IContext*> &context) -> void;
  auto updateContext() -> void;
  auto registerDefaultContainers() -> void;
  auto registerDefaultActions() -> void;
  auto readSettings() -> void;
  auto saveWindowSettings() -> void;

  ICore *m_core_impl = nullptr;
  QStringList m_about_information;
  Context m_high_prio_additional_contexts;
  Context m_low_prio_additional_contexts;
  SettingsDatabase *m_settings_database = nullptr;
  mutable QPrinter *m_printer = nullptr;
  WindowSupport *m_window_support = nullptr;
  EditorManager *m_editor_manager = nullptr;
  ExternalToolManager *m_external_tool_manager = nullptr;
  MessageManager *m_message_manager = nullptr;
  ProgressManagerPrivate *m_progress_manager = nullptr;
  JsExpander *m_js_expander = nullptr;
  VcsManager *m_vcs_manager = nullptr;
  ModeManager *m_mode_manager = nullptr;
  FancyTabWidget *m_mode_stack = nullptr;
  NavigationWidget *m_left_navigation_widget = nullptr;
  NavigationWidget *m_right_navigation_widget = nullptr;
  RightPaneWidget *m_right_pane_widget = nullptr;
  VersionDialog *m_version_dialog = nullptr;
  QList<IContext*> m_active_context;
  std::unordered_map<QWidget*, IContext*> m_context_widgets;
  GeneralSettings *m_general_settings = nullptr;
  SystemSettings *m_system_settings = nullptr;
  ShortcutSettings *m_shortcut_settings = nullptr;
  ToolSettings *m_tool_settings = nullptr;
  MimeTypeSettings *m_mime_type_settings = nullptr;
  SystemEditor *m_system_editor = nullptr;
  QAction *m_focus_to_editor = nullptr;
  QAction *m_new_action = nullptr;
  QAction *m_open_action = nullptr;
  QAction *m_open_with_action = nullptr;
  QAction *m_save_all_action = nullptr;
  QAction *m_exit_action = nullptr;
  QAction *m_options_action = nullptr;
  QAction *m_logger_action = nullptr;
  QAction *m_toggle_left_side_bar_action = nullptr;
  QAction *m_toggle_right_side_bar_action = nullptr;
  QAction *m_cycle_mode_selector_style_action = nullptr;
  QAction *m_set_mode_selector_style_icons_and_text_action = nullptr;
  QAction *m_set_mode_selector_style_hidden_action = nullptr;
  QAction *m_set_mode_selector_style_icons_only_action = nullptr;
  QAction *m_theme_action = nullptr;
  QToolButton *m_toggle_left_side_bar_button = nullptr;
  QToolButton *m_toggle_right_side_bar_button = nullptr;
  bool m_ask_confirmation_before_exit = false;
  QColor m_override_color;
  QList<std::function<bool()>> m_pre_close_listeners;
};

} // namespace Internal
} // namespace Core
