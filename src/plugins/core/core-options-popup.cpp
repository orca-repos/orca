// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-options-popup.hpp"

#include "core-action-manager.hpp"

#include <utils/qtcassert.hpp>

#include <QAction>
#include <QCheckBox>
#include <QEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>

using namespace Utils;

namespace Orca::Plugin::Core {

/*!
    \class Orca::Plugin::Core::OptionsPopup
    \inmodule Orca
    \internal
*/

OptionsPopup::OptionsPopup(QWidget *parent, const QVector<Id> &commands) : QWidget(parent, Qt::Popup)
{
  setAttribute(Qt::WA_DeleteOnClose);
  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);
  setLayout(layout);
  auto first = true;

  for (const auto &command : commands) {
    const auto check_box = createCheckboxForCommand(command);
    if (first) {
      check_box->setFocus();
      first = false;
    }
    layout->addWidget(check_box);
  }

  const auto global_pos = parent->mapToGlobal(QPoint(0, -QWidget::sizeHint().height()));
  const auto screen_geometry = parent->screen()->availableGeometry();

  move(global_pos.x(), std::max(global_pos.y(), screen_geometry.y()));
}

auto OptionsPopup::event(QEvent *ev) -> bool
{
  if (ev->type() == QEvent::ShortcutOverride) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(ev); ke->key() == Qt::Key_Escape && !ke->modifiers()) {
      ev->accept();
      return true;
    }
  }
  return QWidget::event(ev);
}

auto OptionsPopup::eventFilter(QObject *obj, QEvent *ev) -> bool
{
  if (const auto checkbox = qobject_cast<QCheckBox*>(obj); ev->type() == QEvent::KeyPress && checkbox) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(ev); !ke->modifiers() && (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return)) {
      checkbox->click();
      ev->accept();
      return true;
    }
  }
  return QWidget::eventFilter(obj, ev);
}

auto OptionsPopup::actionChanged() const -> void
{
  const auto action = qobject_cast<QAction*>(sender());
  QTC_ASSERT(action, return);
  const auto checkbox = m_checkbox_map.value(action);
  QTC_ASSERT(checkbox, return);
  checkbox->setEnabled(action->isEnabled());
}

auto OptionsPopup::createCheckboxForCommand(const Id id) -> QCheckBox*
{
  const auto action = ActionManager::command(id)->action();
  const auto checkbox = new QCheckBox(action->text());
  checkbox->setToolTip(action->toolTip());
  checkbox->setChecked(action->isChecked());
  checkbox->setEnabled(action->isEnabled());
  checkbox->installEventFilter(this); // enter key handling
  connect(checkbox, &QCheckBox::clicked, action, &QAction::setChecked);
  connect(action, &QAction::changed, this, &OptionsPopup::actionChanged);
  m_checkbox_map.insert(action, checkbox);
  return checkbox;
}

} // namespace Orca::Plugin::Core
