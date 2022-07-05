// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "namevaluevalidator.h"
#include "namevaluemodel.h"
#include "tooltip/tooltip.h"

#include <QTreeView>

namespace Utils {

NameValueValidator::NameValueValidator(QWidget *parent, Utils::NameValueModel *model, QTreeView *view, const QModelIndex &index, const QString &toolTipText) : QValidator(parent), m_toolTipText(toolTipText), m_model(model), m_view(view), m_index(index)
{
  m_hideTipTimer.setInterval(2000);
  m_hideTipTimer.setSingleShot(true);
  connect(&m_hideTipTimer, &QTimer::timeout, this, []() { Utils::ToolTip::hide(); });
}

auto NameValueValidator::validate(QString &in, int &pos) const -> QValidator::State
{
  Q_UNUSED(pos)
  QModelIndex idx = m_model->variableToIndex(in);
  if (idx.isValid() && idx != m_index)
    return QValidator::Intermediate;
  Utils::ToolTip::hide();
  m_hideTipTimer.stop();
  return QValidator::Acceptable;
}

auto NameValueValidator::fixup(QString &input) const -> void
{
  Q_UNUSED(input)

  if (!m_index.isValid())
    return;
  QPoint pos = m_view->mapToGlobal(m_view->visualRect(m_index).topLeft());
  pos -= Utils::ToolTip::offsetFromPosition();
  Utils::ToolTip::show(pos, m_toolTipText);
  m_hideTipTimer.start();
  // do nothing
}

} // namespace Utils
