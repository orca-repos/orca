// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

namespace ProjectExplorer {
namespace Constants {

// Modes and their priorities
constexpr char MODE_SESSION[]         = "Project";

// Actions
constexpr char BUILD[]                = "ProjectExplorer.Build";
constexpr char STOP[]                 = "ProjectExplorer.Stop";
constexpr char ADDNEWFILE[]           = "ProjectExplorer.AddNewFile";
constexpr char FILEPROPERTIES[]       = "ProjectExplorer.FileProperties";
constexpr char RENAMEFILE[]           = "ProjectExplorer.RenameFile";
constexpr char REMOVEFILE[]           = "ProjectExplorer.RemoveFile";

// Context
constexpr char C_PROJECTEXPLORER[]    = "Project Explorer";
constexpr char C_PROJECT_TREE[]       = "ProjectExplorer.ProjectTreeContext";

// Menus
constexpr char M_BUILDPROJECT[]         = "ProjectExplorer.Menu.Build";
constexpr char M_DEBUG[]                = "ProjectExplorer.Menu.Debug";
constexpr char M_DEBUG_STARTDEBUGGING[] = "ProjectExplorer.Menu.Debug.StartDebugging";

// Menu groups
constexpr char G_BUILD_BUILD[]                          = "ProjectExplorer.Group.Build";
constexpr char G_BUILD_ALLPROJECTS[]                    = "ProjectExplorer.Group.AllProjects";
constexpr char G_BUILD_PROJECT[]                        = "ProjectExplorer.Group.Project";
constexpr char G_BUILD_PRODUCT[]                        = "ProjectExplorer.Group.Product";
constexpr char G_BUILD_SUBPROJECT[]                     = "ProjectExplorer.Group.SubProject";
constexpr char G_BUILD_FILE[]                           = "ProjectExplorer.Group.File";
constexpr char G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS[]  = "ProjectExplorer.Group.AllProjects.AllConfigurations";
constexpr char G_BUILD_PROJECT_ALLCONFIGURATIONS[]      = "ProjectExplorer.Group.Project.AllConfigurations";
constexpr char G_BUILD_RUN[]                            = "ProjectExplorer.Group.Run";
constexpr char G_BUILD_CANCEL[]                         = "ProjectExplorer.Group.BuildCancel";

// Context menus
constexpr char M_SESSIONCONTEXT[]       = "Project.Menu.Session";
constexpr char M_PROJECTCONTEXT[]       = "Project.Menu.Project";
constexpr char M_SUBPROJECTCONTEXT[]    = "Project.Menu.SubProject";
constexpr char M_FOLDERCONTEXT[]        = "Project.Menu.Folder";
constexpr char M_FILECONTEXT[]          = "Project.Menu.File";
constexpr char M_OPENFILEWITHCONTEXT[]  = "Project.Menu.File.OpenWith";
constexpr char M_OPENTERMINALCONTEXT[]  = "Project.Menu.File.OpenTerminal";

// Context menu groups
constexpr char G_SESSION_BUILD[]      = "Session.Group.Build";
constexpr char G_SESSION_REBUILD[]    = "Session.Group.Rebuild";
constexpr char G_SESSION_FILES[]      = "Session.Group.Files";
constexpr char G_SESSION_OTHER[]      = "Session.Group.Other";
constexpr char G_PROJECT_FIRST[]      = "Project.Group.Open";
constexpr char G_PROJECT_BUILD[]      = "Project.Group.Build";
constexpr char G_PROJECT_REBUILD[]    = "Project.Group.Rebuild";
constexpr char G_PROJECT_RUN[]        = "Project.Group.Run";
constexpr char G_PROJECT_FILES[]      = "Project.Group.Files";
constexpr char G_PROJECT_TREE[]       = "Project.Group.Tree";
constexpr char G_PROJECT_LAST[]       = "Project.Group.Last";
constexpr char G_FOLDER_LOCATIONS[]   = "ProjectFolder.Group.Locations";
constexpr char G_FOLDER_FILES[]       = "ProjectFolder.Group.Files";
constexpr char G_FOLDER_OTHER[]       = "ProjectFolder.Group.Other";
constexpr char G_FOLDER_CONFIG[]      = "ProjectFolder.Group.Config";
constexpr char G_FILE_OPEN[]          = "ProjectFile.Group.Open";
constexpr char G_FILE_OTHER[]         = "ProjectFile.Group.Other";
constexpr char G_FILE_CONFIG[]        = "ProjectFile.Group.Config";

// Mime types
constexpr char C_SOURCE_MIMETYPE[]    = "text/x-csrc";
constexpr char C_HEADER_MIMETYPE[]    = "text/x-chdr";
constexpr char CPP_SOURCE_MIMETYPE[]  = "text/x-c++src";
constexpr char CPP_HEADER_MIMETYPE[]  = "text/x-c++hdr";
constexpr char LINGUIST_MIMETYPE[]    = "text/vnd.trolltech.linguist";
constexpr char FORM_MIMETYPE[]        = "application/x-designer";
constexpr char QML_MIMETYPE[]         = "text/x-qml"; // separate def also in qmljstoolsconstants.h
constexpr char QMLUI_MIMETYPE[]       = "application/x-qt.ui+qml";
constexpr char RESOURCE_MIMETYPE[]    = "application/vnd.qt.xml.resource";
constexpr char SCXML_MIMETYPE[]       = "application/scxml+xml";

// Kits settings category
constexpr char KITS_SETTINGS_CATEGORY[]  = "A.Kits";

// Kits pages
constexpr char KITS_SETTINGS_PAGE_ID[] = "D.ProjectExplorer.KitsOptions";
constexpr char SSH_SETTINGS_PAGE_ID[] = "F.ProjectExplorer.SshOptions";
constexpr char TOOLCHAIN_SETTINGS_PAGE_ID[] = "M.ProjectExplorer.ToolChainOptions";
constexpr char DEBUGGER_SETTINGS_PAGE_ID[] = "N.ProjectExplorer.DebuggerOptions";
constexpr char CUSTOM_PARSERS_SETTINGS_PAGE_ID[] = "X.ProjectExplorer.CustomParsersSettingsPage";

// Build and Run settings category
constexpr char BUILD_AND_RUN_SETTINGS_CATEGORY[]  = "K.BuildAndRun";

// Build and Run page
constexpr char BUILD_AND_RUN_SETTINGS_PAGE_ID[] = "A.ProjectExplorer.BuildAndRunOptions";

// Device settings page
constexpr char DEVICE_SETTINGS_CATEGORY[] = "XW.Devices";
constexpr char DEVICE_SETTINGS_PAGE_ID[] = "AA.Device Settings";

// Task categories
constexpr char TASK_CATEGORY_COMPILE[] = "Task.Category.Compile";
constexpr char TASK_CATEGORY_BUILDSYSTEM[] = "Task.Category.Buildsystem";
constexpr char TASK_CATEGORY_DEPLOYMENT[] = "Task.Category.Deploy";
constexpr char TASK_CATEGORY_AUTOTEST[] = "Task.Category.Autotest";

// Wizard categories
constexpr char QT_PROJECT_WIZARD_CATEGORY[] = "H.Project";
constexpr char QT_PROJECT_WIZARD_CATEGORY_DISPLAY[] = QT_TRANSLATE_NOOP("ProjectExplorer", "Other Project");
constexpr char QT_APPLICATION_WIZARD_CATEGORY[] = "F.Application";
constexpr char QT_APPLICATION_WIZARD_CATEGORY_DISPLAY[] = QT_TRANSLATE_NOOP("ProjectExplorer", "Application");
constexpr char LIBRARIES_WIZARD_CATEGORY[] = "G.Library";
constexpr char LIBRARIES_WIZARD_CATEGORY_DISPLAY[] = QT_TRANSLATE_NOOP("ProjectExplorer", "Library");
constexpr char IMPORT_WIZARD_CATEGORY[] = "T.Import";
constexpr char IMPORT_WIZARD_CATEGORY_DISPLAY[] = QT_TRANSLATE_NOOP("ProjectExplorer", "Import Project");

// Wizard extra values
constexpr char PREFERRED_PROJECT_NODE[] = "ProjectExplorer.PreferredProjectNode";
constexpr char PREFERRED_PROJECT_NODE_PATH[] = "ProjectExplorer.PreferredProjectPath";
constexpr char PROJECT_POINTER[] = "ProjectExplorer.Project";
constexpr char PROJECT_KIT_IDS[] = "ProjectExplorer.Profile.Ids";
constexpr char QT_KEYWORDS_ENABLED[] = "ProjectExplorer.QtKeywordsEnabled";

// Build step lists ids:
constexpr char BUILDSTEPS_CLEAN[] = "ProjectExplorer.BuildSteps.Clean";
constexpr char BUILDSTEPS_BUILD[] = "ProjectExplorer.BuildSteps.Build";
constexpr char BUILDSTEPS_DEPLOY[] = "ProjectExplorer.BuildSteps.Deploy";

// Language

// Keep these short: These constants are exposed to the MacroExplorer!
constexpr char C_LANGUAGE_ID[] = "C";
constexpr char CXX_LANGUAGE_ID[] = "Cxx";
constexpr char QMLJS_LANGUAGE_ID[] = "QMLJS";
constexpr char PYTHON_LANGUAGE_ID[] = "Python";

// ToolChain TypeIds
constexpr char CLANG_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.Clang";
constexpr char GCC_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.Gcc";
constexpr char LINUXICC_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.LinuxIcc";
constexpr char MINGW_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.Mingw";
constexpr char MSVC_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.Msvc";
constexpr char CLANG_CL_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.ClangCl";
constexpr char CUSTOM_TOOLCHAIN_TYPEID[] = "ProjectExplorer.ToolChain.Custom";

// Default directory to run custom (build) commands in.
constexpr char DEFAULT_WORKING_DIR[] = "%{buildDir}";
constexpr char DEFAULT_WORKING_DIR_ALTERNATE[] = "%{sourceDir}";

// Desktop Device related ids:
constexpr char DESKTOP_DEVICE_ID[] = "Desktop Device";
constexpr char DESKTOP_DEVICE_TYPE[] = "Desktop";
constexpr int DESKTOP_PORT_START = 30000;
constexpr int DESKTOP_PORT_END = 31000;

// Android ABIs
constexpr char ANDROID_ABI_ARMEABI[] = "armeabi";
constexpr char ANDROID_ABI_ARMEABI_V7A[] = "armeabi-v7a";
constexpr char ANDROID_ABI_ARM64_V8A[] = "arm64-v8a";
constexpr char ANDROID_ABI_X86[] = "x86";
constexpr char ANDROID_ABI_X86_64[] = "x86_64";

// Variable Names:
constexpr char VAR_CURRENTPROJECT_PREFIX[] = "CurrentProject";
constexpr char VAR_CURRENTPROJECT_NAME[] = "CurrentProject:Name";
constexpr char VAR_CURRENTBUILD_NAME[] = "CurrentBuild:Name";
constexpr char VAR_CURRENTBUILD_ENV[] = "CurrentBuild:Env";

// JsonWizard:
constexpr char PAGE_ID_PREFIX[] = "PE.Wizard.Page.";
constexpr char GENERATOR_ID_PREFIX[] = "PE.Wizard.Generator.";

// RunMode
constexpr char NO_RUN_MODE[]="RunConfiguration.NoRunMode";
constexpr char NORMAL_RUN_MODE[]="RunConfiguration.NormalRunMode";
constexpr char DEBUG_RUN_MODE[]="RunConfiguration.DebugRunMode";
constexpr char QML_PROFILER_RUN_MODE[]="RunConfiguration.QmlProfilerRunMode";
constexpr char QML_PROFILER_RUNNER[]="RunConfiguration.QmlProfilerRunner";
constexpr char QML_PREVIEW_RUN_MODE[]="RunConfiguration.QmlPreviewRunMode";
constexpr char QML_PREVIEW_RUNNER[]="RunConfiguration.QmlPreviewRunner";
constexpr char PERFPROFILER_RUN_MODE[]="PerfProfiler.RunMode";
constexpr char PERFPROFILER_RUNNER[]="PerfProfiler.Runner";

// Navigation Widget
constexpr char PROJECTTREE_ID[] = "Projects";

// File icon overlays
constexpr char FILEOVERLAY_QT[]=":/projectexplorer/images/fileoverlay_qt.png";
constexpr char FILEOVERLAY_GROUP[] = ":/projectexplorer/images/fileoverlay_group.png";
constexpr char FILEOVERLAY_PRODUCT[] = ":/projectexplorer/images/fileoverlay_product.png";
constexpr char FILEOVERLAY_MODULES[] = ":/projectexplorer/images/fileoverlay_modules.png";
constexpr char FILEOVERLAY_QML[]=":/projectexplorer/images/fileoverlay_qml.png";
constexpr char FILEOVERLAY_UI[]=":/projectexplorer/images/fileoverlay_ui.png";
constexpr char FILEOVERLAY_QRC[]=":/projectexplorer/images/fileoverlay_qrc.png";
constexpr char FILEOVERLAY_C[]=":/projectexplorer/images/fileoverlay_c.png";
constexpr char FILEOVERLAY_CPP[]=":/projectexplorer/images/fileoverlay_cpp.png";
constexpr char FILEOVERLAY_H[]=":/projectexplorer/images/fileoverlay_h.png";
constexpr char FILEOVERLAY_SCXML[]=":/projectexplorer/images/fileoverlay_scxml.png";
constexpr char FILEOVERLAY_PY[]=":/projectexplorer/images/fileoverlay_py.png";
constexpr char FILEOVERLAY_UNKNOWN[]=":/projectexplorer/images/fileoverlay_unknown.png";

// Settings
constexpr char ADD_FILES_DIALOG_FILTER_HISTORY_KEY[] = "ProjectExplorer.AddFilesFilterKey";
constexpr char PROJECT_ROOT_PATH_KEY[] = "ProjectExplorer.Project.RootPath";
constexpr char STARTUPSESSION_KEY[] = "ProjectExplorer/SessionToRestore";
constexpr char LASTSESSION_KEY[] = "ProjectExplorer/StartupSession";
constexpr char SETTINGS_MENU_HIDE_BUILD[] = "Menu/HideBuild";
constexpr char SETTINGS_MENU_HIDE_DEBUG[] = "Menu/HideDebug";
constexpr char SETTINGS_MENU_HIDE_ANALYZE[] = "Menu/HideAnalyze";

// UI texts
PROJECTEXPLORER_EXPORT auto msgAutoDetected() -> QString;
PROJECTEXPLORER_EXPORT auto msgAutoDetectedToolTip() -> QString;
PROJECTEXPLORER_EXPORT auto msgManual() -> QString;

} // namespace Constants
} // namespace ProjectExplorer
