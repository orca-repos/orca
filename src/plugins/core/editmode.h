// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/imode.h>

QT_BEGIN_NAMESPACE
class QSplitter;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Core {

class EditorManager;

namespace Internal {

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

} // namespace Internal
} // namespace Core
