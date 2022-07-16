// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"
#include "basehoverhandler.hpp"

#include <QColor>

namespace Orca::Plugin::Core {
class IEditor;
}

namespace TextEditor {

class TextEditorWidget;

class TEXTEDITOR_EXPORT ColorPreviewHoverHandler : public BaseHoverHandler {
  auto identifyMatch(TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void override;
  auto operateTooltip(TextEditorWidget *editorWidget, const QPoint &point) -> void override;

  QColor m_colorTip;
};

} // namespace TextEditor
