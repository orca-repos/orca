// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QObject>

namespace Orca::Plugin::Core {

class ILocatorFilter;

class CORE_EXPORT LocatorManager final : public QObject {
  Q_OBJECT

public:
  LocatorManager();

  static auto showFilter(const ILocatorFilter *filter) -> void;
  static auto show(const QString &text, int selection_start = -1, int selection_length = 0) -> void;
  static auto createLocatorInputWidget(QWidget *window) -> QWidget*;
  static auto locatorHasFocus() -> bool;
};

} // namespace Orca::Plugin::Core
