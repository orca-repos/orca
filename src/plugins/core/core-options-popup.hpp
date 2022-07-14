// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/id.hpp>

#include <QMap>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QAction;
class QCheckBox;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class CORE_EXPORT OptionsPopup final : public QWidget {
  Q_OBJECT

public:
  OptionsPopup(QWidget *parent, const QVector<Utils::Id> &commands);

protected:
  auto event(QEvent *ev) -> bool override;
  auto eventFilter(QObject *obj, QEvent *ev) -> bool override;

private:
  auto actionChanged() const -> void;
  auto createCheckboxForCommand(Utils::Id id) -> QCheckBox*;

  QMap<QAction*, QCheckBox*> m_checkbox_map;
};

} // namespace Orca::Plugin::Core
