// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-output-pane.hpp"

#include "core-mode-interface.hpp"
#include "core-mode-manager.hpp"
#include "core-output-pane-manager.hpp"

#include <QResizeEvent>
#include <QSplitter>
#include <QVBoxLayout>

using namespace Utils;

namespace Orca::Plugin::Core {

class OutputPanePlaceHolderPrivate {
public:
  explicit OutputPanePlaceHolderPrivate(Id mode, QSplitter *parent);

  Id m_mode;
  QSplitter *m_splitter;
  int m_non_maximized_size = 0;
  bool m_is_maximized = false;
  bool m_initialized = false;
  static OutputPanePlaceHolder *m_current;
};

OutputPanePlaceHolderPrivate::OutputPanePlaceHolderPrivate(const Id mode, QSplitter *parent) : m_mode(mode), m_splitter(parent) {}

OutputPanePlaceHolder *OutputPanePlaceHolderPrivate::m_current = nullptr;

OutputPanePlaceHolder::OutputPanePlaceHolder(const Id mode, QSplitter *parent) : QWidget(parent), d(new OutputPanePlaceHolderPrivate(mode, parent))
{
  QWidget::setVisible(false);
  setLayout(new QVBoxLayout);
  QSizePolicy sp;
  sp.setHorizontalPolicy(QSizePolicy::Preferred);
  sp.setVerticalPolicy(QSizePolicy::Preferred);
  sp.setHorizontalStretch(0);
  setSizePolicy(sp);
  layout()->setContentsMargins(0, 0, 0, 0);
  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &OutputPanePlaceHolder::currentModeChanged);
  // if this is part of a lazily created mode widget,
  // we need to check if this is the current placeholder
  currentModeChanged(ModeManager::currentModeId());
}

OutputPanePlaceHolder::~OutputPanePlaceHolder()
{
  if (OutputPanePlaceHolderPrivate::m_current == this) {
    if (const auto om = OutputPaneManager::instance()) {
      om->setParent(nullptr);
      om->hide();
    }
    OutputPanePlaceHolderPrivate::m_current = nullptr;
  }
  delete d;
}

auto OutputPanePlaceHolder::currentModeChanged(Id mode) -> void
{
  if (OutputPanePlaceHolderPrivate::m_current == this) {
    OutputPanePlaceHolderPrivate::m_current = nullptr;
    if (d->m_initialized)
      OutputPaneManager::setOutputPaneHeightSetting(d->m_non_maximized_size);
    const auto om = OutputPaneManager::instance();
    om->hide();
    om->setParent(nullptr);
    om->updateStatusButtons(false);
  }

  if (d->m_mode == mode) {
    if (OutputPanePlaceHolderPrivate::m_current && OutputPanePlaceHolderPrivate::m_current->d->m_initialized)
      OutputPaneManager::setOutputPaneHeightSetting(OutputPanePlaceHolderPrivate::m_current->d->m_non_maximized_size);
    OutputPanePlaceHolderPrivate::m_current = this;
    const auto om = OutputPaneManager::instance();
    layout()->addWidget(om);
    om->show();
    om->updateStatusButtons(isVisible());
    OutputPaneManager::updateMaximizeButton(d->m_is_maximized);
  }
}

auto OutputPanePlaceHolder::setMaximized(bool maximize) -> void
{
  if (d->m_is_maximized == maximize)
    return;

  if (!d->m_splitter)
    return;

  const auto idx = d->m_splitter->indexOf(this);

  if (idx < 0)
    return;

  d->m_is_maximized = maximize;

  if (OutputPanePlaceHolderPrivate::m_current == this)
    OutputPaneManager::updateMaximizeButton(d->m_is_maximized);

  auto sizes = d->m_splitter->sizes();

  if (maximize) {
    d->m_non_maximized_size = sizes[idx];
    auto sum = 0;
    foreach(int s, sizes)
      sum += s;
    for (auto i = 0; i < sizes.count(); ++i) {
      sizes[i] = 32;
    }
    sizes[idx] = static_cast<int>(sum - (sizes.count() - 1) * 32);
  } else {
    const auto target = d->m_non_maximized_size > 0 ? d->m_non_maximized_size : sizeHint().height();
    if (const auto space = sizes[idx] - target; space > 0) {
      for (auto i = 0; i < sizes.count(); ++i) {
        sizes[i] += static_cast<int>(space / (sizes.count() - 1));
      }
      sizes[idx] = target;
    }
  }

  d->m_splitter->setSizes(sizes);
}

auto OutputPanePlaceHolder::isMaximized() const -> bool
{
  return d->m_is_maximized;
}

auto OutputPanePlaceHolder::setHeight(const int height) -> void
{
  if (height == 0)
    return;

  if (!d->m_splitter)
    return;

  const auto idx = d->m_splitter->indexOf(this);

  if (idx < 0)
    return;

  d->m_splitter->refresh();
  auto sizes = d->m_splitter->sizes();
  const auto difference = height - sizes.at(idx);

  if (difference == 0)
    return;

  const auto adaption = static_cast<int>(difference / (sizes.count() - 1));

  for (auto i = 0; i < sizes.count(); ++i) {
    sizes[i] -= adaption;
  }

  sizes[idx] = height;
  d->m_splitter->setSizes(sizes);
}

auto OutputPanePlaceHolder::ensureSizeHintAsMinimum() -> void
{
  if (!d->m_splitter)
    return;

  const auto om = OutputPaneManager::instance();

  if (const auto minimum = d->m_splitter->orientation() == Qt::Vertical ? om->sizeHint().height() : om->sizeHint().width(); nonMaximizedSize() < minimum && !d->m_is_maximized)
    setHeight(minimum);
}

auto OutputPanePlaceHolder::nonMaximizedSize() const -> int
{
  if (!d->m_initialized)
    return OutputPaneManager::outputPaneHeightSetting();

  return d->m_non_maximized_size;
}

auto OutputPanePlaceHolder::resizeEvent(QResizeEvent *event) -> void
{
  if (d->m_is_maximized || event->size().height() == 0)
    return;

  d->m_non_maximized_size = event->size().height();
}

auto OutputPanePlaceHolder::showEvent(QShowEvent *) -> void
{
  if (!d->m_initialized) {
    d->m_initialized = true;
    setHeight(OutputPaneManager::outputPaneHeightSetting());
  }
}

auto OutputPanePlaceHolder::getCurrent() -> OutputPanePlaceHolder*
{
  return OutputPanePlaceHolderPrivate::m_current;
}

auto OutputPanePlaceHolder::isCurrentVisible() -> bool
{
  return OutputPanePlaceHolderPrivate::m_current && OutputPanePlaceHolderPrivate::m_current->isVisible();
}

} // namespace Orca::Plugin::Core


