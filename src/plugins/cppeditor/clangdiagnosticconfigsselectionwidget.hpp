// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "clangdiagnosticconfigsmodel.hpp"

#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace CppEditor {

class ClangDiagnosticConfigsWidget;

class CPPEDITOR_EXPORT ClangDiagnosticConfigsSelectionWidget : public QWidget {
  Q_OBJECT

public:
  explicit ClangDiagnosticConfigsSelectionWidget(QWidget *parent = nullptr);

  using CreateEditWidget = std::function<ClangDiagnosticConfigsWidget *(const ClangDiagnosticConfigs &configs, const Utils::Id &configToSelect)>;

  auto refresh(const ClangDiagnosticConfigsModel &model, const Utils::Id &configToSelect, const CreateEditWidget &createEditWidget) -> void;
  auto currentConfigId() const -> Utils::Id;
  auto customConfigs() const -> ClangDiagnosticConfigs;

signals:
  auto changed() -> void;

private:
  auto onButtonClicked() -> void;

  ClangDiagnosticConfigsModel m_diagnosticConfigsModel;
  Utils::Id m_currentConfigId;
  bool m_showTidyClazyUi = true;
  QLabel *m_label = nullptr;
  QPushButton *m_button = nullptr;
  CreateEditWidget m_createEditWidget;
};

} // CppEditor namespace
