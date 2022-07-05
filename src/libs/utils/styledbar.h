// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWidget>

namespace Utils {

class ORCA_UTILS_EXPORT StyledBar : public QWidget {
  Q_OBJECT

public:
  StyledBar(QWidget *parent = nullptr);
  auto setSingleRow(bool singleRow) -> void;
  auto isSingleRow() const -> bool;

  auto setLightColored(bool lightColored) -> void;
  auto isLightColored() const -> bool;

protected:
  auto paintEvent(QPaintEvent *event) -> void override;
};

class ORCA_UTILS_EXPORT StyledSeparator : public QWidget {
  Q_OBJECT

public:
  StyledSeparator(QWidget *parent = nullptr);

protected:
  auto paintEvent(QPaintEvent *event) -> void override;
};

} // Utils
