// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>

namespace Core {

namespace Internal {
class ExternalTool;
}

class CORE_EXPORT ExternalToolManager final : public QObject {
  Q_OBJECT

public:
  ExternalToolManager();
  ~ExternalToolManager() override;

  static auto instance() -> ExternalToolManager*;
  static auto toolsByCategory() -> QMap<QString, QList<Internal::ExternalTool*>>;
  static auto toolsById() -> QMap<QString, Internal::ExternalTool*>;
  static auto setToolsByCategory(const QMap<QString, QList<Internal::ExternalTool*>> &tools) -> void;
  static auto emitReplaceSelectionRequested(const QString &output) -> void;

signals:
  auto replaceSelectionRequested(const QString &text) -> void;
};

} // namespace Core
