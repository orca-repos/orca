// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codegensettingspage.hpp"

#include "codegensettings.hpp"
#include "qtsupportconstants.hpp"
#include "ui_codegensettingspagewidget.h"

#include <core/icore.hpp>
#include <cppeditor/cppeditorconstants.hpp>

#include <QCoreApplication>

namespace QtSupport {
namespace Internal {

// ---------- CodeGenSettingsPageWidget

class CodeGenSettingsPageWidget : public Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(QtSupport::Internal::CodeGenSettingsPage)

public:
  CodeGenSettingsPageWidget();

private:
  auto apply() -> void final;
  auto uiEmbedding() const -> int;
  auto setUiEmbedding(int) -> void;

  Ui::CodeGenSettingsPageWidget m_ui;
};

CodeGenSettingsPageWidget::CodeGenSettingsPageWidget()
{
  m_ui.setupUi(this);

  CodeGenSettings parameters;
  parameters.fromSettings(Core::ICore::settings());

  m_ui.retranslateCheckBox->setChecked(parameters.retranslationSupport);
  m_ui.includeQtModuleCheckBox->setChecked(parameters.includeQtModule);
  m_ui.addQtVersionCheckBox->setChecked(parameters.addQtVersionCheck);
  setUiEmbedding(parameters.embedding);

  connect(m_ui.includeQtModuleCheckBox, &QAbstractButton::toggled, m_ui.addQtVersionCheckBox, &QWidget::setEnabled);
}

auto CodeGenSettingsPageWidget::apply() -> void
{
  CodeGenSettings rc;
  rc.embedding = static_cast<CodeGenSettings::UiClassEmbedding>(uiEmbedding());
  rc.retranslationSupport = m_ui.retranslateCheckBox->isChecked();
  rc.includeQtModule = m_ui.includeQtModuleCheckBox->isChecked();
  rc.addQtVersionCheck = m_ui.addQtVersionCheckBox->isChecked();
  rc.toSettings(Core::ICore::settings());
}

auto CodeGenSettingsPageWidget::uiEmbedding() const -> int
{
  if (m_ui.ptrAggregationRadioButton->isChecked())
    return CodeGenSettings::PointerAggregatedUiClass;
  if (m_ui.aggregationButton->isChecked())
    return CodeGenSettings::AggregatedUiClass;
  return CodeGenSettings::InheritedUiClass;
}

auto CodeGenSettingsPageWidget::setUiEmbedding(int v) -> void
{
  switch (v) {
  case CodeGenSettings::PointerAggregatedUiClass:
    m_ui.ptrAggregationRadioButton->setChecked(true);
    break;
  case CodeGenSettings::AggregatedUiClass:
    m_ui.aggregationButton->setChecked(true);
    break;
  case CodeGenSettings::InheritedUiClass:
    m_ui.multipleInheritanceButton->setChecked(true);
    break;
  }
}

// ---------- CodeGenSettingsPage

CodeGenSettingsPage::CodeGenSettingsPage()
{
  setId(Constants::CODEGEN_SETTINGS_PAGE_ID);
  setDisplayName(QCoreApplication::translate("QtSupport", "Qt Class Generation"));
  setCategory(CppEditor::Constants::CPP_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("CppEditor", CppEditor::Constants::CPP_SETTINGS_NAME));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_cpp.png");
  setWidgetCreator([] { return new CodeGenSettingsPageWidget; });
}

} // namespace Internal
} // namespace QtSupport
