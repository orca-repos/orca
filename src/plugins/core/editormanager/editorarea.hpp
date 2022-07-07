// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "editorview.hpp"

#include <QPointer>

namespace Core {

class IContext;

namespace Internal {

class EditorArea final : public SplitterOrView {
  Q_OBJECT

public:
  EditorArea();
  ~EditorArea() override;

  auto currentDocument() const -> IDocument*;

signals:
  auto windowTitleNeedsUpdate() -> void;

private:
  auto focusChanged(const QWidget *old, const QWidget *now) -> void;
  auto setCurrentView(EditorView *view) -> void;
  auto updateCurrentEditor(const IEditor *editor) -> void;
  auto updateCloseSplitButton() const -> void;

  IContext *m_context;
  QPointer<EditorView> m_current_view;
  QPointer<IDocument> m_current_document;
};

} // Internal
} // Core
