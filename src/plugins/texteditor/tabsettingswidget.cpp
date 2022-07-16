// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "tabsettingswidget.hpp"
#include "ui_tabsettingswidget.h"
#include "tabsettings.hpp"

#include <QTextStream>

namespace TextEditor {

TabSettingsWidget::TabSettingsWidget(QWidget *parent) : QGroupBox(parent), ui(new Internal::Ui::TabSettingsWidget)
{
  ui->setupUi(this);
  ui->codingStyleWarning->setVisible(false);

  auto comboIndexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);
  auto spinValueChanged = QOverload<int>::of(&QSpinBox::valueChanged);

  connect(ui->codingStyleWarning, &QLabel::linkActivated, this, &TabSettingsWidget::codingStyleLinkActivated);
  connect(ui->tabPolicy, comboIndexChanged, this, &TabSettingsWidget::slotSettingsChanged);
  connect(ui->tabSize, spinValueChanged, this, &TabSettingsWidget::slotSettingsChanged);
  connect(ui->indentSize, spinValueChanged, this, &TabSettingsWidget::slotSettingsChanged);
  connect(ui->continuationAlignBehavior, comboIndexChanged, this, &TabSettingsWidget::slotSettingsChanged);
}

TabSettingsWidget::~TabSettingsWidget()
{
  delete ui;
}

auto TabSettingsWidget::setTabSettings(const TabSettings &s) -> void
{
  QSignalBlocker blocker(this);
  ui->tabPolicy->setCurrentIndex(s.m_tabPolicy);
  ui->tabSize->setValue(s.m_tabSize);
  ui->indentSize->setValue(s.m_indentSize);
  ui->continuationAlignBehavior->setCurrentIndex(s.m_continuationAlignBehavior);
}

auto TabSettingsWidget::tabSettings() const -> TabSettings
{
  TabSettings set;

  set.m_tabPolicy = (TabSettings::TabPolicy)ui->tabPolicy->currentIndex();
  set.m_tabSize = ui->tabSize->value();
  set.m_indentSize = ui->indentSize->value();
  set.m_continuationAlignBehavior = (TabSettings::ContinuationAlignBehavior)ui->continuationAlignBehavior->currentIndex();

  return set;
}

auto TabSettingsWidget::slotSettingsChanged() -> void
{
  emit settingsChanged(tabSettings());
}

auto TabSettingsWidget::codingStyleLinkActivated(const QString &linkString) -> void
{
  if (linkString == QLatin1String("C++")) emit codingStyleLinkClicked(CppLink);
  else if (linkString == QLatin1String("QtQuick")) emit codingStyleLinkClicked(QtQuickLink);
}

auto TabSettingsWidget::setCodingStyleWarningVisible(bool visible) -> void
{
  ui->codingStyleWarning->setVisible(visible);
}

} // namespace TextEditor
