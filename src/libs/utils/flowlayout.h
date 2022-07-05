// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QLayout>
#include <QStyle>

namespace Utils {

class ORCA_UTILS_EXPORT FlowLayout final : public QLayout {
public:
  explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
  FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
  ~FlowLayout() override;

  auto addItem(QLayoutItem *item) -> void override;
  auto horizontalSpacing() const -> int;
  auto verticalSpacing() const -> int;
  auto expandingDirections() const -> Qt::Orientations override;
  auto hasHeightForWidth() const -> bool override;
  auto heightForWidth(int) const -> int override;
  auto count() const -> int override;
  auto itemAt(int index) const -> QLayoutItem* override;
  auto minimumSize() const -> QSize override;
  auto setGeometry(const QRect &rect) -> void override;
  auto sizeHint() const -> QSize override;
  auto takeAt(int index) -> QLayoutItem* override;

private:
  auto doLayout(const QRect &rect, bool testOnly) const -> int;
  auto smartSpacing(QStyle::PixelMetric pm) const -> int;

  QList<QLayoutItem*> itemList;
  int m_hSpace;
  int m_vSpace;
};

} // namespace Utils
