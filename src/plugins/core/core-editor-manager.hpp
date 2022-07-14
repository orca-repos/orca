// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-document-interface.hpp"
#include "core-document-model.hpp"
#include "core-editor-interface.hpp"
#include "core-global.hpp"

#include <utils/link.hpp>
#include <utils/textfileformat.hpp>

#include <QList>
#include <QWidget>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QMenu)

namespace Utils {
class MimeType;
}

namespace Orca::Plugin::Core {

class IDocument;
class SearchResultItem;
class EditorManagerPrivate;
class MainWindow;

class CORE_EXPORT EditorManagerPlaceHolder final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(EditorManagerPlaceHolder)

public:
  explicit EditorManagerPlaceHolder(QWidget *parent = nullptr);
  ~EditorManagerPlaceHolder() final;

protected:
  auto showEvent(QShowEvent *event) -> void override;
};

class CORE_EXPORT EditorManager : public QObject {
  Q_OBJECT

public:
  using window_title_handler = std::function<QString (const Utils::FilePath &)>;

  static auto instance() -> EditorManager*;

  enum OpenEditorFlag {
    NoFlags = 0,
    DoNotChangeCurrentEditor = 1,
    IgnoreNavigationHistory = 2,
    DoNotMakeVisible = 4,
    OpenInOtherSplit = 8,
    DoNotSwitchToDesignMode = 16,
    DoNotSwitchToEditMode = 32,
    SwitchSplitIfAlreadyVisible = 64,
    DoNotRaise = 128,
    AllowExternalEditor = 256
  };

  Q_DECLARE_FLAGS(OpenEditorFlags, OpenEditorFlag)

  static auto openEditor(const Utils::FilePath &file_path, Utils::Id editor_id = {}, OpenEditorFlags flags = NoFlags, bool *new_editor = nullptr) -> IEditor*;
  static auto openEditorAt(const Utils::Link &link, Utils::Id editor_id = {}, OpenEditorFlags flags = NoFlags, bool *new_editor = nullptr) -> IEditor*;
  static auto openEditorAtSearchResult(const SearchResultItem &item, Utils::Id editor_id = {}, OpenEditorFlags flags = NoFlags, bool *new_editor = nullptr) -> void;
  static auto openEditorWithContents(Utils::Id editor_id, QString *title_pattern = nullptr, const QByteArray &contents = QByteArray(), const QString &unique_id = QString(), OpenEditorFlags flags = NoFlags) -> IEditor*;
  static auto skipOpeningBigTextFile(const Utils::FilePath &file_path) -> bool;
  static auto clearUniqueId(IDocument *document) -> void;
  static auto openExternalEditor(const Utils::FilePath &file_path, Utils::Id editor_id) -> bool;
  static auto addCloseEditorListener(const std::function<bool(IEditor *)> &listener) -> void;
  static auto getOpenFilePaths() -> Utils::FilePaths;
  static auto currentDocument() -> IDocument*;
  static auto currentEditor() -> IEditor*;
  static auto visibleEditors() -> QList<IEditor*>;
  static auto activateEditor(IEditor *editor, OpenEditorFlags flags = NoFlags) -> void;
  static auto activateEditorForEntry(DocumentModel::Entry *entry, OpenEditorFlags flags = NoFlags) -> void;
  static auto activateEditorForDocument(IDocument *document, OpenEditorFlags flags = NoFlags) -> IEditor*;
  static auto closeDocuments(const QList<IDocument*> &documents, bool ask_about_modified_editors = true) -> bool;
  static auto closeDocuments(const QList<DocumentModel::Entry*> &entries) -> bool;
  static auto closeOtherDocuments(IDocument *document) -> void;
  static auto closeAllDocuments() -> bool;
  static auto addCurrentPositionToNavigationHistory(const QByteArray &save_state = QByteArray()) -> void;
  static auto setLastEditLocation(const IEditor *editor) -> void;
  static auto cutForwardNavigationHistory() -> void;
  static auto saveDocument(IDocument *document) -> bool;
  static auto closeEditors(const QList<IEditor*> &editors_to_close, bool ask_about_modified_editors = true) -> bool;
  static auto saveState() -> QByteArray;
  static auto restoreState(const QByteArray &state) -> bool;
  static auto hasSplitter() -> bool;
  static auto showEditorStatusBar(const QString &id, const QString &info_text, const QString &button_text = QString(), QObject *object = nullptr, const std::function<void()> &function = {}) -> void;
  static auto hideEditorStatusBar(const QString &id) -> void;
  static auto isAutoSaveFile(const QString &file_name) -> bool;
  static auto autoSaveAfterRefactoring() -> bool;
  static auto defaultTextCodec() -> QTextCodec*;
  static auto defaultLineEnding() -> Utils::TextFileFormat::LineTerminationMode;
  static auto maxTextFileSize() -> qint64;
  static auto setWindowTitleAdditionHandler(window_title_handler handler) -> void;
  static auto setSessionTitleHandler(window_title_handler handler) -> void;
  static auto setWindowTitleVcsTopicHandler(window_title_handler handler) -> void;
  static auto addSaveAndCloseEditorActions(QMenu *context_menu, DocumentModel::Entry *entry, IEditor *editor = nullptr) -> void;
  static auto addPinEditorActions(QMenu *context_menu, const DocumentModel::Entry *entry) -> void;
  static auto addNativeDirAndOpenWithActions(QMenu *context_menu, DocumentModel::Entry *entry) -> void;
  static auto populateOpenWithMenu(QMenu *menu, const Utils::FilePath &file_path) -> void;
  static auto reloadSetting() -> IDocument::ReloadSetting;
  static auto setReloadSetting(IDocument::ReloadSetting behavior) -> void;

signals:
  auto currentEditorChanged(IEditor *editor) -> void;
  auto currentDocumentStateChanged() -> void;
  auto documentStateChanged(IDocument *document) -> void;
  auto editorCreated(IEditor *editor, const QString &file_name) -> void;
  auto editorOpened(IEditor *editor) -> void;
  auto documentOpened(IDocument *document) -> void;
  auto editorAboutToClose(IEditor *editor) -> void;
  auto editorsClosed(QList<IEditor*> editors) -> void;
  auto documentClosed(IDocument *document) -> void;
  auto findOnFileSystemRequest(const QString &path) -> void;
  auto openFileProperties(const Utils::FilePath &path) -> void;
  auto aboutToSave(IDocument *document) -> void;
  auto saved(IDocument *document) -> void;
  auto autoSaved() -> void;
  auto currentEditorAboutToChange(IEditor *editor) -> void;

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
  void linkOpened();
  #endif

public slots:
  static auto saveDocument() -> void;
  static auto saveDocumentAs() -> void;
  static auto revertToSaved() -> void;
  static auto closeAllEditors(bool ask_about_modified_editors = true) -> bool;
  static auto slotCloseCurrentEditorOrDocument() -> void;
  static auto closeOtherDocuments() -> void;
  static auto splitSideBySide() -> void;
  static auto gotoOtherSplit() -> void;
  static auto goBackInNavigationHistory() -> void;
  static auto goForwardInNavigationHistory() -> void;
  static auto updateWindowTitles() -> void;

private:
  explicit EditorManager(QObject *parent);
  ~EditorManager() override;

  friend class MainWindow;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Orca::Plugin::Core::EditorManager::OpenEditorFlags)
