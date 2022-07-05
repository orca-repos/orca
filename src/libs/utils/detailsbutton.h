// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QAbstractButton>

QT_FORWARD_DECLARE_CLASS(QGraphicsOpacityEffect)

namespace Utils {
class ORCA_UTILS_EXPORT FadingPanel : public QWidget {
  Q_OBJECT

public:
  FadingPanel(QWidget *parent = nullptr) : QWidget(parent) {}

  virtual auto fadeTo(qreal value) -> void = 0;
  virtual auto setOpacity(qreal value) -> void = 0;
};

class ORCA_UTILS_EXPORT FadingWidget : public FadingPanel {
  Q_OBJECT

public:
  FadingWidget(QWidget *parent = nullptr);

  auto fadeTo(qreal value) -> void override;
  auto opacity() -> qreal;
  auto setOpacity(qreal value) -> void override;

protected:
  QGraphicsOpacityEffect *m_opacityEffect;
};

class ORCA_UTILS_EXPORT DetailsButton : public QAbstractButton {
  Q_OBJECT Q_PROPERTY(float fader READ fader WRITE setFader)public:
  DetailsButton(QWidget *parent = nullptr);

  auto sizeHint() const -> QSize override;
  auto fader() -> float { return m_fader; }

  auto setFader(float value) -> void
  {
    m_fader = value;
    update();
  }

protected:
  auto paintEvent(QPaintEvent *e) -> void override;
  auto event(QEvent *e) -> bool override;
  auto changeEvent(QEvent *e) -> void override;

private:
  auto cacheRendering(const QSize &size, bool checked) -> QPixmap;
  QPixmap m_checkedPixmap;
  QPixmap m_uncheckedPixmap;
  float m_fader;
};

class ORCA_UTILS_EXPORT ExpandButton : public QAbstractButton {
  Q_OBJECT

public:
  ExpandButton(QWidget *parent = nullptr);

private:
  auto paintEvent(QPaintEvent *e) -> void override;
  auto sizeHint() const -> QSize override;
  auto cacheRendering() -> QPixmap;

  QPixmap m_checkedPixmap;
  QPixmap m_uncheckedPixmap;
};

} // namespace Utils
