// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-editor-tool-bar.hpp"

#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-document-model.hpp"
#include "core-editor-interface.hpp"
#include "core-editor-manager-private.hpp"
#include "core-editor-manager.hpp"
#include "core-file-icon-provider.hpp"

#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

enum {
    debug = false
};

namespace Orca::Plugin::Core {

struct EditorToolBarPrivate {
  explicit EditorToolBarPrivate(QWidget *parent, EditorToolBar *q);

  QComboBox *m_editor_list;
  QToolButton *m_close_editor_button;
  QToolButton *m_lock_button;
  QToolButton *m_drag_handle;
  QMenu *m_drag_handle_menu;
  EditorToolBar::menu_provider m_menu_provider;
  QAction *m_go_back_action;
  QAction *m_go_forward_action;
  QToolButton *m_back_button;
  QToolButton *m_forward_button;
  QToolButton *m_split_button;
  QAction *m_horizontal_split_action;
  QAction *m_vertical_split_action;
  QAction *m_split_new_window_action;
  QToolButton *m_close_split_button;
  QWidget *m_active_tool_bar;
  QWidget *m_tool_bar_placeholder;
  QWidget *m_default_tool_bar;
  QPoint m_drag_start_position;
  bool m_is_standalone;
};

EditorToolBarPrivate::EditorToolBarPrivate(QWidget *parent, EditorToolBar *q) : m_editor_list(new QComboBox(q)), m_close_editor_button(new QToolButton(q)), m_lock_button(new QToolButton(q)), m_drag_handle(new QToolButton(q)), m_drag_handle_menu(nullptr), m_go_back_action(new QAction(Utils::Icons::PREV_TOOLBAR.icon(), EditorManager::tr("Go Back"), parent)), m_go_forward_action(new QAction(Utils::Icons::NEXT_TOOLBAR.icon(), EditorManager::tr("Go Forward"), parent)), m_back_button(new QToolButton(q)), m_forward_button(new QToolButton(q)), m_split_button(new QToolButton(q)), m_horizontal_split_action(new QAction(Utils::Icons::SPLIT_HORIZONTAL.icon(), EditorManager::tr("Split"), parent)), m_vertical_split_action(new QAction(Utils::Icons::SPLIT_VERTICAL.icon(), EditorManager::tr("Split Side by Side"), parent)), m_split_new_window_action(new QAction(EditorManager::tr("Open in New Window"), parent)), m_close_split_button(new QToolButton(q)), m_active_tool_bar(nullptr), m_tool_bar_placeholder(new QWidget(q)), m_default_tool_bar(new QWidget(q)), m_is_standalone(false) {}

/*!
  Mimic the look of the text editor toolbar as defined in e.g. EditorView::EditorView
  */
EditorToolBar::EditorToolBar(QWidget *parent) : StyledBar(parent), d(new EditorToolBarPrivate(parent, this))
{
  const auto tool_bar_layout = new QHBoxLayout(this);
  tool_bar_layout->setContentsMargins(0, 0, 0, 0);
  tool_bar_layout->setSpacing(0);
  tool_bar_layout->addWidget(d->m_default_tool_bar);

  d->m_tool_bar_placeholder->setLayout(tool_bar_layout);
  d->m_tool_bar_placeholder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  d->m_default_tool_bar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  d->m_active_tool_bar = d->m_default_tool_bar;
  d->m_lock_button->setEnabled(false);
  d->m_drag_handle->setProperty("noArrow", true);
  d->m_drag_handle->setToolTip(tr("Drag to drag documents between splits"));
  d->m_drag_handle->installEventFilter(this);
  d->m_drag_handle_menu = new QMenu(d->m_drag_handle);
  d->m_drag_handle->setMenu(d->m_drag_handle_menu);

  connect(d->m_go_back_action, &QAction::triggered, this, &EditorToolBar::goBackClicked);
  connect(d->m_go_forward_action, &QAction::triggered, this, &EditorToolBar::goForwardClicked);

  d->m_editor_list->setProperty("hideicon", true);
  d->m_editor_list->setProperty("notelideasterisk", true);
  d->m_editor_list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  d->m_editor_list->setMinimumContentsLength(20);
  d->m_editor_list->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->m_editor_list->setModel(DocumentModel::model());
  d->m_editor_list->setMaxVisibleItems(40);
  d->m_editor_list->setContextMenuPolicy(Qt::CustomContextMenu);
  d->m_close_editor_button->setIcon(Utils::Icons::CLOSE_TOOLBAR.icon());
  d->m_close_editor_button->setEnabled(false);
  d->m_close_editor_button->setProperty("showborder", true);
  d->m_tool_bar_placeholder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  d->m_back_button->setDefaultAction(d->m_go_back_action);
  d->m_forward_button->setDefaultAction(d->m_go_forward_action);
  d->m_split_button->setIcon(Utils::Icons::SPLIT_HORIZONTAL_TOOLBAR.icon());
  d->m_split_button->setToolTip(tr("Split"));
  d->m_split_button->setPopupMode(QToolButton::InstantPopup);
  d->m_split_button->setProperty("noArrow", true);

  const auto split_menu = new QMenu(d->m_split_button);
  split_menu->addAction(d->m_horizontal_split_action);
  split_menu->addAction(d->m_vertical_split_action);
  split_menu->addAction(d->m_split_new_window_action);
  d->m_split_button->setMenu(split_menu);

  d->m_close_split_button->setIcon(Utils::Icons::CLOSE_SPLIT_BOTTOM.icon());

  const auto top_layout = new QHBoxLayout(this);
  top_layout->setSpacing(0);
  top_layout->setContentsMargins(0, 0, 0, 0);
  top_layout->addWidget(d->m_back_button);
  top_layout->addWidget(d->m_forward_button);
  top_layout->addWidget(d->m_lock_button);
  top_layout->addWidget(d->m_drag_handle);
  top_layout->addWidget(d->m_editor_list);
  top_layout->addWidget(d->m_close_editor_button);
  top_layout->addWidget(d->m_tool_bar_placeholder, 1); // Custom toolbar stretches
  top_layout->addWidget(d->m_split_button);
  top_layout->addWidget(d->m_close_split_button);

  setLayout(top_layout);

  // this signal is disconnected for standalone toolbars and replaced with
  // a private slot connection
  connect(d->m_editor_list, QOverload<int>::of(&QComboBox::activated), this, &EditorToolBar::listSelectionActivated);

  connect(d->m_editor_list, &QComboBox::customContextMenuRequested, [this](const QPoint p) {
    QMenu menu;
    fillListContextMenu(&menu);
    menu.exec(d->m_editor_list->mapToGlobal(p));
  });

  connect(d->m_drag_handle_menu, &QMenu::aboutToShow, [this]() {
    d->m_drag_handle_menu->clear();
    fillListContextMenu(d->m_drag_handle_menu);
  });

  connect(d->m_lock_button, &QAbstractButton::clicked, this, &EditorToolBar::makeEditorWritable);
  connect(d->m_close_editor_button, &QAbstractButton::clicked, this, &EditorToolBar::closeEditor, Qt::QueuedConnection);
  connect(d->m_horizontal_split_action, &QAction::triggered, this, &EditorToolBar::horizontalSplitClicked, Qt::QueuedConnection);
  connect(d->m_vertical_split_action, &QAction::triggered, this, &EditorToolBar::verticalSplitClicked, Qt::QueuedConnection);
  connect(d->m_split_new_window_action, &QAction::triggered, this, &EditorToolBar::splitNewWindowClicked, Qt::QueuedConnection);
  connect(d->m_close_split_button, &QAbstractButton::clicked, this, &EditorToolBar::closeSplitClicked, Qt::QueuedConnection);
  connect(ActionManager::command(CLOSE), &Command::keySequenceChanged, this, &EditorToolBar::updateActionShortcuts);
  connect(ActionManager::command(GO_BACK), &Command::keySequenceChanged, this, &EditorToolBar::updateActionShortcuts);
  connect(ActionManager::command(GO_FORWARD), &Command::keySequenceChanged, this, &EditorToolBar::updateActionShortcuts);

  updateActionShortcuts();
}

EditorToolBar::~EditorToolBar()
{
  delete d;
}

auto EditorToolBar::removeToolbarForEditor(IEditor *editor) -> void
{
  QTC_ASSERT(editor, return);
  disconnect(editor->document(), &IDocument::changed, this, &EditorToolBar::checkDocumentStatus);

  if (const auto tool_bar = editor->toolBar(); tool_bar != nullptr) {
    if (d->m_active_tool_bar == tool_bar) {
      d->m_active_tool_bar = d->m_default_tool_bar;
      d->m_active_tool_bar->setVisible(true);
    }

    d->m_tool_bar_placeholder->layout()->removeWidget(tool_bar);
    tool_bar->setVisible(false);
    tool_bar->setParent(nullptr);
  }
}

auto EditorToolBar::setCloseSplitEnabled(const bool enable) const -> void
{
  d->m_close_split_button->setVisible(enable);
}

auto EditorToolBar::setCloseSplitIcon(const QIcon &icon) const -> void
{
  d->m_close_split_button->setIcon(icon);
}

auto EditorToolBar::closeEditor() -> void
{
  if (d->m_is_standalone)
    EditorManager::slotCloseCurrentEditorOrDocument();

  emit closeClicked();
}

auto EditorToolBar::addEditor(IEditor *editor) -> void
{
  QTC_ASSERT(editor, return);
  connect(editor->document(), &IDocument::changed, this, &EditorToolBar::checkDocumentStatus);

  if (const auto tool_bar = editor->toolBar(); tool_bar && !d->m_is_standalone)
    addCenterToolBar(tool_bar);
}

auto EditorToolBar::addCenterToolBar(QWidget *tool_bar) const -> void
{
  QTC_ASSERT(tool_bar, return);
  tool_bar->setVisible(false); // will be made visible in setCurrentEditor
  d->m_tool_bar_placeholder->layout()->addWidget(tool_bar);
  updateToolBar(tool_bar);
}

auto EditorToolBar::updateToolBar(QWidget *tool_bar) const -> void
{
  if (!tool_bar)
    tool_bar = d->m_default_tool_bar;
  if (d->m_active_tool_bar == tool_bar)
    return;
  tool_bar->setVisible(true);
  d->m_active_tool_bar->setVisible(false);
  d->m_active_tool_bar = tool_bar;
}

auto EditorToolBar::setToolbarCreationFlags(const ToolbarCreationFlags flags) -> void
{
  d->m_is_standalone = flags & FlagsStandalone;

  if (d->m_is_standalone) {
    connect(EditorManager::instance(), &EditorManager::currentEditorChanged, this, &EditorToolBar::setCurrentEditor);
    disconnect(d->m_editor_list, QOverload<int>::of(&QComboBox::activated), this, &EditorToolBar::listSelectionActivated);
    connect(d->m_editor_list, QOverload<int>::of(&QComboBox::activated), this, &EditorToolBar::changeActiveEditor);
    d->m_split_button->setVisible(false);
    d->m_close_split_button->setVisible(false);
  }
}

auto EditorToolBar::setMenuProvider(const menu_provider &provider) const -> void
{
  d->m_menu_provider = provider;
}

auto EditorToolBar::setCurrentEditor(IEditor *editor) -> void
{
  const auto document = editor ? editor->document() : nullptr;

  if (const auto index = DocumentModel::rowOfDocument(document); QTC_GUARD(index))
    d->m_editor_list->setCurrentIndex(*index);

  // If we never added the toolbar from the editor,  we will never change
  // the editor, so there's no need to update the toolbar either.
  if (!d->m_is_standalone)
    updateToolBar(editor ? editor->toolBar() : nullptr);

  updateDocumentStatus(document);
}

auto EditorToolBar::changeActiveEditor(const int row) -> void
{
  EditorManager::activateEditorForEntry(DocumentModel::entryAtRow(row));
}

auto EditorToolBar::fillListContextMenu(QMenu *menu) const -> void
{
  if (d->m_menu_provider) {
    d->m_menu_provider(menu);
  } else {
    const auto editor = EditorManager::currentEditor();
    const auto entry = editor ? DocumentModel::entryForDocument(editor->document()) : nullptr;
    EditorManager::addSaveAndCloseEditorActions(menu, entry, editor);
    menu->addSeparator();
    EditorManager::addPinEditorActions(menu, entry);
    menu->addSeparator();
    EditorManager::addNativeDirAndOpenWithActions(menu, entry);
  }
}

auto EditorToolBar::makeEditorWritable() -> void
{
  if (const auto current = EditorManager::currentDocument())
    EditorManagerPrivate::makeFileWritable(current);
}

auto EditorToolBar::setCanGoBack(const bool can_go_back) const -> void
{
  d->m_go_back_action->setEnabled(can_go_back);
}

auto EditorToolBar::setCanGoForward(const bool can_go_forward) const -> void
{
  d->m_go_forward_action->setEnabled(can_go_forward);
}

auto EditorToolBar::updateActionShortcuts() const -> void
{
  d->m_close_editor_button->setToolTip(ActionManager::command(CLOSE)->stringWithAppendedShortcut(EditorManager::tr("Close Document")));
  d->m_go_back_action->setToolTip(ActionManager::command(GO_BACK)->action()->toolTip());
  d->m_go_forward_action->setToolTip(ActionManager::command(GO_FORWARD)->action()->toolTip());
  d->m_close_split_button->setToolTip(ActionManager::command(REMOVE_CURRENT_SPLIT)->stringWithAppendedShortcut(tr("Remove Split")));
}

auto EditorToolBar::checkDocumentStatus() const -> void
{
  const auto document = qobject_cast<IDocument*>(sender());
  QTC_ASSERT(document, return);

  if (const auto entry = DocumentModel::entryAtRow(d->m_editor_list->currentIndex()); entry && entry->document && entry->document == document)
    updateDocumentStatus(document);
}

auto EditorToolBar::updateDocumentStatus(const IDocument *document) const -> void
{
  d->m_close_editor_button->setEnabled(document != nullptr);

  if (!document) {
    d->m_lock_button->setIcon(QIcon());
    d->m_lock_button->setEnabled(false);
    d->m_lock_button->setToolTip(QString());
    d->m_drag_handle->setIcon(QIcon());
    d->m_editor_list->setToolTip(QString());
    return;
  }

  if (document->filePath().isEmpty()) {
    d->m_lock_button->setIcon(QIcon());
    d->m_lock_button->setEnabled(false);
    d->m_lock_button->setToolTip(QString());
  } else if (document->isFileReadOnly()) {
    const static auto locked = Utils::Icons::LOCKED_TOOLBAR.icon();
    d->m_lock_button->setIcon(locked);
    d->m_lock_button->setEnabled(true);
    d->m_lock_button->setToolTip(tr("Make Writable"));
  } else {
    const static auto unlocked = Utils::Icons::UNLOCKED_TOOLBAR.icon();
    d->m_lock_button->setIcon(unlocked);
    d->m_lock_button->setEnabled(false);
    d->m_lock_button->setToolTip(tr("File is writable"));
  }

  if (document->filePath().isEmpty())
    d->m_drag_handle->setIcon(QIcon());
  else
    d->m_drag_handle->setIcon(icon(document->filePath()));

  d->m_editor_list->setToolTip(document->filePath().isEmpty() ? document->displayName() : document->filePath().toUserOutput());
}

auto EditorToolBar::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == d->m_drag_handle) {
    if (event->type() == QEvent::MouseButtonPress) {
      if (const auto me = dynamic_cast<QMouseEvent*>(event); me->buttons() == Qt::LeftButton)
        d->m_drag_start_position = me->pos();
      return true; // do not pop up menu already on press
    }

    if (event->type() == QEvent::MouseButtonRelease) {
      d->m_drag_handle->showMenu();
      return true;
    }

    if (event->type() == QEvent::MouseMove) {
      const auto me = dynamic_cast<QMouseEvent*>(event);

      if (me->buttons() != Qt::LeftButton)
        return StyledBar::eventFilter(obj, event);

      if ((me->pos() - d->m_drag_start_position).manhattanLength() < QApplication::startDragDistance())
        return StyledBar::eventFilter(obj, event);

      const auto entry = DocumentModel::entryAtRow(d->m_editor_list->currentIndex());

      if (!entry) // no document
        return StyledBar::eventFilter(obj, event);

      const auto drag = new QDrag(this);
      const auto data = new Utils::DropMimeData;

      data->addFile(entry->fileName());
      drag->setMimeData(data);

      if (const auto action = drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction); action == Qt::MoveAction)
        emit currentDocumentMoved();

      return true;
    }
  }
  return StyledBar::eventFilter(obj, event);
}

auto EditorToolBar::setNavigationVisible(const bool is_visible) const -> void
{
  d->m_go_back_action->setVisible(is_visible);
  d->m_go_forward_action->setVisible(is_visible);
  d->m_back_button->setVisible(is_visible);
  d->m_forward_button->setVisible(is_visible);
}

} // namespace Orca::Plugin::Core
