// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "pluginerroroverview.hpp"
#include "ui_pluginerroroverview.h"
#include "pluginspec.hpp"
#include "pluginmanager.hpp"

Q_DECLARE_METATYPE(ExtensionSystem::PluginSpec*)

namespace ExtensionSystem {

/*!
    \class ExtensionSystem::PluginErrorOverview
    \internal
*/

PluginErrorOverview::PluginErrorOverview(QWidget *parent) : QDialog(parent), m_ui(new Internal::Ui::PluginErrorOverview)
{
  m_ui->setupUi(this);
  m_ui->buttonBox->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);

  for (PluginSpec *spec : PluginManager::plugins()) {
    // only show errors on startup if plugin is enabled.
    if (spec->hasError() && spec->isEffectivelyEnabled()) {
      QListWidgetItem *item = new QListWidgetItem(spec->name());
      item->setData(Qt::UserRole, QVariant::fromValue(spec));
      m_ui->pluginList->addItem(item);
    }
  }

  connect(m_ui->pluginList, &QListWidget::currentItemChanged, this, &PluginErrorOverview::showDetails);

  if (m_ui->pluginList->count() > 0)
    m_ui->pluginList->setCurrentRow(0);
}

PluginErrorOverview::~PluginErrorOverview()
{
  delete m_ui;
}

auto PluginErrorOverview::showDetails(QListWidgetItem *item) -> void
{
  if (item) {
    auto *spec = item->data(Qt::UserRole).value<PluginSpec*>();
    m_ui->pluginError->setText(spec->errorString());
  } else {
    m_ui->pluginError->clear();
  }
}

} // namespace ExtensionSystem
