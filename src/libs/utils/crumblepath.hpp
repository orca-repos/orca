// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QVariant>
#include <QWidget>

namespace Utils {

class CrumblePathButton;

class ORCA_UTILS_EXPORT CrumblePath : public QWidget {
  Q_OBJECT

public:
  explicit CrumblePath(QWidget *parent = nullptr);
  ~CrumblePath() override;

  auto dataForIndex(int index) const -> QVariant;
  auto dataForLastIndex() const -> QVariant;
  auto length() const -> int;

public slots:
  auto pushElement(const QString &title, const QVariant &data = QVariant()) -> void;
  auto addChild(const QString &title, const QVariant &data = QVariant()) -> void;
  auto popElement() -> void;
  virtual auto clear() -> void;

signals:
  auto elementClicked(const QVariant &data) -> void;

private:
  auto emitElementClicked() -> void;
  auto setBackgroundStyle() -> void;

  QList<CrumblePathButton*> m_buttons;
  QLayout *m_buttonsLayout = nullptr;
};

} // namespace Utils
