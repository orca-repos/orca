// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace Orca::Plugin::Core {

// Modes
constexpr char MODE_WELCOME[] = "Welcome";
constexpr char MODE_EDIT[] = "Edit";
constexpr char MODE_DESIGN[] = "Design";
constexpr int  P_MODE_WELCOME = 100;
constexpr int  P_MODE_EDIT = 90;
constexpr int  P_MODE_DESIGN = 89;

// TouchBar
constexpr char TOUCH_BAR[] = "Orca.TouchBar";

// Menubar
constexpr char MENU_BAR[] = "Orca.MenuBar";

// Menus
constexpr char M_FILE[] = "Orca.Menu.File";
constexpr char M_FILE_RECENTFILES[] = "Orca.Menu.File.RecentFiles";
constexpr char M_EDIT[] = "Orca.Menu.Edit";
constexpr char M_EDIT_ADVANCED[] = "Orca.Menu.Edit.Advanced";
constexpr char M_VIEW[] = "Orca.Menu.View";
constexpr char M_VIEW_MODESTYLES[] = "Orca.Menu.View.ModeStyles";
constexpr char M_VIEW_VIEWS[] = "Orca.Menu.View.Views";
constexpr char M_VIEW_PANES[] = "Orca.Menu.View.Panes";
constexpr char M_TOOLS[] = "Orca.Menu.Tools";
constexpr char M_TOOLS_EXTERNAL[] = "Orca.Menu.Tools.External";
constexpr char M_TOOLS_DEBUG[] = "Orca.Menu.Tools.Debug";
constexpr char M_WINDOW[] = "Orca.Menu.Window";
constexpr char M_HELP[] = "Orca.Menu.Help";

// Contexts
constexpr char C_GLOBAL[] = "Global Context";
constexpr char C_WELCOME_MODE[] = "Core.WelcomeMode";
constexpr char C_EDIT_MODE[] = "Core.EditMode";
constexpr char C_DESIGN_MODE[] = "Core.DesignMode";
constexpr char C_EDITORMANAGER[] = "Core.EditorManager";
constexpr char C_NAVIGATION_PANE[] = "Core.NavigationPane";
constexpr char C_PROBLEM_PANE[] = "Core.ProblemPane";
constexpr char C_GENERAL_OUTPUT_PANE[] = "Core.GeneralOutputPane";

// Default editor kind
constexpr char K_DEFAULT_TEXT_EDITOR_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("OpenWith::Editors", "Plain Text Editor");
constexpr char K_DEFAULT_TEXT_EDITOR_ID[] = "Core.PlainTextEditor";
constexpr char K_DEFAULT_BINARY_EDITOR_ID[] = "Core.BinaryEditor";

//actions
constexpr char UNDO[] = "Orca.Undo";
constexpr char REDO[] = "Orca.Redo";
constexpr char COPY[] = "Orca.Copy";
constexpr char PASTE[] = "Orca.Paste";
constexpr char CUT[] = "Orca.Cut";
constexpr char SELECTALL[] = "Orca.SelectAll";
constexpr char GOTO[] = "Orca.Goto";
constexpr char ZOOM_IN[] = "Orca.ZoomIn";
constexpr char ZOOM_OUT[] = "Orca.ZoomOut";
constexpr char ZOOM_RESET[] = "Orca.ZoomReset";
constexpr char NEW[] = "Orca.New";
constexpr char NEW_FILE[] = "Orca.NewFile";
constexpr char OPEN[] = "Orca.Open";
constexpr char OPEN_WITH[] = "Orca.OpenWith";
constexpr char REVERTTOSAVED[] = "Orca.RevertToSaved";
constexpr char SAVE[] = "Orca.Save";
constexpr char SAVEAS[] = "Orca.SaveAs";
constexpr char SAVEALL[] = "Orca.SaveAll";
constexpr char PRINT[] = "Orca.Print";
constexpr char EXIT[] = "Orca.Exit";
constexpr char OPTIONS[] = "Orca.Options";
constexpr char LOGGER[] = "Orca.Logger";
constexpr char TOGGLE_LEFT_SIDEBAR[] = "Orca.ToggleLeftSidebar";
constexpr char TOGGLE_RIGHT_SIDEBAR[] = "Orca.ToggleRightSidebar";
constexpr char CYCLE_MODE_SELECTOR_STYLE[] = "Orca.CycleModeSelectorStyle";
constexpr char TOGGLE_FULLSCREEN[] = "Orca.ToggleFullScreen";
constexpr char THEMEOPTIONS[] = "Orca.ThemeOptions";
constexpr char TR_SHOW_LEFT_SIDEBAR[] = QT_TRANSLATE_NOOP("Core", "Show Left Sidebar");
constexpr char TR_HIDE_LEFT_SIDEBAR[] = QT_TRANSLATE_NOOP("Core", "Hide Left Sidebar");
constexpr char TR_SHOW_RIGHT_SIDEBAR[] = QT_TRANSLATE_NOOP("Core", "Show Right Sidebar");
constexpr char TR_HIDE_RIGHT_SIDEBAR[] = QT_TRANSLATE_NOOP("Core", "Hide Right Sidebar");
constexpr char MINIMIZE_WINDOW[] = "Orca.MinimizeWindow";
constexpr char ZOOM_WINDOW[] = "Orca.ZoomWindow";
constexpr char CLOSE_WINDOW[] = "Orca.CloseWindow";
constexpr char SPLIT[] = "Orca.Split";
constexpr char SPLIT_SIDE_BY_SIDE[] = "Orca.SplitSideBySide";
constexpr char SPLIT_NEW_WINDOW[] = "Orca.SplitNewWindow";
constexpr char REMOVE_CURRENT_SPLIT[] = "Orca.RemoveCurrentSplit";
constexpr char REMOVE_ALL_SPLITS[] = "Orca.RemoveAllSplits";
constexpr char GOTO_PREV_SPLIT[] = "Orca.GoToPreviousSplit";
constexpr char GOTO_NEXT_SPLIT[] = "Orca.GoToNextSplit";
constexpr char CLOSE[] = "Orca.Close";
constexpr char CLOSE_ALTERNATIVE[] = "Orca.Close_Alternative"; // temporary, see ORCABUG-72
constexpr char CLOSEALL[] = "Orca.CloseAll";
constexpr char CLOSEOTHERS[] = "Orca.CloseOthers";
constexpr char CLOSEALLEXCEPTVISIBLE[] = "Orca.CloseAllExceptVisible";
constexpr char GOTONEXTINHISTORY[] = "Orca.GotoNextInHistory";
constexpr char GOTOPREVINHISTORY[] = "Orca.GotoPreviousInHistory";
constexpr char GO_BACK[] = "Orca.GoBack";
constexpr char GO_FORWARD[] = "Orca.GoForward";
constexpr char GOTOLASTEDIT[] = "Orca.GotoLastEdit";
constexpr char ABOUT_ORCA[] = "Orca.AboutOrca";
constexpr char ABOUT_PLUGINS[] = "Orca.AboutPlugins";
constexpr char S_RETURNTOEDITOR[] = "Orca.ReturnToEditor";
constexpr char SHOWINGRAPHICALSHELL[] = "Orca.ShowInGraphicalShell";
constexpr char SHOWINFILESYSTEMVIEW[] = "Orca.ShowInFileSystemView";
constexpr char OUTPUTPANE_CLEAR[] = "Coreplugin.OutputPane.clear";

// Default groups
constexpr char G_DEFAULT_ONE[] = "Orca.Group.Default.One";
constexpr char G_DEFAULT_TWO[] = "Orca.Group.Default.Two";
constexpr char G_DEFAULT_THREE[] = "Orca.Group.Default.Three";

// Main menu bar groups
constexpr char G_FILE[] = "Orca.Group.File";
constexpr char G_EDIT[] = "Orca.Group.Edit";
constexpr char G_VIEW[] = "Orca.Group.View";
constexpr char G_TOOLS[] = "Orca.Group.Tools";
constexpr char G_WINDOW[] = "Orca.Group.Window";
constexpr char G_HELP[] = "Orca.Group.Help";

// File menu groups
constexpr char G_FILE_NEW[] = "Orca.Group.File.New";
constexpr char G_FILE_OPEN[] = "Orca.Group.File.Open";
constexpr char G_FILE_PROJECT[] = "Orca.Group.File.Project";
constexpr char G_FILE_SAVE[] = "Orca.Group.File.Save";
constexpr char G_FILE_EXPORT[] = "Orca.Group.File.Export";
constexpr char G_FILE_CLOSE[] = "Orca.Group.File.Close";
constexpr char G_FILE_PRINT[] = "Orca.Group.File.Print";
constexpr char G_FILE_OTHER[] = "Orca.Group.File.Other";

// Edit menu groups
constexpr char G_EDIT_UNDOREDO[] = "Orca.Group.Edit.UndoRedo";
constexpr char G_EDIT_COPYPASTE[] = "Orca.Group.Edit.CopyPaste";
constexpr char G_EDIT_SELECTALL[] = "Orca.Group.Edit.SelectAll";
constexpr char G_EDIT_ADVANCED[] = "Orca.Group.Edit.Advanced";
constexpr char G_EDIT_FIND[] = "Orca.Group.Edit.Find";
constexpr char G_EDIT_OTHER[] = "Orca.Group.Edit.Other";

// Advanced edit menu groups
constexpr char G_EDIT_FORMAT[] = "Orca.Group.Edit.Format";
constexpr char G_EDIT_COLLAPSING[] = "Orca.Group.Edit.Collapsing";
constexpr char G_EDIT_TEXT[] = "Orca.Group.Edit.Text";
constexpr char G_EDIT_BLOCKS[] = "Orca.Group.Edit.Blocks";
constexpr char G_EDIT_FONT[] = "Orca.Group.Edit.Font";
constexpr char G_EDIT_EDITOR[] = "Orca.Group.Edit.Editor";

// View menu groups
constexpr char G_VIEW_VIEWS[] = "Orca.Group.View.Views";
constexpr char G_VIEW_PANES[] = "Orca.Group.View.Panes";

// Tools menu groups
constexpr char G_TOOLS_DEBUG[] = "Orca.Group.Tools.Debug";
constexpr char G_EDIT_PREFERENCES[] = "Orca.Group.Edit.Preferences";

// Window menu groups
constexpr char G_WINDOW_SIZE[] = "Orca.Group.Window.Size";
constexpr char G_WINDOW_SPLIT[] = "Orca.Group.Window.Split";
constexpr char G_WINDOW_NAVIGATE[] = "Orca.Group.Window.Navigate";
constexpr char G_WINDOW_LIST[] = "Orca.Group.Window.List";
constexpr char G_WINDOW_OTHER[] = "Orca.Group.Window.Other";

// Help groups (global)
constexpr char G_HELP_HELP[] = "Orca.Group.Help.Help";
constexpr char G_HELP_SUPPORT[] = "Orca.Group.Help.Supprt";
constexpr char G_HELP_ABOUT[] = "Orca.Group.Help.About";
constexpr char G_HELP_UPDATES[] = "Orca.Group.Help.Updates";

// Touchbar groups
constexpr char G_TOUCHBAR_HELP[] = "Orca.Group.TouchBar.Help";
constexpr char G_TOUCHBAR_EDITOR[] = "Orca.Group.TouchBar.Editor";
constexpr char G_TOUCHBAR_NAVIGATION[] = "Orca.Group.TouchBar.Navigation";
constexpr char G_TOUCHBAR_OTHER[] = "Orca.Group.TouchBar.Other";
constexpr char WIZARD_CATEGORY_QT[] = "R.Qt";
constexpr char WIZARD_TR_CATEGORY_QT[] = QT_TRANSLATE_NOOP("Core", "Qt");
constexpr char WIZARD_KIND_UNKNOWN[] = "unknown";
constexpr char WIZARD_KIND_PROJECT[] = "project";
constexpr char WIZARD_KIND_FILE[] = "file";
constexpr char SETTINGS_CATEGORY_CORE[] = "B.Core";
constexpr char SETTINGS_ID_INTERFACE[] = "A.Interface";
constexpr char SETTINGS_ID_SYSTEM[] = "B.Core.System";
constexpr char SETTINGS_ID_SHORTCUTS[] = "C.Keyboard";
constexpr char SETTINGS_ID_TOOLS[] = "D.ExternalTools";
constexpr char SETTINGS_ID_MIMETYPES[] = "E.MimeTypes";
constexpr char SETTINGS_DEFAULTTEXTENCODING[] = "General/DefaultFileEncoding";
constexpr char SETTINGS_DEFAULT_LINE_TERMINATOR[] = "General/DefaultLineTerminator";
constexpr char SETTINGS_THEME[] = "Core/OrcaTheme";
constexpr char DEFAULT_THEME[] = "dark";
constexpr char DEFAULT_DARK_THEME[] = "dark";
constexpr char TR_CLEAR_MENU[] = QT_TRANSLATE_NOOP("Core", "Clear Menu");
constexpr int MODEBAR_ICON_SIZE = 34;
constexpr int MODEBAR_ICONSONLY_BUTTON_SIZE = MODEBAR_ICON_SIZE + 4;
constexpr int DEFAULT_MAX_CHAR_COUNT = 10000000;

} // namespace Orca::Plugin::Core
