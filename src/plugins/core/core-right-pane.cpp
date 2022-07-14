// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-right-pane.hpp"

#include "core-mode-interface.hpp"
#include "core-mode-manager.hpp"

#include <utils/qtcsettings.hpp>

#include <QResizeEvent>
#include <QSplitter>
#include <QVBoxLayout>

using namespace Utils;

namespace Orca::Plugin::Core {

RightPanePlaceHolder *RightPanePlaceHolder::m_current = nullptr;

auto RightPanePlaceHolder::current() -> RightPanePlaceHolder*
{
  return m_current;
}

RightPanePlaceHolder::RightPanePlaceHolder(const Id mode, QWidget *parent) : QWidget(parent), m_mode(mode)
{
  setLayout(new QVBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &RightPanePlaceHolder::currentModeChanged);
}

RightPanePlaceHolder::~RightPanePlaceHolder()
{
  if (m_current == this) {
    RightPaneWidget::instance()->setParent(nullptr);
    RightPaneWidget::instance()->hide();
  }
}

auto RightPanePlaceHolder::applyStoredSize(const int width) -> void
{
  if (width) {
    if (const auto splitter = qobject_cast<QSplitter*>(parentWidget())) {
      // A splitter we need to resize the splitter sizes
      auto sizes = splitter->sizes();
      const auto index = splitter->indexOf(this);
      const auto diff = width - sizes.at(index);
      const int adjust = sizes.count() > 1 ? (diff / (sizes.count() - 1)) : 0;
      for (auto i = 0; i < sizes.count(); ++i) {
        if (i != index)
          sizes[i] -= adjust;
      }
      sizes[index] = width;
      splitter->setSizes(sizes);
    } else {
      auto s = size();
      s.setWidth(width);
      resize(s);
    }
  }
}

// This function does work even though the order in which
// the placeHolder get the signal is undefined.
// It does ensure that after all PlaceHolders got the signal
// m_current points to the current PlaceHolder, or zero if there
// is no PlaceHolder in this mode
// And that the parent of the RightPaneWidget gets the correct parent
auto RightPanePlaceHolder::currentModeChanged(const Id mode) -> void
{
  if (m_current == this) {
    m_current = nullptr;
    RightPaneWidget::instance()->setParent(nullptr);
    RightPaneWidget::instance()->hide();
  }
  if (m_mode == mode) {
    m_current = this;
    const auto width = RightPaneWidget::instance()->storedWidth();
    layout()->addWidget(RightPaneWidget::instance());
    RightPaneWidget::instance()->show();
    applyStoredSize(width);
    setVisible(RightPaneWidget::instance()->isShown());
  }
}

RightPaneWidget *RightPaneWidget::m_instance = nullptr;

RightPaneWidget::RightPaneWidget()
{
  m_instance = this;
  const auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

RightPaneWidget::~RightPaneWidget()
{
  clearWidget();
  m_instance = nullptr;
}

auto RightPaneWidget::instance() -> RightPaneWidget*
{
  return m_instance;
}

auto RightPaneWidget::setWidget(QWidget *widget) -> void
{
  if (widget == m_widget)
    return;

  clearWidget();
  m_widget = widget;

  if (m_widget) {
    m_widget->setParent(this);
    layout()->addWidget(m_widget);
    setFocusProxy(m_widget);
    m_widget->show();
  }
}

auto RightPaneWidget::widget() const -> QWidget*
{
  return m_widget;
}

auto RightPaneWidget::storedWidth() const -> int
{
  return m_width;
}

auto RightPaneWidget::resizeEvent(QResizeEvent *re) -> void
{
  if (m_width && re->size().width())
    m_width = re->size().width();
  QWidget::resizeEvent(re);
}

static constexpr bool g_k_visible_default = false;
static constexpr int g_k_width_default = 500;

auto RightPaneWidget::saveSettings(Utils::QtcSettings *settings) const -> void
{
  settings->setValueWithDefault("RightPane/Visible", isShown(), g_k_visible_default);
  settings->setValueWithDefault("RightPane/Width", m_width, g_k_width_default);
}

auto RightPaneWidget::readSettings(const QSettings *settings) -> void
{
  setShown(settings->value(QLatin1String("RightPane/Visible"), g_k_visible_default).toBool());
  m_width = settings->value("RightPane/Width", g_k_width_default).toInt();

  // Apply
  if (RightPanePlaceHolder::m_current)
    RightPanePlaceHolder::m_current->applyStoredSize(m_width);
}

auto RightPaneWidget::setShown(bool b) -> void
{
  if (RightPanePlaceHolder::m_current)
    RightPanePlaceHolder::m_current->setVisible(b);
  m_shown = b;
}

auto RightPaneWidget::isShown() const -> bool
{
  return m_shown;
}

auto RightPaneWidget::clearWidget() -> void
{
  if (m_widget) {
    m_widget->hide();
    m_widget->setParent(nullptr);
    m_widget = nullptr;
  }
}

} // namespace Orca::Plugin::Core
