// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <core/core-help-item.hpp>
#include <core/core-context-interface.hpp>

#include <functional>

QT_BEGIN_NAMESPACE
class QPoint;
QT_END_NAMESPACE

namespace TextEditor {

class TextEditorWidget;

class TEXTEDITOR_EXPORT BaseHoverHandler {
public:
  virtual ~BaseHoverHandler();

  auto contextHelpId(TextEditorWidget *widget, int pos, const Orca::Plugin::Core::IContext::HelpCallback &callback) -> void;
  using ReportPriority = std::function<void(int priority)>;
  auto checkPriority(TextEditorWidget *widget, int pos, ReportPriority report) -> void;
  virtual auto abort() -> void {} // Implement for asynchronous priority reporter
  auto showToolTip(TextEditorWidget *widget, const QPoint &point) -> void;

protected:
  enum {
    Priority_None = 0,
    Priority_Tooltip = 5,
    Priority_Help = 10,
    Priority_Diagnostic = 20
  };

  auto setPriority(int priority) -> void;
  auto priority() const -> int;
  auto setToolTip(const QString &tooltip, Qt::TextFormat format = Qt::PlainText) -> void;
  auto toolTip() const -> const QString&;
  auto setLastHelpItemIdentified(const Orca::Plugin::Core::HelpItem &help) -> void;
  auto lastHelpItemIdentified() const -> const Orca::Plugin::Core::HelpItem&;
  auto isContextHelpRequest() const -> bool;
  auto propagateHelpId(TextEditorWidget *widget, const Orca::Plugin::Core::IContext::HelpCallback &callback) -> void;

  // identifyMatch() is required to report a priority by using the "report" callback.
  // It is recommended to use e.g.
  //    Utils::ExecuteOnDestruction reportPriority([this, report](){ report(priority()); });
  // at the beginning of an implementation to ensure this in any case.
  virtual auto identifyMatch(TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void;
  virtual auto operateTooltip(TextEditorWidget *editorWidget, const QPoint &point) -> void;

private:
  auto process(TextEditorWidget *widget, int pos, ReportPriority report) -> void;

  QString m_toolTip;
  Qt::TextFormat m_textFormat = Qt::PlainText;
  Orca::Plugin::Core::HelpItem m_lastHelpItemIdentified;
  int m_priority = -1;
  bool m_isContextHelpRequest = false;
};

} // namespace TextEditor
