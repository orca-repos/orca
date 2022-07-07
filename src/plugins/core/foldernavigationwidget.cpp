// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "foldernavigationwidget.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreicons.hpp>
#include <core/diffservice.hpp>
#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/fileiconprovider.hpp>
#include <core/fileutils.hpp>
#include <core/icontext.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>
#include <core/iwizardfactory.hpp>

#include <utils/algorithm.hpp>
#include <utils/filecrumblabel.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/navigationtreeview.hpp>
#include <utils/qtcassert.hpp>
#include <utils/removefiledialog.hpp>
#include <utils/stringutils.hpp>
#include <utils/styledbar.hpp>
#include <utils/utilsicons.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QSize>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Utils;

const int PATH_ROLE = Qt::UserRole;
const int ID_ROLE = Qt::UserRole + 1;
const int SORT_ROLE = Qt::UserRole + 2;
const char PROJECTSDIRECTORYROOT_ID[] = "A.Projects";
const char C_FOLDERNAVIGATIONWIDGET[] = "ProjectExplorer.FolderNavigationWidget";
const char kSettingsBase[] = "FolderNavigationWidget.";
const char kHiddenFilesKey[] = ".HiddenFilesFilter";
const char kSyncKey[] = ".SyncWithEditor";
const char kShowBreadCrumbs[] = ".ShowBreadCrumbs";
const char kSyncRootWithEditor[] = ".SyncRootWithEditor";
const char kShowFoldersOnTop[] = ".ShowFoldersOnTop";
const char ADDNEWFILE[] = "Orca.FileSystem.AddNewFile";
const char RENAMEFILE[] = "Orca.FileSystem.RenameFile";
const char REMOVEFILE[] = "Orca.FileSystem.RemoveFile";

namespace Core {

static FolderNavigationWidgetFactory *m_instance = nullptr;

QVector<FolderNavigationWidgetFactory::RootDirectory> FolderNavigationWidgetFactory::m_root_directories;

FilePath FolderNavigationWidgetFactory::m_fallback_sync_file_path;

auto FolderNavigationWidgetFactory::instance() -> FolderNavigationWidgetFactory*
{
  return m_instance;
}

namespace Internal {

static auto createHLine() -> QWidget*
{
  const auto widget = new QFrame;
  widget->setFrameStyle(QFrame::Plain | QFrame::HLine);
  return widget;
}

// Call delayLayoutOnce to delay reporting the new heightForWidget by the double-click interval.
// Call setScrollBarOnce to set a scroll bar's value once during layouting (where heightForWidget
// is called).
class DelayedFileCrumbLabel final : public FileCrumbLabel {
public:
  explicit DelayedFileCrumbLabel(QWidget *parent) : FileCrumbLabel(parent) {}

  auto immediateHeightForWidth(int w) const -> int;
  auto heightForWidth(int w) const -> int final;
  auto delayLayoutOnce() -> void;
  auto setScrollBarOnce(QScrollBar *bar, int value) -> void;

private:
  auto setScrollBarOnce() const -> void;

  QPointer<QScrollBar> m_bar;
  int m_bar_value = 0;
  bool m_delaying = false;
};

// FolderNavigationModel: Shows path as tooltip.
class FolderNavigationModel final : public QFileSystemModel {
public:
  enum Roles {
    IsFolderRole = Qt::UserRole + 50 // leave some gap for the custom roles in QFileSystemModel
  };

  explicit FolderNavigationModel(QObject *parent = nullptr);

  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant final;
  auto supportedDragActions() const -> Qt::DropActions final;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags final;
  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool final;
};

// Sorts folders on top if wanted
class FolderSortProxyModel final : public QSortFilterProxyModel {
public:
  explicit FolderSortProxyModel(QObject *parent = nullptr);

protected:
  auto lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const -> bool override;
};

FolderSortProxyModel::FolderSortProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

auto FolderSortProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const -> bool
{
  const QAbstractItemModel *src = sourceModel();

  if (sortRole() == FolderNavigationModel::IsFolderRole) {
    const auto left_is_folder = src->data(source_left, FolderNavigationModel::IsFolderRole).toBool();
    if (const auto right_is_folder = src->data(source_right, FolderNavigationModel::IsFolderRole).toBool(); left_is_folder != right_is_folder)
      return left_is_folder;
  }

  const auto left_name = src->data(source_left, QFileSystemModel::FileNameRole).toString();
  const auto right_name = src->data(source_right, QFileSystemModel::FileNameRole).toString();

  return FilePath::fromString(left_name) < FilePath::fromString(right_name);
}

FolderNavigationModel::FolderNavigationModel(QObject *parent) : QFileSystemModel(parent) { }

auto FolderNavigationModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (role == Qt::ToolTipRole)
    return QDir::toNativeSeparators(QDir::cleanPath(filePath(index)));

  if (role == IsFolderRole)
    return isDir(index);

  return QFileSystemModel::data(index, role);
}

auto FolderNavigationModel::supportedDragActions() const -> Qt::DropActions
{
  return Qt::MoveAction;
}

auto FolderNavigationModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  if (index.isValid() && !fileInfo(index).isRoot())
    return QFileSystemModel::flags(index) | Qt::ItemIsEditable;

  return QFileSystemModel::flags(index);
}

auto FolderNavigationModel::setData(const QModelIndex &index, const QVariant &value, const int role) -> bool
{
  QTC_ASSERT(index.isValid() && parent(index).isValid() && index.column() == 0 && role == Qt::EditRole && value.canConvert<QString>(), return false);

  const auto after_file_name = value.toString();
  const auto before_file_path = FilePath::fromString(filePath(index));
  const auto parent_path = FilePath::fromString(filePath(parent(index)));
  const auto after_file_path = parent_path.pathAppended(after_file_name);

  if (before_file_path == after_file_path)
    return false;

  // need to rename through file system model, which takes care of not changing our selection
  const auto success = QFileSystemModel::setData(index, value, role);

  // for files we can do more than just rename on disk, for directories the user is on his/her own
  if (success && fileInfo(index).isFile()) {
    DocumentManager::renamedFile(before_file_path, after_file_path);
    emit m_instance->fileRenamed(before_file_path, after_file_path);
  }

  return success;
}

static auto showOnlyFirstColumn(QTreeView *view) -> void
{
  const auto column_count = view->header()->count();
  for (auto i = 1; i < column_count; ++i)
    view->setColumnHidden(i, true);
}

static auto isChildOf(const QModelIndex &index, const QModelIndex &parent) -> bool
{
  if (index == parent)
    return true;

  auto current = index;

  while (current.isValid()) {
    current = current.parent();
    if (current == parent)
      return true;
  }

  return false;
}

} // namespace Internal

using namespace Internal;

/*!
    \class FolderNavigationWidget

    Shows a file system tree, with the root directory selectable from a dropdown.

    \internal
*/
FolderNavigationWidget::FolderNavigationWidget(QWidget *parent) : QWidget(parent), m_list_view(new NavigationTreeView(this)), m_file_system_model(new FolderNavigationModel(this)), m_sort_proxy_model(new FolderSortProxyModel(m_file_system_model)), m_filter_hidden_files_action(new QAction(tr("Show Hidden Files"), this)), m_show_bread_crumbs_action(new QAction(tr("Show Bread Crumbs"), this)), m_show_folders_on_top_action(new QAction(tr("Show Folders on Top"), this)), m_toggle_sync(new QToolButton(this)), m_toggle_root_sync(new QToolButton(this)), m_root_selector(new QComboBox), m_crumb_container(new QWidget(this)), m_crumb_label(new DelayedFileCrumbLabel(this))
{
  const auto context = new IContext(this);
  context->setContext(Context(C_FOLDERNAVIGATIONWIDGET));
  context->setWidget(this);

  ICore::addContextObject(context);

  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setHiddenFilesFilter(false);
  setShowBreadCrumbs(true);
  setShowFoldersOnTop(true);

  m_sort_proxy_model->setSourceModel(m_file_system_model);
  m_sort_proxy_model->setSortRole(FolderNavigationModel::IsFolderRole);
  m_sort_proxy_model->sort(0);
  m_file_system_model->setResolveSymlinks(false);
  m_file_system_model->setIconProvider(FileIconProvider::iconProvider());

  auto filters = QDir::AllEntries | QDir::NoDotAndDotDot;
  if constexpr (HostOsInfo::isWindowsHost()) // Symlinked directories can cause file watcher warnings on Win32.
    filters |= QDir::NoSymLinks;

  m_file_system_model->setFilter(filters);
  m_file_system_model->setRootPath(QString());
  m_filter_hidden_files_action->setCheckable(true);
  m_show_bread_crumbs_action->setCheckable(true);
  m_show_folders_on_top_action->setCheckable(true);
  m_list_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_list_view->setIconSize(QSize(16, 16));
  m_list_view->setModel(m_sort_proxy_model);
  m_list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_list_view->setDragEnabled(true);
  m_list_view->setDragDropMode(QAbstractItemView::DragOnly);
  m_list_view->viewport()->installEventFilter(this);

  showOnlyFirstColumn(m_list_view);
  setFocusProxy(m_list_view);

  const auto selector_widget = new StyledBar(this);
  selector_widget->setLightColored(true);

  const auto selector_layout = new QHBoxLayout(selector_widget);
  selector_widget->setLayout(selector_layout);
  selector_layout->setSpacing(0);
  selector_layout->setContentsMargins(0, 0, 0, 0);
  selector_layout->addWidget(m_root_selector, 10);

  const auto crumb_container_layout = new QVBoxLayout;
  crumb_container_layout->setSpacing(0);
  crumb_container_layout->setContentsMargins(0, 0, 0, 0);
  m_crumb_container->setLayout(crumb_container_layout);

  const auto crumb_layout = new QVBoxLayout;
  crumb_layout->setSpacing(0);
  crumb_layout->setContentsMargins(4, 4, 4, 4);
  crumb_layout->addWidget(m_crumb_label);
  crumb_container_layout->addLayout(crumb_layout);
  crumb_container_layout->addWidget(createHLine());
  m_crumb_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);

  const auto layout = new QVBoxLayout();
  layout->addWidget(selector_widget);
  layout->addWidget(m_crumb_container);
  layout->addWidget(m_list_view);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);

  setLayout(layout);

  m_toggle_sync->setIcon(Utils::Icons::LINK_TOOLBAR.icon());
  m_toggle_sync->setCheckable(true);
  m_toggle_sync->setToolTip(tr("Synchronize with Editor"));
  m_toggle_root_sync->setIcon(Utils::Icons::LINK.icon());
  m_toggle_root_sync->setCheckable(true);
  m_toggle_root_sync->setToolTip(tr("Synchronize Root Directory with Editor"));

  selector_layout->addWidget(m_toggle_root_sync);

  // connections
  connect(EditorManager::instance(), &EditorManager::currentEditorChanged, this, &FolderNavigationWidget::handleCurrentEditorChanged);
  connect(m_list_view, &QAbstractItemView::activated, this, [this](const QModelIndex &index) {
    openItem(m_sort_proxy_model->mapToSource(index));
  });

  // Delay updating crumble path by event loop cylce, because that can scroll, which doesn't
  // work well when done directly in currentChanged (the wrong item can get highlighted).
  // We cannot use Qt::QueuedConnection directly, because the QModelIndex could get invalidated
  // in the meantime, so use a queued invokeMethod instead.
  connect(m_list_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
    const auto source_index = m_sort_proxy_model->mapToSource(index);
    const auto file_path = FilePath::fromString(m_file_system_model->filePath(source_index));
    // QTimer::singleShot only posts directly onto the event loop if you use the SLOT("...")
    // notation, so using a singleShot with a lambda would flicker
    // QTimer::singleShot(0, this, [this, filePath]() { setCrumblePath(filePath); });
    QMetaObject::invokeMethod(this, [this, file_path] { setCrumblePath(file_path); }, Qt::QueuedConnection);
  });

  connect(m_crumb_label, &FileCrumbLabel::pathClicked, [this](const FilePath &path) {
    const auto root_index = m_sort_proxy_model->mapToSource(m_list_view->rootIndex());
    if (const auto file_index = m_file_system_model->index(path.toString()); !isChildOf(file_index, root_index))
      selectBestRootForFile(path);
    selectFile(path);
  });

  connect(m_filter_hidden_files_action, &QAction::toggled, this, &FolderNavigationWidget::setHiddenFilesFilter);
  connect(m_show_bread_crumbs_action, &QAction::toggled, this, &FolderNavigationWidget::setShowBreadCrumbs);
  connect(m_show_folders_on_top_action, &QAction::toggled, this, &FolderNavigationWidget::setShowFoldersOnTop);
  connect(m_toggle_sync, &QAbstractButton::clicked, this, &FolderNavigationWidget::toggleAutoSynchronization);
  connect(m_toggle_root_sync, &QAbstractButton::clicked, this, [this]() { setRootAutoSynchronization(!m_root_auto_sync); });

  connect(m_root_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](const int index) {
    const auto directory = m_root_selector->itemData(index).value<FilePath>();
    m_root_selector->setToolTip(directory.toUserOutput());
    setRootDirectory(directory);
    const auto root_index = m_sort_proxy_model->mapToSource(m_list_view->rootIndex());
    if (const auto file_index = m_sort_proxy_model->mapToSource(m_list_view->currentIndex()); !isChildOf(file_index, root_index))
      selectFile(directory);
  });

  setAutoSynchronization(true);
  setRootAutoSynchronization(true);
}

auto FolderNavigationWidget::toggleAutoSynchronization() -> void
{
  setAutoSynchronization(!m_auto_sync);
}

auto FolderNavigationWidget::setShowBreadCrumbs(const bool show) const -> void
{
  m_show_bread_crumbs_action->setChecked(show);
  m_crumb_container->setVisible(show);
}

auto FolderNavigationWidget::setShowFoldersOnTop(const bool on_top) const -> void
{
  m_show_folders_on_top_action->setChecked(on_top);
  m_sort_proxy_model->setSortRole(on_top ? static_cast<int>(FolderNavigationModel::IsFolderRole) : static_cast<int>(QFileSystemModel::FileNameRole));
}

static auto itemLessThan(const QComboBox *combo, const int index, const FolderNavigationWidgetFactory::RootDirectory &directory) -> bool
{
  return combo->itemData(index, SORT_ROLE).toInt() < directory.sort_value || combo->itemData(index, SORT_ROLE).toInt() == directory.sort_value && combo->itemData(index, Qt::DisplayRole).toString() < directory.display_name;
}

auto FolderNavigationWidget::insertRootDirectory(const FolderNavigationWidgetFactory::RootDirectory &directory) -> void
{
  // Find existing. Do not remove yet, to not mess up the current selection.
  auto previous_index = 0;

  while (previous_index < m_root_selector->count() && m_root_selector->itemData(previous_index, ID_ROLE).toString() != directory.id)
    ++previous_index;

  // Insert sorted.
  auto index = 0;

  while (index < m_root_selector->count() && itemLessThan(m_root_selector, index, directory))
    ++index;

  m_root_selector->insertItem(index, directory.display_name);

  if (index <= previous_index) // item was inserted, update previousIndex
    ++previous_index;

  m_root_selector->setItemData(index, QVariant::fromValue(directory.path), PATH_ROLE);
  m_root_selector->setItemData(index, directory.id, ID_ROLE);
  m_root_selector->setItemData(index, directory.sort_value, SORT_ROLE);
  m_root_selector->setItemData(index, directory.path.toUserOutput(), Qt::ToolTipRole);
  m_root_selector->setItemIcon(index, directory.icon);

  if (m_root_selector->currentIndex() == previous_index)
    m_root_selector->setCurrentIndex(index);

  if (previous_index < m_root_selector->count())
    m_root_selector->removeItem(previous_index);

  if (EditorManager::currentEditor()) {
    if (m_auto_sync) // we might find a better root for current selection now
      handleCurrentEditorChanged(EditorManager::currentEditor());
  } else if (m_root_auto_sync) {
    // assume the new root is better (e.g. because a project was opened)
    m_root_selector->setCurrentIndex(index);
  }
}

auto FolderNavigationWidget::removeRootDirectory(const QString &id) -> void
{
  for (auto i = 0; i < m_root_selector->count(); ++i) {
    if (m_root_selector->itemData(i, ID_ROLE).toString() == id) {
      m_root_selector->removeItem(i);
      break;
    }
  }

  if (m_auto_sync) // we might need to find a new root for current selection
    handleCurrentEditorChanged(EditorManager::currentEditor());
}

auto FolderNavigationWidget::addNewItem() -> void
{
  const auto current = m_sort_proxy_model->mapToSource(m_list_view->currentIndex());

  if (!current.isValid())
    return;

  const auto file_path = FilePath::fromString(m_file_system_model->filePath(current));
  const auto path = file_path.isDir() ? file_path : file_path.parentDir();

  ICore::showNewItemDialog(tr("New File", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), equal(&IWizardFactory::kind, IWizardFactory::FileWizard)), path);
}

auto FolderNavigationWidget::editCurrentItem() const -> void
{
  if (const auto current = m_list_view->currentIndex(); m_list_view->model()->flags(current) & Qt::ItemIsEditable)
    m_list_view->edit(current);
}

auto FolderNavigationWidget::removeCurrentItem() const -> void
{
  const auto current = m_sort_proxy_model->mapToSource(m_list_view->currentIndex());

  if (!current.isValid() || m_file_system_model->isDir(current))
    return;

  const auto file_path = FilePath::fromString(m_file_system_model->filePath(current));
  RemoveFileDialog dialog(file_path, ICore::dialogParent());
  dialog.setDeleteFileVisible(false);

  if (dialog.exec() == QDialog::Accepted) {
    emit m_instance->aboutToRemoveFile(file_path);
    FileChangeBlocker change_guard(file_path);
    FileUtils::removeFiles({file_path}, true /*delete from disk*/);
  }
}

auto FolderNavigationWidget::syncWithFilePath(const FilePath &file_path) -> void
{
  if (file_path.isEmpty())
    return;

  if (m_root_auto_sync)
    selectBestRootForFile(file_path);

  selectFile(file_path);
}

auto FolderNavigationWidget::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == m_list_view->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      // select the current root when clicking outside any other item
      const auto me = dynamic_cast<QMouseEvent*>(event);
      if (const auto index = m_list_view->indexAt(me->pos()); !index.isValid())
        m_list_view->setCurrentIndex(m_list_view->rootIndex());
    }
  }
  return false;
}

auto FolderNavigationWidget::autoSynchronization() const -> bool
{
  return m_auto_sync;
}

auto FolderNavigationWidget::setAutoSynchronization(bool sync) -> void
{
  m_toggle_sync->setChecked(sync);
  m_toggle_root_sync->setEnabled(sync);
  m_toggle_root_sync->setChecked(sync ? m_root_auto_sync : false);

  if (sync == m_auto_sync)
    return;

  m_auto_sync = sync;

  if (m_auto_sync)
    handleCurrentEditorChanged(EditorManager::currentEditor());
}

auto FolderNavigationWidget::setRootAutoSynchronization(const bool sync) -> void
{
  m_toggle_root_sync->setChecked(sync);

  if (sync == m_root_auto_sync)
    return;

  m_root_auto_sync = sync;

  if (m_root_auto_sync)
    handleCurrentEditorChanged(EditorManager::currentEditor());
}

auto FolderNavigationWidget::handleCurrentEditorChanged(const IEditor *editor) -> void
{
  if (!m_auto_sync || !editor || editor->document()->filePath().isEmpty() || editor->document()->isTemporary())
    return;

  syncWithFilePath(editor->document()->filePath());
}

auto FolderNavigationWidget::selectBestRootForFile(const FilePath &file_path) -> void
{
  const auto best_root_index = bestRootForFile(file_path);
  m_root_selector->setCurrentIndex(best_root_index);
}

auto FolderNavigationWidget::selectFile(const FilePath &file_path) -> void
{
  if (const auto file_index = m_sort_proxy_model->mapFromSource(m_file_system_model->index(file_path.toString())); file_index.isValid() || file_path.isEmpty() /* Computer root */) {
    // TODO This only scrolls to the right position if all directory contents are loaded.
    // Unfortunately listening to directoryLoaded was still not enough (there might also
    // be some delayed sorting involved?).
    // Use magic timer for scrolling.
    m_list_view->setCurrentIndex(file_index);

    QTimer::singleShot(200, this, [this, file_path] {
      if (const auto file_index = m_sort_proxy_model->mapFromSource(m_file_system_model->index(file_path.toString())); file_index == m_list_view->rootIndex()) {
        m_list_view->horizontalScrollBar()->setValue(0);
        m_list_view->verticalScrollBar()->setValue(0);
      } else {
        m_list_view->scrollTo(file_index);
      }
      setCrumblePath(file_path);
    });
  }
}

auto FolderNavigationWidget::setRootDirectory(const FilePath &directory) const -> void
{
  const auto index = m_sort_proxy_model->mapFromSource(m_file_system_model->setRootPath(directory.toString()));
  m_list_view->setRootIndex(index);
}

auto FolderNavigationWidget::bestRootForFile(const FilePath &file_path) const -> int
{
  auto index = 0; // Computer is default
  auto common_length = 0;

  for (auto i = 1; i < m_root_selector->count(); ++i) {
    if (const auto root = m_root_selector->itemData(i).value<FilePath>(); (file_path == root || file_path.isChildOf(root)) && root.toString().size() > common_length) {
      index = i;
      common_length = static_cast<int>(root.toString().size());
    }
  }

  return index;
}

auto FolderNavigationWidget::openItem(const QModelIndex &index) const -> void
{
  QTC_ASSERT(index.isValid(), return);

  // signal "activate" is also sent when double-clicking folders
  // but we don't want to do anything in that case
  if (m_file_system_model->isDir(index))
    return;

  const auto path = m_file_system_model->filePath(index);
  EditorManager::openEditor(FilePath::fromString(path), {}, EditorManager::allow_external_editor);
}

auto FolderNavigationWidget::createNewFolder(const QModelIndex &parent) const -> void
{
  static const auto base_name = tr("New Folder");
  // find non-existing name
  const QDir dir(m_file_system_model->filePath(parent));

  const auto existing_items = Utils::transform<QSet>(dir.entryList({base_name + '*'}, QDir::AllEntries), [](const QString &entry) {
    return FilePath::fromString(entry);
  });

  const auto name = makeUniquelyNumbered(FilePath::fromString(base_name), existing_items);
  // create directory and edit
  const auto index = m_sort_proxy_model->mapFromSource(m_file_system_model->mkdir(parent, name.toString()));

  if (!index.isValid())
    return;

  m_list_view->setCurrentIndex(index);
  m_list_view->edit(index);
}

auto FolderNavigationWidget::setCrumblePath(const FilePath &file_path) const -> void
{
  const auto index = m_file_system_model->index(file_path.toString());
  const auto width = m_crumb_label->width();
  const auto previous_height = m_crumb_label->immediateHeightForWidth(width);
  m_crumb_label->setPath(file_path);
  const auto current_height = m_crumb_label->immediateHeightForWidth(width);

  if (const auto diff = current_height - previous_height; diff != 0 && m_crumb_label->isVisible()) {
    // try to fix scroll position, otherwise delay layouting
    const auto bar = m_list_view->verticalScrollBar();
    const auto new_bar_value = bar ? bar->value() + diff : 0;
    const auto current_item_rect = m_list_view->visualRect(index);
    const auto current_item_v_start = current_item_rect.y();
    const auto current_item_v_end = current_item_v_start + current_item_rect.height();

    if (const auto current_item_still_visible_as_before = diff < 0 || current_item_v_start > diff || current_item_v_end <= 0; bar && bar->minimum() <= new_bar_value && bar->maximum() >= new_bar_value && current_item_still_visible_as_before) {
      // we need to set the scroll bar when the layout request from the crumble path is
      // handled, otherwise it will flicker
      m_crumb_label->setScrollBarOnce(bar, new_bar_value);
    } else {
      m_crumb_label->delayLayoutOnce();
    }
  }
}

auto FolderNavigationWidget::contextMenuEvent(QContextMenuEvent *ev) -> void
{
  QMenu menu;

  // Open current item
  const auto current = m_sort_proxy_model->mapToSource(m_list_view->currentIndex());
  const auto has_current_item = current.isValid();
  const QAction *action_open_file = nullptr;
  const QAction *new_folder = nullptr;
  const auto is_dir = m_file_system_model->isDir(current);
  const auto file_path = has_current_item ? FilePath::fromString(m_file_system_model->filePath(current)) : FilePath();

  if (has_current_item) {
    if (!is_dir)
      action_open_file = menu.addAction(tr("Open \"%1\"").arg(file_path.toUserOutput()));
    emit m_instance->aboutToShowContextMenu(&menu, file_path, is_dir);
  }

  // we need dummy DocumentModel::Entry with absolute file path in it
  // to get EditorManager::addNativeDirAndOpenWithActions() working
  DocumentModel::Entry fake_entry;
  IDocument document;
  document.setFilePath(file_path);
  fake_entry.document = &document;
  EditorManager::addNativeDirAndOpenWithActions(&menu, &fake_entry);

  if (has_current_item) {
    menu.addAction(ActionManager::command(ADDNEWFILE)->action());
    if (!is_dir)
      menu.addAction(ActionManager::command(REMOVEFILE)->action());
    if (m_file_system_model->flags(current) & Qt::ItemIsEditable)
      menu.addAction(ActionManager::command(RENAMEFILE)->action());
    new_folder = menu.addAction(tr("New Folder"));
  }

  menu.addSeparator();

  const auto collapse_all_action = menu.addAction(tr("Collapse All"));
  const auto action = menu.exec(ev->globalPos());

  if (!action)
    return;

  ev->accept();

  if (action == action_open_file) {
    openItem(current);
  } else if (action == new_folder) {
    if (is_dir)
      createNewFolder(current);
    else
      createNewFolder(current.parent());
  } else if (action == collapse_all_action) {
    m_list_view->collapseAll();
  }
}

auto FolderNavigationWidget::rootAutoSynchronization() const -> bool
{
  return m_root_auto_sync;
}

auto FolderNavigationWidget::setHiddenFilesFilter(const bool filter) const -> void
{
  auto filters = m_file_system_model->filter();

  if (filter)
    filters |= QDir::Hidden;
  else
    filters &= ~QDir::Hidden;

  m_file_system_model->setFilter(filters);
  m_filter_hidden_files_action->setChecked(filter);
}

auto FolderNavigationWidget::hiddenFilesFilter() const -> bool
{
  return m_filter_hidden_files_action->isChecked();
}

auto FolderNavigationWidget::isShowingBreadCrumbs() const -> bool
{
  return m_show_bread_crumbs_action->isChecked();
}

auto FolderNavigationWidget::isShowingFoldersOnTop() const -> bool
{
  return m_show_folders_on_top_action->isChecked();
}

FolderNavigationWidgetFactory::FolderNavigationWidgetFactory()
{
  m_instance = this;
  setDisplayName(tr("File System"));
  setPriority(400);
  setId("File System");
  setActivationSequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Y,Meta+F") : tr("Alt+Y,Alt+F")));
  insertRootDirectory({QLatin1String("A.Computer"), 0 /*sortValue*/, FolderNavigationWidget::tr("Computer"), FilePath(), Icons::DESKTOP_DEVICE_SMALL.icon()});
  insertRootDirectory({QLatin1String("A.Home"), 10 /*sortValue*/, FolderNavigationWidget::tr("Home"), FilePath::fromString(QDir::homePath()), Utils::Icons::HOME.icon()});
  updateProjectsDirectoryRoot();
  connect(DocumentManager::instance(), &DocumentManager::projectsDirectoryChanged, this, &FolderNavigationWidgetFactory::updateProjectsDirectoryRoot);
  registerActions();
}

auto FolderNavigationWidgetFactory::createWidget() -> NavigationView
{
  const auto fnw = new FolderNavigationWidget;

  for (const auto &root : qAsConst(m_root_directories))
    fnw->insertRootDirectory(root);

  connect(this, &FolderNavigationWidgetFactory::rootDirectoryAdded, fnw, &FolderNavigationWidget::insertRootDirectory);
  connect(this, &FolderNavigationWidgetFactory::rootDirectoryRemoved, fnw, &FolderNavigationWidget::removeRootDirectory);

  if (!EditorManager::currentDocument() && !m_fallback_sync_file_path.isEmpty())
    fnw->syncWithFilePath(m_fallback_sync_file_path);

  NavigationView n;
  n.widget = fnw;

  const auto filter = new QToolButton;
  filter->setIcon(Utils::Icons::FILTER.icon());
  filter->setToolTip(tr("Options"));
  filter->setPopupMode(QToolButton::InstantPopup);
  filter->setProperty("noArrow", true);

  const auto filter_menu = new QMenu(filter);
  filter_menu->addAction(fnw->m_filter_hidden_files_action);
  filter_menu->addAction(fnw->m_show_bread_crumbs_action);
  filter_menu->addAction(fnw->m_show_folders_on_top_action);

  filter->setMenu(filter_menu);
  n.dock_tool_bar_widgets << filter << fnw->m_toggle_sync;
  return n;
}

const bool kHiddenFilesDefault = false;
const bool kAutoSyncDefault = true;
const bool kShowBreadCrumbsDefault = true;
const bool kRootAutoSyncDefault = true;
const bool kShowFoldersOnTopDefault = true;

auto FolderNavigationWidgetFactory::saveSettings(QtcSettings *settings, const int position, QWidget *widget) -> void
{
  const auto fnw = qobject_cast<FolderNavigationWidget*>(widget);
  QTC_ASSERT(fnw, return);
  const QString base = kSettingsBase + QString::number(position);

  settings->setValueWithDefault(base + kHiddenFilesKey, fnw->hiddenFilesFilter(), kHiddenFilesDefault);
  settings->setValueWithDefault(base + kSyncKey, fnw->autoSynchronization(), kAutoSyncDefault);
  settings->setValueWithDefault(base + kShowBreadCrumbs, fnw->isShowingBreadCrumbs(), kShowBreadCrumbsDefault);
  settings->setValueWithDefault(base + kSyncRootWithEditor, fnw->rootAutoSynchronization(), kRootAutoSyncDefault);
  settings->setValueWithDefault(base + kShowFoldersOnTop, fnw->isShowingFoldersOnTop(), kShowFoldersOnTopDefault);
}

auto FolderNavigationWidgetFactory::restoreSettings(QSettings *settings, const int position, QWidget *widget) -> void
{
  const auto fnw = qobject_cast<FolderNavigationWidget*>(widget);
  QTC_ASSERT(fnw, return);
  const QString base = kSettingsBase + QString::number(position);

  fnw->setHiddenFilesFilter(settings->value(base + kHiddenFilesKey, kHiddenFilesDefault).toBool());
  fnw->setAutoSynchronization(settings->value(base + kSyncKey, kAutoSyncDefault).toBool());
  fnw->setShowBreadCrumbs(settings->value(base + kShowBreadCrumbs, kShowBreadCrumbsDefault).toBool());
  fnw->setRootAutoSynchronization(settings->value(base + kSyncRootWithEditor, kRootAutoSyncDefault).toBool());
  fnw->setShowFoldersOnTop(settings->value(base + kShowFoldersOnTop, kShowFoldersOnTopDefault).toBool());
}

auto FolderNavigationWidgetFactory::insertRootDirectory(const RootDirectory &directory) -> void
{
  if (const auto index = rootIndex(directory.id); index < 0)
    m_root_directories.append(directory);
  else
    m_root_directories[index] = directory;

  emit m_instance->rootDirectoryAdded(directory);
}

auto FolderNavigationWidgetFactory::removeRootDirectory(const QString &id) -> void
{
  const auto index = rootIndex(id);
  QTC_ASSERT(index >= 0, return);
  m_root_directories.removeAt(index);
  emit m_instance->rootDirectoryRemoved(id);
}

auto FolderNavigationWidgetFactory::setFallbackSyncFilePath(const FilePath &file_path) -> void
{
  m_fallback_sync_file_path = file_path;
}

auto FolderNavigationWidgetFactory::rootIndex(const QString &id) -> int
{
  return indexOf(m_root_directories, [id](const RootDirectory &entry) { return entry.id == id; });
}

auto FolderNavigationWidgetFactory::updateProjectsDirectoryRoot() -> void
{
  insertRootDirectory({QLatin1String(PROJECTSDIRECTORYROOT_ID), 20 /*sortValue*/, FolderNavigationWidget::tr("Projects"), DocumentManager::projectsDirectory(), Utils::Icons::PROJECT.icon()});
}

static auto currentFolderNavigationWidget() -> FolderNavigationWidget*
{
  return qobject_cast<FolderNavigationWidget*>(ICore::currentContextWidget());
}

auto FolderNavigationWidgetFactory::registerActions() -> void
{
  const Context context(C_FOLDERNAVIGATIONWIDGET);

  const auto add = new QAction(tr("Add New..."), this);
  ActionManager::registerAction(add, ADDNEWFILE, context);

  connect(add, &QAction::triggered, ICore::instance(), [] {
    if (const auto nav_widget = currentFolderNavigationWidget())
      nav_widget->addNewItem();
  });

  const auto rename = new QAction(tr("Rename..."), this);
  ActionManager::registerAction(rename, RENAMEFILE, context);

  connect(rename, &QAction::triggered, ICore::instance(), [] {
    if (const auto nav_widget = currentFolderNavigationWidget())
      nav_widget->editCurrentItem();
  });

  const auto remove = new QAction(tr("Remove..."), this);
  ActionManager::registerAction(remove, REMOVEFILE, context);

  connect(remove, &QAction::triggered, ICore::instance(), [] {
    if (const auto nav_widget = currentFolderNavigationWidget())
      nav_widget->removeCurrentItem();
  });
}

auto DelayedFileCrumbLabel::immediateHeightForWidth(int w) const -> int
{
  return FileCrumbLabel::heightForWidth(w);
}

auto DelayedFileCrumbLabel::heightForWidth(int w) const -> int
{
  static QHash<int, int> old_height;
  setScrollBarOnce();
  auto new_height = FileCrumbLabel::heightForWidth(w);

  if (!m_delaying || !old_height.contains(w)) {
    old_height.insert(w, new_height);
  } else if (old_height.value(w) != new_height) {
    static constexpr auto double_default_interval = 800;
    auto that = const_cast<DelayedFileCrumbLabel*>(this);
    QTimer::singleShot(std::max(2 * QApplication::doubleClickInterval(), double_default_interval), that, [that, w, new_height] {
      old_height.insert(w, new_height);
      that->m_delaying = false;
      that->updateGeometry();
    });
  }

  return old_height.value(w);
}

auto DelayedFileCrumbLabel::delayLayoutOnce() -> void
{
  m_delaying = true;
}

auto DelayedFileCrumbLabel::setScrollBarOnce(QScrollBar *bar, const int value) -> void
{
  m_bar = bar;
  m_bar_value = value;
}

auto DelayedFileCrumbLabel::setScrollBarOnce() const -> void
{
  if (!m_bar)
    return;

  const auto that = const_cast<DelayedFileCrumbLabel*>(this);

  that->m_bar->setValue(m_bar_value);
  that->m_bar.clear();
}

} // namespace ProjectExplorer
