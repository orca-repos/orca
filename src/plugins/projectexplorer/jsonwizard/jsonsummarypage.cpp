// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonsummarypage.hpp"

#include "jsonwizard.hpp"
#include "../project.hpp"
#include "../projectexplorerconstants.hpp"
#include "../projectnodes.hpp"
#include "../projecttree.hpp"
#include "../session.hpp"

#include "../projecttree.hpp"

#include <core/coreconstants.hpp>
#include <core/iversioncontrol.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QDir>
#include <QMessageBox>

using namespace Core;
using namespace Utils;

static char KEY_SELECTED_PROJECT[] = "SelectedProject";
static char KEY_SELECTED_NODE[] = "SelectedFolderNode";
static char KEY_IS_SUBPROJECT[] = "IsSubproject";
static char KEY_VERSIONCONTROL[] = "VersionControl";
static char KEY_QT_KEYWORDS_ENABLED[] = "QtKeywordsEnabled";

namespace ProjectExplorer {

// --------------------------------------------------------------------
// Helper:
// --------------------------------------------------------------------

static auto generatedProjectFilePath(const QList<JsonWizard::GeneratorFile> &files) -> FilePath
{
  for (const auto &file : files)
    if (file.file.attributes() & GeneratedFile::OpenProjectAttribute)
      return file.file.filePath();
  return {};
}

static auto wizardKind(JsonWizard *wiz) -> IWizardFactory::WizardKind
{
  auto kind = IWizardFactory::ProjectWizard;
  const auto kindStr = wiz->stringValue(QLatin1String("kind"));
  if (kindStr == QLatin1String(Core::Constants::WIZARD_KIND_PROJECT))
    kind = IWizardFactory::ProjectWizard;
  else if (kindStr == QLatin1String(Core::Constants::WIZARD_KIND_FILE))
    kind = IWizardFactory::FileWizard;
  else
  QTC_CHECK(false);
  return kind;
}

// --------------------------------------------------------------------
// JsonSummaryPage:
// --------------------------------------------------------------------

JsonSummaryPage::JsonSummaryPage(QWidget *parent) : ProjectWizardPage(parent), m_wizard(nullptr)
{
  connect(this, &ProjectWizardPage::projectNodeChanged, this, &JsonSummaryPage::summarySettingsHaveChanged);
  connect(this, &ProjectWizardPage::versionControlChanged, this, &JsonSummaryPage::summarySettingsHaveChanged);
}

auto JsonSummaryPage::setHideProjectUiValue(const QVariant &hideProjectUiValue) -> void
{
  m_hideProjectUiValue = hideProjectUiValue;
}

auto JsonSummaryPage::initializePage() -> void
{
  m_wizard = qobject_cast<JsonWizard*>(wizard());
  QTC_ASSERT(m_wizard, return);

  m_wizard->setValue(QLatin1String(KEY_SELECTED_PROJECT), QVariant());
  m_wizard->setValue(QLatin1String(KEY_SELECTED_NODE), QVariant());
  m_wizard->setValue(QLatin1String(KEY_IS_SUBPROJECT), false);
  m_wizard->setValue(QLatin1String(KEY_VERSIONCONTROL), QString());
  m_wizard->setValue(QLatin1String(KEY_QT_KEYWORDS_ENABLED), false);

  connect(m_wizard, &JsonWizard::filesReady, this, &JsonSummaryPage::triggerCommit);
  connect(m_wizard, &JsonWizard::filesReady, this, &JsonSummaryPage::addToProject);

  updateFileList();

  auto kind = wizardKind(m_wizard);
  const auto isProject = (kind == IWizardFactory::ProjectWizard);

  FilePaths files;
  if (isProject) {
    const auto f = findOrDefault(m_fileList, [](const JsonWizard::GeneratorFile &f) {
      return f.file.attributes() & GeneratedFile::OpenProjectAttribute;
    });
    files << f.file.filePath();
  } else {
    files = transform(m_fileList, [](const JsonWizard::GeneratorFile &f) {
      return f.file.filePath();
    });
  }

  // Use static cast from void * to avoid qobject_cast (which needs a valid object) in value()
  // in the following code:
  const auto contextNode = findWizardContextNode(static_cast<Node*>(m_wizard->value(Constants::PREFERRED_PROJECT_NODE).value<void*>()));
  const auto currentAction = isProject ? AddSubProject : AddNewFile;

  initializeProjectTree(contextNode, files, kind, currentAction);

  // Refresh combobox on project tree changes:
  connect(ProjectTree::instance(), &ProjectTree::treeChanged, this, [this, files, kind, currentAction]() {
    initializeProjectTree(findWizardContextNode(currentNode()), files, kind, currentAction);
  });

  const auto hideProjectUi = JsonWizard::boolFromVariant(m_hideProjectUiValue, m_wizard->expander());
  setProjectUiVisible(!hideProjectUi);

  initializeVersionControls();

  // Do a new try at initialization, now that we have real values set up:
  summarySettingsHaveChanged();
}

auto JsonSummaryPage::validatePage() -> bool
{
  m_wizard->commitToFileList(m_fileList);
  m_fileList.clear();
  return true;
}

auto JsonSummaryPage::cleanupPage() -> void
{
  disconnect(m_wizard, &JsonWizard::filesReady, this, nullptr);
}

auto JsonSummaryPage::triggerCommit(const JsonWizard::GeneratorFiles &files) -> void
{
  GeneratedFiles coreFiles = transform(files, &JsonWizard::GeneratorFile::file);

  QString errorMessage;
  if (!runVersionControl(coreFiles, &errorMessage)) {
    QMessageBox::critical(wizard(), tr("Failed to Commit to Version Control"), tr("Error message from Version Control System: \"%1\".").arg(errorMessage));
  }
}

auto JsonSummaryPage::addToProject(const JsonWizard::GeneratorFiles &files) -> void
{
  QTC_CHECK(m_fileList.isEmpty()); // Happens after this page is done
  const auto generatedProject = generatedProjectFilePath(files);
  const auto kind = wizardKind(m_wizard);

  const auto folder = currentNode();
  if (!folder)
    return;
  if (kind == IWizardFactory::ProjectWizard) {
    if (!static_cast<ProjectNode*>(folder)->addSubProject(generatedProject)) {
      QMessageBox::critical(m_wizard, tr("Failed to Add to Project"), tr("Failed to add subproject \"%1\"\nto project \"%2\".").arg(generatedProject.toUserOutput()).arg(folder->filePath().toUserOutput()));
      return;
    }
    m_wizard->removeAttributeFromAllFiles(GeneratedFile::OpenProjectAttribute);
  } else {
    const auto filePaths = transform(files, [](const JsonWizard::GeneratorFile &f) {
      return f.file.filePath();
    });
    if (!folder->addFiles(filePaths)) {
      QMessageBox::critical(wizard(), tr("Failed to Add to Project"), tr("Failed to add one or more files to project\n\"%1\" (%2).").arg(folder->filePath().toUserOutput(), FilePath::formatFilePaths(filePaths, ", ")));
      return;
    }
    const auto dependencies = m_wizard->stringValue("Dependencies").split(':', Qt::SkipEmptyParts);
    if (!dependencies.isEmpty())
      folder->addDependencies(dependencies);
  }
  return;
}

auto JsonSummaryPage::summarySettingsHaveChanged() -> void
{
  const auto vc = currentVersionControl();
  m_wizard->setValue(QLatin1String(KEY_VERSIONCONTROL), vc ? vc->id().toString() : QString());

  updateProjectData(currentNode());
}

auto JsonSummaryPage::findWizardContextNode(Node *contextNode) const -> Node*
{
  if (contextNode && !ProjectTree::hasNode(contextNode)) {
    contextNode = nullptr;

    // Static cast from void * to avoid qobject_cast (which needs a valid object) in value().
    const auto project = static_cast<Project*>(m_wizard->value(Constants::PROJECT_POINTER).value<void*>());
    if (SessionManager::projects().contains(project) && project->rootProjectNode()) {
      const auto path = m_wizard->value(Constants::PREFERRED_PROJECT_NODE_PATH).toString();
      contextNode = project->rootProjectNode()->findNode([path](const Node *n) {
        return path == n->filePath().toString();
      });
    }
  }
  return contextNode;
}

auto JsonSummaryPage::updateFileList() -> void
{
  m_fileList = m_wizard->generateFileList();
  const auto filePaths = transform(m_fileList, [](const JsonWizard::GeneratorFile &f) { return f.file.path(); });
  setFiles(filePaths);
}

auto JsonSummaryPage::updateProjectData(FolderNode *node) -> void
{
  const auto project = ProjectTree::projectForNode(node);

  m_wizard->setValue(QLatin1String(KEY_SELECTED_PROJECT), QVariant::fromValue(project));
  m_wizard->setValue(QLatin1String(KEY_SELECTED_NODE), QVariant::fromValue(node));
  m_wizard->setValue(QLatin1String(KEY_IS_SUBPROJECT), node ? true : false);
  auto qtKeyWordsEnabled = true;
  if (ProjectTree::hasNode(node)) {
    const ProjectNode *projectNode = node->asProjectNode();
    if (!projectNode)
      projectNode = node->parentProjectNode();
    while (projectNode) {
      const auto keywordsEnabled = projectNode->data(Constants::QT_KEYWORDS_ENABLED);
      if (keywordsEnabled.isValid()) {
        qtKeyWordsEnabled = keywordsEnabled.toBool();
        break;
      }
      if (projectNode->isProduct())
        break;
      projectNode = projectNode->parentProjectNode();
    }
  }
  m_wizard->setValue(QLatin1String(KEY_QT_KEYWORDS_ENABLED), qtKeyWordsEnabled);

  updateFileList();
}

} // namespace ProjectExplorer
