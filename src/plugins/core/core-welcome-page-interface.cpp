// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-welcome-page-interface.hpp"

#include "core-interface.hpp"
#include "core-welcome-page-helper.hpp"

#include <utils/theme/theme.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include <qdrawutil.h>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr char WITHACCENTCOLOR_PROPERTY_NAME[] = "_withAccentColor";

static QList<IWelcomePage *> g_welcomePages;

auto IWelcomePage::allWelcomePages() -> QList<IWelcomePage*>
{
  return g_welcomePages;
}

IWelcomePage::IWelcomePage()
{
  g_welcomePages.append(this);
}

IWelcomePage::~IWelcomePage()
{
  g_welcomePages.removeOne(this);
}

auto WelcomePageFrame::buttonPalette(const bool is_active, const bool is_cursor_inside, const bool for_text) -> QPalette
{
  QPalette pal;
  pal.setBrush(QPalette::Window, {});
  pal.setBrush(QPalette::WindowText, {});

  const auto theme = orcaTheme();
  if (is_active) {
    if (for_text) {
      pal.setColor(QPalette::Window, theme->color(Theme::Welcome_ForegroundPrimaryColor));
      pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_BackgroundPrimaryColor));
    } else {
      pal.setColor(QPalette::Window, theme->color(Theme::Welcome_AccentColor));
      pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_AccentColor));
    }
  } else {
    if (is_cursor_inside) {
      if (for_text) {
        pal.setColor(QPalette::Window, theme->color(Theme::Welcome_HoverColor));
        pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_TextColor));
      } else {
        pal.setColor(QPalette::Window, theme->color(Theme::Welcome_HoverColor));
        pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_ForegroundSecondaryColor));
      }
    } else {
      if (for_text) {
        pal.setColor(QPalette::Window, theme->color(Theme::Welcome_ForegroundPrimaryColor));
        pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_TextColor));
      } else {
        pal.setColor(QPalette::Window, theme->color(Theme::Welcome_BackgroundPrimaryColor));
        pal.setColor(QPalette::WindowText, theme->color(Theme::Welcome_ForegroundSecondaryColor));
      }
    }
  }
  return pal;
}

WelcomePageFrame::WelcomePageFrame(QWidget *parent) : QWidget(parent)
{
  setContentsMargins(1, 1, 1, 1);
}

auto WelcomePageFrame::paintEvent(QPaintEvent *event) -> void
{
  QWidget::paintEvent(event);
  QPainter p(this);

  qDrawPlainRect(&p, rect(), palette().color(QPalette::WindowText), 1);

  if (property(WITHACCENTCOLOR_PROPERTY_NAME).toBool()) {
    constexpr auto accent_rect_width = 10;
    const auto accent_rect = rect().adjusted(width() - accent_rect_width, 0, 0, 0);
    p.fillRect(accent_rect, orcaTheme()->color(Theme::Welcome_AccentColor));
  }
}

class WelcomePageButtonPrivate {
public:
  explicit WelcomePageButtonPrivate(WelcomePageButton *parent) : q(parent) {}
  auto isActive() const -> bool;
  auto doUpdate(bool cursor_inside) const -> void;

  WelcomePageButton *q;
  QHBoxLayout *m_layout = nullptr;
  QLabel *m_label = nullptr;

  std::function<void()> on_clicked;
  std::function<bool()> active_checker;
};

WelcomePageButton::WelcomePageButton(QWidget *parent) : WelcomePageFrame(parent), d(new WelcomePageButtonPrivate(this))
{
  setAutoFillBackground(true);
  setPalette(buttonPalette(false, false, false));
  setContentsMargins(0, 1, 0, 1);

  d->m_label = new QLabel(this);
  d->m_label->setPalette(buttonPalette(false, false, true));
  d->m_label->setAlignment(Qt::AlignCenter);
  d->m_layout = new QHBoxLayout;
  d->m_layout->setSpacing(0);
  d->m_layout->addWidget(d->m_label);

  setSize(SizeLarge);
  setLayout(d->m_layout);
}

WelcomePageButton::~WelcomePageButton()
{
  delete d;
}

auto WelcomePageButton::mousePressEvent(QMouseEvent *) -> void
{
  if (d->on_clicked)
    d->on_clicked();
}

auto WelcomePageButton::enterEvent(EnterEvent *) -> void
{
  d->doUpdate(true);
}

auto WelcomePageButton::leaveEvent(QEvent *) -> void
{
  d->doUpdate(false);
}

auto WelcomePageButtonPrivate::isActive() const -> bool
{
  return active_checker && active_checker();
}

auto WelcomePageButtonPrivate::doUpdate(const bool cursor_inside) const -> void
{
  const auto active = isActive();
  q->setPalette(WelcomePageFrame::buttonPalette(active, cursor_inside, false));
  const auto lpal = WelcomePageFrame::buttonPalette(active, cursor_inside, true);
  m_label->setPalette(lpal);
  q->update();
}

auto WelcomePageButton::setText(const QString &text) const -> void
{
  d->m_label->setText(text);
}

auto WelcomePageButton::setSize(const Size size) const -> void
{
  const auto h_margin = size == SizeSmall ? 12 : 26;
  const auto v_margin = size == SizeSmall ? 2 : 4;
  d->m_layout->setContentsMargins(h_margin, v_margin, h_margin, v_margin);
  d->m_label->setFont(size == SizeSmall ? font() : brandFont());
}

auto WelcomePageButton::setWithAccentColor(const bool with_accent) -> void
{
  setProperty(WITHACCENTCOLOR_PROPERTY_NAME, with_accent);
}

auto WelcomePageButton::setActiveChecker(const std::function<bool ()> &value) const -> void
{
  d->active_checker = value;
}

auto WelcomePageButton::recheckActive() const -> void
{
  const auto is_active = d->isActive();
  d->doUpdate(is_active);
}

auto WelcomePageButton::click() const -> void
{
  if (d->on_clicked)
    d->on_clicked();
}

auto WelcomePageButton::setOnClicked(const std::function<void ()> &value) const -> void
{
  d->on_clicked = value;
  if (d->isActive())
    click();
}

} // namespace Orca::Plugin::Core
