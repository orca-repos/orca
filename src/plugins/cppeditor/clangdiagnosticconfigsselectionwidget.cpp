// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangdiagnosticconfigsselectionwidget.hpp"

#include "clangdiagnosticconfigswidget.hpp"
#include "cppcodemodelsettings.hpp"
#include "cpptoolsreuse.hpp"

#include <core/core-interface.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace CppEditor {

ClangDiagnosticConfigsSelectionWidget::ClangDiagnosticConfigsSelectionWidget(QWidget *parent) : QWidget(parent), m_label(new QLabel(tr("Diagnostic configuration:"))), m_button(new QPushButton)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  layout->addWidget(m_label);
  layout->addWidget(m_button, 1);
  layout->addStretch();

  connect(m_button, &QPushButton::clicked, this, &ClangDiagnosticConfigsSelectionWidget::onButtonClicked);
}

auto ClangDiagnosticConfigsSelectionWidget::refresh(const ClangDiagnosticConfigsModel &model, const Utils::Id &configToSelect, const CreateEditWidget &createEditWidget) -> void
{
  m_diagnosticConfigsModel = model;
  m_currentConfigId = configToSelect;
  m_createEditWidget = createEditWidget;

  const auto config = m_diagnosticConfigsModel.configWithId(configToSelect);
  m_button->setText(config.displayName());
}

auto ClangDiagnosticConfigsSelectionWidget::currentConfigId() const -> Utils::Id
{
  return m_currentConfigId;
}

auto ClangDiagnosticConfigsSelectionWidget::customConfigs() const -> ClangDiagnosticConfigs
{
  return m_diagnosticConfigsModel.customConfigs();
}

auto ClangDiagnosticConfigsSelectionWidget::onButtonClicked() -> void
{
  auto widget = m_createEditWidget(m_diagnosticConfigsModel.allConfigs(), m_currentConfigId);
  widget->sync();
  widget->layout()->setContentsMargins(0, 0, 0, 0);

  QDialog dialog;
  dialog.setWindowTitle(ClangDiagnosticConfigsWidget::tr("Diagnostic Configurations"));
  dialog.setLayout(new QVBoxLayout);
  dialog.layout()->addWidget(widget);
  auto *buttonsBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  dialog.layout()->addWidget(buttonsBox);

  connect(buttonsBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonsBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  const auto previousEnableLowerClazyLevels = codeModelSettings()->enableLowerClazyLevels();
  if (dialog.exec() == QDialog::Accepted) {
    if (previousEnableLowerClazyLevels != codeModelSettings()->enableLowerClazyLevels())
      codeModelSettings()->toSettings(Orca::Plugin::Core::ICore::settings());

    m_diagnosticConfigsModel = ClangDiagnosticConfigsModel(widget->configs());
    m_currentConfigId = widget->currentConfig().id();
    m_button->setText(widget->currentConfig().displayName());

    emit changed();
  }
}

} // CppEditor namespace
