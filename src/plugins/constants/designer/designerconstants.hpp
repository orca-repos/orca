// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace Designer {
namespace Constants {

constexpr char INFO_READ_ONLY[] = "DesignerXmlEditor.ReadOnly";
constexpr char K_DESIGNER_XML_EDITOR_ID[] = "FormEditor.DesignerXmlEditor";
constexpr char C_DESIGNER_XML_EDITOR[] = "Designer Xml Editor";
constexpr char C_DESIGNER_XML_DISPLAY_NAME[]  = QT_TRANSLATE_NOOP("Designer", "Form Editor");
constexpr char SETTINGS_CATEGORY[] = "P.Designer";
constexpr char SETTINGS_TR_CATEGORY[] = QT_TRANSLATE_NOOP("Designer", "Designer");

// Context
constexpr char C_FORMEDITOR[] = "FormEditor.FormEditor";
constexpr char M_FORMEDITOR[] = "FormEditor.Menu";
constexpr char M_FORMEDITOR_PREVIEW[] = "FormEditor.Menu.Preview";

// Wizard type
constexpr char FORM_FILE_TYPE[] = "Qt4FormFiles";
constexpr char FORM_MIMETYPE[] = "application/x-designer";

enum DesignerSubWindows {
  WidgetBoxSubWindow,
  ObjectInspectorSubWindow,
  PropertyEditorSubWindow,
  SignalSlotEditorSubWindow,
  ActionEditorSubWindow,
  DesignerSubWindowCount
};

enum EditModes {
  EditModeWidgetEditor,
  EditModeSignalsSlotEditor,
  EditModeBuddyEditor,
  EditModeTabOrderEditor,
  NumEditModes
};

namespace Internal {

enum {
  debug = 0
};

} // Internal
} // Constants
} // Designer
