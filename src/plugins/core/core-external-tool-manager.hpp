// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

namespace Orca::Plugin::Core {

class ExternalTool;

class CORE_EXPORT ExternalToolManager final : public QObject {
  Q_OBJECT

public:
  ExternalToolManager();
  ~ExternalToolManager() override;

  static auto instance() -> ExternalToolManager*;
  static auto toolsByCategory() -> QMap<QString, QList<ExternalTool*>>;
  static auto toolsById() -> QMap<QString, ExternalTool*>;
  static auto setToolsByCategory(const QMap<QString, QList<ExternalTool*>> &tools) -> void;
  static auto emitReplaceSelectionRequested(const QString &output) -> void;

signals:
  auto replaceSelectionRequested(const QString &text) -> void;
};

} // namespace Orca::Plugin::Core
