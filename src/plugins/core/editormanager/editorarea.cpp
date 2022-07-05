// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "editorarea.h"
#include "editormanager.h"
#include "ieditor.h"

#include <core/coreconstants.h>
#include <core/icontext.h>
#include <core/idocument.h>
#include <core/icore.h>

#include <QApplication>

namespace Core {
namespace Internal {

EditorArea::EditorArea()
{
  m_context = new IContext;
  m_context->setContext(Context(Constants::C_EDITORMANAGER));
  m_context->setWidget(this);

  ICore::addContextObject(m_context);

  setCurrentView(view());
  updateCloseSplitButton();

  connect(qApp, &QApplication::focusChanged, this, &EditorArea::focusChanged);
  connect(this, &SplitterOrView::splitStateChanged, this, &EditorArea::updateCloseSplitButton);
}

EditorArea::~EditorArea()
{
  // disconnect
  setCurrentView(nullptr);
  disconnect(qApp, &QApplication::focusChanged, this, &EditorArea::focusChanged);

  delete m_context;
}

auto EditorArea::currentDocument() const -> IDocument*
{
  return m_current_document;
}

auto EditorArea::focusChanged(const QWidget *old, const QWidget *now) -> void
{
  Q_UNUSED(old)

  // only interesting if the focus moved within the editor area
  if (!focusWidget() || focusWidget() != now)
    return;

  // find the view with focus
  auto current = findFirstView();

  while (current) {
    if (current->focusWidget() && current->focusWidget() == now) {
      setCurrentView(current);
      break;
    }
    current = current->findNextView();
  }
}

auto EditorArea::setCurrentView(EditorView *view) -> void
{
  if (view == m_current_view)
    return;

  if (m_current_view) {
    disconnect(m_current_view.data(), &EditorView::currentEditorChanged, this, &EditorArea::updateCurrentEditor);
  }

  m_current_view = view;

  if (m_current_view) {
    connect(m_current_view.data(), &EditorView::currentEditorChanged, this, &EditorArea::updateCurrentEditor);
  }

  updateCurrentEditor(m_current_view ? m_current_view->currentEditor() : nullptr);
}

auto EditorArea::updateCurrentEditor(const IEditor *editor) -> void
{
  const auto document = editor ? editor->document() : nullptr;

  if (document == m_current_document)
    return;

  if (m_current_document) {
    disconnect(m_current_document.data(), &IDocument::changed, this, &EditorArea::windowTitleNeedsUpdate);
  }

  m_current_document = document;

  if (m_current_document) {
    connect(m_current_document.data(), &IDocument::changed, this, &EditorArea::windowTitleNeedsUpdate);
  }

  emit windowTitleNeedsUpdate();
}

auto EditorArea::updateCloseSplitButton() const -> void
{
  if (const auto v = view())
    v->setCloseSplitEnabled(false);
}

} // Internal
} // Core
