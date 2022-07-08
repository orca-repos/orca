// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectfilewizardextension.hpp"
#include "projectexplorer.hpp"
#include "session.hpp"
#include "projectnodes.hpp"
#include "projecttree.hpp"
#include "projectwizardpage.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <core/icore.hpp>
#include <projectexplorer/editorconfiguration.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projecttree.hpp>
#include <texteditor/icodestylepreferences.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/textindenter.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QPointer>
#include <QDebug>
#include <QFileInfo>
#include <QTextDocument>
#include <QTextCursor>
#include <QMessageBox>

using namespace TextEditor;
using namespace Core;
using namespace Utils;

/*!
    \class ProjectExplorer::Internal::ProjectFileWizardExtension

    \brief The ProjectFileWizardExtension class implements the post-file
    generating steps of a project wizard.

    This class provides the following functions:
    \list
    \li Add to a project file (*.pri/ *.pro)
    \li Initialize a version control system repository (unless the path is already
        managed) and do 'add' if the VCS supports it.
    \endlist

    \sa ProjectExplorer::Internal::ProjectWizardPage
*/

enum {
  debugExtension = 0
};

namespace ProjectExplorer {
namespace Internal {

// --------- ProjectWizardContext

class ProjectWizardContext {
public:
  auto clear() -> void;

  QPointer<ProjectWizardPage> page = nullptr; // this is managed by the wizard!
  const IWizardFactory *wizard = nullptr;
};

auto ProjectWizardContext::clear() -> void
{
  page = nullptr;
  wizard = nullptr;
}

// ---- ProjectFileWizardExtension

ProjectFileWizardExtension::~ProjectFileWizardExtension()
{
  delete m_context;
}

static auto generatedProjectFilePath(const QList<GeneratedFile> &files) -> FilePath
{
  for (const auto &file : files)
    if (file.attributes() & GeneratedFile::OpenProjectAttribute)
      return file.filePath();
  return {};
}

auto ProjectFileWizardExtension::firstExtensionPageShown(const QList<GeneratedFile> &files, const QVariantMap &extraValues) -> void
{
  if (debugExtension)
    qDebug() << Q_FUNC_INFO << files.size();

  const auto fileNames = transform(files, &GeneratedFile::path);
  m_context->page->setFiles(fileNames);

  FilePaths filePaths;
  ProjectAction projectAction;
  const auto kind = m_context->wizard->kind();
  if (kind == IWizardFactory::ProjectWizard) {
    projectAction = AddSubProject;
    filePaths << generatedProjectFilePath(files);
  } else {
    projectAction = AddNewFile;
    filePaths = transform(files, &GeneratedFile::filePath);
  }

  // Static cast from void * to avoid qobject_cast (which needs a valid object) in value().
  const auto contextNode = static_cast<Node*>(extraValues.value(QLatin1String(Constants::PREFERRED_PROJECT_NODE)).value<void*>());
  auto project = static_cast<Project*>(extraValues.value(Constants::PROJECT_POINTER).value<void*>());
  const auto path = FilePath::fromVariant(extraValues.value(Constants::PREFERRED_PROJECT_NODE_PATH));

  m_context->page->initializeProjectTree(findWizardContextNode(contextNode, project, path), filePaths, m_context->wizard->kind(), projectAction);
  // Refresh combobox on project tree changes:
  connect(ProjectTree::instance(), &ProjectTree::treeChanged, m_context->page, [this, project, path, filePaths, kind, projectAction]() {
    m_context->page->initializeProjectTree(findWizardContextNode(m_context->page->currentNode(), project, path), filePaths, kind, projectAction);
  });

  m_context->page->initializeVersionControls();
}

auto ProjectFileWizardExtension::findWizardContextNode(Node *contextNode, Project *project, const FilePath &path) -> Node*
{
  if (contextNode && !ProjectTree::hasNode(contextNode)) {
    if (SessionManager::projects().contains(project) && project->rootProjectNode()) {
      contextNode = project->rootProjectNode()->findNode([path](const Node *n) {
        return path == n->filePath();
      });
    }
  }
  return contextNode;
}

auto ProjectFileWizardExtension::extensionPages(const IWizardFactory *wizard) -> QList<QWizardPage*>
{
  if (!m_context)
    m_context = new ProjectWizardContext;
  else
    m_context->clear();
  // Init context with page and projects
  m_context->page = new ProjectWizardPage;
  m_context->wizard = wizard;
  return {m_context->page};
}

auto ProjectFileWizardExtension::processFiles(const QList<GeneratedFile> &files, bool *removeOpenProjectAttribute, QString *errorMessage) -> bool
{
  if (!processProject(files, removeOpenProjectAttribute, errorMessage))
    return false;
  if (!m_context->page->runVersionControl(files, errorMessage)) {
    QString message;
    if (errorMessage) {
      message = *errorMessage;
      message.append(QLatin1String("\n\n"));
      errorMessage->clear();
    }
    message.append(tr("Open project anyway?"));
    if (QMessageBox::question(ICore::dialogParent(), tr("Version Control Failure"), message, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
      return false;
  }
  return true;
}

// Add files to project && version control
auto ProjectFileWizardExtension::processProject(const QList<GeneratedFile> &files, bool *removeOpenProjectAttribute, QString *errorMessage) -> bool
{
  *removeOpenProjectAttribute = false;

  const auto generatedProject = generatedProjectFilePath(files);

  const auto folder = m_context->page->currentNode();
  if (!folder)
    return true;
  if (m_context->wizard->kind() == IWizardFactory::ProjectWizard) {
    if (!static_cast<ProjectNode*>(folder)->addSubProject(generatedProject)) {
      *errorMessage = tr("Failed to add subproject \"%1\"\nto project \"%2\".").arg(generatedProject.toUserOutput()).arg(folder->filePath().toUserOutput());
      return false;
    }
    *removeOpenProjectAttribute = true;
  } else {
    const auto filePaths = transform(files, &GeneratedFile::filePath);
    if (!folder->addFiles(filePaths)) {
      *errorMessage = tr("Failed to add one or more files to project\n\"%1\" (%2).").arg(folder->filePath().toUserOutput()).arg(FilePath::formatFilePaths(filePaths, ","));
      return false;
    }
  }
  return true;
}

static auto codeStylePreferences(Project *project, Id languageId) -> ICodeStylePreferences*
{
  if (!languageId.isValid())
    return nullptr;

  if (project)
    return project->editorConfiguration()->codeStyle(languageId);

  return TextEditorSettings::codeStyle(languageId);
}

auto ProjectFileWizardExtension::applyCodeStyle(GeneratedFile *file) const -> void
{
  if (file->isBinary() || file->contents().isEmpty())
    return; // nothing to do

  const auto languageId = TextEditorSettings::languageId(mimeTypeForFile(file->path()).name());

  if (!languageId.isValid())
    return; // don't modify files like *.ui *.pro

  const auto folder = m_context->page->currentNode();
  const auto baseProject = ProjectTree::projectForNode(folder);

  const auto factory = TextEditorSettings::codeStyleFactory(languageId);

  QTextDocument doc(file->contents());
  Indenter *indenter = nullptr;
  if (factory) {
    indenter = factory->createIndenter(&doc);
    indenter->setFileName(file->filePath());
  }
  if (!indenter)
    indenter = new TextIndenter(&doc);

  const auto codeStylePrefs = codeStylePreferences(baseProject, languageId);
  indenter->setCodeStylePreferences(codeStylePrefs);

  QTextCursor cursor(&doc);
  cursor.select(QTextCursor::Document);
  indenter->indent(cursor, QChar::Null, codeStylePrefs->currentTabSettings());
  delete indenter;
  if (TextEditorSettings::storageSettings().m_cleanWhitespace) {
    auto block = doc.firstBlock();
    while (block.isValid()) {
      TabSettings::removeTrailingWhitespace(cursor, block);
      block = block.next();
    }
  }
  file->setContents(doc.toPlainText());
}

} // namespace Internal
} // namespace ProjectExplorer