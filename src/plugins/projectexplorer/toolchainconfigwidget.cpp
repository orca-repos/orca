// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "toolchainconfigwidget.hpp"
#include "toolchain.hpp"

#include <utils/detailswidget.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QString>

#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QPainter>

using namespace Utils;

namespace ProjectExplorer {

ToolChainConfigWidget::ToolChainConfigWidget(ToolChain *tc) : m_toolChain(tc)
{
  Q_ASSERT(tc);

  const auto centralWidget = new DetailsWidget;
  centralWidget->setState(DetailsWidget::NoSummary);

  setFrameShape(NoFrame);
  setWidgetResizable(true);
  setFocusPolicy(Qt::NoFocus);

  setWidget(centralWidget);

  const auto detailsBox = new QWidget();

  m_mainLayout = new QFormLayout(detailsBox);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  centralWidget->setWidget(detailsBox);
  m_mainLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow); // for the Macs...

  m_nameLineEdit = new QLineEdit;
  m_nameLineEdit->setText(tc->displayName());

  m_mainLayout->addRow(tr("Name:"), m_nameLineEdit);

  connect(m_nameLineEdit, &QLineEdit::textChanged, this, &ToolChainConfigWidget::dirty);
}

auto ToolChainConfigWidget::apply() -> void
{
  m_toolChain->setDisplayName(m_nameLineEdit->text());
  applyImpl();
}

auto ToolChainConfigWidget::discard() -> void
{
  m_nameLineEdit->setText(m_toolChain->displayName());
  discardImpl();
}

auto ToolChainConfigWidget::isDirty() const -> bool
{
  return m_nameLineEdit->text() != m_toolChain->displayName() || isDirtyImpl();
}

auto ToolChainConfigWidget::toolChain() const -> ToolChain*
{
  return m_toolChain;
}

auto ToolChainConfigWidget::makeReadOnly() -> void
{
  m_nameLineEdit->setEnabled(false);
  makeReadOnlyImpl();
}

auto ToolChainConfigWidget::addErrorLabel() -> void
{
  if (!m_errorLabel) {
    m_errorLabel = new QLabel;
    m_errorLabel->setVisible(false);
  }
  m_mainLayout->addRow(m_errorLabel);
}

auto ToolChainConfigWidget::setErrorMessage(const QString &m) -> void
{
  QTC_ASSERT(m_errorLabel, return);
  if (m.isEmpty()) {
    clearErrorMessage();
  } else {
    m_errorLabel->setText(m);
    m_errorLabel->setStyleSheet(QLatin1String("background-color: \"red\""));
    m_errorLabel->setVisible(true);
  }
}

auto ToolChainConfigWidget::clearErrorMessage() -> void
{
  QTC_ASSERT(m_errorLabel, return);
  m_errorLabel->clear();
  m_errorLabel->setStyleSheet(QString());
  m_errorLabel->setVisible(false);
}

auto ToolChainConfigWidget::splitString(const QString &s) -> QStringList
{
  ProcessArgs::SplitError splitError;
  const auto osType = HostOsInfo::hostOs();
  auto res = ProcessArgs::splitArgs(s, osType, false, &splitError);
  if (splitError != ProcessArgs::SplitOk) {
    res = ProcessArgs::splitArgs(s + '\\', osType, false, &splitError);
    if (splitError != ProcessArgs::SplitOk) {
      res = ProcessArgs::splitArgs(s + '"', osType, false, &splitError);
      if (splitError != ProcessArgs::SplitOk)
        res = ProcessArgs::splitArgs(s + '\'', osType, false, &splitError);
    }
  }
  return res;
}

} // namespace ProjectExplorer
