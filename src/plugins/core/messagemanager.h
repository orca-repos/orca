// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"
#include "ioutputpane.h"

#include <QMetaType>
#include <QObject>

QT_BEGIN_NAMESPACE
class QFont;
QT_END_NAMESPACE

namespace Core {

namespace Internal {
class MainWindow;
}

class CORE_EXPORT MessageManager final : public QObject {
  Q_OBJECT

public:
  static auto instance() -> MessageManager*;
  static auto setFont(const QFont &font) -> void;
  static auto setWheelZoomEnabled(bool enabled) -> void;
  static auto writeSilently(const QString &message) -> void;
  static auto writeFlashing(const QString &message) -> void;
  static auto writeDisrupting(const QString &message) -> void;
  static auto writeSilently(const QStringList &messages) -> void;
  static auto writeFlashing(const QStringList &messages) -> void;
  static auto writeDisrupting(const QStringList &messages) -> void;

private:
  MessageManager();
  ~MessageManager() override;

  static auto init() -> void;
  friend class Internal::MainWindow;
};

} // namespace Core
