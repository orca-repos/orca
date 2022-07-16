// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "namedwidget.hpp"

namespace ProjectExplorer {

NamedWidget::NamedWidget(const QString &displayName, QWidget *parent) : QWidget(parent), m_displayName(displayName) {}

auto NamedWidget::displayName() const -> QString
{
  return m_displayName;
}

} // ProjectExplorer
