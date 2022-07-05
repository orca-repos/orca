// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "statusbarmanager.h"
#include "imode.h"
#include "mainwindow.h"
#include "minisplitter.h"

#include <utils/qtcassert.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>

namespace Core {

constexpr char g_k_settings_group[] = "StatusBar";
constexpr char g_k_left_split_width_key[] = "LeftSplitWidth";

static QPointer<QSplitter> m_splitter;
static QList<QPointer<QWidget>> m_status_bar_widgets;
static QList<QPointer<IContext>> m_contexts;

/*
    Context that always returns the context of the active's mode widget (if available).
*/
class StatusBarContext final : public IContext {
public:
  explicit StatusBarContext(QObject *parent);
};

static auto createWidget(QWidget *parent) -> QWidget*
{
  const auto w = new QWidget(parent);
  w->setLayout(new QHBoxLayout);
  w->setVisible(true);
  w->layout()->setContentsMargins(0, 0, 0, 0);
  return w;
}

static auto createStatusBarManager() -> void
{
  const auto bar = ICore::statusBar();

  m_splitter = new NonResizingSplitter(bar);
  bar->insertPermanentWidget(0, m_splitter, 10);
  m_splitter->setChildrenCollapsible(false);
  // first
  auto w = createWidget(m_splitter);
  w->layout()->setContentsMargins(0, 0, 3, 0);
  m_splitter->addWidget(w);
  m_status_bar_widgets.append(w);

  const auto w2 = createWidget(m_splitter);
  w2->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_splitter->addWidget(w2);
  // second
  w = createWidget(w2);
  w2->layout()->addWidget(w);
  m_status_bar_widgets.append(w);
  // third
  w = createWidget(w2);
  w2->layout()->addWidget(w);
  m_status_bar_widgets.append(w);

  dynamic_cast<QBoxLayout*>(w2->layout())->addStretch(1);

  const auto right_corner_widget = createWidget(bar);
  bar->insertPermanentWidget(1, right_corner_widget);
  m_status_bar_widgets.append(right_corner_widget);

  auto status_context = new StatusBarContext(bar);
  status_context->setWidget(bar);
  ICore::addContextObject(status_context);

  QObject::connect(ICore::instance(), &ICore::saveSettingsRequested, [] {
    QSettings *s = ICore::settings();
    s->beginGroup(QLatin1String(g_k_settings_group));
    s->setValue(QLatin1String(g_k_left_split_width_key), m_splitter->sizes().at(0));
    s->endGroup();
  });

  QObject::connect(ICore::instance(), &ICore::coreAboutToClose, [status_context] {
    delete status_context;
    // This is the catch-all on rampdown. Individual items may
    // have been removed earlier by destroyStatusBarWidget().
    for (const auto &context : qAsConst(m_contexts)) {
      ICore::removeContextObject(context);
      delete context;
    }
    m_contexts.clear();
  });
}

auto StatusBarManager::addStatusBarWidget(QWidget *widget, const StatusBarPosition position, const Context &ctx) -> void
{
  if (!m_splitter)
    createStatusBarManager();

  QTC_ASSERT(widget, return);
  QTC_CHECK(widget->parent() == nullptr); // We re-parent, so user code does need / should not set it.
  m_status_bar_widgets.at(position)->layout()->addWidget(widget);

  const auto context = new IContext;
  context->setWidget(widget);
  context->setContext(ctx);
  m_contexts.append(context);

  ICore::addContextObject(context);
}

auto StatusBarManager::destroyStatusBarWidget(QWidget *widget) -> void
{
  QTC_ASSERT(widget, return);

  if (const auto it = std::ranges::find_if(m_contexts, [widget](const auto &context) { return context->widget() == widget; }); it != m_contexts.end()) {
    delete *it;
    m_contexts.erase(it);
  }

  widget->setParent(nullptr);
  delete widget;
}

auto StatusBarManager::restoreSettings() -> void
{
  QSettings *s = ICore::settings();
  s->beginGroup(QLatin1String(g_k_settings_group));
  auto left_split_width = s->value(QLatin1String(g_k_left_split_width_key), -1).toInt();
  s->endGroup();

  if (left_split_width < 0) {
    // size first split after its sizeHint + a bit of buffer
    left_split_width = m_splitter->widget(0)->sizeHint().width();
  }

  auto sum = 0;
  for(const auto &w: m_splitter->sizes())
    sum += w;

  m_splitter->setSizes(QList<int>() << left_split_width << sum - left_split_width);
}

StatusBarContext::StatusBarContext(QObject *parent) : IContext(parent) {}

} // Core
