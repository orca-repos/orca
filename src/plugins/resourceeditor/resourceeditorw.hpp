// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-document-interface.hpp>
#include <core/core-editor-interface.hpp>

QT_BEGIN_NAMESPACE
class QMenu;
class QToolBar;
QT_END_NAMESPACE

namespace ResourceEditor {
namespace Internal {

class RelativeResourceModel;
class ResourceEditorPlugin;
class ResourceEditorW;
class QrcEditor;

class ResourceEditorDocument : public Orca::Plugin::Core::IDocument {
  Q_OBJECT
  Q_PROPERTY(QString plainText READ plainText STORED false) // For access by code pasters

public:
  ResourceEditorDocument(QObject *parent = nullptr);

  //IDocument
  auto open(QString *errorString, const Utils::FilePath &filePath, const Utils::FilePath &realFilePath) -> OpenResult override;
  auto save(QString *errorString, const Utils::FilePath &filePath, bool autoSave) -> bool override;
  auto plainText() const -> QString;
  auto contents() const -> QByteArray override;
  auto setContents(const QByteArray &contents) -> bool override;
  auto shouldAutoSave() const -> bool override;
  auto isModified() const -> bool override;
  auto isSaveAsAllowed() const -> bool override;
  auto reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool override;
  auto setFilePath(const Utils::FilePath &newName) -> void override;
  auto setBlockDirtyChanged(bool value) -> void;

  auto model() const -> RelativeResourceModel*;
  auto setShouldAutoSave(bool save) -> void;

signals:
  auto loaded(bool success) -> void;

private:
  auto dirtyChanged(bool) -> void;

  RelativeResourceModel *m_model;
  bool m_blockDirtyChanged = false;
  bool m_shouldAutoSave = false;
};

class ResourceEditorW : public Orca::Plugin::Core::IEditor {
  Q_OBJECT

public:
  ResourceEditorW(const Orca::Plugin::Core::Context &context, ResourceEditorPlugin *plugin, QWidget *parent = nullptr);
  ~ResourceEditorW() override;

  // IEditor
  auto document() const -> Orca::Plugin::Core::IDocument* override { return m_resourceDocument; }
  auto saveState() const -> QByteArray override;
  auto restoreState(const QByteArray &state) -> void override;
  auto toolBar() -> QWidget* override;

private:
  auto onUndoStackChanged(bool canUndo, bool canRedo) -> void;
  auto showContextMenu(const QPoint &globalPoint, const QString &fileName) -> void;
  auto openCurrentFile() -> void;
  auto openFile(const QString &fileName) -> void;
  auto renameCurrentFile() -> void;
  auto copyCurrentResourcePath() -> void;
  auto orderList() -> void;

  const QString m_extension;
  const QString m_fileFilter;
  QString m_displayName;
  QrcEditor *m_resourceEditor;
  ResourceEditorDocument *m_resourceDocument;
  ResourceEditorPlugin *m_plugin;
  QMenu *m_contextMenu;
  QMenu *m_openWithMenu;
  QString m_currentFileName;
  QToolBar *m_toolBar;
  QAction *m_renameAction;
  QAction *m_copyFileNameAction;
  QAction *m_orderList;

public:
  auto onRefresh() -> void;
  auto onUndo() -> void;
  auto onRedo() -> void;

  friend class ResourceEditorDocument;
};

} // namespace Internal
} // namespace ResourceEditor
