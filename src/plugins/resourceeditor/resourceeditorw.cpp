// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourceeditorw.hpp"
#include "resourceeditorplugin.hpp"
#include "resourceeditorconstants.hpp"

#include <resourceeditor/qrceditor/resourcefile_p.hpp>
#include <resourceeditor/qrceditor/qrceditor.hpp>

#include <core/icore.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/commandbutton.hpp>
#include <core/editormanager/editormanager.hpp>
#include <utils/reloadpromptutils.hpp>
#include <utils/fileutils.hpp>

#include <QFileInfo>
#include <QDir>
#include <qdebug.h>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolBar>
#include <QInputDialog>
#include <QClipboard>

using namespace Utils;

namespace ResourceEditor {
namespace Internal {

enum {
  debugResourceEditorW = 0
};

ResourceEditorDocument::ResourceEditorDocument(QObject *parent) : IDocument(parent), m_model(new RelativeResourceModel(this))
{
  setId(ResourceEditor::Constants::RESOURCEEDITOR_ID);
  setMimeType(QLatin1String(ResourceEditor::Constants::C_RESOURCE_MIMETYPE));
  connect(m_model, &RelativeResourceModel::dirtyChanged, this, &ResourceEditorDocument::dirtyChanged);
  connect(m_model, &ResourceModel::contentsChanged, this, &IDocument::contentsChanged);

  if (debugResourceEditorW)
    qDebug() << "ResourceEditorFile::ResourceEditorFile()";
}

ResourceEditorW::ResourceEditorW(const Core::Context &context, ResourceEditorPlugin *plugin, QWidget *parent) : m_resourceDocument(new ResourceEditorDocument(this)), m_plugin(plugin), m_contextMenu(new QMenu), m_toolBar(new QToolBar)
{
  m_resourceEditor = new QrcEditor(m_resourceDocument->model(), parent);

  setContext(context);
  setWidget(m_resourceEditor);

  Core::CommandButton *refreshButton = new Core::CommandButton(Constants::REFRESH, m_toolBar);
  refreshButton->setIcon(QIcon(QLatin1String(":/texteditor/images/finddocuments.png")));
  connect(refreshButton, &QAbstractButton::clicked, this, &ResourceEditorW::onRefresh);
  m_toolBar->addWidget(refreshButton);

  m_resourceEditor->setResourceDragEnabled(true);
  m_contextMenu->addAction(tr("Open File"), this, &ResourceEditorW::openCurrentFile);
  m_openWithMenu = m_contextMenu->addMenu(tr("Open With"));
  m_renameAction = m_contextMenu->addAction(tr("Rename File..."), this, &ResourceEditorW::renameCurrentFile);
  m_copyFileNameAction = m_contextMenu->addAction(tr("Copy Resource Path to Clipboard"), this, &ResourceEditorW::copyCurrentResourcePath);
  m_orderList = m_contextMenu->addAction(tr("Sort Alphabetically"), this, &ResourceEditorW::orderList);

  connect(m_resourceDocument, &ResourceEditorDocument::loaded, m_resourceEditor, &QrcEditor::loaded);
  connect(m_resourceEditor, &QrcEditor::undoStackChanged, this, &ResourceEditorW::onUndoStackChanged);
  connect(m_resourceEditor, &QrcEditor::showContextMenu, this, &ResourceEditorW::showContextMenu);
  connect(m_resourceEditor, &QrcEditor::itemActivated, this, &ResourceEditorW::openFile);
  connect(m_resourceEditor->commandHistory(), &QUndoStack::indexChanged, m_resourceDocument, [this]() { m_resourceDocument->setShouldAutoSave(true); });
  if (debugResourceEditorW)
    qDebug() << "ResourceEditorW::ResourceEditorW()";
}

ResourceEditorW::~ResourceEditorW()
{
  if (m_resourceEditor)
    m_resourceEditor->deleteLater();
  delete m_contextMenu;
  delete m_toolBar;
}

auto ResourceEditorDocument::open(QString *errorString, const FilePath &filePath, const FilePath &realFilePath) -> Core::IDocument::OpenResult
{
  if (debugResourceEditorW)
    qDebug() << "ResourceEditorW::open: " << filePath;

  setBlockDirtyChanged(true);

  m_model->setFilePath(realFilePath);

  OpenResult openResult = m_model->reload();
  if (openResult != OpenResult::Success) {
    *errorString = m_model->errorMessage();
    setBlockDirtyChanged(false);
    emit loaded(false);
    return openResult;
  }

  setFilePath(filePath);
  setBlockDirtyChanged(false);
  m_model->setDirty(filePath != realFilePath);
  m_shouldAutoSave = false;

  emit loaded(true);
  return OpenResult::Success;
}

auto ResourceEditorDocument::save(QString *errorString, const FilePath &filePath, bool autoSave) -> bool
{
  if (debugResourceEditorW)
    qDebug() << ">ResourceEditorW::save: " << filePath;

  const FilePath &actualName = filePath.isEmpty() ? this->filePath() : filePath;
  if (actualName.isEmpty())
    return false;

  m_blockDirtyChanged = true;
  m_model->setFilePath(actualName);
  if (!m_model->save()) {
    *errorString = m_model->errorMessage();
    m_model->setFilePath(this->filePath());
    m_blockDirtyChanged = false;
    return false;
  }

  m_shouldAutoSave = false;
  if (autoSave) {
    m_model->setFilePath(this->filePath());
    m_model->setDirty(true);
    m_blockDirtyChanged = false;
    return true;
  }

  setFilePath(actualName);
  m_blockDirtyChanged = false;

  emit changed();
  return true;
}

auto ResourceEditorDocument::plainText() const -> QString
{
  return m_model->contents();
}

auto ResourceEditorDocument::contents() const -> QByteArray
{
  return m_model->contents().toUtf8();
}

auto ResourceEditorDocument::setContents(const QByteArray &contents) -> bool
{
  TempFileSaver saver;
  saver.write(contents);
  if (!saver.finalize(Core::ICore::dialogParent()))
    return false;

  const auto originalFileName = m_model->filePath();
  m_model->setFilePath(saver.filePath());
  const bool success = (m_model->reload() == OpenResult::Success);
  m_model->setFilePath(originalFileName);
  m_shouldAutoSave = false;
  if (debugResourceEditorW)
    qDebug() << "ResourceEditorW::createNew: " << contents << " (" << saver.filePath() << ") returns " << success;
  emit loaded(success);
  return success;
}

auto ResourceEditorDocument::setFilePath(const FilePath &newName) -> void
{
  m_model->setFilePath(newName);
  IDocument::setFilePath(newName);
}

auto ResourceEditorDocument::setBlockDirtyChanged(bool value) -> void
{
  m_blockDirtyChanged = value;
}

auto ResourceEditorDocument::model() const -> RelativeResourceModel*
{
  return m_model;
}

auto ResourceEditorDocument::setShouldAutoSave(bool save) -> void
{
  m_shouldAutoSave = save;
}

auto ResourceEditorW::saveState() const -> QByteArray
{
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  stream << m_resourceEditor->saveState();
  return bytes;
}

auto ResourceEditorW::restoreState(const QByteArray &state) -> void
{
  QDataStream stream(state);
  QByteArray splitterState;
  stream >> splitterState;
  m_resourceEditor->restoreState(splitterState);
}

auto ResourceEditorW::toolBar() -> QWidget*
{
  return m_toolBar;
}

auto ResourceEditorDocument::shouldAutoSave() const -> bool
{
  return m_shouldAutoSave;
}

auto ResourceEditorDocument::isModified() const -> bool
{
  return m_model->dirty();
}

auto ResourceEditorDocument::isSaveAsAllowed() const -> bool
{
  return true;
}

auto ResourceEditorDocument::reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool
{
  Q_UNUSED(type)
  if (flag == FlagIgnore)
    return true;
  emit aboutToReload();
  const bool success = (open(errorString, filePath(), filePath()) == OpenResult::Success);
  emit reloadFinished(success);
  return success;
}

auto ResourceEditorDocument::dirtyChanged(bool dirty) -> void
{
  if (m_blockDirtyChanged)
    return; // We emit changed() afterwards, unless it was an autosave

  if (debugResourceEditorW)
    qDebug() << " ResourceEditorW::dirtyChanged" << dirty;
  emit changed();
}

auto ResourceEditorW::onUndoStackChanged(bool canUndo, bool canRedo) -> void
{
  m_plugin->onUndoStackChanged(this, canUndo, canRedo);
}

auto ResourceEditorW::showContextMenu(const QPoint &globalPoint, const QString &fileName) -> void
{
  Core::EditorManager::populateOpenWithMenu(m_openWithMenu, FilePath::fromString(fileName));
  m_currentFileName = fileName;
  m_renameAction->setEnabled(!document()->isFileReadOnly());
  m_contextMenu->popup(globalPoint);
}

auto ResourceEditorW::openCurrentFile() -> void
{
  openFile(m_currentFileName);
}

auto ResourceEditorW::openFile(const QString &fileName) -> void
{
  Core::EditorManager::openEditor(FilePath::fromString(fileName));
}

auto ResourceEditorW::onRefresh() -> void
{
  m_resourceEditor->refresh();
}

auto ResourceEditorW::renameCurrentFile() -> void
{
  m_resourceEditor->editCurrentItem();
}

auto ResourceEditorW::copyCurrentResourcePath() -> void
{
  QApplication::clipboard()->setText(m_resourceEditor->currentResourcePath());
}

auto ResourceEditorW::orderList() -> void
{
  m_resourceDocument->model()->orderList();
}

auto ResourceEditorW::onUndo() -> void
{
  m_resourceEditor->onUndo();
}

auto ResourceEditorW::onRedo() -> void
{
  m_resourceEditor->onRedo();
}

} // namespace Internal
} // namespace ResourceEditor
