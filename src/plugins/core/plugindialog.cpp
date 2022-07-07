// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "plugindialog.hpp"
#include "icore.hpp"
#include "plugininstallwizard.hpp"

#include <core/dialogs/restartdialog.hpp>

#include <extensionsystem/plugindetailsview.hpp>
#include <extensionsystem/pluginerrorview.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <extensionsystem/pluginspec.hpp>
#include <extensionsystem/pluginview.hpp>

#include <utils/fancylineedit.hpp>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

using namespace Utils;

namespace Core {
namespace Internal {

static bool g_s_is_restart_required = false;

PluginDialog::PluginDialog(QWidget *parent) : QDialog(parent), m_view(new ExtensionSystem::PluginView(this))
{
  const auto vl = new QVBoxLayout(this);
  const auto filter_layout = new QHBoxLayout;
  vl->addLayout(filter_layout);
  const auto filter_edit = new FancyLineEdit(this);
  filter_edit->setFiltering(true);
  connect(filter_edit, &FancyLineEdit::filterChanged, m_view, &ExtensionSystem::PluginView::setFilter);
  filter_layout->addWidget(filter_edit);
  vl->addWidget(m_view);

  m_details_button = new QPushButton(tr("Details"), this);
  m_error_details_button = new QPushButton(tr("Error Details"), this);
  m_close_button = new QPushButton(tr("Close"), this);
  m_install_button = new QPushButton(tr("Install Plugin..."), this);
  m_details_button->setEnabled(false);
  m_error_details_button->setEnabled(false);
  m_close_button->setEnabled(true);
  m_close_button->setDefault(true);
  m_restart_required = new QLabel(tr("Restart required."), this);

  if (!g_s_is_restart_required)
    m_restart_required->setVisible(false);

  const auto hl = new QHBoxLayout;
  hl->addWidget(m_details_button);
  hl->addWidget(m_error_details_button);
  hl->addWidget(m_install_button);
  hl->addSpacing(10);
  hl->addWidget(m_restart_required);
  hl->addStretch(5);
  hl->addWidget(m_close_button);
  vl->addLayout(hl);

  resize(650, 400);
  setWindowTitle(tr("Installed Plugins"));

  connect(m_view, &ExtensionSystem::PluginView::currentPluginChanged, this, &PluginDialog::updateButtons);
  connect(m_view, &ExtensionSystem::PluginView::pluginActivated, this, &PluginDialog::openDetails);
  connect(m_view, &ExtensionSystem::PluginView::pluginSettingsChanged, this, &PluginDialog::updateRestartRequired);
  connect(m_details_button, &QAbstractButton::clicked, [this] { openDetails(m_view->currentPlugin()); });
  connect(m_error_details_button, &QAbstractButton::clicked, this, &PluginDialog::openErrorDetails);
  connect(m_install_button, &QAbstractButton::clicked, this, &PluginDialog::showInstallWizard);
  connect(m_close_button, &QAbstractButton::clicked, this, &PluginDialog::closeDialog);

  updateButtons();
}

auto PluginDialog::closeDialog() -> void
{
  ExtensionSystem::PluginManager::writeSettings();
  if (g_s_is_restart_required) {
    RestartDialog restart_dialog(ICore::dialogParent(), tr("Plugin changes will take effect after restart."));
    restart_dialog.exec();
  }
  accept();
}

auto PluginDialog::showInstallWizard() const -> void
{
  if (PluginInstallWizard::exec())
    updateRestartRequired();
}

auto PluginDialog::updateRestartRequired() const -> void
{
  // just display the notice all the time after once changing something
  g_s_is_restart_required = true;
  m_restart_required->setVisible(true);
}

auto PluginDialog::updateButtons() const -> void
{
  if (const auto selected_spec = m_view->currentPlugin()) {
    m_details_button->setEnabled(true);
    m_error_details_button->setEnabled(selected_spec->hasError());
  } else {
    m_details_button->setEnabled(false);
    m_error_details_button->setEnabled(false);
  }
}

auto PluginDialog::openDetails(ExtensionSystem::PluginSpec *spec) -> void
{
  if (!spec)
    return;

  QDialog dialog(this);

  dialog.setWindowTitle(tr("Plugin Details of %1").arg(spec->name()));
  const auto layout = new QVBoxLayout;
  dialog.setLayout(layout);
  const auto details = new ExtensionSystem::PluginDetailsView(&dialog);
  layout->addWidget(details);
  details->update(spec);
  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dialog);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  dialog.resize(400, 500);
  dialog.exec();
}

auto PluginDialog::openErrorDetails() -> void
{
  const auto spec = m_view->currentPlugin();

  if (!spec)
    return;

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Plugin Errors of %1").arg(spec->name()));
  const auto layout = new QVBoxLayout;
  dialog.setLayout(layout);
  const auto errors = new ExtensionSystem::PluginErrorView(&dialog);
  layout->addWidget(errors);
  errors->update(spec);
  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dialog);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  dialog.resize(500, 300);
  dialog.exec();
}

} // namespace Internal
} // namespace Core
