// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findplaceholder.hpp"
#include "find/findtoolbar.hpp"

#include <QVBoxLayout>

using namespace Core;

FindToolBarPlaceHolder *FindToolBarPlaceHolder::m_current = nullptr;

static QList<FindToolBarPlaceHolder*> g_findToolBarPlaceHolders;

FindToolBarPlaceHolder::FindToolBarPlaceHolder(QWidget *owner, QWidget *parent) : QWidget(parent), m_owner(owner), m_sub_widget(nullptr)
{
  g_findToolBarPlaceHolders.append(this);
  setLayout(new QVBoxLayout);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  layout()->setContentsMargins(0, 0, 0, 0);
}

FindToolBarPlaceHolder::~FindToolBarPlaceHolder()
{
  g_findToolBarPlaceHolders.removeOne(this);

  if (m_sub_widget) {
    m_sub_widget->setVisible(false);
    m_sub_widget->setParent(nullptr);
  }

  if (m_current == this)
    m_current = nullptr;
}

auto FindToolBarPlaceHolder::allFindToolbarPlaceHolders() -> QList<FindToolBarPlaceHolder*>
{
  return g_findToolBarPlaceHolders;
}

auto FindToolBarPlaceHolder::owner() const -> QWidget*
{
  return m_owner;
}

/*!
 * Returns if \a widget is a subwidget of the place holder's owner
 */
auto FindToolBarPlaceHolder::isUsedByWidget(const QWidget *widget) const -> bool
{
  auto current = widget;

  while (current) {
    if (current == m_owner)
      return true;
    current = current->parentWidget();
  }

  return false;
}

auto FindToolBarPlaceHolder::setWidget(Internal::FindToolBar *widget) -> void
{
  if (m_sub_widget) {
    m_sub_widget->setVisible(false);
    m_sub_widget->setParent(nullptr);
  }

  m_sub_widget = widget;

  if (m_sub_widget) {
    m_sub_widget->setLightColored(m_light_colored);
    m_sub_widget->setLightColoredIcon(m_light_colored);
    layout()->addWidget(m_sub_widget);
  }
}

auto FindToolBarPlaceHolder::getCurrent() -> FindToolBarPlaceHolder*
{
  return m_current;
}

auto FindToolBarPlaceHolder::setCurrent(FindToolBarPlaceHolder *place_holder) -> void
{
  m_current = place_holder;
}

auto FindToolBarPlaceHolder::setLightColored(const bool light_colored) -> void
{
  m_light_colored = light_colored;
}

auto FindToolBarPlaceHolder::isLightColored() const -> bool
{
  return m_light_colored;
}
