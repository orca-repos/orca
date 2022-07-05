// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <QPointer>
#include <QWidget>

namespace Core {
namespace Internal { class FindToolBar; }

class CORE_EXPORT FindToolBarPlaceHolder final : public QWidget {
  Q_OBJECT

public:
  explicit FindToolBarPlaceHolder(QWidget *owner, QWidget *parent = nullptr);
  ~FindToolBarPlaceHolder() override;

  static auto allFindToolbarPlaceHolders() -> QList<FindToolBarPlaceHolder*>;
  auto owner() const -> QWidget*;
  auto isUsedByWidget(const QWidget *widget) const -> bool;
  auto setWidget(Internal::FindToolBar *widget) -> void;
  static auto getCurrent() -> FindToolBarPlaceHolder*;
  static auto setCurrent(FindToolBarPlaceHolder *place_holder) -> void;
  auto setLightColored(bool light_colored) -> void;
  auto isLightColored() const -> bool;

private:
  QWidget *m_owner;
  QPointer<Internal::FindToolBar> m_sub_widget;
  bool m_light_colored = false;
  static FindToolBarPlaceHolder *m_current;
};

} // namespace Core
