// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

namespace Core {
namespace Internal {

class EditorArea;

class EditorWindow final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(EditorWindow)

public:
  explicit EditorWindow(QWidget *parent = nullptr);
  ~EditorWindow() override;

  auto editorArea() const -> EditorArea*;
  auto saveState() const -> QVariantHash;
  auto restoreState(const QVariantHash &state) -> void;

private:
  auto updateWindowTitle() const -> void;

  EditorArea *m_area;
};

} // Internal
} // Core
