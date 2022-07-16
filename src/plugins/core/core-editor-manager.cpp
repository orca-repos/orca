// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-editor-manager.hpp"

#include "core-action-container.hpp"
#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-constants.hpp"
#include "core-diff-service.hpp"
#include "core-document-manager.hpp"
#include "core-document-model-private.hpp"
#include "core-document-model.hpp"
#include "core-editor-factory-interface.hpp"
#include "core-editor-factory-private-interface.hpp"
#include "core-editor-interface.hpp"
#include "core-editor-manager-private.hpp"
#include "core-editor-view.hpp"
#include "core-editor-window.hpp"
#include "core-external-editor-interface.hpp"
#include "core-file-utils.hpp"
#include "core-find-placeholder.hpp"
#include "core-interface.hpp"
#include "core-open-editors-view.hpp"
#include "core-open-editors-window.hpp"
#include "core-open-with-dialog.hpp"
#include "core-output-pane-manager.hpp"
#include "core-output-pane.hpp"
#include "core-readonly-files-dialog.hpp"
#include "core-right-pane.hpp"
#include "core-search-result-item.hpp"
#include "core-settings-database.hpp"
#include "core-vcs-manager.hpp"
#include "core-version-control-interface.hpp"

#include <app/app_version.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/infobar.hpp>
#include <utils/link.hpp>
#include <utils/macroexpander.hpp>
#include <utils/overridecursor.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/utilsicons.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/mimetypes/mimetype.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QTextCodec>
#include <QTimer>

#include <utility>

#if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)
#include <core/core-plugin.hpp>
#include <QTest>
#endif

enum {
  debug_editor_manager=0
};

static constexpr char k_current_document_prefix[] = "CurrentDocument";
static constexpr char k_current_document_x_pos[] = "CurrentDocument:XPos";
static constexpr char k_current_document_y_pos[] = "CurrentDocument:YPos";
static constexpr char k_make_writable_warning[] = "Core.EditorManager.MakeWritable";
static constexpr char document_states_key[] = "EditorManager/DocumentStates";
static constexpr char reload_behavior_key[] = "EditorManager/ReloadBehavior";
static constexpr char auto_save_enabled_key[] = "EditorManager/AutoSaveEnabled";
static constexpr char auto_save_interval_key[] = "EditorManager/AutoSaveInterval";
static constexpr char auto_save_after_refactoring_key[] = "EditorManager/AutoSaveAfterRefactoring";
static constexpr char auto_suspend_enabled_key[] = "EditorManager/AutoSuspendEnabled";
static constexpr char auto_suspend_min_document_count_key[] = "EditorManager/AutoSuspendMinDocuments";
static constexpr char warn_before_opening_big_text_files_key[] = "EditorManager/WarnBeforeOpeningBigTextFiles";
static constexpr char big_text_file_size_limit_key[] = "EditorManager/BigTextFileSizeLimitInMB";
static constexpr char max_recent_files_key[] = "EditorManager/MaxRecentFiles";
static constexpr char file_system_case_sensitivity_key[] = "Core/FileSystemCaseSensitivity";
static constexpr char preferred_editor_factories_key[] = "EditorManager/PreferredEditorFactories";
static constexpr char scratch_buffer_key[] = "_q_emScratchBuffer";

// for lupdate

namespace Orca::Plugin::Core {

using namespace Utils;

static auto checkEditorFlags(const EditorManager::OpenEditorFlags flags) -> void
{
  if (flags & EditorManager::OpenInOtherSplit) {
    QTC_CHECK(!(flags & EditorManager::SwitchSplitIfAlreadyVisible));
    QTC_CHECK(!(flags & EditorManager::AllowExternalEditor));
  }
}

//===================EditorManager=====================

/*!
    \class Orca::Plugin::Core::EditorManagerPlaceHolder
    \inheaderfile coreplugin/editormanager/editormanager.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The EditorManagerPlaceHolder class is used to integrate an editor
    area into a \l{Orca::Plugin::Core::IMode}{mode}.

    Create an instance of EditorManagerPlaceHolder and integrate it into your
    mode widget's layout, to add the main editor area into your mode. The best
    place for the editor area is the central widget of a QMainWindow.
    Examples are the Edit and Debug modes.
*/

/*!
    Creates an EditorManagerPlaceHolder with the specified \a parent.
*/
EditorManagerPlaceHolder::EditorManagerPlaceHolder(QWidget *parent) : QWidget(parent)
{
  setLayout(new QVBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  setFocusProxy(EditorManagerPrivate::mainEditorArea());
}

/*!
    \internal
*/
EditorManagerPlaceHolder::~EditorManagerPlaceHolder()
{
  // EditorManager will be deleted in ~MainWindow()
  if (QWidget *em = EditorManagerPrivate::mainEditorArea(); em && em->parent() == this) {
    em->hide();
    em->setParent(nullptr);
  }
}

/*!
    \internal
*/
auto EditorManagerPlaceHolder::showEvent(QShowEvent *) -> void
{
  QWidget *previous_focus = nullptr;
  QWidget *em = EditorManagerPrivate::mainEditorArea();

  if (em->focusWidget() && em->focusWidget()->hasFocus())
    previous_focus = em->focusWidget();

  layout()->addWidget(em);
  em->show();

  if (previous_focus)
    previous_focus->setFocus();
}

// ---------------- EditorManager

/*!
    \class Orca::Plugin::Core::EditorManager
    \inheaderfile coreplugin/editormanager/editormanager.h
    \inmodule Orca

    \brief The EditorManager class manages the editors created for files
    according to their MIME type.

    Whenever a user wants to edit or create a file, the EditorManager scans all
    IEditorFactory interfaces for suitable editors. The selected IEditorFactory
    is then asked to create an editor, as determined by the MIME type of the
    file.

    Users can split the editor view or open the editor in a new window when
    to work on and view multiple files on the same screen or on multiple
    screens. For more information, see
    \l{https://doc.qt.io/orca/creator-coding-navigating.html#splitting-the-editor-view}
    {Splitting the Editor View}.

    Plugins use the EditorManager to open documents in editors or close them,
    and to get notified when documents are opened, closed or saved.
*/

/*!
    \enum Orca::Plugin::Core::MakeWritableResult
    \internal

    This enum specifies whether the document has successfully been made writable.

    \value OpenedWithVersionControl
           The document was opened under version control.
    \value MadeWritable
           The document was made writable.
    \value SavedAs
           The document was saved under another name.
    \value Failed
           The document cannot be made writable.
*/

/*!
    \enum EditorManager::OpenEditorFlag

    This enum specifies settings for opening a file in an editor.

    \value NoFlags
           Does not use any settings.
    \value DoNotChangeCurrentEditor
           Does not switch focus to the newly opened editor.
    \value IgnoreNavigationHistory
           Does not add an entry to the navigation history for the
           opened editor.
    \value DoNotMakeVisible
           Does not force the editor to become visible.
    \value OpenInOtherSplit
           Opens the document in another split of the window.
    \value DoNotSwitchToDesignMode
           Opens the document in the current mode.
    \value DoNotSwitchToEditMode
           Opens the document in the current mode.
    \value SwitchSplitIfAlreadyVisible
           Switches to another split if the document is already
           visible there.
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::currentEditorChanged(Orca::Plugin::Core::IEditor *editor)

    This signal is emitted after the current editor changed to \a editor.
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::currentDocumentStateChanged()

    This signal is emitted when the meta data of the current document, for
    example file name or modified state, changed.

    \sa IDocument::changed()
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::documentStateChanged(Orca::Plugin::Core::IDocument *document)

    This signal is emitted when the meta data of the \a document, for
    example file name or modified state, changed.

    \sa IDocument::changed()
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::editorCreated(Orca::Plugin::Core::IEditor *editor, const QString &fileName)

    This signal is emitted after an \a editor was created for \a fileName, but
    before it was opened in an editor view.
*/
/*!
    \fn void Orca::Plugin::Core::EditorManager::editorOpened(Orca::Plugin::Core::IEditor *editor)

    This signal is emitted after a new \a editor was opened in an editor view.

    Usually the more appropriate signal to listen to is documentOpened().
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::documentOpened(Orca::Plugin::Core::IDocument *document)

    This signal is emitted after the first editor for \a document opened in an
    editor view.
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::editorAboutToClose(Orca::Plugin::Core::IEditor *editor)

    This signal is emitted before \a editor is closed. This can be used to free
    resources that were allocated for the editor separately from the editor
    itself. It cannot be used to prevent the editor from closing. See
    addCloseEditorListener() for that.

    Usually the more appropriate signal to listen to is documentClosed().

    \sa addCloseEditorListener()
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::editorsClosed(QList<Orca::Plugin::Core::IEditor *> editors)

    This signal is emitted after the \a editors closed, but before they are
    deleted.

    Usually the more appropriate signal to listen to is documentClosed().
*/

/*!
    \fn void Orca::Plugin::Core::EditorManager::documentClosed(Orca::Plugin::Core::IDocument *document)

    This signal is emitted after the \a document closed, but before it is deleted.
*/
/*!
    \fn void EditorManager::findOnFileSystemRequest(const QString &path)

    \internal
*/
/*!
    \fn void Orca::Plugin::Core::EditorManager::aboutToSave(Orca::Plugin::Core::IDocument *document)

    This signal is emitted before the \a document is saved.
*/
/*!
    \fn void Orca::Plugin::Core::EditorManager::saved(Orca::Plugin::Core::IDocument *document)

    This signal is emitted after the \a document was saved.
*/
/*!
    \fn void Orca::Plugin::Core::EditorManager::autoSaved()

    This signal is emitted after auto-save was triggered.
*/
/*!
    \fn void Orca::Plugin::Core::EditorManager::currentEditorAboutToChange(Orca::Plugin::Core::IEditor *editor)

    This signal is emitted before the current editor changes to \a editor.
*/

static EditorManager *m_instance = nullptr;
static EditorManagerPrivate *d;

static auto autoSaveName(const FilePath &file_path) -> FilePath
{
  return file_path.stringAppended(".autosave");
}

static auto setFocusToEditorViewAndUnmaximizePanes(EditorView *view) -> void
{
  const auto editor = view->currentEditor();
  const auto target = editor ? editor->widget() : view;
  const auto focus = target->focusWidget();
  const auto w = focus ? focus : target;

  w->setFocus();
  ICore::raiseWindow(w);

  if (const auto holder = OutputPanePlaceHolder::getCurrent(); holder && holder->window() == view->window()) {
    // unmaximize output pane if necessary
    if (holder->isVisible() && holder->isMaximized())
      holder->setMaximized(false);
  }
}

EditorManagerPrivate::EditorManagerPrivate(QObject *parent) : QObject(parent),
                                                              m_revert_to_saved_action(new QAction(EditorManager::tr("Revert to Saved"), this)),
                                                              m_save_action(new QAction(this)), m_save_as_action(new QAction(this)),
                                                              m_close_current_editor_action(new QAction(EditorManager::tr("Close"), this)),
                                                              m_close_all_editors_action(new QAction(EditorManager::tr("Close All"), this)),
                                                              m_close_other_documents_action(new QAction(EditorManager::tr("Close Others"), this)),
                                                              m_close_all_editors_except_visible_action(new QAction(EditorManager::tr("Close All Except Visible"), this)),
                                                              m_goto_next_doc_history_action(new QAction(EditorManager::tr("Next Open Document in History"), this)),
                                                              m_goto_previous_doc_history_action(new QAction(EditorManager::tr("Previous Open Document in History"), this)),
                                                              m_go_back_action(new QAction(Icons::PREV.icon(), EditorManager::tr("Go Back"), this)),
                                                              m_go_forward_action(new QAction(Icons::NEXT.icon(), EditorManager::tr("Go Forward"), this)),
                                                              m_goto_last_edit_action(new QAction(EditorManager::tr("Go to Last Edit"), this)),
                                                              m_copy_file_path_context_action(new QAction(EditorManager::tr("Copy Full Path"), this)),
                                                              m_copy_location_context_action(new QAction(EditorManager::tr("Copy Path and Line Number"), this)),
                                                              m_copy_file_name_context_action(new QAction(EditorManager::tr("Copy File Name"), this)),
                                                              m_save_current_editor_context_action(new QAction(EditorManager::tr("&Save"), this)),
                                                              m_save_as_current_editor_context_action(new QAction(EditorManager::tr("Save &As..."), this)),
                                                              m_revert_to_saved_current_editor_context_action(new QAction(EditorManager::tr("Revert to Saved"), this)),
                                                              m_close_current_editor_context_action(new QAction(EditorManager::tr("Close"), this)),
                                                              m_close_all_editors_context_action(new QAction(EditorManager::tr("Close All"), this)),
                                                              m_close_other_documents_context_action(new QAction(EditorManager::tr("Close Others"), this)),
                                                              m_close_all_editors_except_visible_context_action(new QAction(EditorManager::tr("Close All Except Visible"), this)),
                                                              m_open_graphical_shell_action(new QAction(FileUtils::msgGraphicalShellAction(), this)),
                                                              m_open_graphical_shell_context_action(new QAction(FileUtils::msgGraphicalShellAction(), this)),
                                                              m_show_in_file_system_view_action(new QAction(FileUtils::msgFileSystemAction(), this)),
                                                              m_show_in_file_system_view_context_action(new QAction(FileUtils::msgFileSystemAction(), this)),
                                                              m_open_terminal_action(new QAction(FileUtils::msgTerminalHereAction(), this)),
                                                              m_find_in_directory_action(new QAction(FileUtils::msgFindInDirectory(), this)),
                                                              m_file_properties_action(new QAction(tr("Properties..."), this)),
                                                              m_pin_action(new QAction(tr("Pin"), this))
{
  d = this;
}

EditorManagerPrivate::~EditorManagerPrivate()
{
  if (ICore::instance())
    delete m_open_editors_factory;

  // close all extra windows
  for (const auto area : m_editor_areas) {
    disconnect(area, &QObject::destroyed, this, &EditorManagerPrivate::editorAreaDestroyed);
    delete area;
  }
  m_editor_areas.clear();

  DocumentModel::destroy();
  d = nullptr;
}

auto EditorManagerPrivate::init() -> void
{
  DocumentModel::init();

  connect(ICore::instance(), &ICore::contextAboutToChange, this, &EditorManagerPrivate::handleContextChange);
  connect(qApp, &QApplication::applicationStateChanged, this, [](Qt::ApplicationState state) {
    if (state == Qt::ApplicationActive)
      EditorManager::updateWindowTitles();
  });

  const Context edit_manager_context(C_EDITORMANAGER);
  const Context edit_design_context(C_EDITORMANAGER, C_DESIGN_MODE);   // combined context for edit & design modes

  const auto mfile = ActionManager::actionContainer(M_FILE);

  // Revert to saved
  m_revert_to_saved_action->setIcon(QIcon::fromTheme("document-revert"));
  auto cmd = ActionManager::registerAction(m_revert_to_saved_action, REVERTTOSAVED, edit_manager_context);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(tr("Revert File to Saved"));
  mfile->addAction(cmd, G_FILE_SAVE);
  connect(m_revert_to_saved_action, &QAction::triggered, m_instance, &EditorManager::revertToSaved);

  // Save Action
  ActionManager::registerAction(m_save_action, SAVE, edit_manager_context);
  connect(m_save_action, &QAction::triggered, m_instance, [] { EditorManager::saveDocument(); });

  // Save As Action
  ActionManager::registerAction(m_save_as_action, SAVEAS, edit_manager_context);
  connect(m_save_as_action, &QAction::triggered, m_instance, &EditorManager::saveDocumentAs);

  // Window Menu
  const auto mwindow = ActionManager::actionContainer(M_WINDOW);

  // Window menu separators
  mwindow->addSeparator(edit_manager_context, G_WINDOW_SPLIT);
  mwindow->addSeparator(edit_manager_context, G_WINDOW_NAVIGATE);

  // Close Action
  cmd = ActionManager::registerAction(m_close_current_editor_action, CLOSE, edit_manager_context, true);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+W")));
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(m_close_current_editor_action->text());
  mfile->addAction(cmd, G_FILE_CLOSE);
  connect(m_close_current_editor_action, &QAction::triggered, m_instance, &EditorManager::slotCloseCurrentEditorOrDocument);

  if constexpr (HostOsInfo::isWindowsHost()) {
    // workaround for ORCABUG-72
    const auto action = new QAction(tr("Alternative Close"), this);
    cmd = ActionManager::registerAction(action, CLOSE_ALTERNATIVE, edit_manager_context);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+F4")));
    cmd->setDescription(EditorManager::tr("Close"));
    connect(action, &QAction::triggered, m_instance, &EditorManager::slotCloseCurrentEditorOrDocument);
  }

  // Close All Action
  cmd = ActionManager::registerAction(m_close_all_editors_action, CLOSEALL, edit_manager_context, true);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+W")));
  mfile->addAction(cmd, G_FILE_CLOSE);
  connect(m_close_all_editors_action, &QAction::triggered, m_instance, &EditorManager::closeAllDocuments);

  // Close All Others Action
  cmd = ActionManager::registerAction(m_close_other_documents_action, CLOSEOTHERS, edit_manager_context, true);
  mfile->addAction(cmd, G_FILE_CLOSE);
  cmd->setAttribute(Command::CA_UpdateText);
  connect(m_close_other_documents_action, &QAction::triggered, m_instance, [] { EditorManager::closeOtherDocuments(); });

  // Close All Others Except Visible Action
  cmd = ActionManager::registerAction(m_close_all_editors_except_visible_action, CLOSEALLEXCEPTVISIBLE, edit_manager_context, true);
  mfile->addAction(cmd, G_FILE_CLOSE);
  connect(m_close_all_editors_except_visible_action, &QAction::triggered, this, &EditorManagerPrivate::closeAllEditorsExceptVisible);

  ActionManager::registerAction(m_open_graphical_shell_action, SHOWINGRAPHICALSHELL, edit_manager_context);
  connect(m_open_graphical_shell_action, &QAction::triggered, this, [] {
    if (!EditorManager::currentDocument())
      return;
    if (const auto fp = EditorManager::currentDocument()->filePath(); !fp.isEmpty())
      FileUtils::showInGraphicalShell(ICore::dialogParent(), fp);
  });

  ActionManager::registerAction(m_show_in_file_system_view_action, SHOWINFILESYSTEMVIEW, edit_manager_context);
  connect(m_show_in_file_system_view_action, &QAction::triggered, this, [] {
    if (!EditorManager::currentDocument())
      return;
    if (const auto fp = EditorManager::currentDocument()->filePath(); !fp.isEmpty())
      FileUtils::showInFileSystemView(fp);
  });

  //Save XXX Context Actions
  connect(m_copy_file_path_context_action, &QAction::triggered, this, &EditorManagerPrivate::copyFilePathFromContextMenu);
  connect(m_copy_location_context_action, &QAction::triggered, this, &EditorManagerPrivate::copyLocationFromContextMenu);
  connect(m_copy_file_name_context_action, &QAction::triggered, this, &EditorManagerPrivate::copyFileNameFromContextMenu);
  connect(m_save_current_editor_context_action, &QAction::triggered, this, &EditorManagerPrivate::saveDocumentFromContextMenu);
  connect(m_save_as_current_editor_context_action, &QAction::triggered, this, &EditorManagerPrivate::saveDocumentAsFromContextMenu);
  connect(m_revert_to_saved_current_editor_context_action, &QAction::triggered, this, &EditorManagerPrivate::revertToSavedFromContextMenu);

  // Close XXX Context Actions
  connect(m_close_all_editors_context_action, &QAction::triggered, m_instance, &EditorManager::closeAllDocuments);
  connect(m_close_current_editor_context_action, &QAction::triggered, this, &EditorManagerPrivate::closeEditorFromContextMenu);
  connect(m_close_other_documents_context_action, &QAction::triggered, this, &EditorManagerPrivate::closeOtherDocumentsFromContextMenu);
  connect(m_close_all_editors_except_visible_context_action, &QAction::triggered, this, &EditorManagerPrivate::closeAllEditorsExceptVisible);

  connect(m_open_graphical_shell_context_action, &QAction::triggered, this, [this] {
    if (!m_context_menu_entry || m_context_menu_entry->fileName().isEmpty())
      return;
    FileUtils::showInGraphicalShell(ICore::dialogParent(), m_context_menu_entry->fileName());
  });

  connect(m_show_in_file_system_view_context_action, &QAction::triggered, this, [this] {
    if (!m_context_menu_entry || m_context_menu_entry->fileName().isEmpty())
      return;
    FileUtils::showInFileSystemView(m_context_menu_entry->fileName());
  });

  connect(m_open_terminal_action, &QAction::triggered, this, &EditorManagerPrivate::openTerminal);
  connect(m_find_in_directory_action, &QAction::triggered, this, &EditorManagerPrivate::findInDirectory);

  connect(m_file_properties_action, &QAction::triggered, this, [] {
    if (!d->m_context_menu_entry || d->m_context_menu_entry->fileName().isEmpty())
      return;
    DocumentManager::showFilePropertiesDialog(d->m_context_menu_entry->fileName());
  });

  connect(m_pin_action, &QAction::triggered, this, &EditorManagerPrivate::togglePinned);

  // Goto Previous In History Action
  cmd = ActionManager::registerAction(m_goto_previous_doc_history_action, GOTOPREVINHISTORY, edit_design_context);

  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Tab")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Tab")));

  mwindow->addAction(cmd, G_WINDOW_NAVIGATE);
  connect(m_goto_previous_doc_history_action, &QAction::triggered, this, &EditorManagerPrivate::gotoPreviousDocHistory);

  // Goto Next In History Action
  cmd = ActionManager::registerAction(m_goto_next_doc_history_action, GOTONEXTINHISTORY, edit_design_context);

  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+Tab")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+Tab")));

  mwindow->addAction(cmd, G_WINDOW_NAVIGATE);
  connect(m_goto_next_doc_history_action, &QAction::triggered, this, &EditorManagerPrivate::gotoNextDocHistory);

  // Go back in navigation history
  cmd = ActionManager::registerAction(m_go_back_action, GO_BACK, edit_design_context);

  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+Left")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Left")));

  mwindow->addAction(cmd, G_WINDOW_NAVIGATE);
  connect(m_go_back_action, &QAction::triggered, m_instance, &EditorManager::goBackInNavigationHistory);

  // Go forward in navigation history
  cmd = ActionManager::registerAction(m_go_forward_action, GO_FORWARD, edit_design_context);

  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+Right")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Right")));

  mwindow->addAction(cmd, G_WINDOW_NAVIGATE);
  connect(m_go_forward_action, &QAction::triggered, m_instance, &EditorManager::goForwardInNavigationHistory);

  // Go to last edit
  cmd = ActionManager::registerAction(m_goto_last_edit_action, GOTOLASTEDIT, edit_design_context);
  mwindow->addAction(cmd, G_WINDOW_NAVIGATE);
  connect(m_goto_last_edit_action, &QAction::triggered, this, &EditorManagerPrivate::gotoLastEditLocation);

  m_split_action = new QAction(Icons::SPLIT_HORIZONTAL.icon(), tr("Split"), this);
  cmd = ActionManager::registerAction(m_split_action, SPLIT, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,2")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,2")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_split_action, &QAction::triggered, this, [] { split(Qt::Vertical); });

  m_split_side_by_side_action = new QAction(Icons::SPLIT_VERTICAL.icon(), tr("Split Side by Side"), this);
  cmd = ActionManager::registerAction(m_split_side_by_side_action, SPLIT_SIDE_BY_SIDE, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,3")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,3")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_split_side_by_side_action, &QAction::triggered, m_instance, &EditorManager::splitSideBySide);

  m_split_new_window_action = new QAction(tr("Open in New Window"), this);
  cmd = ActionManager::registerAction(m_split_new_window_action, SPLIT_NEW_WINDOW, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,4")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,4")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_split_new_window_action, &QAction::triggered, this, [] { splitNewWindow(currentEditorView()); });

  m_remove_current_split_action = new QAction(tr("Remove Current Split"), this);
  cmd = ActionManager::registerAction(m_remove_current_split_action, REMOVE_CURRENT_SPLIT, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,0")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,0")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_remove_current_split_action, &QAction::triggered, this, &EditorManagerPrivate::removeCurrentSplit);

  m_remove_all_splits_action = new QAction(tr("Remove All Splits"), this);
  cmd = ActionManager::registerAction(m_remove_all_splits_action, REMOVE_ALL_SPLITS, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,1")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,1")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_remove_all_splits_action, &QAction::triggered, this, &EditorManagerPrivate::removeAllSplits);

  m_goto_previous_split_action = new QAction(tr("Go to Previous Split or Window"), this);
  cmd = ActionManager::registerAction(m_goto_previous_split_action, GOTO_PREV_SPLIT, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,i")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,i")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_goto_previous_split_action, &QAction::triggered, this, &EditorManagerPrivate::gotoPreviousSplit);

  m_goto_next_split_action = new QAction(tr("Go to Next Split or Window"), this);
  cmd = ActionManager::registerAction(m_goto_next_split_action, GOTO_NEXT_SPLIT, edit_manager_context);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequence(QKeySequence(tr("Meta+E,o")));
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E,o")));
  mwindow->addAction(cmd, G_WINDOW_SPLIT);
  connect(m_goto_next_split_action, &QAction::triggered, this, &EditorManagerPrivate::gotoNextSplit);

  const auto medit = ActionManager::actionContainer(M_EDIT);
  const auto advanced_menu = ActionManager::createMenu(M_EDIT_ADVANCED);
  medit->addMenu(advanced_menu, G_EDIT_ADVANCED);
  advanced_menu->menu()->setTitle(tr("Ad&vanced"));
  advanced_menu->appendGroup(G_EDIT_FORMAT);
  advanced_menu->appendGroup(G_EDIT_TEXT);
  advanced_menu->appendGroup(G_EDIT_COLLAPSING);
  advanced_menu->appendGroup(G_EDIT_BLOCKS);
  advanced_menu->appendGroup(G_EDIT_FONT);
  advanced_menu->appendGroup(G_EDIT_EDITOR);

  // Advanced menu separators
  advanced_menu->addSeparator(edit_manager_context, G_EDIT_TEXT);
  advanced_menu->addSeparator(edit_manager_context, G_EDIT_COLLAPSING);
  advanced_menu->addSeparator(edit_manager_context, G_EDIT_BLOCKS);
  advanced_menu->addSeparator(edit_manager_context, G_EDIT_FONT);
  advanced_menu->addSeparator(edit_manager_context, G_EDIT_EDITOR);

  // other setup
  const auto main_editor_area = new EditorArea();
  // assign parent to avoid failing updates (e.g. windowTitle) before it is displayed first time
  main_editor_area->setParent(ICore::mainWindow());
  main_editor_area->hide();
  connect(main_editor_area, &EditorArea::windowTitleNeedsUpdate, this, &EditorManagerPrivate::updateWindowTitle);
  connect(main_editor_area, &QObject::destroyed, this, &EditorManagerPrivate::editorAreaDestroyed);
  d->m_editor_areas.append(main_editor_area);
  d->m_current_view = main_editor_area->view();
  updateActions();

  // The popup needs a parent to get keyboard focus.
  m_window_popup = new OpenEditorsWindow(main_editor_area);
  m_window_popup->hide();

  m_auto_save_timer = new QTimer(this);
  m_auto_save_timer->setObjectName("EditorManager::m_autoSaveTimer");
  connect(m_auto_save_timer, &QTimer::timeout, this, &EditorManagerPrivate::autoSave);
  updateAutoSave();

  d->m_open_editors_factory = new OpenEditorsViewFactory();

  globalMacroExpander()->registerFileVariables(k_current_document_prefix, tr("Current document"), [] {
    const auto document = EditorManager::currentDocument();
    return document ? document->filePath() : FilePath();
  });

  globalMacroExpander()->registerIntVariable(k_current_document_x_pos, tr("X-coordinate of the current editor's upper left corner, relative to screen."), []() -> int {
    const auto editor = EditorManager::currentEditor();
    return editor ? editor->widget()->mapToGlobal(QPoint(0, 0)).x() : 0;
  });

  globalMacroExpander()->registerIntVariable(k_current_document_y_pos, tr("Y-coordinate of the current editor's upper left corner, relative to screen."), []() -> int {
    const auto editor = EditorManager::currentEditor();
    return editor ? editor->widget()->mapToGlobal(QPoint(0, 0)).y() : 0;
  });
}

auto EditorManagerPrivate::extensionsInitialized() -> void
{
  // Do not ask for files to save.
  // MainWindow::closeEvent has already done that.
  ICore::addPreCloseListener([]() -> bool { return EditorManager::closeAllEditors(false); });
}

auto EditorManagerPrivate::instance() -> EditorManagerPrivate*
{
  return d;
}

auto EditorManagerPrivate::mainEditorArea() -> EditorArea*
{
  return d->m_editor_areas.at(0);
}

auto EditorManagerPrivate::skipOpeningBigTextFile(const FilePath &file_path) -> bool
{
  if (!d->m_settings.warn_before_opening_big_files_enabled)
    return false;

  if (!file_path.exists())
    return false;

  if (const auto mime_type = mimeTypeForFile(file_path); !mime_type.inherits("text/plain"))
    return false;

  const auto file_size = file_path.fileSize();

  if (const auto file_size_in_mb = static_cast<double>(file_size) / 1000.0 / 1000.0; file_size_in_mb > d->m_settings.big_file_size_limit_in_mb && file_size < EditorManager::maxTextFileSize()) {
    const auto title = EditorManager::tr("Continue Opening Huge Text File?");
    const auto text = EditorManager::tr("The text file \"%1\" has the size %2MB and might take more memory to open" " and process than available.\n" "\n" "Continue?").arg(file_path.fileName()).arg(file_size_in_mb, 0, 'f', 2);
    CheckableMessageBox message_box(ICore::dialogParent());
    message_box.setWindowTitle(title);
    message_box.setText(text);
    message_box.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    message_box.setDefaultButton(QDialogButtonBox::No);
    message_box.setIcon(QMessageBox::Question);
    message_box.setCheckBoxVisible(true);
    message_box.setCheckBoxText(CheckableMessageBox::msgDoNotAskAgain());
    message_box.exec();
    setWarnBeforeOpeningBigFilesEnabled(!message_box.isChecked());
    return message_box.clickedStandardButton() != QDialogButtonBox::Yes;
  }

  return false;
}

auto EditorManagerPrivate::openEditor(EditorView *view, const FilePath &file_path, const Id editor_id, const EditorManager::OpenEditorFlags flags, bool *new_editor) -> IEditor*
{
  if constexpr (debug_editor_manager)
    qDebug() << Q_FUNC_INFO << file_path << editor_id.name();

  if (file_path.isEmpty())
    return nullptr;

  if (new_editor)
    *new_editor = false;

  if (const auto editors = DocumentModel::editorsForFilePath(file_path); !editors.isEmpty()) {
    auto editor = editors.first();
    if (flags & EditorManager::SwitchSplitIfAlreadyVisible) {
      for (const auto ed : editors) {
        // Don't switch to a view where editor is not its current editor
        if (const auto v = viewForEditor(ed); v && v->currentEditor() == ed) {
          editor = ed;
          view = v;
          break;
        }
      }
    }
    return activateEditor(view, editor, flags);
  }

  if (skipOpeningBigTextFile(file_path))
    return nullptr;

  auto real_fp = autoSaveName(file_path);
  if (!file_path.exists() || !real_fp.exists() || file_path.lastModified() >= real_fp.lastModified()) {
    real_fp.removeFile();
    real_fp = file_path;
  }

  auto factories = EditorType::preferredEditorTypes(file_path);
  if (!(flags & EditorManager::AllowExternalEditor)) {
    factories = filtered(factories, [](EditorType *type) {
      return type->asEditorFactory() != nullptr;
    });
  }

  if (factories.isEmpty()) {
    const auto mime_type = mimeTypeForFile(file_path);
    QMessageBox msgbox(QMessageBox::Critical, EditorManager::tr("File Error"), tr(R"(Could not open "%1": Cannot open files of type "%2".)").arg(real_fp.toUserOutput(), mime_type.name()), QMessageBox::Ok, ICore::dialogParent());
    msgbox.exec();
    return nullptr;
  }

  if (editor_id.isValid()) {
    if (const auto factory = EditorType::editorTypeForId(editor_id)) {
      QTC_CHECK(factory->asEditorFactory() || flags & EditorManager::AllowExternalEditor);
      factories.removeOne(factory);
      factories.push_front(factory);
    }
  }

  IEditor *editor = nullptr;
  auto override_cursor = OverrideCursor(QCursor(Qt::WaitCursor));
  auto factory = factories.takeFirst();

  while (factory) {
    QString error_string;

    if (factory->asEditorFactory()) {
      editor = createEditor(factory->asEditorFactory(), file_path);

      if (!editor) {
        factory = factories.isEmpty() ? nullptr : factories.takeFirst();
        continue;
      }

      const auto open_result = editor->document()->open(&error_string, file_path, real_fp);
      if (open_result == IDocument::OpenResult::Success)
        break;

      override_cursor.reset();
      delete editor;
      editor = nullptr;

      if (open_result == IDocument::OpenResult::ReadError) {
        QMessageBox msgbox(QMessageBox::Critical, EditorManager::tr("File Error"), tr("Could not open \"%1\" for reading. " "Either the file does not exist or you do not have " "the permissions to open it.").arg(real_fp.toUserOutput()), QMessageBox::Ok, ICore::dialogParent());
        msgbox.exec();
        return nullptr;
      }

      // can happen e.g. when trying to open an completely empty .qrc file
      QTC_CHECK(open_result == IDocument::OpenResult::CannotHandle);
    } else {
      QTC_ASSERT(factory->asExternalEditor(), factory = factories.isEmpty() ? nullptr : factories.takeFirst(); continue);
      if (factory->asExternalEditor()->startEditor(file_path, &error_string))
        break;
    }

    if (error_string.isEmpty())
      error_string = tr("Could not open \"%1\": Unknown error.").arg(real_fp.toUserOutput());

    QMessageBox msgbox(QMessageBox::Critical, EditorManager::tr("File Error"), error_string, QMessageBox::Open | QMessageBox::Cancel, ICore::dialogParent());
    EditorType *selected_factory = nullptr;

    if (!factories.isEmpty()) {
      const auto button = qobject_cast<QPushButton*>(msgbox.button(QMessageBox::Open));
      QTC_ASSERT(button, return nullptr);
      const auto menu = new QMenu(button);
      for (auto& factory : qAsConst(factories)) {
        const auto action = menu->addAction(factory->displayName());
        connect(action, &QAction::triggered, &msgbox, [&selected_factory, factory, &msgbox] {
          selected_factory = factory;
          msgbox.done(QMessageBox::Open);
        });
      }
      button->setMenu(menu);
    } else {
      msgbox.setStandardButtons(QMessageBox::Ok);
    }

    if (const auto ret = msgbox.exec(); ret == QMessageBox::Cancel || ret == QMessageBox::Ok)
      return nullptr;

    override_cursor.set();
    factories.removeOne(selected_factory);
    factory = selected_factory;
  }

  if (!editor)
    return nullptr;

  if (real_fp != file_path)
    editor->document()->setRestoredFrom(real_fp);
  addEditor(editor);

  if (new_editor)
    *new_editor = true;

  const auto result = activateEditor(view, editor, flags);
  if (editor == result)
    restoreEditorState(editor);

  return result;
}

auto EditorManagerPrivate::openEditorAt(EditorView *view, const Link &link, const Id editor_id, const EditorManager::OpenEditorFlags flags, bool *new_editor) -> IEditor*
{
  EditorManager::cutForwardNavigationHistory();
  EditorManager::addCurrentPositionToNavigationHistory();

  const auto temp_flags = flags | EditorManager::IgnoreNavigationHistory;
  const auto editor = openEditor(view, link.targetFilePath, editor_id, temp_flags, new_editor);

  if (editor && link.targetLine != -1)
    editor->gotoLine(link.targetLine, link.targetColumn);

  return editor;
}

auto EditorManagerPrivate::openEditorWith(const FilePath &file_path, const Id editor_id) -> IEditor*
{
  // close any open editors that have this file open
  // remember the views to open new editors in there
  QList<EditorView*> views;
  auto editors_open_for_file = DocumentModel::editorsForFilePath(file_path);

  for (const auto open_editor : editors_open_for_file) {
    if (const auto view = viewForEditor(open_editor); view && view->currentEditor() == open_editor) // visible
      views.append(view);
  }

  if (!EditorManager::closeEditors(editors_open_for_file)) // don't open if cancel was pressed
    return nullptr;

  IEditor *opened_editor = nullptr;

  if (views.isEmpty()) {
    opened_editor = EditorManager::openEditor(file_path, editor_id);
  } else {
    if (const auto current_view = currentEditorView()) {
      if (views.removeOne(current_view))
        views.prepend(current_view); // open editor in current view first
    }
    EditorManager::OpenEditorFlags flags;
    for (const auto &view : qAsConst(views)) {
      const auto editor = openEditor(view, file_path, editor_id, flags);
      if (!opened_editor && editor)
        opened_editor = editor;
      // Do not change the current editor after opening the first one. That
      // * prevents multiple updates of focus etc which are not necessary
      // * lets us control which editor is made current by putting the current editor view
      //   to the front (if that was in the list in the first place)
      flags |= EditorManager::DoNotChangeCurrentEditor;
      // do not try to open more editors if this one failed, or editor type does not
      // support duplication anyhow
      if (!editor || !editor->duplicateSupported())
        break;
    }
  }

  return opened_editor;
}

auto EditorManagerPrivate::activateEditorForDocument(EditorView *view, IDocument *document, const EditorManager::OpenEditorFlags flags) -> IEditor*
{
  Q_ASSERT(view);
  auto editor = view->editorForDocument(document);
  if (!editor) {
    const auto editors = DocumentModel::editorsForDocument(document);
    if (editors.isEmpty())
      return nullptr;
    editor = editors.first();
  }
  return activateEditor(view, editor, flags);
}

auto EditorManagerPrivate::viewForEditor(const IEditor *editor) -> EditorView*
{
  auto w = editor->widget();
  while (w) {
    w = w->parentWidget();
    if (const auto view = qobject_cast<EditorView*>(w))
      return view;
  }
  return nullptr;
}

auto EditorManagerPrivate::makeFileWritable(IDocument *document) -> make_writable_result
{
  if (!document)
    return failed;
  switch (ReadOnlyFilesDialog ro_dialog(document, ICore::dialogParent(), document->isSaveAsAllowed()); ro_dialog.exec()) {
  case ReadOnlyFilesDialog::RO_MakeWritable:
  case ReadOnlyFilesDialog::RO_OpenVCS:
    return made_writable;
  case ReadOnlyFilesDialog::RO_SaveAs:
    return saved_as;
  default:
    return failed;
  }
}

/*!
    Implements the logic of the escape key shortcut (ReturnToEditor).
    Should only be called by the shortcut handler.
    \internal
*/
auto EditorManagerPrivate::doEscapeKeyFocusMoveMagic() -> void
{
  // use cases to cover:
  // 1. if app focus is in mode or external window without editor view (e.g. Design, Projects, ext. Help)
  //      if there are extra views (e.g. output)
  //        Hide them
  //      otherwise
  //        activate & raise the current editor view (can be external)
  //        if that is in edit mode
  //          activate edit mode and unmaximize output pane
  // 2. if app focus is in external window with editor view
  //      Hide find if necessary
  // 2. if app focus is in mode with editor view
  //      if current editor view is in external window
  //        raise and activate current editor view
  //      otherwise if the current editor view is not app focus
  //        move focus to editor view in mode and unmaximize output pane
  //      otherwise if the current view is app focus
  //        if mode is not edit mode
  //          if there are extra views (find, help, output)
  //            Hide them
  //          otherwise
  //            activate edit mode and unmaximize output pane
  //        otherwise (i.e. mode is edit mode)
  //          Hide extra views (find, help, output)

  const auto active_window = QApplication::activeWindow();
  if (!active_window)
    return;

  const auto focus = QApplication::focusWidget();
  const auto editor_view = currentEditorView();
  const auto editor_view_active = focus && focus == editor_view->focusWidget();
  const auto editor_view_visible = editor_view->isVisible();

  auto stuff_hidden = false;

  if (const auto find_pane = FindToolBarPlaceHolder::getCurrent(); find_pane && find_pane->isVisible() && find_pane->isUsedByWidget(focus)) {
    find_pane->hide();
    stuff_hidden = true;
  } else if (!(editor_view_visible && !editor_view_active && editor_view->window() == active_window)) {
    if (const QWidget *output_pane = OutputPanePlaceHolder::getCurrent(); output_pane && output_pane->isVisible() && output_pane->window() == active_window) {
      stuff_hidden = true;
    }
    if (const QWidget *right_pane = RightPanePlaceHolder::current(); right_pane && right_pane->isVisible() && right_pane->window() == active_window) {
      RightPaneWidget::instance()->setShown(false);
      stuff_hidden = true;
    }
    if (find_pane && find_pane->isVisible() && find_pane->window() == active_window) {
      find_pane->hide();
      stuff_hidden = true;
    }
  }

  if (stuff_hidden)
    return;

  if (!editor_view_active && editor_view_visible) {
    setFocusToEditorViewAndUnmaximizePanes(editor_view);
    return;
  }
}

auto EditorManagerPrivate::windowPopup() -> OpenEditorsWindow*
{
  return d->m_window_popup;
}

auto EditorManagerPrivate::showPopupOrSelectDocument() -> void
{
  if (QApplication::keyboardModifiers() == Qt::NoModifier) {
    windowPopup()->selectAndHide();
  } else {
    const auto active_window = QApplication::activeWindow();

    // decide where to show the popup
    // if the active window has editors, we want that editor area as a reference
    // TODO: this does not work correctly with multiple editor areas in the same window
    const EditorArea *active_editor_area = nullptr;
    for(const auto area: d->m_editor_areas) {
      if (area->window() == active_window) {
        active_editor_area = area;
        break;
      }
    }

    // otherwise we take the "current" editor area
    if (!active_editor_area)
      active_editor_area = findEditorArea(currentEditorView());

    QTC_ASSERT(active_editor_area, active_editor_area = d->m_editor_areas.first());
    // editor area in main window is invisible when invoked from Design Mode.
    const auto reference_widget = active_editor_area->isVisible() ? active_editor_area : active_editor_area->window();
    QTC_CHECK(reference_widget->isVisible());
    const auto p = reference_widget->mapToGlobal(QPoint(0, 0));
    const auto popup = windowPopup();

    popup->setMaximumSize(qMax(popup->minimumWidth(), reference_widget->width() / 2), qMax(popup->minimumHeight(), reference_widget->height() / 2));
    popup->adjustSize();
    popup->move((reference_widget->width() - popup->width()) / 2 + p.x(), (reference_widget->height() - popup->height()) / 2 + p.y());
    popup->setVisible(true);
  }
}

// Run the OpenWithDialog and return the editor id
// selected by the user.
auto EditorManagerPrivate::getOpenWithEditorId(const FilePath &file_name, bool *is_external_editor) -> Id
{
  // Collect editors that can open the file
  QList<Id> all_editor_ids;
  QStringList all_editor_display_names;

  // Built-in
  const auto editors = EditorType::preferredEditorTypes(file_name);
  const auto size = editors.size();

  all_editor_display_names.reserve(size);

  for (auto i = 0; i < size; i++) {
    all_editor_ids.push_back(editors.at(i)->id());
    all_editor_display_names.push_back(editors.at(i)->displayName());
  }

  if (all_editor_ids.empty())
    return {};

  QTC_ASSERT(all_editor_ids.size() == all_editor_display_names.size(), return Id());

  // Run dialog.
  OpenWithDialog dialog(file_name, ICore::dialogParent());
  dialog.setEditors(all_editor_display_names);
  dialog.setCurrentEditor(0);

  if (dialog.exec() != QDialog::Accepted)
    return {};

  const auto &selected_id = all_editor_ids.at(dialog.editor());

  if (is_external_editor) {
    const auto type = EditorType::editorTypeForId(selected_id);
    *is_external_editor = type && type->asExternalEditor() != nullptr;
  }

  return selected_id;
}

static auto toMap(const QHash<MimeType, EditorType*> &hash) -> QMap<QString, QVariant>
{
  QMap<QString, QVariant> map;

  auto it = hash.begin();
  const auto end = hash.end();

  while (it != end) {
    map.insert(it.key().name(), it.value()->id().toSetting());
    ++it;
  }
  return map;
}

static auto fromMap(const QMap<QString, QVariant> &map) -> QHash<MimeType, EditorType*>
{
  const auto factories = EditorType::allEditorTypes();
  QHash<MimeType, EditorType*> hash;

  auto it = map.begin();
  const auto end = map.end();

  while (it != end) {
    if (const auto mime_type = mimeTypeForName(it.key()); mime_type.isValid()) {
      const auto factory_id = Id::fromSetting(it.value());
      if (auto factory = findOrDefault(factories, equal(&EditorType::id, factory_id)))
        hash.insert(mime_type, factory);
    }
    ++it;
  }
  return hash;
}

auto EditorManagerPrivate::saveSettings() -> void
{
  ICore::settingsDatabase()->setValue(document_states_key, d->m_editor_states);

  const Settings def;
  const auto qsettings = ICore::settings();
  qsettings->setValueWithDefault(reload_behavior_key, static_cast<int>(d->m_settings.reload_setting), static_cast<int>(def.reload_setting));
  qsettings->setValueWithDefault(auto_save_enabled_key, d->m_settings.auto_save_enabled, def.auto_save_enabled);
  qsettings->setValueWithDefault(auto_save_interval_key, d->m_settings.auto_save_interval, def.auto_save_interval);
  qsettings->setValueWithDefault(auto_save_after_refactoring_key, d->m_settings.auto_save_after_refactoring, def.auto_save_after_refactoring);
  qsettings->setValueWithDefault(auto_suspend_enabled_key, d->m_settings.auto_suspend_enabled, def.auto_suspend_enabled);
  qsettings->setValueWithDefault(auto_suspend_min_document_count_key, d->m_settings.auto_suspend_min_document_count, def.auto_suspend_min_document_count);
  qsettings->setValueWithDefault(warn_before_opening_big_text_files_key, d->m_settings.warn_before_opening_big_files_enabled, def.warn_before_opening_big_files_enabled);
  qsettings->setValueWithDefault(big_text_file_size_limit_key, d->m_settings.big_file_size_limit_in_mb, def.big_file_size_limit_in_mb);
  qsettings->setValueWithDefault(max_recent_files_key, d->m_settings.max_recent_files, def.max_recent_files);
  qsettings->setValueWithDefault(preferred_editor_factories_key, toMap(userPreferredEditorTypes()));
}

auto EditorManagerPrivate::readSettings() -> void
{
  const Settings def;
  QSettings *qs = ICore::settings();

  d->m_settings.warn_before_opening_big_files_enabled = qs->value(warn_before_opening_big_text_files_key, def.warn_before_opening_big_files_enabled).toBool();
  d->m_settings.big_file_size_limit_in_mb = qs->value(big_text_file_size_limit_key, def.big_file_size_limit_in_mb).toInt();

  if (const auto max_recent_files = qs->value(max_recent_files_key, def.max_recent_files).toInt(); max_recent_files > 0)
    d->m_settings.max_recent_files = max_recent_files;

  const auto default_sensitivity = OsSpecificAspects::fileNameCaseSensitivity(HostOsInfo::hostOs());
  if (const auto sensitivity = readFileSystemSensitivity(qs); sensitivity == default_sensitivity)
    HostOsInfo::unsetOverrideFileNameCaseSensitivity();
  else
    HostOsInfo::setOverrideFileNameCaseSensitivity(sensitivity);

  const auto preferred_editor_factories = fromMap(qs->value(preferred_editor_factories_key).toMap());
  setUserPreferredEditorTypes(preferred_editor_factories);

  if (const auto settings = ICore::settingsDatabase(); settings->contains(document_states_key)) {
    d->m_editor_states = settings->value(document_states_key).value<QMap<QString, QVariant>>();
  }

  d->m_settings.reload_setting = static_cast<IDocument::ReloadSetting>(qs->value(reload_behavior_key, def.reload_setting).toInt());
  d->m_settings.auto_save_enabled = qs->value(auto_save_enabled_key, def.auto_save_enabled).toBool();
  d->m_settings.auto_save_interval = qs->value(auto_save_interval_key, def.auto_save_interval).toInt();
  d->m_settings.auto_save_after_refactoring = qs->value(auto_save_after_refactoring_key, def.auto_save_after_refactoring).toBool();
  d->m_settings.auto_suspend_enabled = qs->value(auto_suspend_enabled_key, def.auto_suspend_enabled).toBool();
  d->m_settings.auto_suspend_min_document_count = qs->value(auto_suspend_min_document_count_key, def.auto_suspend_min_document_count).toInt();

  updateAutoSave();
}

auto EditorManagerPrivate::readFileSystemSensitivity(const QSettings *settings) -> Qt::CaseSensitivity
{
  const auto default_sensitivity = OsSpecificAspects::fileNameCaseSensitivity(HostOsInfo::hostOs());

  if (!settings->contains(file_system_case_sensitivity_key))
    return default_sensitivity;

  auto ok = false;
  const auto sensitivity_setting = settings->value(file_system_case_sensitivity_key).toInt(&ok);

  if (ok) {
    switch (static_cast<Qt::CaseSensitivity>(sensitivity_setting)) {
    case Qt::CaseSensitive:
      return Qt::CaseSensitive;
    case Qt::CaseInsensitive:
      return Qt::CaseInsensitive;
    }
  }

  return default_sensitivity;
}

auto EditorManagerPrivate::writeFileSystemSensitivity(QtcSettings *settings, const Qt::CaseSensitivity sensitivity) -> void
{
  settings->setValueWithDefault(file_system_case_sensitivity_key, static_cast<int>(sensitivity), static_cast<int>(OsSpecificAspects::fileNameCaseSensitivity(HostOsInfo::hostOs())));
}

auto EditorManagerPrivate::setAutoSaveEnabled(const bool enabled) -> void
{
  d->m_settings.auto_save_enabled = enabled;
  updateAutoSave();
}

auto EditorManagerPrivate::autoSaveEnabled() -> bool
{
  return d->m_settings.auto_save_enabled;
}

auto EditorManagerPrivate::setAutoSaveInterval(const int interval) -> void
{
  d->m_settings.auto_save_interval = interval;
  updateAutoSave();
}

auto EditorManagerPrivate::autoSaveInterval() -> int
{
  return d->m_settings.auto_save_interval;
}

auto EditorManagerPrivate::setAutoSaveAfterRefactoring(const bool enabled) -> void
{
  d->m_settings.auto_save_after_refactoring = enabled;
}

auto EditorManagerPrivate::autoSaveAfterRefactoring() -> bool
{
  return d->m_settings.auto_save_after_refactoring;
}

auto EditorManagerPrivate::setAutoSuspendEnabled(const bool enabled) -> void
{
  d->m_settings.auto_suspend_enabled = enabled;
}

auto EditorManagerPrivate::autoSuspendEnabled() -> bool
{
  return d->m_settings.auto_suspend_enabled;
}

auto EditorManagerPrivate::setAutoSuspendMinDocumentCount(const int count) -> void
{
  d->m_settings.auto_suspend_min_document_count = count;
}

auto EditorManagerPrivate::autoSuspendMinDocumentCount() -> int
{
  return d->m_settings.auto_suspend_min_document_count;
}

auto EditorManagerPrivate::warnBeforeOpeningBigFilesEnabled() -> bool
{
  return d->m_settings.warn_before_opening_big_files_enabled;
}

auto EditorManagerPrivate::setWarnBeforeOpeningBigFilesEnabled(const bool enabled) -> void
{
  d->m_settings.warn_before_opening_big_files_enabled = enabled;
}

auto EditorManagerPrivate::bigFileSizeLimit() -> int
{
  return d->m_settings.big_file_size_limit_in_mb;
}

auto EditorManagerPrivate::setMaxRecentFiles(const int count) -> void
{
  d->m_settings.max_recent_files = count;
}

auto EditorManagerPrivate::maxRecentFiles() -> int
{
  return d->m_settings.max_recent_files;
}

auto EditorManagerPrivate::setBigFileSizeLimit(const int limit_in_mb) -> void
{
  d->m_settings.big_file_size_limit_in_mb = limit_in_mb;
}

auto EditorManagerPrivate::findFactories(const Id editor_id, const FilePath &file_path) -> editor_factory_list
{
  if constexpr (debug_editor_manager)
    qDebug() << Q_FUNC_INFO << editor_id.name() << file_path;

  editor_factory_list factories;

  if (!editor_id.isValid()) {
    factories = IEditorFactory::preferredEditorFactories(file_path);
  } else {
    // Find by editor id
    if (const auto factory = findOrDefault(IEditorFactory::allEditorFactories(), equal(&IEditorFactory::id, editor_id)))
      factories.push_back(factory);
  }

  if (factories.empty()) {
    qWarning("%s: unable to find an editor factory for the file '%s', editor Id '%s'.", Q_FUNC_INFO, file_path.toString().toUtf8().constData(), editor_id.name().constData());
  }

  return factories;
}

auto EditorManagerPrivate::createEditor(const IEditorFactory *factory, const FilePath &file_path) -> IEditor*
{
  if (!factory)
    return nullptr;

  const auto editor = factory->createEditor();

  if (editor) {
    QTC_CHECK(editor->document()->id().isValid()); // sanity check that the editor has an id set
    connect(editor->document(), &IDocument::changed, d, &EditorManagerPrivate::handleDocumentStateChange);
    emit m_instance->editorCreated(editor, file_path.toString());
  }

  return editor;
}

auto EditorManagerPrivate::addEditor(IEditor *editor) -> void
{
  if (!editor)
    return;

  ICore::addContextObject(editor);
  auto is_new_document = false;
  DocumentModelPrivate::addEditor(editor, &is_new_document);

  if (is_new_document) {
    const auto document = editor->document();
    const auto is_temporary = document->isTemporary() || document->filePath().isEmpty();
    const auto add_watcher = !is_temporary;
    DocumentManager::addDocument(document, add_watcher);
    if (!is_temporary)
      DocumentManager::addToRecentFiles(document->filePath(), document->id());
    emit m_instance->documentOpened(document);
  }

  emit m_instance->editorOpened(editor);
  QMetaObject::invokeMethod(d, &EditorManagerPrivate::autoSuspendDocuments, Qt::QueuedConnection);
}

auto EditorManagerPrivate::removeEditor(IEditor *editor, const bool removeSuspendedEntry) -> void
{
  const auto entry = DocumentModelPrivate::removeEditor(editor);
  QTC_ASSERT(entry, return);

  if (entry->isSuspended) {
    const auto document = editor->document();
    DocumentManager::removeDocument(document);
    if (removeSuspendedEntry)
      DocumentModelPrivate::removeEntry(entry);
    emit m_instance->documentClosed(document);
  }

  ICore::removeContextObject(editor);
}

auto EditorManagerPrivate::placeEditor(EditorView *view, IEditor *editor) -> IEditor*
{
  Q_ASSERT(view && editor);

  if (view->hasEditor(editor))
    return editor;
  if (const auto e = view->editorForDocument(editor->document()))
    return e;

  const auto state = editor->saveState();

  if (const auto source_view = viewForEditor(editor)) {
    // try duplication or pull editor over to new view
    if (const auto duplicate_supported = editor->duplicateSupported(); editor != source_view->currentEditor() || !duplicate_supported) {
      // pull the IEditor over to the new view
      source_view->removeEditor(editor);
      view->addEditor(editor);
      view->setCurrentEditor(editor);
      // possibly adapts old state to new layout
      editor->restoreState(state);
      if (!source_view->currentEditor()) {
        EditorView *replacement_view = nullptr;
        if (const auto replacement = pickUnusedEditor(&replacement_view)) {
          if (replacement_view)
            replacement_view->removeEditor(replacement);
          source_view->addEditor(replacement);
          source_view->setCurrentEditor(replacement);
        }
      }
      return editor;
    } else if (duplicate_supported) {
      editor = duplicateEditor(editor);
      Q_ASSERT(editor);
    }
  }

  view->addEditor(editor);
  view->setCurrentEditor(editor);
  editor->restoreState(state);  // possibly adapts old state to new layout

  return editor;
}

auto EditorManagerPrivate::duplicateEditor(IEditor *editor) -> IEditor*
{
  if (!editor->duplicateSupported())
    return nullptr;

  const auto duplicate = editor->duplicate();
  emit m_instance->editorCreated(duplicate, duplicate->document()->filePath().toString());
  addEditor(duplicate);
  return duplicate;
}

auto EditorManagerPrivate::activateEditor(EditorView *view, IEditor *editor, const EditorManager::OpenEditorFlags flags) -> IEditor*
{
  Q_ASSERT(view);

  if (!editor)
    return nullptr;

  editor = placeEditor(view, editor);

  if (!(flags & EditorManager::DoNotChangeCurrentEditor)) {
    setCurrentEditor(editor, flags & EditorManager::IgnoreNavigationHistory);
    if (!(flags & EditorManager::DoNotMakeVisible)) {
        editor->widget()->setFocus();
        if (!(flags & EditorManager::DoNotRaise))
          ICore::raiseWindow(editor->widget());
      }
  } else if (!(flags & EditorManager::DoNotMakeVisible)) {
    view->setCurrentEditor(editor);
  }
  return editor;
}

auto EditorManagerPrivate::activateEditorForEntry(EditorView *view, DocumentModel::Entry *entry, const EditorManager::OpenEditorFlags flags) -> bool
{
  QTC_ASSERT(view, return false);

  if (!entry) {
    // no document
    view->setCurrentEditor(nullptr);
    setCurrentView(view);
    setCurrentEditor(nullptr);
    return false;
  }

  const auto document = entry->document;

  if (!entry->isSuspended) {
    const auto editor = activateEditorForDocument(view, document, flags);
    return editor != nullptr;
  }

  if (!openEditor(view, entry->fileName(), entry->id(), flags)) {
    DocumentModelPrivate::removeEntry(entry);
    return false;
  }

  return true;
}

auto EditorManagerPrivate::closeEditorOrDocument(IEditor *editor) -> void
{
  QTC_ASSERT(editor, return);
  if (const auto visible = EditorManager::visibleEditors(); contains(visible, [&editor](const IEditor *other) {
    return editor != other && other->document() == editor->document();
  })) {
    EditorManager::closeEditors({editor});
  } else {
    EditorManager::closeDocuments({editor->document()});
  }
}

auto EditorManagerPrivate::closeEditors(const QList<IEditor*> &editors, const close_flag flag) -> bool
{
  if (editors.isEmpty())
    return true;

  auto closing_failed = false;
  // close Editor History list
  windowPopup()->setVisible(false);
  auto current_view = currentEditorView();

  // go through all editors to close and
  // 1. ask all core listeners to check whether the editor can be closed
  // 2. keep track of the document and all the editors that might remain open for it
  QSet<IEditor*> accepted_editors;
  QHash<IDocument*, QList<IEditor*>> editors_for_documents;

  for (auto editor : qAsConst(editors)) {
    auto editor_accepted = true;
    for (const auto listeners = d->m_close_editor_listeners; const auto &listener : listeners) {
      if (!listener(editor)) {
        editor_accepted = false;
        closing_failed = true;
        break;
      }
    }
    if (editor_accepted) {
      accepted_editors.insert(editor);
      auto document = editor->document();
      if (!editors_for_documents.contains(document)) // insert the document to track
        editors_for_documents.insert(document, DocumentModel::editorsForDocument(document));
      // keep track that we'll close this editor for the document
      editors_for_documents[document].removeAll(editor);
    }
  }
  if (accepted_editors.isEmpty())
    return false;

  //ask whether to save modified documents that we are about to close
  if (flag == close_flag::close_with_asking) {
    // Check for which documents we will close all editors, and therefore might have to ask the user
    QList<IDocument*> documents_to_close;
    for (auto i = editors_for_documents.constBegin(); i != editors_for_documents.constEnd(); ++i) {
      if (i.value().isEmpty())
        documents_to_close.append(i.key());
    }

    auto cancelled = false;
    QList<IDocument*> rejected_list;
    DocumentManager::saveModifiedDocuments(documents_to_close, QString(), &cancelled, QString(), nullptr, &rejected_list);

    if (cancelled)
      return false;

    if (!rejected_list.isEmpty()) {
      closing_failed = true;
      const auto skip_set = toSet(DocumentModel::editorsForDocuments(rejected_list));
      accepted_editors = accepted_editors.subtract(skip_set);
    }
  }

  if (accepted_editors.isEmpty())
    return false;

  // save editor states
  for (const auto editor : qAsConst(accepted_editors)) {
    if (!editor->document()->filePath().isEmpty() && !editor->document()->isTemporary()) {
      if (auto state = editor->saveState(); !state.isEmpty())
        d->m_editor_states.insert(editor->document()->filePath().toString(), QVariant(state));
    }
  }

  EditorView *focus_view = nullptr;

  // Remove accepted editors from document model/manager and context list,
  // and sort them per view, so we can remove them from views in an orderly
  // manner.
  QMultiHash<EditorView*, IEditor*> editorsPerView;
  for (auto editor : qAsConst(accepted_editors)) {
    emit m_instance->editorAboutToClose(editor);
    removeEditor(editor, flag != close_flag::suspend);
    if (auto view = viewForEditor(editor)) {
      editorsPerView.insert(view, editor);
      if (QApplication::focusWidget() && QApplication::focusWidget() == editor->widget()->focusWidget()) {
        focus_view = view;
      }
    }
  }
  QTC_CHECK(!focus_view || focus_view == current_view);

  // Go through views, remove the editors from them.
  // Sort such that views for which the current editor is closed come last,
  // and if the global current view is one of them, that comes very last.
  // When handling the last view in the list we handle the case where all
  // visible editors are closed, and we need to e.g. revive an invisible or
  // a suspended editor
  auto views = editorsPerView.keys();
  sort(views, [editorsPerView, current_view](EditorView *a, EditorView *b) {
    if (a == b)
      return false;
    const auto a_has_current = editorsPerView.values(a).contains(a->currentEditor());
    const auto b_has_current = editorsPerView.values(b).contains(b->currentEditor());
    const auto a_has_global_current = a == current_view && a_has_current;
    if (const auto b_has_global_current = b == current_view && b_has_current; b_has_global_current && !a_has_global_current)
      return true;
    if (b_has_current && !a_has_current)
      return true;
    return false;
  });

  for (auto view : qAsConst(views)) {
    auto editors = editorsPerView.values(view);
    // handle current editor in view last
    auto view_current_editor = view->currentEditor();
    if (editors.contains(view_current_editor) && editors.last() != view_current_editor) {
      editors.removeAll(view_current_editor);
      editors.append(view_current_editor);
    }
    for (const auto editor : qAsConst(editors)) {
      if (editor == view_current_editor && view == views.last()) {
        // Avoid removing the globally current editor from its view,
        // set a new current editor before.
        EditorManager::OpenEditorFlags flags = view != current_view ? EditorManager::DoNotChangeCurrentEditor : EditorManager::NoFlags;
        const auto view_editors = view->editors();
        auto new_current = view_editors.size() > 1 ? view_editors.at(view_editors.size() - 2) : nullptr;
        if (!new_current)
          new_current = pickUnusedEditor();
        if (new_current) {
          activateEditor(view, new_current, flags);
        } else {
          if (const auto entry = DocumentModelPrivate::firstSuspendedEntry()) {
            activateEditorForEntry(view, entry, flags);
          } else {
            // no "suspended" ones, so any entry left should have a document
            if (const auto documents = DocumentModel::entries(); !documents.isEmpty()) {
              if (const auto document = documents.last()->document) {
                // Do not auto-switch to design mode if the new editor will be for
                // the same document as the one that was closed.
                if (view == current_view && document == editor->document())
                  flags = EditorManager::DoNotSwitchToDesignMode;
                activateEditorForDocument(view, document, flags);
              }
            } else {
              // no documents left - set current view since view->removeEditor can
              // trigger a focus change, context change, and updateActions, which
              // requests the current EditorView
              setCurrentView(current_view);
            }
          }
        }
      }
      view->removeEditor(editor);
    }
  }

  emit m_instance->editorsClosed(toList(accepted_editors));

  if (focus_view) {
    activateView(focus_view);
  } else {
    setCurrentView(current_view);
    setCurrentEditor(current_view->currentEditor());
  }

  qDeleteAll(accepted_editors);

  if (!EditorManager::currentEditor()) {
    emit m_instance->currentEditorChanged(nullptr);
    updateActions();
  }

  return !closing_failed;
}

auto EditorManagerPrivate::activateView(EditorView *view) -> void
{
  QTC_ASSERT(view, return);
  QWidget *focus_widget
  ;
  if (const auto editor = view->currentEditor()) {
    setCurrentEditor(editor, true);
    focus_widget = editor->widget();
  } else {
    setCurrentView(view);
    focus_widget = view;
  }

  focus_widget->setFocus();
  ICore::raiseWindow(focus_widget);
}

auto EditorManagerPrivate::restoreEditorState(IEditor *editor) -> void
{
  QTC_ASSERT(editor, return);
  const auto file_name = editor->document()->filePath().toString();
  editor->restoreState(d->m_editor_states.value(file_name).toByteArray());
}

auto EditorManagerPrivate::visibleDocumentsCount() -> int
{
  const auto editors = EditorManager::visibleEditors();
  if (const int editors_count = editors.count(); editors_count < 2)
    return editors_count;

  QSet<const IDocument*> visible_documents;
  foreach(IEditor *editor, editors) {
    if (const IDocument *document = editor->document())
      visible_documents << document;
  }

  return static_cast<int>(visible_documents.count());
}

auto EditorManagerPrivate::setCurrentEditor(IEditor *editor, const bool ignore_navigation_history) -> void
{
  if (editor)
    setCurrentView(nullptr);

  if (d->m_current_editor == editor)
    return;

  emit m_instance->currentEditorAboutToChange(d->m_current_editor);

  if (d->m_current_editor && !ignore_navigation_history)
    EditorManager::addCurrentPositionToNavigationHistory();

  d->m_current_editor = editor;

  if (editor) {
    if (const auto view = viewForEditor(editor))
      view->setCurrentEditor(editor);
    // update global history
    EditorView::updateEditorHistory(editor, d->m_global_history);
  }

  updateActions();
  emit m_instance->currentEditorChanged(editor);
}

auto EditorManagerPrivate::setCurrentView(EditorView *view) -> void
{
  if (view == d->m_current_view)
    return;

  EditorView *old = d->m_current_view;
  d->m_current_view = view;

  if (old)
    old->update();
  if (view)
    view->update();
}

auto EditorManagerPrivate::findEditorArea(const EditorView *view, int *area_index) -> EditorArea*
{
  auto current = view->parentSplitterOrView();

  while (current) {
    if (const auto area = qobject_cast<EditorArea*>(current)) {
      const auto index = static_cast<int>(d->m_editor_areas.indexOf(area));
      QTC_ASSERT(index >= 0, return nullptr);
      if (area_index)
        *area_index = index;
      return area;
    }
    current = current->findParentSplitter();
  }

  QTC_CHECK(false); // we should never have views without a editor area
  return nullptr;
}

auto EditorManagerPrivate::closeView(EditorView *view) -> void
{
  if (!view)
    return;

  const auto editors_to_delete = emptyView(view);
  const auto splitter_or_view = view->parentSplitterOrView();

  Q_ASSERT(splitter_or_view);
  Q_ASSERT(splitter_or_view->view() == view);

  const auto splitter = splitter_or_view->findParentSplitter();
  Q_ASSERT(splitter_or_view->hasEditors() == false);
  splitter_or_view->hide();
  delete splitter_or_view;

  splitter->unsplit();

  if (const auto new_current = splitter->findFirstView())
    activateView(new_current);

  deleteEditors(editors_to_delete);
}

/*!
    Removes all editors from the view and from the document model, taking care of
    the handling of editors that are the last ones for the document.
    Returns the list of editors that were actually removed from the document model and
    need to be deleted with \c EditorManagerPrivate::deleteEditors.
    \internal
*/
auto EditorManagerPrivate::emptyView(EditorView *view) -> QList<IEditor*>
{
  if (!view)
    return {};

  const auto editors = view->editors();
  QList<IEditor*> removed_editors;

  for (const auto editor : editors) {
    if (DocumentModel::editorsForDocument(editor->document()).size() == 1) {
      // it's the only editor for that file
      // so we need to keep it around (--> in the editor model)
      if (EditorManager::currentEditor() == editor) {
        // we don't want a current editor that is not open in a view
        setCurrentView(view);
        setCurrentEditor(nullptr);
      }
      view->removeEditor(editor);
    } else {
      emit m_instance->editorAboutToClose(editor);
      removeEditor(editor, true /*=removeSuspendedEntry, but doesn't matter since it's not the last editor anyhow*/);
      view->removeEditor(editor);
      removed_editors.append(editor);
    }
  }

  return removed_editors;
}

/*!
    Signals editorsClosed() and deletes the editors.
    \internal
*/
auto EditorManagerPrivate::deleteEditors(const QList<IEditor*> &editors) -> void
{
  if (!editors.isEmpty()) {
    emit m_instance->editorsClosed(editors);
    qDeleteAll(editors);
  }
}

auto EditorManagerPrivate::createEditorWindow() -> EditorWindow*
{
  const auto win = new EditorWindow;
  const auto area = win->editorArea();
  d->m_editor_areas.append(area);
  connect(area, &QObject::destroyed, d, &EditorManagerPrivate::editorAreaDestroyed);
  return win;
}

auto EditorManagerPrivate::splitNewWindow(const EditorView *view) -> void
{
  const auto editor = view->currentEditor();
  IEditor *new_editor = nullptr;
  const auto state = editor ? editor->saveState() : QByteArray();

  if (editor && editor->duplicateSupported())
    new_editor = duplicateEditor(editor);
  else
    new_editor = editor; // move to the new view

  const auto win = createEditorWindow();
  win->show();
  ICore::raiseWindow(win);

  if (new_editor) {
    activateEditor(win->editorArea()->view(), new_editor, EditorManager::IgnoreNavigationHistory);
    // possibly adapts old state to new layout
    new_editor->restoreState(state);
  } else {
    win->editorArea()->view()->setFocus();
  }

  updateActions();
}

auto EditorManagerPrivate::pickUnusedEditor(EditorView **found_view) -> IEditor*
{
  for (const auto editors = DocumentModel::editorsForOpenedDocuments(); const auto &editor : editors) {
    if (const auto view = viewForEditor(editor); !view || view->currentEditor() != editor) {
      if (found_view)
        *found_view = view;
      return editor;
    }
  }
  return nullptr;
}

/* Adds the file name to the recent files if there is at least one non-temporary editor for it */
auto EditorManagerPrivate::addDocumentToRecentFiles(IDocument *document) -> void
{
  if (document->isTemporary())
    return;

  const auto entry = DocumentModel::entryForDocument(document);

  if (!entry)
    return;

  DocumentManager::addToRecentFiles(document->filePath(), entry->id());
}

auto EditorManagerPrivate::updateAutoSave() -> void
{
  if (d->m_settings.auto_save_enabled)
    d->m_auto_save_timer->start(d->m_settings.auto_save_interval * (60 * 1000));
  else
    d->m_auto_save_timer->stop();
}

auto EditorManagerPrivate::updateMakeWritableWarning() -> void
{
  auto document = EditorManager::currentDocument();
  QTC_ASSERT(document, return);

  if (auto ww = document->isModified() && document->isFileReadOnly(); ww != document->hasWriteWarning()) {
    document->setWriteWarning(ww);

    // Do this after setWriteWarning so we don't re-evaluate this part even
    // if we do not really show a warning.
    auto prompt_vcs = false;
    const auto directory = document->filePath().parentDir();
    auto version_control = VcsManager::findVersionControlForDirectory(directory);
    if (version_control && version_control->openSupportMode(document->filePath()) != IVersionControl::NoOpen) {
      if (version_control->settingsFlags() & IVersionControl::AutoOpen) {
        vcsOpenCurrentEditor();
        ww = false;
      } else {
        prompt_vcs = true;
      }
    }

    if (ww) {
      // we are about to change a read-only file, warn user
      if (prompt_vcs) {
        InfoBarEntry info(Id(k_make_writable_warning), tr("<b>Warning:</b> This file was not opened in %1 yet.").arg(version_control->displayName()));
        info.addCustomButton(tr("Open"), &vcsOpenCurrentEditor);
        document->infoBar()->addInfo(info);
      } else {
        InfoBarEntry info(Id(k_make_writable_warning), tr("<b>Warning:</b> You are changing a read-only file."));
        info.addCustomButton(tr("Make Writable"), &makeCurrentEditorWritable);
        document->infoBar()->addInfo(info);
      }
    } else {
      document->infoBar()->removeInfo(Id(k_make_writable_warning));
    }
  }
}

auto EditorManagerPrivate::setupSaveActions(const IDocument *document, QAction *save_action, QAction *save_as_action, QAction *revert_to_saved_action) -> void
{
  const auto has_file = document && !document->filePath().isEmpty();

  save_action->setEnabled(has_file && document->isModified());
  save_as_action->setEnabled(document && document->isSaveAsAllowed());
  revert_to_saved_action->setEnabled(has_file);

  if (document && !document->displayName().isEmpty()) {
    const QString quoted_name = QLatin1Char('"') + quoteAmpersands(document->displayName()) + QLatin1Char('"');
    save_action->setText(tr("&Save %1").arg(quoted_name));
    save_as_action->setText(tr("Save %1 &As...").arg(quoted_name));
    revert_to_saved_action->setText(document->isModified() ? tr("Revert %1 to Saved").arg(quoted_name) : tr("Reload %1").arg(quoted_name));
  } else {
    save_action->setText(EditorManager::tr("&Save"));
    save_as_action->setText(EditorManager::tr("Save &As..."));
    revert_to_saved_action->setText(EditorManager::tr("Revert to Saved"));
  }
}

auto EditorManagerPrivate::updateActions() -> void
{
  const auto cur_document = EditorManager::currentDocument();
  const auto opened_count = DocumentModel::entryCount();

  if (cur_document)
    updateMakeWritableWarning();

  QString quoted_name;
  if (cur_document)
    quoted_name = QLatin1Char('"') + quoteAmpersands(cur_document->displayName()) + QLatin1Char('"');

  setupSaveActions(cur_document, d->m_save_action, d->m_save_as_action, d->m_revert_to_saved_action);

  d->m_close_current_editor_action->setEnabled(cur_document);
  d->m_close_current_editor_action->setText(tr("Close %1").arg(quoted_name));
  d->m_close_all_editors_action->setEnabled(opened_count > 0);
  d->m_close_other_documents_action->setEnabled(opened_count > 1);
  d->m_close_other_documents_action->setText(opened_count > 1 ? tr("Close All Except %1").arg(quoted_name) : tr("Close Others"));
  d->m_close_all_editors_except_visible_action->setEnabled(visibleDocumentsCount() < opened_count);
  d->m_goto_next_doc_history_action->setEnabled(opened_count != 0);
  d->m_goto_previous_doc_history_action->setEnabled(opened_count != 0);

  const auto view = currentEditorView();
  d->m_go_back_action->setEnabled(view ? view->canGoBack() : false);
  d->m_go_forward_action->setEnabled(view ? view->canGoForward() : false);

  const auto view_parent = view ? view->parentSplitterOrView() : nullptr;
  const auto parent_splitter = view_parent ? view_parent->findParentSplitter() : nullptr;
  const auto has_splitter = parent_splitter && parent_splitter->isSplitter();
  d->m_remove_current_split_action->setEnabled(has_splitter);
  d->m_remove_all_splits_action->setEnabled(has_splitter);
  d->m_goto_next_split_action->setEnabled(has_splitter || d->m_editor_areas.size() > 1);
}

auto EditorManagerPrivate::updateWindowTitleForDocument(const IDocument *document, const QWidget *window) -> void
{
  QTC_ASSERT(window, return);
  QString window_title;
  const QString dash_sep(" - ");

  if (const auto document_name = document ? document->displayName() : QString(); !document_name.isEmpty())
    window_title.append(document_name);

  const auto file_path = document ? document->filePath().absoluteFilePath() : FilePath();

  if (const auto window_title_addition = d->m_title_addition_handler ? d->m_title_addition_handler(file_path) : QString(); !window_title_addition.isEmpty()) {
    if (!window_title.isEmpty())
      window_title.append(" ");
    window_title.append(window_title_addition);
  }

  if (const auto window_title_vcs_topic = d->m_title_vcs_topic_handler ? d->m_title_vcs_topic_handler(file_path) : QString(); !window_title_vcs_topic.isEmpty()) {
    if (!window_title.isEmpty())
      window_title.append(" ");
    window_title.append(QStringLiteral("[") + window_title_vcs_topic + QStringLiteral("]"));
  }

  if (const auto session_title = d->m_session_title_handler ? d->m_session_title_handler(file_path) : QString(); !session_title.isEmpty()) {
    if (!window_title.isEmpty())
      window_title.append(dash_sep);
    window_title.append(session_title);
  }

  if (!window_title.isEmpty())
    window_title.append(dash_sep);

  window_title.append(IDE_DISPLAY_NAME);
  window->window()->setWindowTitle(window_title);
  window->window()->setWindowFilePath(file_path.path());

  if constexpr (HostOsInfo::isMacHost()) {
    if (document)
      window->window()->setWindowModified(document->isModified());
    else
      window->window()->setWindowModified(false);
  }
}

auto EditorManagerPrivate::updateWindowTitle() -> void
{
  const auto main_area = mainEditorArea();
  const auto document = main_area->currentDocument();
  updateWindowTitleForDocument(document, main_area->window());
}

auto EditorManagerPrivate::gotoNextDocHistory() -> void
{
  if (const auto dialog = windowPopup(); dialog->isVisible()) {
    dialog->selectNextEditor();
  } else {
    const auto view = currentEditorView();
    dialog->setEditors(d->m_global_history, view);
    dialog->selectNextEditor();
    showPopupOrSelectDocument();
  }
}

auto EditorManagerPrivate::gotoPreviousDocHistory() -> void
{
  if (const auto dialog = windowPopup(); dialog->isVisible()) {
    dialog->selectPreviousEditor();
  } else {
    const auto view = currentEditorView();
    dialog->setEditors(d->m_global_history, view);
    dialog->selectPreviousEditor();
    showPopupOrSelectDocument();
  }
}

auto EditorManagerPrivate::gotoLastEditLocation() -> void
{
  currentEditorView()->goToEditLocation(d->m_global_last_edit_location);
}

auto EditorManagerPrivate::gotoNextSplit() -> void
{
  const auto view = currentEditorView();

  if (!view)
    return;

  auto next_view = view->findNextView();

  if (!next_view) {
    // we are in the "last" view in this editor area
    auto index = -1;
    const auto area = findEditorArea(view, &index);
    QTC_ASSERT(area, return);
    QTC_ASSERT(index >= 0 && index < d->m_editor_areas.size(), return);
    // find next editor area. this might be the same editor area if there's only one.
    auto next_index = index + 1;

    if (next_index >= d->m_editor_areas.size())
      next_index = 0;

    next_view = d->m_editor_areas.at(next_index)->findFirstView();
  }

  if (QTC_GUARD(next_view))
    activateView(next_view);
}

auto EditorManagerPrivate::gotoPreviousSplit() -> void
{
  const auto view = currentEditorView();

  if (!view)
    return;

  auto prev_view = view->findPreviousView();

  if (!prev_view) {
    // we are in the "first" view in this editor area
    auto index = -1;
    const auto area = findEditorArea(view, &index);
    QTC_ASSERT(area, return);
    QTC_ASSERT(index >= 0 && index < d->m_editor_areas.size(), return);
    // find previous editor area. this might be the same editor area if there's only one.
    auto next_index = index - 1;
    if (next_index < 0)
      next_index = d->m_editor_areas.count() - 1;
    prev_view = d->m_editor_areas.at(next_index)->findLastView();
  }

  if (QTC_GUARD(prev_view))
    activateView(prev_view);
}

auto EditorManagerPrivate::makeCurrentEditorWritable() -> void
{
  if (const auto doc = EditorManager::currentDocument())
    makeFileWritable(doc);
}

auto EditorManagerPrivate::setPlaceholderText(const QString &text) -> void
{
  if (d->m_placeholder_text == text)
    return;
  d->m_placeholder_text = text;
  emit d->placeholderTextChanged(d->m_placeholder_text);
}

auto EditorManagerPrivate::placeholderText() -> QString
{
  return d->m_placeholder_text;
}

auto EditorManagerPrivate::vcsOpenCurrentEditor() -> void
{
  const auto document = EditorManager::currentDocument();
  if (!document)
    return;

  const auto directory = document->filePath().parentDir();
  const auto version_control = VcsManager::findVersionControlForDirectory(directory);
  if (!version_control || version_control->openSupportMode(document->filePath()) == IVersionControl::NoOpen)
    return;

  if (!version_control->vcsOpen(document->filePath())) {
    QMessageBox::warning(ICore::dialogParent(), tr("Cannot Open File"), tr("Cannot open the file for editing with VCS."));
  }
}

auto EditorManagerPrivate::handleDocumentStateChange() const -> void
{
  updateActions();
  const auto document = qobject_cast<IDocument*>(sender());
  if (!document->isModified())
    document->removeAutoSaveFile();
  if (EditorManager::currentDocument() == document) emit m_instance->currentDocumentStateChanged();
  emit m_instance->documentStateChanged(document);
}

auto EditorManagerPrivate::editorAreaDestroyed(const QObject *area) const -> void
{
  const auto active_win = QApplication::activeWindow();
  EditorArea *new_active_area = nullptr;

  for (auto i = 0; i < d->m_editor_areas.size(); ++i) {
    if (const auto r = d->m_editor_areas.at(i); r == area) {
      d->m_editor_areas.removeAt(i);
      --i; // we removed the current one
    } else if (r->window() == active_win) {
      new_active_area = r;
    }
  }

  // check if the destroyed editor area had the current view or current editor
  if (d->m_current_editor || d->m_current_view && d->m_current_view->parentSplitterOrView() != area)
    return;

  // we need to set a new current editor or view
  if (!new_active_area) {
    // some window managers behave weird and don't activate another window
    // or there might be a Qt Creator toplevel activated that doesn't have editor windows
    new_active_area = d->m_editor_areas.first();
  }

  // check if the focusWidget points to some view
  SplitterOrView *focus_splitter_or_view = nullptr;
  auto candidate = new_active_area->focusWidget();
  while (candidate && candidate != new_active_area) {
    if ((focus_splitter_or_view = qobject_cast<SplitterOrView*>(candidate)))
      break;
    candidate = candidate->parentWidget();
  }

  // focusWidget might have been 0
  if (!focus_splitter_or_view)
    focus_splitter_or_view = new_active_area->findFirstView()->parentSplitterOrView();

  QTC_ASSERT(focus_splitter_or_view, focus_splitter_or_view = new_active_area);
  auto focus_view = focus_splitter_or_view->findFirstView(); // can be just focusSplitterOrView
  QTC_ASSERT(focus_view, focus_view = new_active_area->findFirstView());
  QTC_ASSERT(focus_view, return);
  activateView(focus_view);
}

auto EditorManagerPrivate::autoSave() -> void
{
  QStringList errors;

  // FIXME: the saving should be staggered
  for (const auto documents = DocumentModel::openedDocuments(); const auto document : documents) {
    if (!document->isModified() || !document->shouldAutoSave())
      continue;

    const auto save_name = autoSaveName(document->filePath());

    if (const auto save_path = save_name.absolutePath(); document->filePath().isEmpty() || !save_path.isWritableDir()) // FIXME: save them to a dedicated directory
      continue;

    QString error_string;
    if (!document->autoSave(&error_string, save_name))
      errors << error_string;
  }

  if (!errors.isEmpty())
    QMessageBox::critical(ICore::dialogParent(), tr("File Error"), errors.join(QLatin1Char('\n')));

  emit m_instance->autoSaved();
}

auto EditorManagerPrivate::handleContextChange(const QList<IContext*> &context) -> void
{
  if constexpr (debug_editor_manager)
    qDebug() << Q_FUNC_INFO;

  d->m_scheduled_current_editor = nullptr;

  IEditor *editor = nullptr;
  for (const auto c : context)
    if ((editor = qobject_cast<IEditor*>(c)))
      break;

  if (editor && editor != d->m_current_editor) {
    d->m_scheduled_current_editor = editor;
    // Delay actually setting the current editor to after the current event queue has been handled
    // Without doing this, e.g. clicking into projects tree or locator would always open editors
    // in the main window. That is because clicking anywhere in the main window (even over e.g.
    // the locator line edit) first activates the window and sets focus to its focus widget.
    // Only afterwards the focus is shifted to the widget that received the click.

    // 1) During this event handling, focus landed in the editor.
    // 2) During the following event handling, focus might change to the project tree.
    // So, delay setting the current editor by two events.
    // If focus changes to e.g. the project tree in (2), then m_scheduledCurrentEditor is set to
    // nullptr, and the setCurrentEditorFromContextChange call becomes a no-op.
    QMetaObject::invokeMethod(d, [] {
      QMetaObject::invokeMethod(d, &EditorManagerPrivate::setCurrentEditorFromContextChange, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
  } else {
    updateActions();
  }
}

auto EditorManagerPrivate::copyFilePathFromContextMenu() -> void
{
  if (!d->m_context_menu_entry)
    return;

  QApplication::clipboard()->setText(d->m_context_menu_entry->fileName().toUserOutput());
}

auto EditorManagerPrivate::copyLocationFromContextMenu() const -> void
{
  const auto action = qobject_cast<const QAction*>(sender());

  if (!d->m_context_menu_entry || !action)
    return;

  const QString text = d->m_context_menu_entry->fileName().toUserOutput() + QLatin1Char(':') + action->data().toString();
  QApplication::clipboard()->setText(text);
}

auto EditorManagerPrivate::copyFileNameFromContextMenu() -> void
{
  if (!d->m_context_menu_entry)
    return;

  QApplication::clipboard()->setText(d->m_context_menu_entry->fileName().fileName());
}

auto EditorManagerPrivate::saveDocumentFromContextMenu() -> void
{
  if (const auto document = d->m_context_menu_entry ? d->m_context_menu_entry->document : nullptr)
    saveDocument(document);
}

auto EditorManagerPrivate::saveDocumentAsFromContextMenu() -> void
{
  if (const auto document = d->m_context_menu_entry ? d->m_context_menu_entry->document : nullptr)
    saveDocumentAs(document);
}

auto EditorManagerPrivate::revertToSavedFromContextMenu() -> void
{
  if (const auto document = d->m_context_menu_entry ? d->m_context_menu_entry->document : nullptr)
    revertToSaved(document);
}

auto EditorManagerPrivate::closeEditorFromContextMenu() -> void
{
  if (d->m_context_menu_editor) {
    closeEditorOrDocument(d->m_context_menu_editor);
  } else {
    if (auto document = d->m_context_menu_entry ? d->m_context_menu_entry->document : nullptr)
      EditorManager::closeDocuments({document});
  }
}

auto EditorManagerPrivate::closeOtherDocumentsFromContextMenu() -> void
{
  const auto document = d->m_context_menu_entry ? d->m_context_menu_entry->document : nullptr;
  EditorManager::closeOtherDocuments(document);
}

auto EditorManagerPrivate::saveDocument(IDocument *document) -> bool
{
  if (!document)
    return false;

  document->checkPermissions();

  if (const auto file_name = document->filePath().toString(); file_name.isEmpty())
    return saveDocumentAs(document);

  auto success = false;
  bool is_read_only;

  emit m_instance->aboutToSave(document);

  // try saving, no matter what isReadOnly tells us
  success = DocumentManager::saveDocument(document, FilePath(), &is_read_only);
  if (!success && is_read_only) {
    const auto answer = makeFileWritable(document);
    if (answer == failed)
      return false;
    if (answer == saved_as)
      return true;
    document->checkPermissions();
    success = DocumentManager::saveDocument(document);
  }

  if (success) {
    addDocumentToRecentFiles(document);
    emit m_instance->saved(document);
  }

  return success;
}

auto EditorManagerPrivate::saveDocumentAs(IDocument *document) -> bool
{
  if (!document)
    return false;

  const auto absolute_file_path = DocumentManager::getSaveAsFileName(document);

  if (absolute_file_path.isEmpty())
    return false;

  if (DocumentManager::filePathKey(absolute_file_path, DocumentManager::ResolveLinks) != DocumentManager::filePathKey(document->filePath(), DocumentManager::ResolveLinks)) {
    // close existing editors for the new file name
    if (auto other_document = DocumentModel::documentForFilePath(absolute_file_path))
      EditorManager::closeDocuments({other_document}, false);
  }

  emit m_instance->aboutToSave(document);
  const auto success = DocumentManager::saveDocument(document, absolute_file_path);
  document->checkPermissions();

  // TODO: There is an issue to be treated here. The new file might be of a different mime
  // type than the original and thus require a different editor. An alternative strategy
  // would be to close the current editor and open a new appropriate one, but this is not
  // a good way out either (also the undo stack would be lost). Perhaps the best is to
  // re-think part of the editors design.

  if (success) {
    addDocumentToRecentFiles(document);
    emit m_instance->saved(document);
  }

  updateActions();
  return success;
}

auto EditorManagerPrivate::closeAllEditorsExceptVisible() -> void
{
  DocumentModelPrivate::removeAllSuspendedEntries(DocumentModelPrivate::pinned_file_removal_policy::do_not_remove_pinned_files);
  auto documents_to_close = DocumentModel::openedDocuments();

  // Remove all pinned files from the list of files to close.
  documents_to_close = filtered(documents_to_close, [](IDocument *document) {
    const auto entry = DocumentModel::entryForDocument(document);
    return !entry->pinned;
  });

  foreach(IEditor *editor, EditorManager::visibleEditors())
    documents_to_close.removeAll(editor->document());

  EditorManager::closeDocuments(documents_to_close, true);
}

auto EditorManagerPrivate::revertToSaved(IDocument *document) -> void
{
  if (!document)
    return;

  const auto file_name = document->filePath().toString();

  if (file_name.isEmpty())
    return;

  if (document->isModified()) {
    QMessageBox msg_box(QMessageBox::Question, tr("Revert to Saved"), tr("You will lose your current changes if you proceed reverting %1.").arg(QDir::toNativeSeparators(file_name)), QMessageBox::Yes | QMessageBox::No, ICore::dialogParent());
    msg_box.button(QMessageBox::Yes)->setText(tr("Proceed"));
    msg_box.button(QMessageBox::No)->setText(tr("Cancel"));

    const QPushButton *diff_button = nullptr;
    const auto diff_service = DiffService::instance();

    if (diff_service)
      diff_button = msg_box.addButton(tr("Cancel && &Diff"), QMessageBox::RejectRole);

    msg_box.setDefaultButton(QMessageBox::No);
    msg_box.setEscapeButton(QMessageBox::No);
    if (msg_box.exec() == QMessageBox::No)
      return;

    if (diff_service && msg_box.clickedButton() == diff_button) {
      diff_service->diffModifiedFiles(QStringList(file_name));
      return;
    }
  }

  QString error_string;
  if (!document->reload(&error_string, IDocument::FlagReload, IDocument::TypeContents))
    QMessageBox::critical(ICore::dialogParent(), tr("File Error"), error_string);
}

auto EditorManagerPrivate::autoSuspendDocuments() -> void
{
  if (!d->m_settings.auto_suspend_enabled)
    return;

  const auto visible_documents = Utils::transform<QSet>(EditorManager::visibleEditors(), &IEditor::document);
  auto kept_editor_count = 0;
  QList<IDocument*> documents_to_suspend;

  for (const auto &edit_location : qAsConst(d->m_global_history)) {
    IDocument *document = edit_location.document;
    if (!document || !document->isSuspendAllowed() || document->isModified() || document->isTemporary() || document->filePath().isEmpty() || visible_documents.contains(document))
      continue;
    if (kept_editor_count >= d->m_settings.auto_suspend_min_document_count)
      documents_to_suspend.append(document);
    else
      ++kept_editor_count;
  }

  closeEditors(DocumentModel::editorsForDocuments(documents_to_suspend), close_flag::suspend);
}

auto EditorManagerPrivate::openTerminal() -> void
{
  if (!d->m_context_menu_entry || d->m_context_menu_entry->fileName().isEmpty())
    return;

  FileUtils::openTerminal(d->m_context_menu_entry->fileName().parentDir());
}

auto EditorManagerPrivate::findInDirectory() -> void
{
  if (!d->m_context_menu_entry || d->m_context_menu_entry->fileName().isEmpty())
    return;

  const auto path = d->m_context_menu_entry->fileName();
  emit m_instance->findOnFileSystemRequest((path.isDir() ? path : path.parentDir()).toString());
}

auto EditorManagerPrivate::togglePinned() -> void
{
  if (!d->m_context_menu_entry || d->m_context_menu_entry->fileName().isEmpty())
    return;

  const auto currently_pinned = d->m_context_menu_entry->pinned;
  DocumentModelPrivate::setPinned(d->m_context_menu_entry, !currently_pinned);
}

auto EditorManagerPrivate::split(const Qt::Orientation orientation) -> void
{
  if (const auto view = currentEditorView())
    view->parentSplitterOrView()->split(orientation);

  updateActions();
}

auto EditorManagerPrivate::removeCurrentSplit() -> void
{
  const auto view_to_close = currentEditorView();

  QTC_ASSERT(view_to_close, return);
  QTC_ASSERT(!qobject_cast<EditorArea *>(view_to_close->parentSplitterOrView()), return);

  closeView(view_to_close);
  updateActions();
}

auto EditorManagerPrivate::removeAllSplits() -> void
{
  const auto view = currentEditorView();
  QTC_ASSERT(view, return);
  const auto current_area = findEditorArea(view);
  QTC_ASSERT(current_area, return);
  current_area->unsplitAll();
}

auto EditorManagerPrivate::setCurrentEditorFromContextChange() -> void
{
  if (!d->m_scheduled_current_editor)
    return;

  IEditor *new_current = d->m_scheduled_current_editor;
  d->m_scheduled_current_editor = nullptr;
  setCurrentEditor(new_current);
}

auto EditorManagerPrivate::currentEditorView() -> EditorView*
{
  EditorView *view = d->m_current_view;

  if (!view) {
    if (d->m_current_editor) {
      view = viewForEditor(d->m_current_editor);
      QTC_ASSERT(view, view = d->m_editor_areas.first()->findFirstView());
    }
    QTC_CHECK(view);
    if (!view) {
      // should not happen, we should always have either currentview or currentdocument
      for(const auto area: d->m_editor_areas) {
        if (area->window()->isActiveWindow()) {
          view = area->findFirstView();
          break;
        }
      }
      QTC_ASSERT(view, view = d->m_editor_areas.first()->findFirstView());
    }
  }

  return view;
}

/*!
    Returns the pointer to the instance. Only use for connecting to signals.
*/
auto EditorManager::instance() -> EditorManager*
{
  return m_instance;
}

/*!
    \internal
*/
EditorManager::EditorManager(QObject *parent) : QObject(parent)
{
  m_instance = this;
  d = new EditorManagerPrivate(this);
  d->init();
}

/*!
    \internal
*/
EditorManager::~EditorManager()
{
  delete d;
  m_instance = nullptr;
}

/*!
    Returns the document of the currently active editor.

    \sa currentEditor()
*/
auto EditorManager::currentDocument() -> IDocument*
{
  return d->m_current_editor ? d->m_current_editor->document() : nullptr;
}

/*!
    Returns the currently active editor.

    \sa currentDocument()
*/
auto EditorManager::currentEditor() -> IEditor*
{
  return d->m_current_editor;
}

/*!
    Closes all open editors. If \a askAboutModifiedEditors is \c true, prompts
    users to save their changes before closing the editors.

    Returns whether all editors were closed.
*/
auto EditorManager::closeAllEditors(const bool ask_about_modified_editors) -> bool
{
  DocumentModelPrivate::removeAllSuspendedEntries();
  return closeDocuments(DocumentModel::openedDocuments(), ask_about_modified_editors);
}

/*!
    Closes all open documents except \a document and pinned files.
*/
auto EditorManager::closeOtherDocuments(IDocument *document) -> void
{
  DocumentModelPrivate::removeAllSuspendedEntries(DocumentModelPrivate::pinned_file_removal_policy::do_not_remove_pinned_files);
  auto documents_to_close = DocumentModel::openedDocuments();

  // Remove all pinned files from the list of files to close.
  documents_to_close = filtered(documents_to_close, [](IDocument *document) {
    const auto entry = DocumentModel::entryForDocument(document);
    return !entry->pinned;
  });

  documents_to_close.removeAll(document);
  closeDocuments(documents_to_close, true);
}

/*!
    Closes all open documents except pinned files.

    Returns whether all editors were closed.
*/
auto EditorManager::closeAllDocuments() -> bool
{
  // Only close the files that aren't pinned.
  const auto entries_to_close = filtered(DocumentModel::entries(), equal(&DocumentModel::Entry::pinned, false));
  return closeDocuments(entries_to_close);
}

/*!
    \internal
*/
auto EditorManager::slotCloseCurrentEditorOrDocument() -> void
{
  if (!d->m_current_editor)
    return;

  addCurrentPositionToNavigationHistory();
  d->closeEditorOrDocument(d->m_current_editor);
}

/*!
    Closes all open documents except the current document.
*/
auto EditorManager::closeOtherDocuments() -> void
{
  closeOtherDocuments(currentDocument());
}

static auto assignAction(QAction *self, const QAction *other) -> void
{
  self->setText(other->text());
  self->setIcon(other->icon());
  self->setShortcut(other->shortcut());
  self->setEnabled(other->isEnabled());
  self->setIconVisibleInMenu(other->isIconVisibleInMenu());
}

/*!
    Adds save, close and other editor context menu items for the document
    \a entry and editor \a editor to the context menu \a contextMenu.
*/
auto EditorManager::addSaveAndCloseEditorActions(QMenu *context_menu, DocumentModel::Entry *entry, IEditor *editor) -> void
{
  QTC_ASSERT(context_menu, return);

  d->m_context_menu_entry = entry;
  d->m_context_menu_editor = editor;

  const auto file_path = entry ? entry->fileName() : FilePath();
  const auto copy_actions_enabled = !file_path.isEmpty();

  d->m_copy_file_path_context_action->setEnabled(copy_actions_enabled);
  d->m_copy_location_context_action->setEnabled(copy_actions_enabled);
  d->m_copy_file_name_context_action->setEnabled(copy_actions_enabled);

  context_menu->addAction(d->m_copy_file_path_context_action);

  if (editor && entry) {
    if (const auto line_number = editor->currentLine()) {
      d->m_copy_location_context_action->setData(QVariant(line_number));
      context_menu->addAction(d->m_copy_location_context_action);
    }
  }

  context_menu->addAction(d->m_copy_file_name_context_action);
  context_menu->addSeparator();

  assignAction(d->m_save_current_editor_context_action, ActionManager::command(SAVE)->action());
  assignAction(d->m_save_as_current_editor_context_action, ActionManager::command(SAVEAS)->action());
  assignAction(d->m_revert_to_saved_current_editor_context_action, ActionManager::command(REVERTTOSAVED)->action());

  const auto document = entry ? entry->document : nullptr;

  EditorManagerPrivate::setupSaveActions(document, d->m_save_current_editor_context_action, d->m_save_as_current_editor_context_action, d->m_revert_to_saved_current_editor_context_action);

  context_menu->addAction(d->m_save_current_editor_context_action);
  context_menu->addAction(d->m_save_as_current_editor_context_action);
  context_menu->addAction(ActionManager::command(SAVEALL)->action());
  context_menu->addAction(d->m_revert_to_saved_current_editor_context_action);
  context_menu->addSeparator();

  const auto quoted_display_name = entry ? quoteAmpersands(entry->displayName()) : QString();

  d->m_close_current_editor_context_action->setText(entry ? tr("Close \"%1\"").arg(quoted_display_name) : tr("Close Editor"));
  d->m_close_other_documents_context_action->setText(entry ? tr("Close All Except \"%1\"").arg(quoted_display_name) : tr("Close Other Editors"));
  d->m_close_current_editor_context_action->setEnabled(entry != nullptr);
  d->m_close_other_documents_context_action->setEnabled(entry != nullptr);
  d->m_close_all_editors_context_action->setEnabled(!DocumentModel::entries().isEmpty());
  d->m_close_all_editors_except_visible_context_action->setEnabled(EditorManagerPrivate::visibleDocumentsCount() < DocumentModel::entries().count());

  context_menu->addAction(d->m_close_current_editor_context_action);
  context_menu->addAction(d->m_close_all_editors_context_action);
  context_menu->addAction(d->m_close_other_documents_context_action);
  context_menu->addAction(d->m_close_all_editors_except_visible_context_action);
}

/*!
    Adds the pin editor menu items for the document \a entry to the context menu
    \a contextMenu.
*/
auto EditorManager::addPinEditorActions(QMenu *context_menu, const DocumentModel::Entry *entry) -> void
{
  const auto quoted_display_name = entry ? quoteAmpersands(entry->displayName()) : QString();
  if (entry) {
    d->m_pin_action->setText(entry->pinned ? tr("Unpin \"%1\"").arg(quoted_display_name) : tr("Pin \"%1\"").arg(quoted_display_name));
  } else {
    d->m_pin_action->setText(tr("Pin Editor"));
  }
  d->m_pin_action->setEnabled(entry != nullptr);
  context_menu->addAction(d->m_pin_action);
}

/*!
    Adds the native directory handling and open with menu items for the document
    \a entry to the context menu \a contextMenu.
*/
auto EditorManager::addNativeDirAndOpenWithActions(QMenu *context_menu, DocumentModel::Entry *entry) -> void
{
  QTC_ASSERT(context_menu, return);

  d->m_context_menu_entry = entry;
  const auto enabled = entry && !entry->fileName().isEmpty();
  d->m_open_graphical_shell_context_action->setEnabled(enabled);
  d->m_show_in_file_system_view_context_action->setEnabled(enabled);
  d->m_open_terminal_action->setEnabled(enabled);
  d->m_find_in_directory_action->setEnabled(enabled);
  d->m_file_properties_action->setEnabled(enabled);

  context_menu->addAction(d->m_open_graphical_shell_context_action);
  context_menu->addAction(d->m_show_in_file_system_view_context_action);
  context_menu->addAction(d->m_open_terminal_action);
  context_menu->addAction(d->m_find_in_directory_action);
  context_menu->addAction(d->m_file_properties_action);

  const auto open_with = context_menu->addMenu(tr("Open With"));
  open_with->setEnabled(enabled);

  if (enabled)
    populateOpenWithMenu(open_with, entry->fileName());
}

/*!
    Populates the \uicontrol {Open With} menu \a menu with editors that are
    suitable for opening the document \a filePath.
*/
auto EditorManager::populateOpenWithMenu(QMenu *menu, const FilePath &file_path) -> void
{
  menu->clear();

  const auto factories = IEditorFactory::preferredEditorTypes(file_path);
  const auto any_matches = !factories.empty();

  if (any_matches) {
    // Add all suitable editors
    for (const auto editor_type : factories) {
      const auto editor_id = editor_type->id();
      // Add action to open with this very editor factory
      const auto action_title = editor_type->displayName();
      const auto action = menu->addAction(action_title);
      // Below we need QueuedConnection because otherwise, if a qrc file
      // is inside of a qrc file itself, and the qrc editor opens the Open with menu,
      // crashes happen, because the editor instance is deleted by openEditorWith
      // while the menu is still being processed.
      connect(action, &QAction::triggered, d, [file_path, editor_id] {
        if (const auto type = EditorType::editorTypeForId(editor_id); type && type->asExternalEditor())
          openExternalEditor(file_path, editor_id);
        else
          EditorManagerPrivate::openEditorWith(file_path, editor_id);
      }, Qt::QueuedConnection);
    }
  }

  menu->setEnabled(any_matches);
}

/*!
    Returns reload behavior settings.
*/
auto EditorManager::reloadSetting() -> IDocument::ReloadSetting
{
  return d->m_settings.reload_setting;
}

/*!
    \internal

    Sets editor reaload behavior settings to \a behavior.
*/
auto EditorManager::setReloadSetting(const IDocument::ReloadSetting behavior) -> void
{
  d->m_settings.reload_setting = behavior;
}

/*!
    Saves the current document.
*/
auto EditorManager::saveDocument() -> void
{
  EditorManagerPrivate::saveDocument(currentDocument());
}

/*!
    Saves the current document under a different file name.
*/
auto EditorManager::saveDocumentAs() -> void
{
  EditorManagerPrivate::saveDocumentAs(currentDocument());
}

/*!
    Reverts the current document to its last saved state.
*/
auto EditorManager::revertToSaved() -> void
{
  EditorManagerPrivate::revertToSaved(currentDocument());
}

/*!
    Closes the documents specified by \a entries.

    Returns whether all documents were closed.
*/
auto EditorManager::closeDocuments(const QList<DocumentModel::Entry*> &entries) -> bool
{
  QList<IDocument*> documents_to_close;

  for (const auto entry : entries) {
    if (!entry)
      continue;
    if (entry->isSuspended)
      DocumentModelPrivate::removeEntry(entry);
    else
      documents_to_close << entry->document;
  }

  return closeDocuments(documents_to_close);
}

/*!
    Closes the editors specified by \a editorsToClose. If
    \a askAboutModifiedEditors is \c true, prompts users
    to save their changes before closing the editor.

    Returns whether all editors were closed.

    Usually closeDocuments() is the better alternative.

    \sa closeDocuments()
*/
auto EditorManager::closeEditors(const QList<IEditor*> &editors_to_close, const bool ask_about_modified_editors) -> bool
{
  return EditorManagerPrivate::closeEditors(editors_to_close, ask_about_modified_editors ? EditorManagerPrivate::close_flag::close_with_asking : EditorManagerPrivate::close_flag::close_without_asking);
}

/*!
    Activates an editor for the document specified by \a entry in the active
    split using the specified \a flags.
*/
auto EditorManager::activateEditorForEntry(DocumentModel::Entry *entry, OpenEditorFlags flags) -> void
{
  QTC_CHECK(!(flags & EditorManager::AllowExternalEditor));

  EditorManagerPrivate::activateEditorForEntry(EditorManagerPrivate::currentEditorView(), entry, flags);
}

/*!
    Activates the \a editor in the active split using the specified \a flags.

    \sa currentEditor()
*/
auto EditorManager::activateEditor(IEditor *editor, const OpenEditorFlags flags) -> void
{
  QTC_CHECK(!(flags & EditorManager::AllowExternalEditor));
  QTC_ASSERT(editor, return);

  auto view = EditorManagerPrivate::viewForEditor(editor);

  // an IEditor doesn't have to belong to a view, it might be kept in storage by the editor model
  if (!view)
    view = EditorManagerPrivate::currentEditorView();

  EditorManagerPrivate::activateEditor(view, editor, flags);
}

/*!
    Activates an editor for the \a document in the active split using the
    specified \a flags.
*/
auto EditorManager::activateEditorForDocument(IDocument *document, const OpenEditorFlags flags) -> IEditor*
{
  QTC_CHECK(!(flags & EditorManager::AllowExternalEditor));

  return EditorManagerPrivate::activateEditorForDocument(EditorManagerPrivate::currentEditorView(), document, flags);
}

/*!
    Opens the document specified by \a filePath using the editor type \a
    editorId and the specified \a flags.

    If \a editorId is \c Id(), the editor type is derived from the file's MIME
    type.

    If \a newEditor is not \c nullptr, and a new editor instance was created,
    it is set to \c true. If an existing editor instance was used, it is set
    to \c false.

    \sa openEditorAt()
    \sa openEditorWithContents()
    \sa openExternalEditor()
*/
auto EditorManager::openEditor(const FilePath &file_path, const Id editor_id, const OpenEditorFlags flags, bool *new_editor) -> IEditor*
{
  checkEditorFlags(flags);

  if (flags & OpenInOtherSplit)
    gotoOtherSplit();

  return EditorManagerPrivate::openEditor(EditorManagerPrivate::currentEditorView(), file_path, editor_id, flags, new_editor);
}

/*!
    Opens the document specified by \a link using the editor type \a
    editorId and the specified \a flags.

    Moves the text cursor to the \e line and \e column specified in \a link.

    If \a editorId is \c Id(), the editor type is derived from the file's MIME
    type.

    If \a newEditor is not \c nullptr, and a new editor instance was created,
    it is set to \c true. If an existing editor instance was used, it is set
    to \c false.

    \sa openEditor()
    \sa openEditorAtSearchResult()
    \sa openEditorWithContents()
    \sa openExternalEditor()
    \sa IEditor::gotoLine()
*/
auto EditorManager::openEditorAt(const Link &link, const Id editor_id, const OpenEditorFlags flags, bool *new_editor) -> IEditor*
{
  checkEditorFlags(flags);

  if (flags & OpenInOtherSplit)
    gotoOtherSplit();

  return EditorManagerPrivate::openEditorAt(EditorManagerPrivate::currentEditorView(), link, editor_id, flags, new_editor);
}

/*!
    Opens the document at the position of the search result \a item using the
    editor type \a editorId and the specified \a flags.

    If \a editorId is \c Id(), the editor type is derived from the file's MIME
    type.

    If \a newEditor is not \c nullptr, and a new editor instance was created,
    it is set to \c true. If an existing editor instance was used, it is set to
    \c false.

    \sa openEditorAt()
*/
auto EditorManager::openEditorAtSearchResult(const SearchResultItem &item, const Id editor_id, const OpenEditorFlags flags, bool *new_editor) -> void
{
  if (item.path().empty()) {
    openEditor(FilePath::fromUserInput(item.lineText()), editor_id, flags, new_editor);
    return;
  }

  openEditorAt({FilePath::fromUserInput(item.path().first()), item.mainRange().begin.line, item.mainRange().begin.column}, editor_id, flags, new_editor);
}

/*!
    Returns whether \a filePath is an auto-save file created by \QC.
*/
auto EditorManager::isAutoSaveFile(const QString &file_name) -> bool
{
  return file_name.endsWith(".autosave");
}

auto EditorManager::autoSaveAfterRefactoring() -> bool
{
  return EditorManagerPrivate::autoSaveAfterRefactoring();
}

/*!
    Opens the document specified by \a filePath in the external editor specified
    by \a editorId.

    Returns \c false and displays an error message if \a editorId is not the ID
    of an external editor or the external editor cannot be opened.

    \sa openEditor()
*/
auto EditorManager::openExternalEditor(const FilePath &file_path, const Id editor_id) -> bool
{
  const auto ee = findOrDefault(IExternalEditor::allExternalEditors(), equal(&IExternalEditor::id, editor_id));

  if (!ee)
    return false;

  QString error_message;
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  const auto ok = ee->startEditor(file_path, &error_message);
  QApplication::restoreOverrideCursor();

  if (!ok)
    QMessageBox::critical(ICore::dialogParent(), tr("Opening File"), error_message);

  return ok;
}

/*!
    Adds \a listener to the hooks that are asked if editors may be closed.

    When an editor requests to close, all listeners are called. If one of the
    calls returns \c false, the process is aborted and the event is ignored. If
    all calls return \c true, editorAboutToClose() is emitted and the event is
    accepted.
*/
auto EditorManager::addCloseEditorListener(const std::function<bool (IEditor *)> &listener) -> void
{
  d->m_close_editor_listeners.append(listener);
}

/*!
    Asks the user for a list of files to open and returns the choice.

    \sa DocumentManager::getOpenFileNames()
*/
auto EditorManager::getOpenFilePaths() -> FilePaths
{
  QString selected_filter;
  const auto &file_filters = DocumentManager::fileDialogFilter(&selected_filter);
  return DocumentManager::getOpenFileNames(file_filters, {}, &selected_filter);
}

static auto makeTitleUnique(QString *title_pattern) -> QString
{
  QString title;
  if (title_pattern) {
    constexpr QChar dollar = QLatin1Char('$');

    auto &base = *title_pattern;

    if (base.isEmpty())
      base = "unnamed$";

    if (base.contains(dollar)) {
      auto i = 1;
      QSet<QString> docnames;
      for (const auto entries = DocumentModel::entries(); const DocumentModel::Entry *entry : entries) {
        auto name = entry->fileName().toString();
        if (name.isEmpty())
          name = entry->displayName();
        else
          name = QFileInfo(name).completeBaseName();
        docnames << name;
      }

      do {
        title = base;
        title.replace(QString(dollar), QString::number(i++));
      } while (docnames.contains(title));
    } else {
      title = *title_pattern;
    }
    *title_pattern = title;
  }

  return title;
}

/*!
    Opens \a contents in an editor of the type \a editorId using the specified
    \a flags.

    The editor is given a display name based on \a titlePattern. If a non-empty
    \a uniqueId is specified and an editor with that unique ID is found, it is
    re-used. Otherwise, a new editor with that unique ID is created.

    Returns the new or re-used editor.

    \sa clearUniqueId()
*/
auto EditorManager::openEditorWithContents(const Id editor_id, QString *title_pattern, const QByteArray &contents, const QString &unique_id, const OpenEditorFlags flags) -> IEditor*
{
  QTC_CHECK(!(flags & EditorManager::AllowExternalEditor));
  checkEditorFlags(flags);

  if constexpr (debug_editor_manager)
    qDebug() << Q_FUNC_INFO << editor_id.name() << title_pattern << unique_id << contents;

  if (flags & OpenInOtherSplit)
    gotoOtherSplit();

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  const ExecuteOnDestruction app_restore_cursor(&QApplication::restoreOverrideCursor);

  Q_UNUSED(app_restore_cursor)
  const auto title = makeTitleUnique(title_pattern);

  IEditor *edt = nullptr;

  if (!unique_id.isEmpty()) {
    foreach(IDocument *document, DocumentModel::openedDocuments()) if (document->property(scratch_buffer_key).toString() == unique_id) {
      edt = DocumentModel::editorsForDocument(document).constFirst();
      document->setContents(contents);
      if (!title.isEmpty())
        edt->document()->setPreferredDisplayName(title);
      activateEditor(edt, flags);
      return edt;
    }
  }

  const auto file_path = FilePath::fromString(title);
  auto factories = EditorManagerPrivate::findFactories(editor_id, file_path);

  if (factories.isEmpty())
    return nullptr;

  edt = EditorManagerPrivate::createEditor(factories.first(), file_path);
  if (!edt)
    return nullptr;

  if (!edt->document()->setContents(contents)) {
    delete edt;
    edt = nullptr;
    return nullptr;
  }

  if (!unique_id.isEmpty())
    edt->document()->setProperty(scratch_buffer_key, unique_id);

  if (!title.isEmpty())
    edt->document()->setPreferredDisplayName(title);

  EditorManagerPrivate::addEditor(edt);
  activateEditor(edt, flags);
  return edt;
}

/*!
    Returns whether the document specified by \a filePath should be opened even
    though it is big. Depending on the settings this might ask the user to
    decide whether the file should be opened.
*/
auto EditorManager::skipOpeningBigTextFile(const FilePath &file_path) -> bool
{
  return EditorManagerPrivate::skipOpeningBigTextFile(file_path);
}

/*!
    Clears the unique ID of \a document.

    \sa openEditorWithContents()
*/
auto EditorManager::clearUniqueId(IDocument *document) -> void
{
  document->setProperty(scratch_buffer_key, QVariant());
}

/*!
    Saves the changes in \a document.

    Returns whether the operation was successful.
*/
auto EditorManager::saveDocument(IDocument *document) -> bool
{
  return EditorManagerPrivate::saveDocument(document);
}

/*!
    \internal
*/
auto EditorManager::hasSplitter() -> bool
{
  const auto view = EditorManagerPrivate::currentEditorView();
  QTC_ASSERT(view, return false);
  const auto area = EditorManagerPrivate::findEditorArea(view);
  QTC_ASSERT(area, return false);
  return area->isSplitter();
}

/*!
    Returns the list of visible editors.
*/
auto EditorManager::visibleEditors() -> QList<IEditor*>
{
  QList<IEditor*> editors;

  for(const auto area: d->m_editor_areas) {
    if (area->isSplitter()) {
      const auto first_view = area->findFirstView();
      if (auto view = first_view) {
        do {
          if (view->currentEditor())
            editors.append(view->currentEditor());
          view = view->findNextView();
          QTC_ASSERT(view != first_view, break); // we start with firstView and shouldn't have cycles
        } while (view);
      }
    } else {
      if (area->editor())
        editors.append(area->editor());
    }
  }

  return editors;
}

/*!
    Closes \a documents. If \a askAboutModifiedEditors is \c true, prompts
    users to save their changes before closing the documents.

    Returns whether the documents were closed.
*/
auto EditorManager::closeDocuments(const QList<IDocument*> &documents, const bool ask_about_modified_editors) -> bool
{
  return m_instance->closeEditors(DocumentModel::editorsForDocuments(documents), ask_about_modified_editors);
}

/*!
    Adds the current cursor position specified by \a saveState to the
    navigation history. If \a saveState is \l{QByteArray::isNull()}{null} (the
    default), the current state of the active editor is used. Otherwise \a
    saveState must be a valid state of the active editor.

    \sa IEditor::saveState()
*/
auto EditorManager::addCurrentPositionToNavigationHistory(const QByteArray &save_state) -> void
{
  EditorManagerPrivate::currentEditorView()->addCurrentPositionToNavigationHistory(save_state);
  EditorManagerPrivate::updateActions();
}

/*!
    Sets the location that was last modified to \a editor.
    Used for \uicontrol{Window} > \uicontrol{Go to Last Edit}.
*/
auto EditorManager::setLastEditLocation(const IEditor *editor) -> void
{
  const auto document = editor->document();

  if (!document)
    return;

  const auto &state = editor->saveState();
  EditLocation location;
  location.document = document;
  location.file_path = document->filePath();
  location.id = document->id();
  location.state = QVariant(state);

  d->m_global_last_edit_location = location;
}

/*!
    Cuts the forward part of the navigation history, so the user cannot
    \uicontrol{Go Forward} anymore (until the user goes backward again).

    \sa goForwardInNavigationHistory()
    \sa addCurrentPositionToNavigationHistory()
*/
auto EditorManager::cutForwardNavigationHistory() -> void
{
  EditorManagerPrivate::currentEditorView()->cutForwardNavigationHistory();
  EditorManagerPrivate::updateActions();
}

/*!
    Goes back in the navigation history.

    \sa goForwardInNavigationHistory()
    \sa addCurrentPositionToNavigationHistory()
*/
auto EditorManager::goBackInNavigationHistory() -> void
{
  EditorManagerPrivate::currentEditorView()->goBackInNavigationHistory();
  EditorManagerPrivate::updateActions();
  return;
}

/*!
    Goes forward in the navigation history.

    \sa goBackInNavigationHistory()
    \sa addCurrentPositionToNavigationHistory()
*/
auto EditorManager::goForwardInNavigationHistory() -> void
{
  EditorManagerPrivate::currentEditorView()->goForwardInNavigationHistory();
  EditorManagerPrivate::updateActions();
}

auto windowForEditorArea(const EditorArea *area) -> EditorWindow*
{
  return qobject_cast<EditorWindow*>(area->window());
}

auto editorWindows(const QList<EditorArea*> &areas) -> QVector<EditorWindow*>
{
  QVector<EditorWindow*> result;

  for (const auto area : areas)
    if (const auto window = windowForEditorArea(area))
      result.append(window);

  return result;
}

/*!
    \internal

    Returns the serialized state of all non-temporary editors, the split layout
    and external editor windows.

    \sa restoreState()
*/
auto EditorManager::saveState() -> QByteArray
{
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);

  stream << QByteArray("EditorManagerV5");

  // TODO: In case of split views it's not possible to restore these for all correctly with this
  for (auto documents = DocumentModel::openedDocuments(); const auto document : documents) {
    if (!document->filePath().isEmpty() && !document->isTemporary()) {
      const auto editor = DocumentModel::editorsForDocument(document).constFirst();
      if (auto state = editor->saveState(); !state.isEmpty())
        d->m_editor_states.insert(document->filePath().toString(), QVariant(state));
    }
  }

  stream << d->m_editor_states;

  auto entries = DocumentModel::entries();
  auto entries_count = 0;

  for(const auto entry: entries) {
    // The editor may be 0 if it was not loaded yet: In that case it is not temporary
    if (!entry->document->isTemporary())
      ++entries_count;
  }

  stream << entries_count;

  for(const auto entry:  entries) {
    if (!entry->document->isTemporary()) {
      stream << entry->fileName().toString() << entry->plainDisplayName() << entry->id() << entry->pinned;
    }
  }

  stream << d->m_editor_areas.first()->saveState(); // TODO

  // windows
  const auto windows = editorWindows(d->m_editor_areas);
  const auto window_states = transform(windows, &EditorWindow::saveState);
  stream << window_states;
  return bytes;
}

/*!
    \internal

    Restores the \a state of the split layout, editor windows and editors.

    Returns \c true if the state can be restored.

    \sa saveState()
*/
auto EditorManager::restoreState(const QByteArray &state) -> bool
{
  closeAllEditors(true);
  // remove extra windows
  for (auto i = d->m_editor_areas.count() - 1; i > 0 /* keep first alive */; --i)
    delete d->m_editor_areas.at(i); // automatically removes it from list

  if (d->m_editor_areas.first()->isSplitter())
    EditorManagerPrivate::removeAllSplits();

  QDataStream stream(state);
  QByteArray version;

  stream >> version;

  const auto is_version5 = version == "EditorManagerV5";
  if (version != "EditorManagerV4" && !is_version5)
    return false;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  stream >> d->m_editor_states;

  auto editor_count = 0;
  stream >> editor_count;
  while (--editor_count >= 0) {
    QString file_name;
    stream >> file_name;
    QString display_name;
    stream >> display_name;
    Id id;
    stream >> id;
    auto pinned = false;

    if (is_version5)
      stream >> pinned;

    if (!file_name.isEmpty() && !display_name.isEmpty()) {
      const auto file_path = FilePath::fromUserInput(file_name);

      if (!file_path.exists())
        continue;

      if (const auto rfp = autoSaveName(file_path); rfp.exists() && file_path.lastModified() < rfp.lastModified()) {
        if (const auto editor = openEditor(file_path, id, DoNotMakeVisible))
          DocumentModelPrivate::setPinned(DocumentModel::entryForDocument(editor->document()), pinned);
      } else {
        if (const auto entry = DocumentModelPrivate::addSuspendedDocument(file_path, display_name, id))
          DocumentModelPrivate::setPinned(entry, pinned);
      }
    }
  }

  QByteArray splitterstates;
  stream >> splitterstates;
  d->m_editor_areas.first()->restoreState(splitterstates); // TODO

  if (!stream.atEnd()) {
    // safety for settings from Qt Creator 4.5 and earlier
    // restore windows
    QVector<QVariantHash> window_states;
    stream >> window_states;
    for (const auto &window_state : qAsConst(window_states)) {
      const auto window = d->createEditorWindow();
      window->restoreState(window_state);
      window->show();
    }
  }

  // splitting and stuff results in focus trouble, that's why we set the focus again after restoration
  if (d->m_current_editor) {
    d->m_current_editor->widget()->setFocus();
  } else if (const auto view = EditorManagerPrivate::currentEditorView()) {
    if (const auto e = view->currentEditor())
      e->widget()->setFocus();
    else
      view->setFocus();
  }

  QApplication::restoreOverrideCursor();
  return true;
}

/*!
    \internal
*/
auto EditorManager::showEditorStatusBar(const QString &id, const QString &info_text, const QString &button_text, QObject *object, const std::function<void()> &function) -> void
{
  EditorManagerPrivate::currentEditorView()->showEditorStatusBar(id, info_text, button_text, object, function);
}

/*!
    \internal
*/
auto EditorManager::hideEditorStatusBar(const QString &id) -> void
{
  // TODO: what if the current editor view betwenn show and hideEditorStatusBar changed?
  EditorManagerPrivate::currentEditorView()->hideEditorStatusBar(id);
}

/*!
    Returns the default text codec as the user specified in the settings.
*/
auto EditorManager::defaultTextCodec() -> QTextCodec*
{
  const QSettings *settings = ICore::settings();
  const auto codec_name = settings->value(SETTINGS_DEFAULTTEXTENCODING).toByteArray();

  if (const auto candidate = QTextCodec::codecForName(codec_name))
    return candidate;

  // Qt5 doesn't return a valid codec when looking up the "System" codec, but will return
  // such a codec when asking for the codec for locale and no matching codec is available.
  // So check whether such a codec was saved to the settings.

  if (const auto locale_codec = QTextCodec::codecForLocale(); codec_name == locale_codec->name())
    return locale_codec;

  if (const auto default_utf8 = QTextCodec::codecForName("UTF-8"))
    return default_utf8;

  return QTextCodec::codecForLocale();
}

/*!
    Returns the default line ending as the user specified in the settings.
*/
auto EditorManager::defaultLineEnding() -> TextFileFormat::LineTerminationMode
{
  const QSettings *settings = ICore::settings();
  const auto default_line_terminator = settings->value(SETTINGS_DEFAULT_LINE_TERMINATOR, TextFileFormat::LineTerminationMode::NativeLineTerminator).toInt();

  return static_cast<TextFileFormat::LineTerminationMode>(default_line_terminator);
}

/*!
    Splits the editor view horizontally into adjacent views.
*/
auto EditorManager::splitSideBySide() -> void
{
  EditorManagerPrivate::split(Qt::Horizontal);
}

/*!
 * Moves focus to another split, creating it if necessary.
 * If there's no split and no other window, a side-by-side split is created.
 * If the current window is split, focus is moved to the next split within this window, cycling.
 * If the current window is not split, focus is moved to the next window.
 */
auto EditorManager::gotoOtherSplit() -> void
{
  auto view = EditorManagerPrivate::currentEditorView();

  if (!view)
    return;
  auto next_view = view->findNextView();

  if (!next_view) {
    // we are in the "last" view in this editor area
    auto index = -1;
    const auto area = EditorManagerPrivate::findEditorArea(view, &index);

    QTC_ASSERT(area, return);
    QTC_ASSERT(index >= 0 && index < d->m_editor_areas.size(), return);

    // stay in same window if it is split
    if (area->isSplitter()) {
      next_view = area->findFirstView();
      QTC_CHECK(next_view != view);
    } else {
      // find next editor area. this might be the same editor area if there's only one.
      auto next_index = index + 1;

      if (next_index >= d->m_editor_areas.size())
        next_index = 0;

      next_view = d->m_editor_areas.at(next_index)->findFirstView();
      QTC_CHECK(next_view);

      // if we had only one editor area with only one view, we end up at the startpoint
      // in that case we need to split
      if (next_view == view) {
        QTC_CHECK(!area->isSplitter());
        splitSideBySide(); // that deletes 'view'
        view = area->findFirstView();
        next_view = view->findNextView();
        QTC_CHECK(next_view != view);
        QTC_CHECK(next_view);
      }
    }
  }

  if (next_view)
    EditorManagerPrivate::activateView(next_view);
}

/*!
    Returns the maximum file size that should be opened in a text editor.
*/
auto EditorManager::maxTextFileSize() -> qint64
{
  return static_cast<qint64>(3) << 24;
}

/*!
    \internal

    Sets the window title addition handler to \a handler.
*/
auto EditorManager::setWindowTitleAdditionHandler(window_title_handler handler) -> void
{
  d->m_title_addition_handler = std::move(handler);
}

/*!
    \internal

    Sets the session title addition handler to \a handler.
*/
auto EditorManager::setSessionTitleHandler(window_title_handler handler) -> void
{
  d->m_session_title_handler = std::move(handler);
}

/*!
    \internal
*/
auto EditorManager::updateWindowTitles() -> void
{
  for(const auto area: d->m_editor_areas)
    emit area->windowTitleNeedsUpdate();
}

/*!
    \internal
*/
auto EditorManager::setWindowTitleVcsTopicHandler(window_title_handler handler) -> void
{
  d->m_title_vcs_topic_handler = std::move(handler);
}

} // namespace Orca::Plugin::Core
