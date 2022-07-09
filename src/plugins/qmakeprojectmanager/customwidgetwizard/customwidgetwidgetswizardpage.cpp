// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwidgetwidgetswizardpage.hpp"
#include "ui_customwidgetwidgetswizardpage.h"
#include "classdefinition.hpp"

#include <utils/utilsicons.hpp>
#include <utils/wizard.hpp>

#include <QTimer>

#include <QStackedLayout>
#include <QIcon>

namespace QmakeProjectManager {
namespace Internal {

CustomWidgetWidgetsWizardPage::CustomWidgetWidgetsWizardPage(QWidget *parent) : QWizardPage(parent), m_ui(new Ui::CustomWidgetWidgetsWizardPage), m_tabStackLayout(new QStackedLayout), m_complete(false)
{
  m_ui->setupUi(this);
  m_ui->tabStackWidget->setLayout(m_tabStackLayout);
  m_ui->addButton->setIcon(Utils::Icons::PLUS_TOOLBAR.icon());
  connect(m_ui->addButton, &QAbstractButton::clicked, m_ui->classList, &ClassList::startEditingNewClassItem);
  m_ui->deleteButton->setIcon(Utils::Icons::MINUS.icon());
  connect(m_ui->deleteButton, &QAbstractButton::clicked, m_ui->classList, &ClassList::removeCurrentClass);
  m_ui->deleteButton->setEnabled(false);

  // Disabled dummy for <new class> column>.
  auto *dummy = new ClassDefinition;
  dummy->setFileNamingParameters(m_fileNamingParameters);
  dummy->setEnabled(false);
  m_tabStackLayout->addWidget(dummy);

  connect(m_ui->classList, &ClassList::currentRowChanged, this, &CustomWidgetWidgetsWizardPage::slotCurrentRowChanged);
  connect(m_ui->classList, &ClassList::classAdded, this, &CustomWidgetWidgetsWizardPage::slotClassAdded);
  connect(m_ui->classList, &ClassList::classDeleted, this, &CustomWidgetWidgetsWizardPage::slotClassDeleted);
  connect(m_ui->classList, &ClassList::classRenamed, this, &CustomWidgetWidgetsWizardPage::slotClassRenamed);

  setProperty(Utils::SHORT_TITLE_PROPERTY, tr("Custom Widgets"));
}

CustomWidgetWidgetsWizardPage::~CustomWidgetWidgetsWizardPage()
{
  delete m_ui;
}

auto CustomWidgetWidgetsWizardPage::isComplete() const -> bool
{
  return m_complete;
}

auto CustomWidgetWidgetsWizardPage::initializePage() -> void
{
  // Takes effect only if visible.
  QTimer::singleShot(0, m_ui->classList, &ClassList::startEditingNewClassItem);
}

auto CustomWidgetWidgetsWizardPage::slotCurrentRowChanged(int row) -> void
{
  const auto onDummyItem = row == m_tabStackLayout->count() - 1;
  m_ui->deleteButton->setEnabled(!onDummyItem);
  m_tabStackLayout->setCurrentIndex(row);
}

auto CustomWidgetWidgetsWizardPage::slotClassAdded(const QString &name) -> void
{
  auto *cdef = new ClassDefinition;
  cdef->setFileNamingParameters(m_fileNamingParameters);
  const int index = m_uiClassDefs.count();
  m_tabStackLayout->insertWidget(index, cdef);
  m_tabStackLayout->setCurrentIndex(index);
  m_uiClassDefs.append(cdef);
  cdef->enableButtons();
  slotClassRenamed(index, name);
  // First class or collection class, re-check.
  slotCheckCompleteness();
}

auto CustomWidgetWidgetsWizardPage::slotClassDeleted(int index) -> void
{
  delete m_tabStackLayout->widget(index);
  m_uiClassDefs.removeAt(index);
  if (m_uiClassDefs.empty())
    slotCheckCompleteness();
}

auto CustomWidgetWidgetsWizardPage::slotClassRenamed(int index, const QString &name) -> void
{
  m_uiClassDefs[index]->setClassName(name);
}

auto CustomWidgetWidgetsWizardPage::classNameAt(int i) const -> QString
{
  return m_ui->classList->className(i);
}

auto CustomWidgetWidgetsWizardPage::widgetOptions() const -> QList<PluginOptions::WidgetOptions>
{
  QList<PluginOptions::WidgetOptions> rc;
  for (auto i = 0; i < m_uiClassDefs.count(); i++) {
    const ClassDefinition *cdef = m_uiClassDefs[i];
    rc.push_back(cdef->widgetOptions(classNameAt(i)));
  }
  return rc;
}

auto CustomWidgetWidgetsWizardPage::slotCheckCompleteness() -> void
{
  // Complete if either a single custom widget or a collection
  // with a collection class name specified.
  auto completeNow = !m_uiClassDefs.isEmpty();
  if (completeNow != m_complete) {
    m_complete = completeNow;
    emit completeChanged();
  }
}

} // namespace Internal
} // namespace QmakeProjectManager
