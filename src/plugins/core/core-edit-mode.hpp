// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-mode-interface.hpp"

QT_BEGIN_NAMESPACE
class QSplitter;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class EditorManager;

class EditMode final : public IMode {
  Q_OBJECT

public:
  EditMode();
  ~EditMode() override;

private:
  auto grabEditorManager(Utils::Id mode) const -> void;

  QSplitter *m_splitter;
  QVBoxLayout *m_right_split_widget_layout;
};

} // namespace Orca::Plugin::Core
