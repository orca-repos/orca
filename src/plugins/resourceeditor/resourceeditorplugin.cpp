// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourceeditorplugin.hpp"

#include "resourceeditorw.hpp"
#include "resourceeditorconstants.hpp"
#include "resourceeditorfactory.hpp"
#include "resourcenode.hpp"

#include <core/core-interface.hpp>
#include <core/core-constants.hpp>
#include <core/core-action-container.hpp>
#include <core/core-action-manager.hpp>
#include <core/core-command.hpp>
#include <core/core-editor-manager.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <extensionsystem/pluginmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/parameteraction.hpp>
#include <utils/qtcassert.hpp>
#include <utils/threadutils.hpp>

#include <QCoreApplication>
#include <QAction>
#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QApplication>

using namespace ProjectExplorer;
using namespace Utils;

namespace ResourceEditor {
namespace Internal {

static constexpr char resourcePrefix[] = ":";
static constexpr char urlPrefix[] = "qrc:";

class PrefixLangDialog : public QDialog {
  Q_OBJECT

public:
  PrefixLangDialog(const QString &title, const QString &prefix, const QString &lang, QWidget *parent) : QDialog(parent)
  {
    setWindowTitle(title);
    const auto layout = new QFormLayout(this);
    m_prefixLineEdit = new QLineEdit(this);
    m_prefixLineEdit->setText(prefix);
    layout->addRow(tr("Prefix:"), m_prefixLineEdit);

    m_langLineEdit = new QLineEdit(this);
    m_langLineEdit->setText(lang);
    layout->addRow(tr("Language:"), m_langLineEdit);

    const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  }

  auto prefix() const -> QString
  {
    return m_prefixLineEdit->text();
  }

  auto lang() const -> QString
  {
    return m_langLineEdit->text();
  }

private:
  QLineEdit *m_prefixLineEdit;
  QLineEdit *m_langLineEdit;
};

class ResourceEditorPluginPrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(ResourceEditor::Internal::ResourceEditorPlugin)

public:
  explicit ResourceEditorPluginPrivate(ResourceEditorPlugin *q);

  auto onUndo() -> void;
  auto onRedo() -> void;
  auto onRefresh() -> void;
  auto addPrefixContextMenu() -> void;
  auto renamePrefixContextMenu() -> void;
  auto removePrefixContextMenu() -> void;
  auto renameFileContextMenu() -> void;
  auto removeFileContextMenu() -> void;
  auto removeNonExisting() -> void;
  auto openEditorContextMenu() -> void;
  auto copyPathContextMenu() -> void;
  auto copyUrlContextMenu() -> void;
  auto updateContextActions(Node *node) -> void;
  auto currentEditor() const -> ResourceEditorW*;

  QAction *m_redoAction = nullptr;
  QAction *m_undoAction = nullptr;
  QAction *m_refreshAction = nullptr;

  // project tree's folder context menu
  QAction *m_addPrefix = nullptr;
  QAction *m_removePrefix = nullptr;
  QAction *m_renamePrefix = nullptr;
  QAction *m_removeNonExisting = nullptr;
  QAction *m_renameResourceFile = nullptr;
  QAction *m_removeResourceFile = nullptr;
  QAction *m_openInEditor = nullptr;
  QMenu *m_openWithMenu = nullptr;

  // file context menu
  Utils::ParameterAction *m_copyPath = nullptr;
  Utils::ParameterAction *m_copyUrl = nullptr;

  ResourceEditorFactory m_editorFactory;
};

ResourceEditorPluginPrivate::ResourceEditorPluginPrivate(ResourceEditorPlugin *q) : m_editorFactory(q)
{
  // Register undo and redo
  const Orca::Plugin::Core::Context context(Constants::C_RESOURCEEDITOR);
  m_undoAction = new QAction(tr("&Undo"), this);
  m_redoAction = new QAction(tr("&Redo"), this);
  m_refreshAction = new QAction(tr("Recheck Existence of Referenced Files"), this);
  Orca::Plugin::Core::ActionManager::registerAction(m_undoAction, Orca::Plugin::Core::UNDO, context);
  Orca::Plugin::Core::ActionManager::registerAction(m_redoAction, Orca::Plugin::Core::REDO, context);
  Orca::Plugin::Core::ActionManager::registerAction(m_refreshAction, Constants::REFRESH, context);
  connect(m_undoAction, &QAction::triggered, this, &ResourceEditorPluginPrivate::onUndo);
  connect(m_redoAction, &QAction::triggered, this, &ResourceEditorPluginPrivate::onRedo);
  connect(m_refreshAction, &QAction::triggered, this, &ResourceEditorPluginPrivate::onRefresh);

  const Orca::Plugin::Core::Context projectTreeContext(ProjectExplorer::Constants::C_PROJECT_TREE);
  const auto folderContextMenu = Orca::Plugin::Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_FOLDERCONTEXT);
  const auto fileContextMenu = Orca::Plugin::Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_FILECONTEXT);
  Orca::Plugin::Core::Command *command = nullptr;

  m_addPrefix = new QAction(tr("Add Prefix..."), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_addPrefix, Constants::C_ADD_PREFIX, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_addPrefix, &QAction::triggered, this, &ResourceEditorPluginPrivate::addPrefixContextMenu);

  m_renamePrefix = new QAction(tr("Change Prefix..."), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_renamePrefix, Constants::C_RENAME_PREFIX, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_renamePrefix, &QAction::triggered, this, &ResourceEditorPluginPrivate::renamePrefixContextMenu);

  m_removePrefix = new QAction(tr("Remove Prefix..."), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_removePrefix, Constants::C_REMOVE_PREFIX, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_removePrefix, &QAction::triggered, this, &ResourceEditorPluginPrivate::removePrefixContextMenu);

  m_removeNonExisting = new QAction(tr("Remove Missing Files"), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_removeNonExisting, Constants::C_REMOVE_NON_EXISTING, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_removeNonExisting, &QAction::triggered, this, &ResourceEditorPluginPrivate::removeNonExisting);

  m_renameResourceFile = new QAction(tr("Rename..."), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_renameResourceFile, Constants::C_RENAME_FILE, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_renameResourceFile, &QAction::triggered, this, &ResourceEditorPluginPrivate::renameFileContextMenu);

  m_removeResourceFile = new QAction(tr("Remove File..."), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_removeResourceFile, Constants::C_REMOVE_FILE, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_removeResourceFile, &QAction::triggered, this, &ResourceEditorPluginPrivate::removeFileContextMenu);

  m_openInEditor = new QAction(tr("Open in Editor"), this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_openInEditor, Constants::C_OPEN_EDITOR, projectTreeContext);
  folderContextMenu->addAction(command, ProjectExplorer::Constants::G_FOLDER_FILES);
  connect(m_openInEditor, &QAction::triggered, this, &ResourceEditorPluginPrivate::openEditorContextMenu);

  m_openWithMenu = new QMenu(tr("Open With"), folderContextMenu->menu());
  folderContextMenu->menu()->insertMenu(folderContextMenu->insertLocation(ProjectExplorer::Constants::G_FOLDER_FILES), m_openWithMenu);

  m_copyPath = new Utils::ParameterAction(tr("Copy Path"), tr("Copy Path \"%1\""), Utils::ParameterAction::AlwaysEnabled, this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_copyPath, Constants::C_COPY_PATH, projectTreeContext);
  command->setAttribute(Orca::Plugin::Core::Command::CA_UpdateText);
  fileContextMenu->addAction(command, ProjectExplorer::Constants::G_FILE_OTHER);
  connect(m_copyPath, &QAction::triggered, this, &ResourceEditorPluginPrivate::copyPathContextMenu);

  m_copyUrl = new Utils::ParameterAction(tr("Copy URL"), tr("Copy URL \"%1\""), Utils::ParameterAction::AlwaysEnabled, this);
  command = Orca::Plugin::Core::ActionManager::registerAction(m_copyUrl, Constants::C_COPY_URL, projectTreeContext);
  command->setAttribute(Orca::Plugin::Core::Command::CA_UpdateText);
  fileContextMenu->addAction(command, ProjectExplorer::Constants::G_FILE_OTHER);
  connect(m_copyUrl, &QAction::triggered, this, &ResourceEditorPluginPrivate::copyUrlContextMenu);

  m_addPrefix->setEnabled(false);
  m_removePrefix->setEnabled(false);
  m_renamePrefix->setEnabled(false);
  m_removeNonExisting->setEnabled(false);
  m_renameResourceFile->setEnabled(false);
  m_removeResourceFile->setEnabled(false);

  connect(ProjectTree::instance(), &ProjectTree::currentNodeChanged, this, &ResourceEditorPluginPrivate::updateContextActions);
}

auto ResourceEditorPlugin::extensionsInitialized() -> void
{
  ProjectTree::registerTreeManager([](FolderNode *folder, ProjectTree::ConstructionPhase phase) {
    switch (phase) {
    case ProjectTree::AsyncPhase: {
      QList<FileNode*> toReplace;
      folder->forEachNode([&toReplace](FileNode *fn) {
                            if (fn->fileType() == FileType::Resource)
                              toReplace.append(fn);
                          }, {}, [](const FolderNode *fn) {
                            return dynamic_cast<const ResourceTopLevelNode*>(fn) == nullptr;
                          });
      for (const auto file : qAsConst(toReplace)) {
        const auto pn = file->parentFolderNode();
        QTC_ASSERT(pn, continue);
        const auto path = file->filePath();
        auto topLevel = std::make_unique<ResourceTopLevelNode>(path, pn->filePath());
        topLevel->setEnabled(file->isEnabled());
        topLevel->setIsGenerated(file->isGenerated());
        pn->replaceSubtree(file, std::move(topLevel));
      }
      break;
    }
    case ProjectTree::FinalPhase: {
      folder->forEachNode({}, [](FolderNode *fn) {
        auto *topLevel = dynamic_cast<ResourceTopLevelNode*>(fn);
        if (topLevel)
          topLevel->setupWatcherIfNeeded();
      });
      break;
    }
    }
  });
}

auto ResourceEditorPluginPrivate::onUndo() -> void
{
  currentEditor()->onUndo();
}

auto ResourceEditorPluginPrivate::onRedo() -> void
{
  currentEditor()->onRedo();
}

auto ResourceEditorPluginPrivate::onRefresh() -> void
{
  currentEditor()->onRefresh();
}

auto ResourceEditorPluginPrivate::addPrefixContextMenu() -> void
{
  const auto topLevel = dynamic_cast<ResourceTopLevelNode*>(ProjectTree::currentNode());
  QTC_ASSERT(topLevel, return);
  PrefixLangDialog dialog(tr("Add Prefix"), QString(), QString(), Orca::Plugin::Core::ICore::dialogParent());
  if (dialog.exec() != QDialog::Accepted)
    return;
  const auto prefix = dialog.prefix();
  if (prefix.isEmpty())
    return;
  topLevel->addPrefix(prefix, dialog.lang());
}

auto ResourceEditorPluginPrivate::removePrefixContextMenu() -> void
{
  const auto rfn = dynamic_cast<ResourceFolderNode*>(ProjectTree::currentNode());
  QTC_ASSERT(rfn, return);
  if (QMessageBox::question(Orca::Plugin::Core::ICore::dialogParent(), tr("Remove Prefix"), tr("Remove prefix %1 and all its files?").arg(rfn->displayName())) == QMessageBox::Yes) {
    const auto rn = rfn->resourceNode();
    rn->removePrefix(rfn->prefix(), rfn->lang());
  }
}

auto ResourceEditorPluginPrivate::removeNonExisting() -> void
{
  const auto topLevel = dynamic_cast<ResourceTopLevelNode*>(ProjectTree::currentNode());
  QTC_ASSERT(topLevel, return);
  topLevel->removeNonExistingFiles();
}

auto ResourceEditorPluginPrivate::renameFileContextMenu() -> void
{
  ProjectExplorerPlugin::initiateInlineRenaming();
}

auto ResourceEditorPluginPrivate::removeFileContextMenu() -> void
{
  const auto rfn = dynamic_cast<ResourceTopLevelNode*>(ProjectTree::currentNode());
  QTC_ASSERT(rfn, return);
  auto path = rfn->filePath();
  const auto parent = rfn->parentFolderNode();
  QTC_ASSERT(parent, return);
  if (parent->removeFiles({path}) != RemovedFilesFromProject::Ok)
    QMessageBox::warning(Orca::Plugin::Core::ICore::dialogParent(), tr("File Removal Failed"), tr("Removing file %1 from the project failed.").arg(path.toUserOutput()));
}

auto ResourceEditorPluginPrivate::openEditorContextMenu() -> void
{
  Orca::Plugin::Core::EditorManager::openEditor(ProjectTree::currentNode()->filePath());
}

auto ResourceEditorPluginPrivate::copyPathContextMenu() -> void
{
  const auto node = dynamic_cast<ResourceFileNode*>(ProjectTree::currentNode());
  QTC_ASSERT(node, return);
  QApplication::clipboard()->setText(QLatin1String(resourcePrefix) + node->qrcPath());
}

auto ResourceEditorPluginPrivate::copyUrlContextMenu() -> void
{
  const auto node = dynamic_cast<ResourceFileNode*>(ProjectTree::currentNode());
  QTC_ASSERT(node, return);
  QApplication::clipboard()->setText(QLatin1String(urlPrefix) + node->qrcPath());
}

auto ResourceEditorPluginPrivate::renamePrefixContextMenu() -> void
{
  const auto node = dynamic_cast<ResourceFolderNode*>(ProjectTree::currentNode());
  QTC_ASSERT(node, return);

  PrefixLangDialog dialog(tr("Rename Prefix"), node->prefix(), node->lang(), Orca::Plugin::Core::ICore::dialogParent());
  if (dialog.exec() != QDialog::Accepted)
    return;
  const auto prefix = dialog.prefix();
  if (prefix.isEmpty())
    return;

  node->renamePrefix(prefix, dialog.lang());
}

auto ResourceEditorPluginPrivate::updateContextActions(Node *node) -> void
{
  const bool isResourceNode = dynamic_cast<const ResourceTopLevelNode*>(node);
  m_addPrefix->setEnabled(isResourceNode);
  m_addPrefix->setVisible(isResourceNode);

  auto enableRename = false;
  auto enableRemove = false;

  if (isResourceNode) {
    const auto parent = node ? node->parentFolderNode() : nullptr;
    enableRename = parent && parent->supportsAction(Rename, node);
    enableRemove = parent && parent->supportsAction(RemoveFile, node);
  }

  m_renameResourceFile->setEnabled(isResourceNode && enableRename);
  m_renameResourceFile->setVisible(isResourceNode && enableRename);
  m_removeResourceFile->setEnabled(isResourceNode && enableRemove);
  m_removeResourceFile->setVisible(isResourceNode && enableRemove);

  m_openInEditor->setEnabled(isResourceNode);
  m_openInEditor->setVisible(isResourceNode);

  const bool isResourceFolder = dynamic_cast<const ResourceFolderNode*>(node);
  m_removePrefix->setEnabled(isResourceFolder);
  m_removePrefix->setVisible(isResourceFolder);

  m_renamePrefix->setEnabled(isResourceFolder);
  m_renamePrefix->setVisible(isResourceFolder);

  m_removeNonExisting->setEnabled(isResourceNode);
  m_removeNonExisting->setVisible(isResourceNode);

  if (isResourceNode)
    Orca::Plugin::Core::EditorManager::populateOpenWithMenu(m_openWithMenu, node->filePath());
  else
    m_openWithMenu->clear();
  m_openWithMenu->menuAction()->setVisible(!m_openWithMenu->actions().isEmpty());

  const bool isResourceFile = dynamic_cast<const ResourceFileNode*>(node);
  m_copyPath->setEnabled(isResourceFile);
  m_copyPath->setVisible(isResourceFile);
  m_copyUrl->setEnabled(isResourceFile);
  m_copyUrl->setVisible(isResourceFile);
  if (isResourceFile) {
    const auto fileNode = dynamic_cast<const ResourceFileNode*>(node);
    QTC_ASSERT(fileNode, return);
    const auto qrcPath = fileNode->qrcPath();
    m_copyPath->setParameter(QLatin1String(resourcePrefix) + qrcPath);
    m_copyUrl->setParameter(QLatin1String(urlPrefix) + qrcPath);
  }
}

auto ResourceEditorPluginPrivate::currentEditor() const -> ResourceEditorW*
{
  auto const focusEditor = qobject_cast<ResourceEditorW*>(Orca::Plugin::Core::EditorManager::currentEditor());
  QTC_ASSERT(focusEditor, return nullptr);
  return focusEditor;
}

// ResourceEditorPlugin

ResourceEditorPlugin::~ResourceEditorPlugin()
{
  delete d;
}

auto ResourceEditorPlugin::initialize(const QStringList &arguments, QString *errorMessage) -> bool
{
  Q_UNUSED(arguments)
  Q_UNUSED(errorMessage)

  d = new ResourceEditorPluginPrivate(this);

  return true;
}

auto ResourceEditorPlugin::onUndoStackChanged(ResourceEditorW const *editor, bool canUndo, bool canRedo) -> void
{
  if (editor == d->currentEditor()) {
    d->m_undoAction->setEnabled(canUndo);
    d->m_redoAction->setEnabled(canRedo);
  }
}

} // namespace Internal
} // namespace ResourceEditor

#include "resourceeditorplugin.moc"
