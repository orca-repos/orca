// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <QWidget>

namespace QtSupport {

class QTSUPPORT_EXPORT QtConfigWidget : public QWidget {
  Q_OBJECT
public:
  QtConfigWidget();

signals:
  auto changed() -> void;
};

} // namespace QtSupport
