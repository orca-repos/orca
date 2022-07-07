// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "editorview.hpp"

#include "editormanager.hpp"
#include "editormanager_p.hpp"
#include "documentmodel.hpp"
#include "documentmodel_p.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/editortoolbar.hpp>
#include <core/findplaceholder.hpp>
#include <core/icore.hpp>
#include <core/locator/locatorconstants.hpp>
#include <core/minisplitter.hpp>
#include <utils/algorithm.hpp>
#include <utils/infobar.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>
#include <utils/link.hpp>
#include <utils/utilsicons.hpp>

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QToolButton>
#include <QSplitter>
#include <QStackedLayout>

using namespace Utils;

namespace Core {
namespace Internal {

EditorView::EditorView(SplitterOrView *parent_splitter_or_view, QWidget *parent) : QWidget(parent), m_parent_splitter_or_view(parent_splitter_or_view), m_tool_bar(new EditorToolBar(this)), m_container(new QStackedWidget(this)), m_info_bar_display(new InfoBarDisplay(this)), m_status_h_line(new QFrame(this)), m_status_widget(new QFrame(this))
{
  const auto tl = new QVBoxLayout(this);

  tl->setSpacing(0);
  tl->setContentsMargins(0, 0, 0, 0);

  connect(m_tool_bar, &EditorToolBar::goBackClicked, this, &EditorView::goBackInNavigationHistory);
  connect(m_tool_bar, &EditorToolBar::goForwardClicked, this, &EditorView::goForwardInNavigationHistory);
  connect(m_tool_bar, &EditorToolBar::closeClicked, this, &EditorView::closeCurrentEditor);
  connect(m_tool_bar, &EditorToolBar::listSelectionActivated, this, &EditorView::listSelectionActivated);
  connect(m_tool_bar, &EditorToolBar::currentDocumentMoved, this, &EditorView::closeCurrentEditor);
  connect(m_tool_bar, &EditorToolBar::horizontalSplitClicked, this, &EditorView::splitHorizontally);
  connect(m_tool_bar, &EditorToolBar::verticalSplitClicked, this, &EditorView::splitVertically);
  connect(m_tool_bar, &EditorToolBar::splitNewWindowClicked, this, &EditorView::splitNewWindow);
  connect(m_tool_bar, &EditorToolBar::closeSplitClicked, this, &EditorView::closeSplit);

  m_tool_bar->setMenuProvider([this](QMenu *menu) { fillListContextMenu(menu); });
  tl->addWidget(m_tool_bar);
  m_info_bar_display->setTarget(tl, 1);
  tl->addWidget(m_container);

  tl->addWidget(new FindToolBarPlaceHolder(this));
  m_status_h_line->setFrameStyle(QFrame::HLine);
  m_status_widget->setFrameStyle(QFrame::NoFrame);
  m_status_widget->setLineWidth(0);
  m_status_widget->setAutoFillBackground(true);

  const auto hbox = new QHBoxLayout(m_status_widget);
  hbox->setContentsMargins(1, 0, 1, 1);
  m_status_widget_label = new QLabel;
  m_status_widget_label->setContentsMargins(3, 0, 3, 0);
  hbox->addWidget(m_status_widget_label);
  hbox->addStretch(1);

  m_status_widget_button = new QToolButton;
  m_status_widget_button->setContentsMargins(0, 0, 0, 0);
  hbox->addWidget(m_status_widget_button);

  m_status_h_line->setVisible(false);
  m_status_widget->setVisible(false);
  tl->addWidget(m_status_h_line);
  tl->addWidget(m_status_widget);

  // for the case of no document selected
  const auto empty = new QWidget;
  empty->hide();
  const auto empty_layout = new QGridLayout(empty);
  empty->setLayout(empty_layout);

  m_empty_view_label = new QLabel;
  connect(EditorManagerPrivate::instance(), &EditorManagerPrivate::placeholderTextChanged, m_empty_view_label, &QLabel::setText);
  m_empty_view_label->setText(EditorManagerPrivate::placeholderText());
  empty_layout->addWidget(m_empty_view_label);
  m_container->addWidget(empty);
  m_widget_editor_map.insert(empty, nullptr);

  const auto drop_support = new DropSupport(this, [this](QDropEvent *event, DropSupport *) {
    // do not accept move events except from other editor views (i.e. their tool bars)
    // otherwise e.g. item views that support moving items within themselves would
    // also "move" the item into the editor view, i.e. the item would be removed from the
    // item view
    if (!qobject_cast<EditorToolBar*>(event->source()))
      event->setDropAction(Qt::CopyAction);
    if (event->type() == QDropEvent::DragEnter && !DropSupport::isFileDrop(event))
      return false;                      // do not accept drops without files
    return event->source() != m_tool_bar; // do not accept drops on ourselves
  });

  connect(drop_support, &DropSupport::filesDropped, this, &EditorView::openDroppedFiles);
  updateNavigatorActions();
}

EditorView::~EditorView() = default;

auto EditorView::parentSplitterOrView() const -> SplitterOrView*
{
  return m_parent_splitter_or_view;
}

auto EditorView::findNextView() const -> EditorView*
{
  auto current = parentSplitterOrView();
  QTC_ASSERT(current, return nullptr);
  auto parent = current->findParentSplitter();

  while (parent) {
    const auto splitter = parent->splitter();
    QTC_ASSERT(splitter, return nullptr);
    QTC_ASSERT(splitter->count() == 2, return nullptr);
    // is current the first child? then the next view is the first one in current's sibling
    if (splitter->widget(0) == current) {
      const auto second = qobject_cast<SplitterOrView*>(splitter->widget(1));
      QTC_ASSERT(second, return nullptr);
      return second->findFirstView();
    }
    // otherwise go up the hierarchy
    current = parent;
    parent = current->findParentSplitter();
  }
  // current has no parent, so we are at the top and there is no "next" view
  return nullptr;
}

auto EditorView::findPreviousView() const -> EditorView*
{
  auto current = parentSplitterOrView();
  QTC_ASSERT(current, return nullptr);
  auto parent = current->findParentSplitter();
  while (parent) {
    const auto splitter = parent->splitter();
    QTC_ASSERT(splitter, return nullptr);
    QTC_ASSERT(splitter->count() == 2, return nullptr);
    // is current the last child? then the previous view is the first child in current's sibling
    if (splitter->widget(1) == current) {
      const auto first = qobject_cast<SplitterOrView*>(splitter->widget(0));
      QTC_ASSERT(first, return nullptr);
      return first->findFirstView();
    }
    // otherwise go up the hierarchy
    current = parent;
    parent = current->findParentSplitter();
  }
  // current has no parent, so we are at the top and there is no "previous" view
  return nullptr;
}

auto EditorView::closeCurrentEditor() const -> void
{
  if (const auto editor = currentEditor())
    EditorManagerPrivate::closeEditorOrDocument(editor);
}

auto EditorView::showEditorStatusBar(const QString &id, const QString &info_text, const QString &button_text, const QObject *object, const std::function<void()> &function) -> void
{
  m_status_widget_id = id;
  m_status_widget_label->setText(info_text);
  m_status_widget_button->setText(button_text);
  m_status_widget_button->setToolTip(button_text);
  m_status_widget_button->disconnect();

  if (object && function)
    connect(m_status_widget_button, &QToolButton::clicked, object, function);

  m_status_widget->setVisible(true);
  m_status_h_line->setVisible(true);
}

auto EditorView::hideEditorStatusBar(const QString &id) const -> void
{
  if (id == m_status_widget_id) {
    m_status_widget->setVisible(false);
    m_status_h_line->setVisible(false);
  }
}

auto EditorView::setCloseSplitEnabled(const bool enable) const -> void
{
  m_tool_bar->setCloseSplitEnabled(enable);
}

auto EditorView::setCloseSplitIcon(const QIcon &icon) const -> void
{
  m_tool_bar->setCloseSplitIcon(icon);
}

auto EditorView::updateEditorHistory(const IEditor *editor, QList<EditLocation> &history) -> void
{
  if (!editor)
    return;

  const auto document = editor->document();

  if (!document)
    return;

  const auto state = editor->saveState();

  EditLocation location;
  location.document = document;
  location.file_path = document->filePath();
  location.id = document->id();
  location.state = QVariant(state);

  for (auto i = 0; i < history.size(); ++i) {
    const auto &item = history.at(i);
    if (item.document == document || (!item.document && !DocumentModel::indexOfFilePath(item.file_path))) {
      history.removeAt(i--);
    }
  }

  history.prepend(location);
}

auto EditorView::paintEvent(QPaintEvent *) -> void
{
  if (const auto editor_view = EditorManagerPrivate::currentEditorView(); editor_view != this)
    return;

  if (m_container->currentIndex() != 0) // so a document is selected
    return;

  // Discreet indication where an editor would be if there is none
  QPainter painter(this);

  const auto &rect = m_container->geometry();
  if (orcaTheme()->flag(Theme::FlatToolBars)) {
    painter.fillRect(rect, orcaTheme()->color(Theme::EditorPlaceholderColor));
  } else {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(orcaTheme()->color(Theme::EditorPlaceholderColor));
    const auto r = 3;
    painter.drawRoundedRect(rect.adjusted(r, r, -r, -r), r * 2, r * 2);
  }
}

auto EditorView::mousePressEvent(QMouseEvent *e) -> void
{
  if (e->button() != Qt::LeftButton)
    return;

  setFocus(Qt::MouseFocusReason);
}

auto EditorView::focusInEvent(QFocusEvent *) -> void
{
  EditorManagerPrivate::setCurrentView(this);
}

auto EditorView::addEditor(IEditor *editor) -> void
{
  if (m_editors.contains(editor))
    return;

  m_editors.append(editor);
  m_container->addWidget(editor->widget());
  m_widget_editor_map.insert(editor->widget(), editor);
  m_tool_bar->addEditor(editor);

  if (editor == currentEditor())
    setCurrentEditor(editor);
}

auto EditorView::hasEditor(IEditor *editor) const -> bool
{
  return m_editors.contains(editor);
}

auto EditorView::removeEditor(IEditor *editor) -> void
{
  QTC_ASSERT(editor, return);

  if (!m_editors.contains(editor))
    return;

  const auto index = m_container->indexOf(editor->widget());
  QTC_ASSERT(index != -1, return);
  const auto was_current = (index == m_container->currentIndex());
  m_editors.removeAll(editor);

  m_container->removeWidget(editor->widget());
  m_widget_editor_map.remove(editor->widget());
  editor->widget()->setParent(nullptr);
  m_tool_bar->removeToolbarForEditor(editor);

  if (was_current)
    setCurrentEditor(!m_editors.isEmpty() ? m_editors.last() : nullptr);
}

auto EditorView::currentEditor() const -> IEditor*
{
  if (!m_editors.isEmpty())
    return m_widget_editor_map.value(m_container->currentWidget());
  return nullptr;
}

auto EditorView::listSelectionActivated(const int index) -> void
{
  EditorManagerPrivate::activateEditorForEntry(this, DocumentModel::entryAtRow(index));
}

auto EditorView::fillListContextMenu(QMenu *menu) const -> void
{
  const auto editor = currentEditor();
  const auto entry = editor ? DocumentModel::entryForDocument(editor->document()) : nullptr;

  EditorManager::addSaveAndCloseEditorActions(menu, entry, editor);
  menu->addSeparator();
  EditorManager::addNativeDirAndOpenWithActions(menu, entry);
}

auto EditorView::splitHorizontally() const -> void
{
  if (m_parent_splitter_or_view)
    m_parent_splitter_or_view->split(Qt::Vertical);
  EditorManagerPrivate::updateActions();
}

auto EditorView::splitVertically() const -> void
{
  if (m_parent_splitter_or_view)
    m_parent_splitter_or_view->split(Qt::Horizontal);
  EditorManagerPrivate::updateActions();
}

auto EditorView::splitNewWindow() const -> void
{
  EditorManagerPrivate::splitNewWindow(this);
}

auto EditorView::closeSplit() -> void
{
  EditorManagerPrivate::closeView(this);
  EditorManagerPrivate::updateActions();
}

auto EditorView::openDroppedFiles(const QList<DropSupport::FileSpec> &files) -> void
{
  auto first = true;
  auto spec_to_link = [](const DropSupport::FileSpec &spec) {
    return Link(spec.filePath, spec.line, spec.column);
  };

  auto open_entry = [&](const DropSupport::FileSpec &spec) {
    if (first) {
      first = false;
      EditorManagerPrivate::openEditorAt(this, spec_to_link(spec));
    } else if (spec.column != -1 || spec.line != -1) {
      EditorManagerPrivate::openEditorAt(this, spec_to_link(spec), Id(), EditorManager::do_not_change_current_editor | EditorManager::do_not_make_visible);
    } else {
      const auto factory = IEditorFactory::preferredEditorFactories(spec.filePath).value(0);
      DocumentModelPrivate::addSuspendedDocument(spec.filePath, {}, factory ? factory->id() : Id());
    }
  };

  reverseForeach(files, open_entry);
}

auto EditorView::setParentSplitterOrView(SplitterOrView *splitter_or_view) -> void
{
  m_parent_splitter_or_view = splitter_or_view;
}

auto EditorView::setCurrentEditor(IEditor *editor) -> void
{
  if (!editor || m_container->indexOf(editor->widget()) == -1) {
    QTC_CHECK(!editor);
    m_tool_bar->setCurrentEditor(nullptr);
    m_info_bar_display->setInfoBar(nullptr);
    m_container->setCurrentIndex(0);
    emit currentEditorChanged(nullptr);
    return;
  }

  m_editors.removeAll(editor);
  m_editors.append(editor);

  const auto idx = m_container->indexOf(editor->widget());

  QTC_ASSERT(idx >= 0, return);
  m_container->setCurrentIndex(idx);
  m_tool_bar->setCurrentEditor(editor);

  updateEditorHistory(editor);

  m_info_bar_display->setInfoBar(editor->document()->infoBar());
  emit currentEditorChanged(editor);
}

auto EditorView::editorCount() const -> int
{
  return static_cast<int>(m_editors.size());
}

auto EditorView::editors() const -> QList<IEditor*>
{
  return m_editors;
}

auto EditorView::editorForDocument(const IDocument *document) const -> IEditor*
{
  for(const auto editor:  m_editors) if (editor->document() == document)
    return editor;

  return nullptr;
}

auto EditorView::updateEditorHistory(const IEditor *editor) -> void
{
  updateEditorHistory(editor, m_editor_history);
}

auto EditorView::addCurrentPositionToNavigationHistory(const QByteArray &save_state) -> void
{
  const auto editor = currentEditor();

  if (!editor)
    return;

  const auto document = editor->document();

  if (!document)
    return;

  QByteArray state;

  if (save_state.isNull())
    state = editor->saveState();
  else
    state = save_state;

  EditLocation location;
  location.document = document;
  location.file_path = document->filePath();
  location.id = document->id();
  location.state = QVariant(state);

  m_current_navigation_history_position = static_cast<int>(qMin(m_current_navigation_history_position, m_navigation_history.size())); // paranoia
  m_navigation_history.insert(m_current_navigation_history_position, location);

  ++m_current_navigation_history_position;
  while (m_navigation_history.size() >= 30) {
    if (m_current_navigation_history_position > 15) {
      m_navigation_history.removeFirst();
      --m_current_navigation_history_position;
    } else {
      m_navigation_history.removeLast();
    }
  }

  updateNavigatorActions();
}

auto EditorView::cutForwardNavigationHistory() -> void
{
  while (m_current_navigation_history_position < m_navigation_history.size() - 1)
    m_navigation_history.removeLast();
}

auto EditorView::updateNavigatorActions() const -> void
{
  m_tool_bar->setCanGoBack(canGoBack());
  m_tool_bar->setCanGoForward(canGoForward());
}

auto EditorView::copyNavigationHistoryFrom(const EditorView *other) -> void
{
  if (!other)
    return;

  m_current_navigation_history_position = other->m_current_navigation_history_position;
  m_navigation_history = other->m_navigation_history;
  m_editor_history = other->m_editor_history;

  updateNavigatorActions();
}

auto EditorView::updateCurrentPositionInNavigationHistory() -> void
{
  const auto editor = currentEditor();

  if (!editor || !editor->document())
    return;

  const auto document = editor->document();

  EditLocation *location;
  if (m_current_navigation_history_position < m_navigation_history.size()) {
    location = &m_navigation_history[m_current_navigation_history_position];
  } else {
    m_navigation_history.append(EditLocation());
    location = &m_navigation_history[m_navigation_history.size() - 1];
  }

  location->document = document;
  location->file_path = document->filePath();
  location->id = document->id();
  location->state = QVariant(editor->saveState());
}

static auto fileNameWasRemoved(const FilePath &file_path) -> bool
{
  return !file_path.isEmpty() && !file_path.exists();
}

auto EditorView::goBackInNavigationHistory() -> void
{
  updateCurrentPositionInNavigationHistory();

  while (m_current_navigation_history_position > 0) {
    --m_current_navigation_history_position;
    auto location = m_navigation_history.at(m_current_navigation_history_position);
    IEditor *editor = nullptr;
    if (location.document) {
      editor = EditorManagerPrivate::activateEditorForDocument(this, location.document, EditorManager::ignore_navigation_history);
    }
    if (!editor) {
      if (fileNameWasRemoved(location.file_path)) {
        m_navigation_history.removeAt(m_current_navigation_history_position);
        continue;
      }
      editor = EditorManagerPrivate::openEditor(this, location.file_path, location.id, EditorManager::ignore_navigation_history);
      if (!editor) {
        m_navigation_history.removeAt(m_current_navigation_history_position);
        continue;
      }
    }
    editor->restoreState(location.state.toByteArray());
    break;
  }

  updateNavigatorActions();
}

auto EditorView::goForwardInNavigationHistory() -> void
{
  updateCurrentPositionInNavigationHistory();

  if (m_current_navigation_history_position >= m_navigation_history.size() - 1)
    return;

  ++m_current_navigation_history_position;

  while (m_current_navigation_history_position < m_navigation_history.size()) {
    IEditor *editor = nullptr;
    auto location = m_navigation_history.at(m_current_navigation_history_position);
    if (location.document) {
      editor = EditorManagerPrivate::activateEditorForDocument(this, location.document, EditorManager::ignore_navigation_history);
    }
    if (!editor) {
      if (fileNameWasRemoved(location.file_path)) {
        m_navigation_history.removeAt(m_current_navigation_history_position);
        continue;
      }
      editor = EditorManagerPrivate::openEditor(this, location.file_path, location.id, EditorManager::ignore_navigation_history);
      if (!editor) {
        m_navigation_history.removeAt(m_current_navigation_history_position);
        continue;
      }
    }
    editor->restoreState(location.state.toByteArray());
    break;
  }

  if (m_current_navigation_history_position >= m_navigation_history.size())
    m_current_navigation_history_position = qMax<int>(static_cast<int>(m_navigation_history.size() - 1), 0);

  updateNavigatorActions();
}

auto EditorView::goToEditLocation(const EditLocation &location) -> void
{
  IEditor *editor = nullptr;

  if (location.document) {
    editor = EditorManagerPrivate::activateEditorForDocument(this, location.document, EditorManager::ignore_navigation_history);
  }

  if (!editor) {
    if (fileNameWasRemoved(location.file_path))
      return;

    editor = EditorManagerPrivate::openEditor(this, location.file_path, location.id, EditorManager::ignore_navigation_history);
  }

  if (editor) {
    editor->restoreState(location.state.toByteArray());
  }
}

SplitterOrView::SplitterOrView(IEditor *editor)
{
  m_layout = new QStackedLayout(this);
  m_layout->setSizeConstraint(QLayout::SetNoConstraint);
  m_view = new EditorView(this);

  if (editor)
    m_view->addEditor(editor);

  m_splitter = nullptr;
  m_layout->addWidget(m_view);
}

SplitterOrView::SplitterOrView(EditorView *view)
{
  Q_ASSERT(view);
  m_layout = new QStackedLayout(this);
  m_layout->setSizeConstraint(QLayout::SetNoConstraint);
  m_view = view;
  m_view->setParentSplitterOrView(this);
  m_splitter = nullptr;
  m_layout->addWidget(m_view);
}

SplitterOrView::~SplitterOrView()
{
  delete m_layout;
  m_layout = nullptr;

  if (m_view)
    EditorManagerPrivate::deleteEditors(EditorManagerPrivate::emptyView(m_view));

  delete m_view;
  m_view = nullptr;
  delete m_splitter;
  m_splitter = nullptr;
}

auto SplitterOrView::findFirstView() const -> EditorView*
{
  if (m_splitter) {
    for (auto i = 0; i < m_splitter->count(); ++i) {
      if (const auto splitter_or_view = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
        if (const auto result = splitter_or_view->findFirstView())
          return result;
    }
    return nullptr;
  }
  return m_view;
}

auto SplitterOrView::findLastView() const -> EditorView*
{
  if (m_splitter) {
    for (auto i = m_splitter->count() - 1; 0 < i; --i) {
      if (const auto splitter_or_view = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
        if (const auto result = splitter_or_view->findLastView())
          return result;
    }
    return nullptr;
  }
  return m_view;
}

auto SplitterOrView::findParentSplitter() const -> SplitterOrView*
{
  auto w = parentWidget();
  while (w) {
    if (const auto splitter = qobject_cast<SplitterOrView*>(w)) {
      QTC_CHECK(splitter->splitter());
      return splitter;
    }
    w = w->parentWidget();
  }
  return nullptr;
}

auto SplitterOrView::minimumSizeHint() const -> QSize
{
  if (m_splitter)
    return m_splitter->minimumSizeHint();
  return {64, 64};
}

auto SplitterOrView::takeSplitter() -> QSplitter*
{
  const auto old_splitter = m_splitter;

  if (m_splitter)
    m_layout->removeWidget(m_splitter);

  m_splitter = nullptr;
  return old_splitter;
}

auto SplitterOrView::takeView() -> EditorView*
{
  const auto old_view = m_view;

  if (m_view) {
    // the focus update that is triggered by removing should already have 0 parent
    // so we do that first
    m_view->setParentSplitterOrView(nullptr);
    m_layout->removeWidget(m_view);
  }

  m_view = nullptr;
  return old_view;
}

auto SplitterOrView::split(const Qt::Orientation orientation, const bool activate_view) -> void
{
  Q_ASSERT(m_view && m_splitter == nullptr);

  m_splitter = new MiniSplitter(this);
  m_splitter->setOrientation(orientation);
  m_layout->addWidget(m_splitter);
  m_layout->removeWidget(m_view);

  const auto editor_view = m_view;
  editor_view->setCloseSplitEnabled(true); // might have been disabled for root view
  m_view = nullptr;
  const auto e = editor_view->currentEditor();
  const auto state = e ? e->saveState() : QByteArray();

  SplitterOrView *view = nullptr;
  SplitterOrView *other_view = nullptr;

  const auto duplicate = e && e->duplicateSupported() ? EditorManagerPrivate::duplicateEditor(e) : nullptr;

  m_splitter->addWidget((view = new SplitterOrView(duplicate)));
  m_splitter->addWidget((other_view = new SplitterOrView(editor_view)));
  m_layout->setCurrentWidget(m_splitter);

  view->view()->copyNavigationHistoryFrom(editor_view);
  view->view()->setCurrentEditor(duplicate);

  if (orientation == Qt::Horizontal) {
    view->view()->setCloseSplitIcon(Icons::CLOSE_SPLIT_LEFT.icon());
    other_view->view()->setCloseSplitIcon(Icons::CLOSE_SPLIT_RIGHT.icon());
  } else {
    view->view()->setCloseSplitIcon(Icons::CLOSE_SPLIT_TOP.icon());
    other_view->view()->setCloseSplitIcon(Icons::CLOSE_SPLIT_BOTTOM.icon());
  }

  // restore old state, possibly adapted to the new layout (the editors can e.g. make sure that
  // a previously visible text cursor stays visible)
  if (duplicate)
    duplicate->restoreState(state);

  if (e)
    e->restoreState(state);

  if (activate_view)
    EditorManagerPrivate::activateView(other_view->view());

  emit splitStateChanged();
}

auto SplitterOrView::unsplitAll() -> void
{
  QTC_ASSERT(m_splitter, return);
  // avoid focus changes while unsplitting is in progress
  auto had_focus = false;

  if (const auto w = focusWidget()) {
    if (w->hasFocus()) {
      w->clearFocus();
      had_focus = true;
    }
  }

  auto current_view = EditorManagerPrivate::currentEditorView();
  if (current_view) {
    current_view->parentSplitterOrView()->takeView();
    current_view->setParentSplitterOrView(this);
  } else {
    current_view = new EditorView(this);
  }

  m_splitter->hide();
  m_layout->removeWidget(m_splitter); // workaround Qt bug
  const auto editors_to_delete = unsplitAllHelper();
  m_view = current_view;
  m_layout->addWidget(m_view);
  delete m_splitter;
  m_splitter = nullptr;

  // restore some focus
  if (had_focus) {
    if (const auto editor = m_view->currentEditor())
      editor->widget()->setFocus();
    else
      m_view->setFocus();
  }
  EditorManagerPrivate::deleteEditors(editors_to_delete);
  emit splitStateChanged();
}

/*!
    Recursively empties all views.
    Returns the editors to delete with EditorManagerPrivate::deleteEditors.
    \internal
*/
auto SplitterOrView::unsplitAllHelper() const -> QList<IEditor*>
{
  if (m_view)
    return EditorManagerPrivate::emptyView(m_view);
  QList<IEditor*> editors_to_delete;
  if (m_splitter) {
    for (auto i = 0; i < m_splitter->count(); ++i) {
      if (const auto splitter_or_view = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
        editors_to_delete.append(splitter_or_view->unsplitAllHelper());
    }
  }
  return editors_to_delete;
}

auto SplitterOrView::unsplit() -> void
{
  if (!m_splitter)
    return;

  Q_ASSERT(m_splitter->count() == 1);

  const auto child_splitter_or_view = qobject_cast<SplitterOrView*>(m_splitter->widget(0));
  const auto old_splitter = m_splitter;

  m_splitter = nullptr;
  QList<IEditor*> editors_to_delete;

  if (child_splitter_or_view->isSplitter()) {
    Q_ASSERT(child_splitter_or_view->view() == nullptr);
    m_splitter = child_splitter_or_view->takeSplitter();
    m_layout->addWidget(m_splitter);
    m_layout->setCurrentWidget(m_splitter);
  } else {
    const auto child_view = child_splitter_or_view->view();
    Q_ASSERT(child_view);
    if (m_view) {
      m_view->copyNavigationHistoryFrom(child_view);
      if (const auto e = child_view->currentEditor()) {
        child_view->removeEditor(e);
        m_view->addEditor(e);
        m_view->setCurrentEditor(e);
      }
      editors_to_delete = EditorManagerPrivate::emptyView(child_view);
    } else {
      m_view = child_splitter_or_view->takeView();
      m_view->setParentSplitterOrView(this);
      m_layout->addWidget(m_view);
      if (const auto parent_splitter = qobject_cast<QSplitter*>(parentWidget())) {
        // not the toplevel splitterOrView
        if (parent_splitter->orientation() == Qt::Horizontal)
          m_view->setCloseSplitIcon(parent_splitter->widget(0) == this ? Icons::CLOSE_SPLIT_LEFT.icon() : Icons::CLOSE_SPLIT_RIGHT.icon());
        else
          m_view->setCloseSplitIcon(parent_splitter->widget(0) == this ? Icons::CLOSE_SPLIT_TOP.icon() : Icons::CLOSE_SPLIT_BOTTOM.icon());
      }
    }
    m_layout->setCurrentWidget(m_view);
  }

  delete old_splitter;
  if (const auto new_current = findFirstView())
    EditorManagerPrivate::activateView(new_current);
  else
    EditorManagerPrivate::setCurrentView(nullptr);

  EditorManagerPrivate::deleteEditors(editors_to_delete);
  emit splitStateChanged();
}

auto SplitterOrView::saveState() const -> QByteArray
{
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);

  if (m_splitter) {
    stream << QByteArray("splitter") << static_cast<qint32>(m_splitter->orientation()) << m_splitter->saveState() << dynamic_cast<SplitterOrView*>(m_splitter->widget(0))->saveState() << dynamic_cast<SplitterOrView*>(m_splitter->widget(1))->saveState();
  } else {
    auto e = editor();
    // don't save state of temporary or ad-hoc editors
    if (e && (e->document()->isTemporary() || e->document()->filePath().isEmpty())) {
      // look for another editor that is more suited
      e = nullptr;
      for(const auto &other_editor: editors()) {
        if (!other_editor->document()->isTemporary() && !other_editor->document()->filePath().isEmpty()) {
          e = other_editor;
          break;
        }
      }
    }
    if (!e) {
      stream << QByteArray("empty");
    } else if (e == EditorManager::currentEditor()) {
      stream << QByteArray("currenteditor") << e->document()->filePath().toString() << e->document()->id().toString() << e->saveState();
    } else {
      stream << QByteArray("editor") << e->document()->filePath().toString() << e->document()->id().toString() << e->saveState();
    }
  }

  return bytes;
}

auto SplitterOrView::restoreState(const QByteArray &state) -> void
{
  QDataStream stream(state);
  QByteArray mode;
  stream >> mode;
  if (mode == "splitter") {
    qint32 orientation;
    QByteArray splitter, first, second;
    stream >> orientation >> splitter >> first >> second;
    split(static_cast<Qt::Orientation>(orientation), false);
    m_splitter->restoreState(splitter);
    dynamic_cast<SplitterOrView*>(m_splitter->widget(0))->restoreState(first);
    dynamic_cast<SplitterOrView*>(m_splitter->widget(1))->restoreState(second);
  } else if (mode == "editor" || mode == "currenteditor") {
    QString file_name;
    QString id;
    QByteArray editor_state;
    stream >> file_name >> id >> editor_state;
    if (!QFile::exists(file_name))
      return;
    const auto e = EditorManagerPrivate::openEditor(view(), FilePath::fromString(file_name), Id::fromString(id), EditorManager::ignore_navigation_history | EditorManager::do_not_change_current_editor);
    if (!e) {
      if (const auto entry = DocumentModelPrivate::firstSuspendedEntry()) {
        EditorManagerPrivate::activateEditorForEntry(view(), entry, EditorManager::ignore_navigation_history | EditorManager::do_not_change_current_editor);
      }
    }
    if (e) {
      e->restoreState(editor_state);
      if (mode == "currenteditor")
        EditorManagerPrivate::setCurrentEditor(e);
    }
  }
}

} // namespace Internal
} // namespace Core
