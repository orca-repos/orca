// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.hpp>

#include <QAction>
#include <QHash>
#include <QObject>

#include <functional>

namespace Utils {
class InfoBar;
}

namespace CppEditor {
namespace Internal {

class MinimizableInfoBars : public QObject {
  Q_OBJECT

public:
  using DiagnosticWidgetCreator = std::function<QWidget *()>;
  using ActionCreator = std::function<QAction *(QWidget *widget)>;
  using Actions = QHash<Utils::Id, QAction*>;

  static auto createShowInfoBarActions(const ActionCreator &actionCreator) -> Actions;

  explicit MinimizableInfoBars(Utils::InfoBar &infoBar, QObject *parent = nullptr);

  // Expected call order: processHasProjectPart(), processHeaderDiagnostics()
  auto processHasProjectPart(bool hasProjectPart) -> void;
  auto processHeaderDiagnostics(const DiagnosticWidgetCreator &diagnosticWidgetCreator) -> void;

signals:
  auto showAction(const Utils::Id &id, bool show) -> void;

private:
  auto updateNoProjectConfiguration() -> void;
  auto updateHeaderErrors() -> void;
  auto addNoProjectConfigurationEntry(const Utils::Id &id) -> void;
  auto addHeaderErrorEntry(const Utils::Id &id, const DiagnosticWidgetCreator &diagnosticWidgetCreator) -> void;

  Utils::InfoBar &m_infoBar;
  bool m_hasProjectPart = true;
  DiagnosticWidgetCreator m_diagnosticWidgetCreator;
};

} // namespace Internal
} // namespace CppEditor
