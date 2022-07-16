// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "basehoverhandler.hpp"
#include "texteditor.hpp"

#include <utils/executeondestruction.hpp>
#include <utils/qtcassert.hpp>
#include <utils/tooltip/tooltip.hpp>

#include <QVBoxLayout>

namespace TextEditor {

BaseHoverHandler::~BaseHoverHandler() = default;

auto BaseHoverHandler::showToolTip(TextEditorWidget *widget, const QPoint &point) -> void
{
  operateTooltip(widget, point);
}

auto BaseHoverHandler::checkPriority(TextEditorWidget *widget, int pos, ReportPriority report) -> void
{
  widget->setContextHelpItem({});

  process(widget, pos, report);
}

auto BaseHoverHandler::priority() const -> int
{
  if (m_priority >= 0)
    return m_priority;

  if (lastHelpItemIdentified().isValid())
    return Priority_Help;

  if (!toolTip().isEmpty())
    return Priority_Tooltip;

  return Priority_None;
}

auto BaseHoverHandler::setPriority(int priority) -> void
{
  m_priority = priority;
}

auto BaseHoverHandler::contextHelpId(TextEditorWidget *widget, int pos, const Orca::Plugin::Core::IContext::HelpCallback &callback) -> void
{
  m_isContextHelpRequest = true;

  // If the tooltip is visible and there is a help match, this match is used to update
  // the help id. Otherwise, let the identification process happen.
  if (!Utils::ToolTip::isVisible() || !lastHelpItemIdentified().isValid()) {
    process(widget, pos, [this, widget = QPointer(widget), callback](int) {
      if (widget)
        propagateHelpId(widget, callback);
    });
  } else {
    propagateHelpId(widget, callback);
  }

  m_isContextHelpRequest = false;
}

auto BaseHoverHandler::setToolTip(const QString &tooltip, Qt::TextFormat format) -> void
{
  m_toolTip = tooltip;
  m_textFormat = format;
}

auto BaseHoverHandler::toolTip() const -> const QString&
{
  return m_toolTip;
}

auto BaseHoverHandler::setLastHelpItemIdentified(const Orca::Plugin::Core::HelpItem &help) -> void
{
  m_lastHelpItemIdentified = help;
}

auto BaseHoverHandler::lastHelpItemIdentified() const -> const Orca::Plugin::Core::HelpItem&
{
  return m_lastHelpItemIdentified;
}

auto BaseHoverHandler::isContextHelpRequest() const -> bool
{
  return m_isContextHelpRequest;
}

auto BaseHoverHandler::propagateHelpId(TextEditorWidget *widget, const Orca::Plugin::Core::IContext::HelpCallback &callback) -> void
{
  const Orca::Plugin::Core::HelpItem contextHelp = lastHelpItemIdentified();
  widget->setContextHelpItem(contextHelp);
  callback(contextHelp);
}

auto BaseHoverHandler::process(TextEditorWidget *widget, int pos, ReportPriority report) -> void
{
  m_toolTip.clear();
  m_priority = -1;
  m_lastHelpItemIdentified = Orca::Plugin::Core::HelpItem();

  identifyMatch(widget, pos, report);
}

auto BaseHoverHandler::identifyMatch(TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void
{
  Utils::ExecuteOnDestruction reportPriority([this, report]() { report(priority()); });

  const auto tooltip = editorWidget->extraSelectionTooltip(pos);
  if (!tooltip.isEmpty())
    setToolTip(tooltip);
}

auto BaseHoverHandler::operateTooltip(TextEditorWidget *editorWidget, const QPoint &point) -> void
{
  const QVariant helpItem = m_lastHelpItemIdentified.isValid() ? QVariant::fromValue(m_lastHelpItemIdentified) : QVariant();
  const bool extractHelp = m_lastHelpItemIdentified.isValid() && !m_lastHelpItemIdentified.isFuzzyMatch();
  const QString helpContents = extractHelp ? m_lastHelpItemIdentified.firstParagraph() : QString();
  if (m_toolTip.isEmpty() && helpContents.isEmpty()) {
    Utils::ToolTip::hide();
  } else {
    if (helpContents.isEmpty()) {
      Utils::ToolTip::show(point, m_toolTip, m_textFormat, editorWidget, helpItem);
    } else if (m_toolTip.isEmpty()) {
      Utils::ToolTip::show(point, helpContents, Qt::RichText, editorWidget, helpItem);
    } else {
      // separate labels for tool tip text and help,
      // so the text format (plain, rich, markdown) can be handled differently
      const auto layout = new QVBoxLayout;
      layout->setContentsMargins(0, 0, 0, 0);
      const auto label = new QLabel;
      label->setObjectName("qcWidgetTipTopLabel");
      label->setTextFormat(m_textFormat);
      label->setText(m_toolTip);
      layout->addWidget(label);
      const auto helpContentLabel = new QLabel("<hr/>" + helpContents);
      helpContentLabel->setObjectName("qcWidgetTipHelpLabel");
      layout->addWidget(helpContentLabel);
      Utils::ToolTip::show(point, layout, editorWidget, helpItem);
    }
  }
}

} // namespace TextEditor
