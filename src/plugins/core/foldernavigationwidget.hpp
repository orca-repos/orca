// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"
#include "inavigationwidgetfactory.hpp"

#include <utils/fileutils.hpp>

#include <QIcon>
#include <QWidget>

namespace Core {
class IContext;
class IEditor;
}

namespace Utils {
class NavigationTreeView;
class FileCrumbLabel;
class QtcSettings;
}

QT_BEGIN_NAMESPACE
class QAction;
class QComboBox;
class QFileSystemModel;
class QMenu;
class QModelIndex;
class QSortFilterProxyModel;
QT_END_NAMESPACE

namespace Core {

namespace Internal {
class DelayedFileCrumbLabel;
} // namespace Internal

class CORE_EXPORT FolderNavigationWidgetFactory final : public Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  struct RootDirectory {
    QString id;
    int sort_value;
    QString display_name;
    Utils::FilePath path;
    QIcon icon;
  };

  FolderNavigationWidgetFactory();

  static auto instance()->FolderNavigationWidgetFactory*;
  auto createWidget() -> Core::NavigationView override;
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;
  static auto insertRootDirectory(const RootDirectory &directory) -> void;
  static auto removeRootDirectory(const QString &id) -> void;
  static auto setFallbackSyncFilePath(const Utils::FilePath &file_path) -> void;
  static auto fallbackSyncFilePath() -> const Utils::FilePath&;

signals:
  auto rootDirectoryAdded(const RootDirectory &directory) -> void;
  auto rootDirectoryRemoved(const QString &id) -> void;
  auto aboutToShowContextMenu(QMenu *menu, const Utils::FilePath &file_path, bool is_dir) -> void;
  auto fileRenamed(const Utils::FilePath &before, const Utils::FilePath &after) -> void;
  auto aboutToRemoveFile(const Utils::FilePath &file_path) -> void;

private:
  static auto rootIndex(const QString &id) -> int;
  static auto updateProjectsDirectoryRoot() -> void;
  auto registerActions() -> void;

  static QVector<RootDirectory> m_root_directories;
  static Utils::FilePath m_fallback_sync_file_path;
};

class CORE_EXPORT FolderNavigationWidget final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(bool autoSynchronization READ autoSynchronization WRITE setAutoSynchronization)

public:
  explicit FolderNavigationWidget(QWidget *parent = nullptr);

  auto autoSynchronization() const -> bool;
  auto hiddenFilesFilter() const -> bool;
  auto isShowingBreadCrumbs() const -> bool;
  auto isShowingFoldersOnTop() const -> bool;
  auto setAutoSynchronization(bool sync) -> void;
  auto toggleAutoSynchronization() -> void;
  auto setShowBreadCrumbs(bool show) const -> void;
  auto setShowFoldersOnTop(bool on_top) const -> void;
  auto insertRootDirectory(const FolderNavigationWidgetFactory::RootDirectory &directory) -> void;
  auto removeRootDirectory(const QString &id) -> void;
  auto addNewItem() -> void;
  auto editCurrentItem() const -> void;
  auto removeCurrentItem() const -> void;
  auto syncWithFilePath(const Utils::FilePath &file_path) -> void;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

protected:
  auto contextMenuEvent(QContextMenuEvent *ev) -> void override;

private slots:
  auto setCrumblePath(const Utils::FilePath &file_path) const -> void;

private:
  auto rootAutoSynchronization() const -> bool;
  auto setRootAutoSynchronization(bool sync) -> void;
  auto setHiddenFilesFilter(bool filter) const -> void;
  auto selectBestRootForFile(const Utils::FilePath &file_path) -> void;
  auto handleCurrentEditorChanged(const Core::IEditor *editor) -> void;
  auto selectFile(const Utils::FilePath &file_path) -> void;
  auto setRootDirectory(const Utils::FilePath &directory) const -> void;
  auto bestRootForFile(const Utils::FilePath &file_path) const -> int;
  auto openItem(const QModelIndex &index) const -> void;
  auto createNewFolder(const QModelIndex &parent) const -> void;

  Utils::NavigationTreeView *m_list_view = nullptr;
  QFileSystemModel *m_file_system_model = nullptr;
  QSortFilterProxyModel *m_sort_proxy_model = nullptr;
  QAction *m_filter_hidden_files_action = nullptr;
  QAction *m_show_bread_crumbs_action = nullptr;
  QAction *m_show_folders_on_top_action = nullptr;
  bool m_auto_sync = false;
  bool m_root_auto_sync = true;
  QToolButton *m_toggle_sync = nullptr;
  QToolButton *m_toggle_root_sync = nullptr;
  QComboBox *m_root_selector = nullptr;
  QWidget *m_crumb_container = nullptr;
  Internal::DelayedFileCrumbLabel *m_crumb_label = nullptr;

  // FolderNavigationWidgetFactory needs private members to build a menu
  friend class FolderNavigationWidgetFactory;
};

} // namespace Core
