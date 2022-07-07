// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>
#include <QWidget>

namespace Core {
namespace Internal {

class ProgressBar final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(float cancelButtonFader READ cancelButtonFader WRITE setCancelButtonFader)

public:
  explicit ProgressBar(QWidget *parent = nullptr);

  auto title() const -> QString;
  auto setTitle(const QString &title) -> void;
  auto setTitleVisible(bool visible) -> void;
  auto isTitleVisible() const -> bool;
  auto setSubtitle(const QString &subtitle) -> void;
  auto subtitle() const -> QString;
  auto setSeparatorVisible(bool visible) -> void;
  auto isSeparatorVisible() const -> bool;
  auto setCancelEnabled(bool enabled) -> void;
  auto isCancelEnabled() const -> bool;
  auto setError(bool on) -> void;
  auto hasError() const -> bool;
  auto sizeHint() const -> QSize override;
  auto paintEvent(QPaintEvent *) -> void override;
  auto mouseMoveEvent(QMouseEvent *) -> void override;
  auto minimum() const -> int { return m_minimum; }
  auto maximum() const -> int { return m_maximum; }
  auto value() const -> int { return m_value; }
  auto finished() const -> bool { return m_finished; }
  auto reset() -> void;
  auto setRange(int minimum, int maximum) -> void;
  auto setValue(int value) -> void;
  auto setFinished(bool b) -> void;
  auto cancelButtonFader() -> float { return m_cancel_button_fader; }

  auto setCancelButtonFader(const float value) -> void
  {
    update();
    m_cancel_button_fader = value;
  }

  auto event(QEvent *) -> bool override;

signals:
  auto clicked() -> void;

protected:
  auto mousePressEvent(QMouseEvent *event) -> void override;

private:
  auto titleFont() const -> QFont;
  auto subtitleFont() const -> QFont;

  QString m_text;
  QString m_title;
  QString m_subtitle;
  bool m_title_visible = true;
  bool m_separator_visible = true;
  bool m_cancel_enabled = true;
  bool m_finished = false;
  bool m_error = false;
  float m_cancel_button_fader = 0.0;
  int m_minimum = 1;
  int m_maximum = 100;
  int m_value = 1;
  QRect m_cancel_rect;
};

} // namespace Internal
} // namespace Core
