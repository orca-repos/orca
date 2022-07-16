// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once
#include <core/core-options-page-interface.hpp>
#include <QPointer>

namespace CppEditor {
namespace Internal {

class CppQuickFixSettingsWidget;

class CppQuickFixSettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  CppQuickFixSettingsPage();

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;

private:
  QPointer<CppQuickFixSettingsWidget> m_widget;
};

} // namespace Internal
} // namespace CppEditor
