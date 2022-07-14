// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QBasicTimer>
#include <QPointer>
#include <QStyle>
#include <QTime>
#include <QWidget>

/*
 * This is a set of helper classes to allow for widget animations in
 * the style. Its mostly taken from Vista style so it should be fully documented
 * there.
 *
 */

namespace Orca::Plugin::Core {

class Animation {
public :
  Animation() = default;
  virtual ~Animation() = default;

  auto widget() const -> QWidget* { return m_widget; }
  auto running() const -> bool { return m_running; }
  auto startTime() const -> const QTime& { return m_start_time; }
  auto setRunning(const bool val) -> void { m_running = val; }
  auto setWidget(QWidget *widget) -> void { m_widget = widget; }
  auto setStartTime(const QTime &start_time) -> void { m_start_time = start_time; }
  virtual auto paint(QPainter *painter, const QStyleOption *option) -> void;

protected:
  auto drawBlendedImage(QPainter *painter, const QRect &rect, float alpha) -> void;

  QTime m_start_time;
  QPointer<QWidget> m_widget;
  QImage m_primary_image;
  QImage m_secondary_image;
  QImage m_temp_image;
  bool m_running = true;
};

// Handles state transition animations
class Transition final : public Animation {
public :
  Transition() = default;
  ~Transition() override = default;

  auto setDuration(const int duration) -> void { m_duration = duration; }
  auto setStartImage(const QImage &image) -> void { m_primary_image = image; }
  auto setEndImage(const QImage &image) -> void { m_secondary_image = image; }
  auto paint(QPainter *painter, const QStyleOption *option) -> void override;
  auto duration() const -> int { return m_duration; }

  int m_duration = 100; //set time in ms to complete a state transition
};

class StyleAnimator final : public QObject {
  Q_OBJECT

public:
  explicit StyleAnimator(QObject *parent = nullptr) : QObject(parent) {}

  auto timerEvent(QTimerEvent *) -> void override;
  auto startAnimation(Animation *) -> void;
  auto stopAnimation(const QWidget *) -> void;
  auto widgetAnimation(const QWidget *) const -> Animation*;

private:
  QBasicTimer animation_timer;
  QList<Animation*> animations;
};

} // namespace Orca::Plugin::Core
