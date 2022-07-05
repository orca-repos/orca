// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/id.h>

#include <QPointer>
#include <QString>
#include <QToolButton>

namespace Core {

class Command;

class CORE_EXPORT CommandButton final : public QToolButton {
  Q_OBJECT
  Q_PROPERTY(QString toolTipBase READ toolTipBase WRITE setToolTipBase)

public:
  explicit CommandButton(QWidget *parent = nullptr);
  explicit CommandButton(Utils::Id id, QWidget *parent = nullptr);

  auto setCommandId(Utils::Id id) -> void;
  auto toolTipBase() const -> QString;
  auto setToolTipBase(const QString &tool_tip_base) -> void;

private:
  auto updateToolTip() -> void;

  QPointer<Command> m_command;
  QString m_tool_tip_base;
};

} // namespace Core
