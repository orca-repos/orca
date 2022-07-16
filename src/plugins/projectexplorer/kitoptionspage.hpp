// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <core/core-options-page-interface.hpp>

#include <QPointer>

namespace ProjectExplorer {

namespace Internal {
class KitOptionsPageWidget;
}

class Kit;

class PROJECTEXPLORER_EXPORT KitOptionsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  KitOptionsPage();

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;
  auto showKit(Kit *k) -> void;
  static auto instance() -> KitOptionsPage*;

private:
  QPointer<Internal::KitOptionsPageWidget> m_widget;
};

} // namespace ProjectExplorer
