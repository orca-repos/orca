// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "styleanimator.hpp"

#include <utils/algorithm.hpp>

#include <QStyleOption>

auto StyleAnimator::widgetAnimation(const QWidget *widget) const -> Animation*
{
  if (!widget)
    return nullptr;
  return Utils::findOrDefault(animations, Utils::equal(&Animation::widget, widget));
}

auto Animation::paint(QPainter *painter, const QStyleOption *option) -> void
{
  Q_UNUSED(option)
  Q_UNUSED(painter)
}

auto Animation::drawBlendedImage(QPainter *painter, const QRect &rect, const float alpha) -> void
{
  if (m_secondary_image.isNull() || m_primary_image.isNull())
    return;

  if (m_temp_image.isNull())
    m_temp_image = m_secondary_image;

  const auto a = qRound(alpha * 256);
  const auto ia = 256 - a;
  const auto sw = m_primary_image.width();
  const auto sh = m_primary_image.height();
  const auto bpl = static_cast<int>(m_primary_image.bytesPerLine());

  switch (m_primary_image.depth()) {
  case 32: {
    auto mixed_data = m_temp_image.bits();
    auto back_data = m_primary_image.constBits();
    auto front_data = m_secondary_image.constBits();
    for (auto sy = 0; sy < sh; sy++) {
      const auto mixed = reinterpret_cast<quint32*>(mixed_data);
      const auto back = reinterpret_cast<const quint32*>(back_data);
      const auto front = reinterpret_cast<const quint32*>(front_data);
      for (auto sx = 0; sx < sw; sx++) {
        const auto bp = back[sx];
        const auto fp = front[sx];
        mixed[sx] = qRgba((qRed(bp) * ia + qRed(fp) * a) >> 8, (qGreen(bp) * ia + qGreen(fp) * a) >> 8, (qBlue(bp) * ia + qBlue(fp) * a) >> 8, (qAlpha(bp) * ia + qAlpha(fp) * a) >> 8);
      }
      mixed_data += bpl;
      back_data += bpl;
      front_data += bpl;
    }
  }
  default:
    break;
  }
  painter->drawImage(rect, m_temp_image);
}

auto Transition::paint(QPainter *painter, const QStyleOption *option) -> void
{
  float alpha = 1.0;

  if (m_duration > 0) {
    const auto current = QTime::currentTime();
    if (m_start_time > current)
      m_start_time = current;
    const auto time_diff = m_start_time.msecsTo(current);
    alpha = time_diff / static_cast<float>(m_duration);
    if (time_diff > m_duration) {
      m_running = false;
      alpha = 1.0;
    }
  } else {
    m_running = false;
  }

  drawBlendedImage(painter, option->rect, alpha);
}

auto StyleAnimator::timerEvent(QTimerEvent *) -> void
{
  for (auto i = static_cast<int>(animations.size()) - 1; i >= 0; --i) {
    if (animations[i]->widget())
      animations[i]->widget()->update();
    if (!animations[i]->widget() || !animations[i]->widget()->isEnabled() || !animations[i]->widget()->isVisible() || animations[i]->widget()->window()->isMinimized() || !animations[i]->running()) {
      const auto a = animations.takeAt(i);
      delete a;
    }
  }

  if (animations.empty() && animation_timer.isActive())
    animation_timer.stop();
}

auto StyleAnimator::stopAnimation(const QWidget *w) -> void
{
  for (auto i = static_cast<int>(animations.size()) - 1; i >= 0; --i) {
    if (animations[i]->widget() == w) {
      const auto a = animations.takeAt(i);
      delete a;
      break;
    }
  }
}

auto StyleAnimator::startAnimation(Animation *t) -> void
{
  stopAnimation(t->widget());
  animations.append(t);
  if (!animations.empty() && !animation_timer.isActive())
    animation_timer.start(35, this);
}
