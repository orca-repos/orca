// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

// This is a copy of a Qt private header. Please read comments in tooltip.h.

QT_BEGIN_NAMESPACE

class QWidget;

struct QEffects {
  enum Direction {
    LeftScroll = 0x0001,
    RightScroll = 0x0002,
    UpScroll = 0x0004,
    DownScroll = 0x0008
  };

  using DirFlags = uint;
};

extern Q_GUI_EXPORT auto qScrollEffect(QWidget *, QEffects::DirFlags dir = QEffects::DownScroll, int time = -1) -> void;
extern Q_GUI_EXPORT auto qFadeEffect(QWidget *, int time = -1) -> void;

QT_END_NAMESPACE
