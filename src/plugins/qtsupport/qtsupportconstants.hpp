// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace QtSupport {
namespace Constants {

// Qt settings pages
constexpr char QTVERSION_SETTINGS_PAGE_ID[] = "H.Qt Versions";
constexpr char CODEGEN_SETTINGS_PAGE_ID[] = "Class Generation";

// QtVersions
constexpr char DESKTOPQT[]   = "Qt4ProjectManager.QtVersion.Desktop";

// QtVersion settings
static constexpr char QTVERSIONID[] = "Id";
static constexpr char QTVERSIONNAME[] = "Name";

// Qt Features
constexpr char FEATURE_QT_PREFIX[] = "QtSupport.Wizards.FeatureQt";
constexpr char FEATURE_QWIDGETS[] = "QtSupport.Wizards.FeatureQWidgets";
constexpr char FEATURE_QT_QUICK_PREFIX[] = "QtSupport.Wizards.FeatureQtQuick";
constexpr char FEATURE_QMLPROJECT[] = "QtSupport.Wizards.FeatureQtQuickProject";
constexpr char FEATURE_QT_QUICK_CONTROLS_PREFIX[] = "QtSupport.Wizards.FeatureQtQuick.Controls";
constexpr char FEATURE_QT_QUICK_CONTROLS_2_PREFIX[] = "QtSupport.Wizards.FeatureQtQuick.Controls.2";
constexpr char FEATURE_QT_LABS_CONTROLS_PREFIX[] = "QtSupport.Wizards.FeatureQt.labs.controls";
constexpr char FEATURE_QT_QUICK_UI_FILES[] = "QtSupport.Wizards.FeatureQtQuick.UiFiles";
constexpr char FEATURE_QT_WEBKIT[] = "QtSupport.Wizards.FeatureQtWebkit";
constexpr char FEATURE_QT_3D[] = "QtSupport.Wizards.FeatureQt3d";
constexpr char FEATURE_QT_CANVAS3D_PREFIX[] = "QtSupport.Wizards.FeatureQtCanvas3d";
constexpr char FEATURE_QT_CONSOLE[] = "QtSupport.Wizards.FeatureQtConsole";
constexpr char FEATURE_MOBILE[] = "QtSupport.Wizards.FeatureMobile";
constexpr char FEATURE_DESKTOP[] = "QtSupport.Wizards.FeatureDesktop";

// Kit flags
constexpr char FLAGS_SUPPLIES_QTQUICK_IMPORT_PATH[] = "QtSupport.SuppliesQtQuickImportPath";
constexpr char KIT_QML_IMPORT_PATH[] = "QtSupport.KitQmlImportPath";
constexpr char KIT_HAS_MERGED_HEADER_PATHS_WITH_QML_IMPORT_PATHS[] = "QtSupport.KitHasMergedHeaderPathsWithQmlImportPaths";

} // namepsace Constants
} // namepsace QtSupport
