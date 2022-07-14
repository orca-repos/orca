// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-output-pane-interface.hpp"

namespace Orca::Plugin::Core {

class OutputWindow;

class MessageOutputWindow final : public IOutputPane {
  Q_OBJECT

public:
  MessageOutputWindow();
  ~MessageOutputWindow() override;

  auto outputWidget(QWidget *parent) -> QWidget* override;
  auto displayName() const -> QString override;
  auto priorityInStatusBar() const -> int override;
  auto clearContents() -> void override;
  auto append(const QString &text) const -> void;
  auto canFocus() const -> bool override;
  auto hasFocus() const -> bool override;
  auto setFocus() -> void override;
  auto canNext() const -> bool override;
  auto canPrevious() const -> bool override;
  auto goToNext() -> void override;
  auto goToPrev() -> void override;
  auto canNavigate() const -> bool override;

private:
  auto updateFilter() -> void override;

  OutputWindow *m_widget;
};

} // namespace Orca::Plugin::Core
