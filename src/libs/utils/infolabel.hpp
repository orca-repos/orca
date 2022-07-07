// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "elidinglabel.hpp"

namespace Utils {

class ORCA_UTILS_EXPORT InfoLabel : public ElidingLabel {
public:
  enum InfoType {
    Information,
    Warning,
    Error,
    Ok,
    NotOk,
    None
  };

  explicit InfoLabel(QWidget *parent);
  explicit InfoLabel(const QString &text = {}, InfoType type = Information, QWidget *parent = nullptr);

  auto type() const -> InfoType;
  auto setType(InfoType type) -> void;
  auto filled() const -> bool;
  auto setFilled(bool filled) -> void;
  auto minimumSizeHint() const -> QSize override;

protected:
  auto paintEvent(QPaintEvent *event) -> void override;

private:
  InfoType m_type = Information;
  bool m_filled = false;
};

} // namespace Utils
