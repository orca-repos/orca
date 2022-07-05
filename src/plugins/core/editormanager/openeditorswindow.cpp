// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "openeditorswindow.h"
#include "editormanager.h"
#include "editormanager_p.h"
#include "editorview.h"

#include <core/idocument.h>

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/utilsicons.h>

#include <QFocusEvent>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QScrollBar>

Q_DECLARE_METATYPE(Core::Internal::EditorView*)
Q_DECLARE_METATYPE(Core::IDocument*)

namespace Core {
namespace Internal {

enum class role {
  entry = Qt::UserRole,
  view = Qt::UserRole + 1
};

OpenEditorsWindow::OpenEditorsWindow(QWidget *parent) : QFrame(parent, Qt::Popup), m_empty_icon(Utils::Icons::EMPTY14.icon()), m_editor_list(new OpenEditorsTreeWidget(this))
{
  setMinimumSize(300, 200);
  m_editor_list->setColumnCount(1);
  m_editor_list->header()->hide();
  m_editor_list->setIndentation(0);
  m_editor_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_editor_list->setTextElideMode(Qt::ElideMiddle);

  if constexpr (Utils::HostOsInfo::isMacHost())
    m_editor_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_editor_list->installEventFilter(this);

  // We disable the frame on this list view and use a QFrame around it instead.
  // This improves the look with QGTKStyle.
  if constexpr (!Utils::HostOsInfo::isMacHost())
    setFrameStyle(m_editor_list->frameStyle());

  m_editor_list->setFrameStyle(QFrame::NoFrame);

  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_editor_list);

  connect(m_editor_list, &QTreeWidget::itemClicked, this, &OpenEditorsWindow::editorClicked);
}

auto OpenEditorsWindow::selectAndHide() -> void
{
  setVisible(false);
  selectEditor(m_editor_list->currentItem());
}

auto OpenEditorsWindow::setVisible(const bool visible) -> void
{
  QWidget::setVisible(visible);
  if (visible)
    setFocus();
}

auto OpenEditorsWindow::eventFilter(QObject *obj, QEvent *e) -> bool
{
  if (obj == m_editor_list) {
    if (e->type() == QEvent::KeyPress) {
      const auto ke = dynamic_cast<QKeyEvent*>(e);
      if (ke->key() == Qt::Key_Escape) {
        setVisible(false);
        return true;
      }
      if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        selectEditor(m_editor_list->currentItem());
        return true;
      }
    } else if (e->type() == QEvent::KeyRelease) {
      const auto ke = dynamic_cast<QKeyEvent*>(e);
      if (ke->modifiers() == 0
        /*HACK this is to overcome some event inconsistencies between platforms*/
        || (ke->modifiers() == Qt::AltModifier && (ke->key() == Qt::Key_Alt || ke->key() == -1))) {
        selectAndHide();
      }
    }
  }
  return QWidget::eventFilter(obj, e);
}

auto OpenEditorsWindow::focusInEvent(QFocusEvent *) -> void
{
  m_editor_list->setFocus();
}

auto OpenEditorsWindow::selectUpDown(const bool up) -> void
{
  const auto item_count = m_editor_list->topLevelItemCount();

  if (item_count < 2)
    return;

  auto index = m_editor_list->indexOfTopLevelItem(m_editor_list->currentItem());

  if (index < 0)
    return;

  QTreeWidgetItem *editor = nullptr;
  auto count = 0;

  while (!editor && count < item_count) {
    if (up) {
      index--;
      if (index < 0)
        index = item_count - 1;
    } else {
      index++;
      if (index >= item_count)
        index = 0;
    }
    editor = m_editor_list->topLevelItem(index);
    count++;
  }

  if (editor) {
    m_editor_list->setCurrentItem(editor);
    ensureCurrentVisible();
  }
}

auto OpenEditorsWindow::selectPreviousEditor() -> void
{
  selectUpDown(false);
}

auto OpenEditorsTreeWidget::sizeHint() const -> QSize
{
  return {sizeHintForColumn(0) + verticalScrollBar()->width() + frameWidth() * 2, viewportSizeHint().height() + frameWidth() * 2};
}

auto OpenEditorsWindow::sizeHint() const -> QSize
{
  return m_editor_list->sizeHint() + QSize(frameWidth() * 2, frameWidth() * 2);
}

auto OpenEditorsWindow::selectNextEditor() -> void
{
  selectUpDown(true);
}

auto OpenEditorsWindow::setEditors(const QList<EditLocation> &global_history, EditorView *view) -> void
{
  m_editor_list->clear();

  QSet<const DocumentModel::Entry*> entriesDone;
  addHistoryItems(view->editorHistory(), view, entriesDone);

  // add missing editors from the global history
  addHistoryItems(global_history, view, entriesDone);

  // add purely suspended editors which are not initialised yet
  addRemainingItems(view, entriesDone);
}

auto OpenEditorsWindow::selectEditor(const QTreeWidgetItem *item) -> void
{
  if (!item)
    return;

  const auto entry = item->data(0, static_cast<int>(role::entry)).value<DocumentModel::Entry*>();
  QTC_ASSERT(entry, return);

  if (const auto view = item->data(0, static_cast<int>(role::view)).value<EditorView*>(); !EditorManagerPrivate::activateEditorForEntry(view, entry))
    delete item;
}

auto OpenEditorsWindow::editorClicked(const QTreeWidgetItem *item) -> void
{
  selectEditor(item);
  setFocus();
}

auto OpenEditorsWindow::ensureCurrentVisible() const -> void
{
  m_editor_list->scrollTo(m_editor_list->currentIndex(), QAbstractItemView::PositionAtCenter);
}

static auto entryForEditLocation(const EditLocation &item) -> DocumentModel::Entry*
{
  if (!item.document.isNull())
    return DocumentModel::entryForDocument(item.document);
  return DocumentModel::entryForFilePath(item.file_path);
}

auto OpenEditorsWindow::addHistoryItems(const QList<EditLocation> &history, EditorView *view, QSet<const DocumentModel::Entry*> &entries_done) const -> void
{
  for(const auto &edit_location: history) {
    if (const auto entry = entryForEditLocation(edit_location))
      addItem(entry, entries_done, view);
  }
}

auto OpenEditorsWindow::addRemainingItems(EditorView *view, QSet<const DocumentModel::Entry*> &entriesDone) const -> void
{
  for(const auto entry: DocumentModel::entries())
    addItem(entry, entriesDone, view);
}

auto OpenEditorsWindow::addItem(DocumentModel::Entry *entry, QSet<const DocumentModel::Entry*> &entriesDone, EditorView *view) const -> void
{
  if (entriesDone.contains(entry))
    return;

  entriesDone.insert(entry);
  auto title = entry->displayName();
  QTC_ASSERT(!title.isEmpty(), return);

  const auto item = new QTreeWidgetItem();
  if (entry->document->isModified())
    title += tr("*");

  item->setIcon(0, !entry->fileName().isEmpty() && entry->document->isFileReadOnly() ? DocumentModel::lockedIcon() : m_empty_icon);
  item->setText(0, title);
  item->setToolTip(0, entry->fileName().toString());
  item->setData(0, static_cast<int>(role::entry), QVariant::fromValue(entry));
  item->setData(0, static_cast<int>(role::view), QVariant::fromValue(view));
  item->setTextAlignment(0, Qt::AlignLeft);

  m_editor_list->addTopLevelItem(item);
  if (m_editor_list->topLevelItemCount() == 1)
    m_editor_list->setCurrentItem(item);
}

} // namespace Internal
} // namespace Core
