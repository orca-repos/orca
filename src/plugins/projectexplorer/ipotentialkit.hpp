// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>
#include <QMetaType>
#include "projectexplorer_export.hpp"

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT IPotentialKit : public QObject {
  Q_OBJECT

public:
  IPotentialKit();
  ~IPotentialKit() override;

  virtual auto displayName() const -> QString = 0;
  virtual auto executeFromMenu() -> void = 0;
  virtual auto createWidget(QWidget *parent) const -> QWidget* = 0;
  virtual auto isEnabled() const -> bool = 0;
};

}
