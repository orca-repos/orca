// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "documentmodel.hpp"
#include "editorarea.hpp"
#include "editormanager.hpp"
#include "editorview.hpp"
#include "ieditor.hpp"
#include "ieditorfactory.hpp"

#include <core/idocument.hpp>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QAction;
class QSettings;
class QTimer;
QT_END_NAMESPACE

namespace Utils {
class QtcSettings;
}

namespace Core {

class EditorManager;

namespace Internal {

class EditorWindow;
class MainWindow;
class OpenEditorsViewFactory;
class OpenEditorsWindow;

enum make_writable_result {
  opened_with_version_control,
  made_writable,
  saved_as,
  failed
};

class EditorManagerPrivate final : public QObject {
  Q_OBJECT
  friend class EditorManager;

public:
  enum class close_flag {
    close_with_asking,
    close_without_asking,
    suspend
  };

  static auto instance() -> EditorManagerPrivate*;
  static auto extensionsInitialized() -> void; // only use from MainWindow
  static auto mainEditorArea() -> EditorArea*;
  static auto currentEditorView() -> EditorView*;
  static auto setCurrentEditor(IEditor *editor, bool ignore_navigation_history = false) -> void;
  static auto openEditor(EditorView *view, const Utils::FilePath &file_path, Utils::Id editor_id = {}, EditorManager::OpenEditorFlags flags = EditorManager::NoFlags, bool *new_editor = nullptr) -> IEditor*;
  static auto openEditorAt(EditorView *view, const Utils::Link &link, Utils::Id editor_id = {}, EditorManager::OpenEditorFlags flags = EditorManager::NoFlags, bool *new_editor = nullptr) -> IEditor*;
  static auto openEditorWith(const Utils::FilePath &file_path, Utils::Id editor_id) -> IEditor*;
  static auto duplicateEditor(IEditor *editor) -> IEditor*;
  static auto activateEditor(EditorView *view, IEditor *editor, EditorManager::OpenEditorFlags flags = EditorManager::NoFlags) -> IEditor*;
  static auto activateEditorForDocument(EditorView *view, IDocument *document, EditorManager::OpenEditorFlags flags = {}) -> IEditor*;
  static auto activateEditorForEntry(EditorView *view, DocumentModel::Entry *entry, EditorManager::OpenEditorFlags flags = EditorManager::NoFlags) -> bool;
  /* closes the document if there is no other editor on the document visible */
  static auto closeEditorOrDocument(IEditor *editor) -> void;
  static auto closeEditors(const QList<IEditor*> &editors, close_flag flag) -> bool;
  static auto viewForEditor(const IEditor *editor) -> EditorView*;
  static auto setCurrentView(EditorView *view) -> void;
  static auto activateView(EditorView *view) -> void;
  static auto makeFileWritable(IDocument *document) -> make_writable_result;
  static auto doEscapeKeyFocusMoveMagic() -> void;
  static auto getOpenWithEditorId(const Utils::FilePath &file_name, bool *is_external_editor = nullptr) -> Utils::Id;
  static auto saveSettings() -> void;
  static auto readSettings() -> void;
  static auto readFileSystemSensitivity(const QSettings *settings) -> Qt::CaseSensitivity;
  static auto writeFileSystemSensitivity(Utils::QtcSettings *settings, Qt::CaseSensitivity sensitivity) -> void;
  static auto setAutoSaveEnabled(bool enabled) -> void;
  static auto autoSaveEnabled() -> bool;
  static auto setAutoSaveInterval(int interval) -> void;
  static auto autoSaveInterval() -> int;
  static auto setAutoSaveAfterRefactoring(bool enabled) -> void;
  static auto autoSaveAfterRefactoring() -> bool;
  static auto setAutoSuspendEnabled(bool enabled) -> void;
  static auto autoSuspendEnabled() -> bool;
  static auto setAutoSuspendMinDocumentCount(int count) -> void;
  static auto autoSuspendMinDocumentCount() -> int;
  static auto setWarnBeforeOpeningBigFilesEnabled(bool enabled) -> void;
  static auto warnBeforeOpeningBigFilesEnabled() -> bool;
  static auto setBigFileSizeLimit(int limit_in_mb) -> void;
  static auto bigFileSizeLimit() -> int;
  static auto setMaxRecentFiles(int count) -> void;
  static auto maxRecentFiles() -> int;
  static auto createEditorWindow() -> EditorWindow*;
  static auto splitNewWindow(const EditorView *view) -> void;
  static auto closeView(EditorView *view) -> void;
  static auto emptyView(EditorView *view) -> QList<IEditor*>;
  static auto deleteEditors(const QList<IEditor*> &editors) -> void;
  static auto updateActions() -> void;
  static auto updateWindowTitleForDocument(const IDocument *document, const QWidget *window) -> void;
  static auto vcsOpenCurrentEditor() -> void;
  static auto makeCurrentEditorWritable() -> void;
  static auto setPlaceholderText(const QString &text) -> void;
  static auto placeholderText() -> QString;

public slots:
  static auto saveDocument(IDocument *document) -> bool;
  static auto saveDocumentAs(IDocument *document) -> bool;
  static auto split(Qt::Orientation orientation) -> void;
  static auto removeAllSplits() -> void;
  static auto gotoPreviousSplit() -> void;
  static auto gotoNextSplit() -> void;
  auto handleDocumentStateChange() const -> void;
  auto editorAreaDestroyed(const QObject *area) const -> void;

signals:
  auto placeholderTextChanged(const QString &text) -> void;

private:
  static auto gotoNextDocHistory() -> void;
  static auto gotoPreviousDocHistory() -> void;
  static auto gotoLastEditLocation() -> void;
  static auto autoSave() -> void;
  static auto handleContextChange(const QList<IContext*> &context) -> void;
  static auto copyFilePathFromContextMenu() -> void;
  auto copyLocationFromContextMenu() const -> void;
  static auto copyFileNameFromContextMenu() -> void;
  static auto saveDocumentFromContextMenu() -> void;
  static auto saveDocumentAsFromContextMenu() -> void;
  static auto revertToSavedFromContextMenu() -> void;
  static auto closeEditorFromContextMenu() -> void;
  static auto closeOtherDocumentsFromContextMenu() -> void;
  static auto closeAllEditorsExceptVisible() -> void;
  static auto revertToSaved(IDocument *document) -> void;
  static auto autoSuspendDocuments() -> void;
  static auto openTerminal() -> void;
  static auto findInDirectory() -> void;
  static auto togglePinned() -> void;
  static auto removeCurrentSplit() -> void;
  static auto setCurrentEditorFromContextChange() -> void;
  static auto windowPopup() -> OpenEditorsWindow*;
  static auto showPopupOrSelectDocument() -> void;
  static auto findFactories(Utils::Id editor_id, const Utils::FilePath &file_path) -> editor_factory_list;
  static auto createEditor(const IEditorFactory *factory, const Utils::FilePath &file_path) -> IEditor*;
  static auto addEditor(IEditor *editor) -> void;
  static auto removeEditor(IEditor *editor, bool removeSusependedEntry) -> void;
  static auto placeEditor(EditorView *view, IEditor *editor) -> IEditor*;
  static auto restoreEditorState(IEditor *editor) -> void;
  static auto visibleDocumentsCount() -> int;
  static auto findEditorArea(const EditorView *view, int *area_index = nullptr) -> EditorArea*;
  static auto pickUnusedEditor(EditorView **found_view = nullptr) -> IEditor*;
  static auto addDocumentToRecentFiles(IDocument *document) -> void;
  static auto updateAutoSave() -> void;
  static auto updateMakeWritableWarning() -> void;
  static auto setupSaveActions(const IDocument *document, QAction *save_action, QAction *save_as_action, QAction *revert_to_saved_action) -> void;
  static auto updateWindowTitle() -> void;
  static auto skipOpeningBigTextFile(const Utils::FilePath &file_path) -> bool;
  
  explicit EditorManagerPrivate(QObject *parent);
  ~EditorManagerPrivate() override;

  auto init() -> void;

  EditLocation m_global_last_edit_location;
  QList<EditLocation> m_global_history;
  QList<EditorArea*> m_editor_areas;
  QPointer<IEditor> m_current_editor;
  QPointer<IEditor> m_scheduled_current_editor;
  QPointer<EditorView> m_current_view;
  QTimer *m_auto_save_timer = nullptr;

  // actions
  QAction *m_revert_to_saved_action;
  QAction *m_save_action;
  QAction *m_save_as_action;
  QAction *m_close_current_editor_action;
  QAction *m_close_all_editors_action;
  QAction *m_close_other_documents_action;
  QAction *m_close_all_editors_except_visible_action;
  QAction *m_goto_next_doc_history_action;
  QAction *m_goto_previous_doc_history_action;
  QAction *m_go_back_action;
  QAction *m_go_forward_action;
  QAction *m_goto_last_edit_action;
  QAction *m_split_action{};
  QAction *m_split_side_by_side_action{};
  QAction *m_split_new_window_action{};
  QAction *m_remove_current_split_action{};
  QAction *m_remove_all_splits_action{};
  QAction *m_goto_previous_split_action{};
  QAction *m_goto_next_split_action{};
  QAction *m_copy_file_path_context_action;
  QAction *m_copy_location_context_action; // Copy path and line number.
  QAction *m_copy_file_name_context_action;
  QAction *m_save_current_editor_context_action;
  QAction *m_save_as_current_editor_context_action;
  QAction *m_revert_to_saved_current_editor_context_action;
  QAction *m_close_current_editor_context_action;
  QAction *m_close_all_editors_context_action;
  QAction *m_close_other_documents_context_action;
  QAction *m_close_all_editors_except_visible_context_action;
  QAction *m_open_graphical_shell_action;
  QAction *m_open_graphical_shell_context_action;
  QAction *m_show_in_file_system_view_action;
  QAction *m_show_in_file_system_view_context_action;
  QAction *m_open_terminal_action;
  QAction *m_find_in_directory_action;
  QAction *m_file_properties_action = nullptr;
  QAction *m_pin_action = nullptr;
  DocumentModel::Entry *m_context_menu_entry = nullptr;
  IEditor *m_context_menu_editor = nullptr;
  OpenEditorsWindow *m_window_popup = nullptr;
  QMap<QString, QVariant> m_editor_states;
  OpenEditorsViewFactory *m_open_editors_factory = nullptr;
  EditorManager::window_title_handler m_title_addition_handler;
  EditorManager::window_title_handler m_session_title_handler;
  EditorManager::window_title_handler m_title_vcs_topic_handler;

  struct Settings {
    IDocument::ReloadSetting reload_setting = IDocument::AlwaysAsk;
    bool auto_save_enabled = true;
    int auto_save_interval = 5;
    bool auto_suspend_enabled = true;
    int auto_suspend_min_document_count = 30;
    bool auto_save_after_refactoring = true;
    bool warn_before_opening_big_files_enabled = true;
    int big_file_size_limit_in_mb = 5;
    int max_recent_files = 8;
  };

  Settings m_settings;
  QString m_placeholder_text;
  QList<std::function<bool(IEditor *)>> m_close_editor_listeners;
};

} // Internal
} // Core
