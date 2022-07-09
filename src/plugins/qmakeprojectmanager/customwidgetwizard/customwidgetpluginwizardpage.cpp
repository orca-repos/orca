// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwidgetpluginwizardpage.hpp"
#include "customwidgetwidgetswizardpage.hpp"
#include "ui_customwidgetpluginwizardpage.h"

#include <utils/wizard.hpp>

namespace QmakeProjectManager {
namespace Internal {

// Determine name for Q_EXPORT_PLUGIN
static auto createPluginName(const QString &prefix) -> QString
{
  return prefix.toLower() + QLatin1String("plugin");
}

CustomWidgetPluginWizardPage::CustomWidgetPluginWizardPage(QWidget *parent) : QWizardPage(parent), m_ui(new Ui::CustomWidgetPluginWizardPage), m_classCount(-1), m_complete(false)
{
  m_ui->setupUi(this);
  connect(m_ui->collectionClassEdit, &QLineEdit::textEdited, this, &CustomWidgetPluginWizardPage::slotCheckCompleteness);
  connect(m_ui->collectionClassEdit, &QLineEdit::textChanged, this, [this](const QString &collectionClass) {
    m_ui->collectionHeaderEdit->setText(m_fileNamingParameters.headerFileName(collectionClass));
    m_ui->pluginNameEdit->setText(createPluginName(collectionClass));
  });
  connect(m_ui->pluginNameEdit, &QLineEdit::textEdited, this, &CustomWidgetPluginWizardPage::slotCheckCompleteness);
  connect(m_ui->collectionHeaderEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    m_ui->collectionSourceEdit->setText(m_fileNamingParameters.headerToSourceFileName(text));
  });

  setProperty(Utils::SHORT_TITLE_PROPERTY, tr("Plugin Details"));
}

CustomWidgetPluginWizardPage::~CustomWidgetPluginWizardPage()
{
  delete m_ui;
}

auto CustomWidgetPluginWizardPage::collectionClassName() const -> QString
{
  return m_ui->collectionClassEdit->text();
}

auto CustomWidgetPluginWizardPage::pluginName() const -> QString
{
  return m_ui->pluginNameEdit->text();
}

auto CustomWidgetPluginWizardPage::init(const CustomWidgetWidgetsWizardPage *widgetsPage) -> void
{
  m_classCount = widgetsPage->classCount();
  const QString empty;
  if (m_classCount == 1) {
    m_ui->pluginNameEdit->setText(createPluginName(widgetsPage->classNameAt(0)));
    setCollectionEnabled(false);
  } else {
    m_ui->pluginNameEdit->setText(empty);
    setCollectionEnabled(true);
  }
  m_ui->collectionClassEdit->setText(empty);
  m_ui->collectionHeaderEdit->setText(empty);
  m_ui->collectionSourceEdit->setText(empty);

  slotCheckCompleteness();
}

auto CustomWidgetPluginWizardPage::setCollectionEnabled(bool enColl) -> void
{
  m_ui->collectionClassLabel->setEnabled(enColl);
  m_ui->collectionClassEdit->setEnabled(enColl);
  m_ui->collectionHeaderLabel->setEnabled(enColl);
  m_ui->collectionHeaderEdit->setEnabled(enColl);
  m_ui->collectionSourceLabel->setEnabled(enColl);
  m_ui->collectionSourceEdit->setEnabled(enColl);
}

auto CustomWidgetPluginWizardPage::basicPluginOptions() const -> QSharedPointer<PluginOptions>
{
  QSharedPointer<PluginOptions> po(new PluginOptions);
  po->pluginName = pluginName();
  po->resourceFile = m_ui->resourceFileEdit->text();
  po->collectionClassName = collectionClassName();
  po->collectionHeaderFile = m_ui->collectionHeaderEdit->text();
  po->collectionSourceFile = m_ui->collectionSourceEdit->text();
  return po;
}

auto CustomWidgetPluginWizardPage::slotCheckCompleteness() -> void
{
  // A collection is complete only with class name
  auto completeNow = false;
  if (!pluginName().isEmpty()) {
    if (m_classCount > 1)
      completeNow = !collectionClassName().isEmpty();
    else
      completeNow = true;
  }
  if (completeNow != m_complete) {
    m_complete = completeNow;
    emit completeChanged();
  }
}

auto CustomWidgetPluginWizardPage::isComplete() const -> bool
{
  return m_complete;
}

} // namespace Internal
} // namespace QmakeProjectManager
