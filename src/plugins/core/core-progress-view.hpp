// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-progress-manager.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class ProgressView final : public QWidget {
  Q_OBJECT

public:
  explicit ProgressView(QWidget *parent = nullptr);
  ~ProgressView() override;

  auto addProgressWidget(QWidget *widget) const -> void;
  auto removeProgressWidget(QWidget *widget) const -> void;
  auto isHovered() const -> bool;
  auto setReferenceWidget(QWidget *widget) -> void;

protected:
  auto event(QEvent *event) -> bool override;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

signals:
  auto hoveredChanged(bool hovered) -> void;

private:
  auto reposition() -> void;

  QVBoxLayout *m_layout;
  QWidget *m_reference_widget = nullptr;
  bool m_hovered = false;
};

} // namespace Orca::Plugin::Core
