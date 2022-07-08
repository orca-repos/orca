// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "itaskhandler.hpp"

namespace Core {
class IOutputPane;
}

namespace ProjectExplorer {
namespace Internal {

class ShowOutputTaskHandler : public ITaskHandler {
  Q_OBJECT

public:
  explicit ShowOutputTaskHandler(Core::IOutputPane *window, const QString &text, const QString &tooltip, const QString &shortcut);

  auto canHandle(const Task &) const -> bool override;
  auto handle(const Task &task) -> void override;
  auto createAction(QObject *parent) const -> QAction* override;

private:
  Core::IOutputPane *const m_window;
  const QString m_text;
  const QString m_tooltip;
  const QString m_shortcut;
};

} // namespace Internal
} // namespace ProjectExplorer
