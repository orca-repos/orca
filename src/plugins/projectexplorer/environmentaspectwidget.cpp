// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environmentaspectwidget.hpp"

#include "environmentwidget.hpp"

#include <utils/environment.hpp>
#include <utils/qtcassert.hpp>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace ProjectExplorer {

// --------------------------------------------------------------------
// EnvironmentAspectWidget:
// --------------------------------------------------------------------

EnvironmentAspectWidget::EnvironmentAspectWidget(EnvironmentAspect *aspect, QWidget *additionalWidget) : m_aspect(aspect), m_additionalWidget(additionalWidget)
{
  QTC_CHECK(m_aspect);

  setContentsMargins(0, 0, 0, 0);
  const auto topLayout = new QVBoxLayout(this);
  topLayout->setContentsMargins(0, 0, 0, 25);

  const auto baseEnvironmentWidget = new QWidget;
  const auto baseLayout = new QHBoxLayout(baseEnvironmentWidget);
  baseLayout->setContentsMargins(0, 0, 0, 0);
  const auto label = new QLabel(tr("Base environment for this run configuration:"), this);
  baseLayout->addWidget(label);

  m_baseEnvironmentComboBox = new QComboBox;
  for (const auto &displayName : m_aspect->displayNames())
    m_baseEnvironmentComboBox->addItem(displayName);
  if (m_baseEnvironmentComboBox->count() == 1)
    m_baseEnvironmentComboBox->setEnabled(false);
  m_baseEnvironmentComboBox->setCurrentIndex(m_aspect->baseEnvironmentBase());

  connect(m_baseEnvironmentComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EnvironmentAspectWidget::baseEnvironmentSelected);

  baseLayout->addWidget(m_baseEnvironmentComboBox);
  baseLayout->addStretch(10);
  if (additionalWidget)
    baseLayout->addWidget(additionalWidget);

  const auto widgetType = aspect->isLocal() ? EnvironmentWidget::TypeLocal : EnvironmentWidget::TypeRemote;
  m_environmentWidget = new EnvironmentWidget(this, widgetType, baseEnvironmentWidget);
  m_environmentWidget->setBaseEnvironment(m_aspect->modifiedBaseEnvironment());
  m_environmentWidget->setBaseEnvironmentText(m_aspect->currentDisplayName());
  m_environmentWidget->setUserChanges(m_aspect->userEnvironmentChanges());
  m_environmentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  topLayout->addWidget(m_environmentWidget);

  connect(m_environmentWidget, &EnvironmentWidget::userChangesChanged, this, &EnvironmentAspectWidget::userChangesEdited);

  connect(m_aspect, &EnvironmentAspect::baseEnvironmentChanged, this, &EnvironmentAspectWidget::changeBaseEnvironment);
  connect(m_aspect, &EnvironmentAspect::userEnvironmentChangesChanged, this, &EnvironmentAspectWidget::changeUserChanges);
  connect(m_aspect, &EnvironmentAspect::environmentChanged, this, &EnvironmentAspectWidget::environmentChanged);
}

auto EnvironmentAspectWidget::aspect() const -> EnvironmentAspect*
{
  return m_aspect;
}

auto EnvironmentAspectWidget::additionalWidget() const -> QWidget*
{
  return m_additionalWidget;
}

auto EnvironmentAspectWidget::baseEnvironmentSelected(int idx) -> void
{
  m_ignoreChange = true;
  m_aspect->setBaseEnvironmentBase(idx);
  m_environmentWidget->setBaseEnvironment(m_aspect->modifiedBaseEnvironment());
  m_environmentWidget->setBaseEnvironmentText(m_aspect->currentDisplayName());
  m_ignoreChange = false;
}

auto EnvironmentAspectWidget::changeBaseEnvironment() -> void
{
  if (m_ignoreChange)
    return;

  const auto base = m_aspect->baseEnvironmentBase();
  for (auto i = 0; i < m_baseEnvironmentComboBox->count(); ++i) {
    if (m_baseEnvironmentComboBox->itemData(i).toInt() == base)
      m_baseEnvironmentComboBox->setCurrentIndex(i);
  }
  m_environmentWidget->setBaseEnvironmentText(m_aspect->currentDisplayName());
  m_environmentWidget->setBaseEnvironment(m_aspect->modifiedBaseEnvironment());
}

auto EnvironmentAspectWidget::userChangesEdited() -> void
{
  m_ignoreChange = true;
  m_aspect->setUserEnvironmentChanges(m_environmentWidget->userChanges());
  m_ignoreChange = false;
}

auto EnvironmentAspectWidget::changeUserChanges(Utils::EnvironmentItems changes) -> void
{
  if (m_ignoreChange)
    return;
  m_environmentWidget->setUserChanges(changes);
}

auto EnvironmentAspectWidget::environmentChanged() -> void
{
  if (m_ignoreChange)
    return;
  m_environmentWidget->setBaseEnvironment(m_aspect->modifiedBaseEnvironment());
}

} // namespace ProjectExplorer
