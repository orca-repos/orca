// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "clangdiagnosticconfig.hpp"

#include <QHash>
#include <QWidget>

#include <memory>

QT_BEGIN_NAMESPACE
class QTabWidget;
QT_END_NAMESPACE

namespace CppEditor {
class ClangDiagnosticConfig;

namespace Ui {
class ClangDiagnosticConfigsWidget;
class ClangBaseChecks;
}

class ConfigsModel;

class CPPEDITOR_EXPORT ClangDiagnosticConfigsWidget : public QWidget {
  Q_OBJECT

public:
  explicit ClangDiagnosticConfigsWidget(const ClangDiagnosticConfigs &configs, const Utils::Id &configToSelect, QWidget *parent = nullptr);
  ~ClangDiagnosticConfigsWidget() override;

  auto sync() -> void;

  auto configs() const -> ClangDiagnosticConfigs;
  auto currentConfig() const -> const ClangDiagnosticConfig;

protected:
  auto updateConfig(const ClangDiagnosticConfig &config) -> void;
  virtual auto syncExtraWidgets(const ClangDiagnosticConfig &) -> void {}
  auto tabWidget() const -> QTabWidget*;

private:
  auto onCopyButtonClicked() -> void;
  auto onRenameButtonClicked() -> void;
  auto onRemoveButtonClicked() -> void;
  auto onClangOnlyOptionsChanged() -> void;
  auto setDiagnosticOptions(const QString &options) -> void;
  auto updateValidityWidgets(const QString &errorMessage) -> void;
  auto connectClangOnlyOptionsChanged() -> void;
  auto disconnectClangOnlyOptionsChanged() -> void;

  Ui::ClangDiagnosticConfigsWidget *m_ui;
  ConfigsModel *m_configsModel = nullptr;
  QHash<Utils::Id, QString> m_notAcceptedOptions;
  std::unique_ptr<Ui::ClangBaseChecks> m_clangBaseChecks;
  QWidget *m_clangBaseChecksWidget = nullptr;
};

} // CppEditor namespace
