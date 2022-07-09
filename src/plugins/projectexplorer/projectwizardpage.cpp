// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectwizardpage.hpp"
#include "ui_projectwizardpage.h"

#include "project.hpp"
#include "projectmodels.hpp"
#include "session.hpp"

#include <core/icore.hpp>
#include <core/iversioncontrol.hpp>
#include <core/iwizardfactory.hpp>
#include <core/vcsmanager.hpp>
#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/treemodel.hpp>
#include <utils/wizard.hpp>
#include <constants/vcsbase/vcsbaseconstants.hpp>

#include <QDir>
#include <QTextStream>
#include <QTreeView>

/*!
    \class ProjectExplorer::Internal::ProjectWizardPage

    \brief The ProjectWizardPage class provides a wizard page showing projects
    and version control to add new files to.

    \sa ProjectExplorer::Internal::ProjectFileWizardExtension
*/

using namespace Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class AddNewTree : public TreeItem {
public:
  AddNewTree(const QString &displayName);
  AddNewTree(FolderNode *node, QList<AddNewTree*> children, const QString &displayName);
  AddNewTree(FolderNode *node, QList<AddNewTree*> children, const FolderNode::AddNewInformation &info);

  auto data(int column, int role) const -> QVariant override;
  auto flags(int column) const -> Qt::ItemFlags override;
  auto displayName() const -> QString { return m_displayName; }
  auto node() const -> FolderNode* { return m_node; }
  auto priority() const -> int { return m_priority; }

private:
  QString m_displayName;
  QString m_toolTip;
  FolderNode *m_node = nullptr;
  bool m_canAdd = true;
  int m_priority = -1;
};

AddNewTree::AddNewTree(const QString &displayName) : m_displayName(displayName) { }

// FIXME: potentially merge the following two functions.
// Note the different handling of 'node' and m_canAdd.
AddNewTree::AddNewTree(FolderNode *node, QList<AddNewTree*> children, const QString &displayName) : m_displayName(displayName), m_node(node), m_canAdd(false)
{
  if (node)
    m_toolTip = node->directory().toString();
  foreach(AddNewTree *child, children)
    appendChild(child);
}

AddNewTree::AddNewTree(FolderNode *node, QList<AddNewTree*> children, const FolderNode::AddNewInformation &info) : m_displayName(info.displayName), m_node(node), m_priority(info.priority)
{
  if (node)
    m_toolTip = node->directory().toString();
  foreach(AddNewTree *child, children)
    appendChild(child);
}

auto AddNewTree::data(int, int role) const -> QVariant
{
  switch (role) {
  case Qt::DisplayRole:
    return m_displayName;
  case Qt::ToolTipRole:
    return m_toolTip;
  case Qt::UserRole:
    return QVariant::fromValue(static_cast<void*>(node()));
  default:
    return QVariant();
  }
}

auto AddNewTree::flags(int) const -> Qt::ItemFlags
{
  if (m_canAdd)
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
  return Qt::NoItemFlags;
}

// --------------------------------------------------------------------
// BestNodeSelector:
// --------------------------------------------------------------------

class BestNodeSelector {
public:
  BestNodeSelector(const QString &commonDirectory, const FilePaths &files);
  auto inspect(AddNewTree *tree, bool isContextNode) -> void;
  auto bestChoice() const -> AddNewTree*;
  auto deploys() -> bool;
  auto deployingProjects() const -> QString;

private:
  QString m_commonDirectory;
  FilePaths m_files;
  bool m_deploys = false;
  QString m_deployText;
  AddNewTree *m_bestChoice = nullptr;
  int m_bestMatchLength = -1;
  int m_bestMatchPriority = -1;
};

BestNodeSelector::BestNodeSelector(const QString &commonDirectory, const FilePaths &files) : m_commonDirectory(commonDirectory), m_files(files), m_deployText(QCoreApplication::translate("ProjectWizard", "The files are implicitly added to the projects:") + QLatin1Char('\n')) { }

// Find the project the new files should be added
// If any node deploys the files, then we don't want to add the files.
// Otherwise consider their common path. Either a direct match on the directory
// or the directory with the longest matching path (list containing"/project/subproject1"
// matching common path "/project/subproject1/newuserpath").
auto BestNodeSelector::inspect(AddNewTree *tree, bool isContextNode) -> void
{
  const auto node = tree->node();
  if (node->isProjectNodeType()) {
    if (static_cast<ProjectNode*>(node)->deploysFolder(m_commonDirectory)) {
      m_deploys = true;
      m_deployText += tree->displayName() + QLatin1Char('\n');
    }
  }
  if (m_deploys)
    return;

  const auto projectDirectory = node->directory().toString();
  const int projectDirectorySize = projectDirectory.size();
  if (m_commonDirectory != projectDirectory && !m_commonDirectory.startsWith(projectDirectory + QLatin1Char('/')) && !isContextNode)
    return;

  const auto betterMatch = isContextNode || (tree->priority() > 0 && (projectDirectorySize > m_bestMatchLength || (projectDirectorySize == m_bestMatchLength && tree->priority() > m_bestMatchPriority)));

  if (betterMatch) {
    m_bestMatchPriority = tree->priority();
    m_bestMatchLength = isContextNode ? std::numeric_limits<int>::max() : projectDirectorySize;
    m_bestChoice = tree;
  }
}

auto BestNodeSelector::bestChoice() const -> AddNewTree*
{
  if (m_deploys)
    return nullptr;
  return m_bestChoice;
}

auto BestNodeSelector::deploys() -> bool
{
  return m_deploys;
}

auto BestNodeSelector::deployingProjects() const -> QString
{
  if (m_deploys)
    return m_deployText;
  return QString();
}

// --------------------------------------------------------------------
// Helper:
// --------------------------------------------------------------------

static inline auto createNoneNode(BestNodeSelector *selector) -> AddNewTree*
{
  auto displayName = QCoreApplication::translate("ProjectWizard", "<None>");
  if (selector->deploys())
    displayName = QCoreApplication::translate("ProjectWizard", "<Implicitly Add>");
  return new AddNewTree(displayName);
}

static inline auto buildAddProjectTree(ProjectNode *root, const FilePath &projectPath, Node *contextNode, BestNodeSelector *selector) -> AddNewTree*
{
  QList<AddNewTree*> children;
  for (const auto node : root->nodes()) {
    if (const auto pn = node->asProjectNode()) {
      if (const auto child = buildAddProjectTree(pn, projectPath, contextNode, selector))
        children.append(child);
    }
  }

  if (root->supportsAction(AddSubProject, root) && !root->supportsAction(InheritedFromParent, root)) {
    if (projectPath.isEmpty() || root->canAddSubProject(projectPath)) {
      const auto info = root->addNewInformation({projectPath}, contextNode);
      const auto item = new AddNewTree(root, children, info);
      selector->inspect(item, root == contextNode);
      return item;
    }
  }

  if (children.isEmpty())
    return nullptr;
  return new AddNewTree(root, children, root->displayName());
}

static auto buildAddFilesTree(FolderNode *root, const FilePaths &files, Node *contextNode, BestNodeSelector *selector) -> AddNewTree*
{
  QList<AddNewTree*> children;
  foreach(FolderNode *fn, root->folderNodes()) {
    const auto child = buildAddFilesTree(fn, files, contextNode, selector);
    if (child)
      children.append(child);
  }

  if (root->supportsAction(AddNewFile, root) && !root->supportsAction(InheritedFromParent, root)) {
    const auto info = root->addNewInformation(files, contextNode);
    const auto item = new AddNewTree(root, children, info);
    selector->inspect(item, root == contextNode);
    return item;
  }
  if (children.isEmpty())
    return nullptr;
  return new AddNewTree(root, children, root->displayName());
}

// --------------------------------------------------------------------
// ProjectWizardPage:
// --------------------------------------------------------------------

ProjectWizardPage::ProjectWizardPage(QWidget *parent) : WizardPage(parent), m_ui(new Ui::WizardPage)
{
  m_ui->setupUi(this);
  m_ui->vcsManageButton->setText(ICore::msgShowOptionsDialog());
  connect(m_ui->projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectWizardPage::projectChanged);
  connect(m_ui->addToVersionControlComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectWizardPage::versionControlChanged);
  connect(m_ui->vcsManageButton, &QAbstractButton::clicked, this, &ProjectWizardPage::manageVcs);
  setProperty(SHORT_TITLE_PROPERTY, tr("Summary"));

  connect(VcsManager::instance(), &VcsManager::configurationChanged, this, &ProjectWizardPage::initializeVersionControls);

  m_ui->projectComboBox->setModel(&m_model);
}

ProjectWizardPage::~ProjectWizardPage()
{
  disconnect(m_ui->projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectWizardPage::projectChanged);
  delete m_ui;
}

auto ProjectWizardPage::expandTree(const QModelIndex &root) -> bool
{
  auto expand = false;
  if (!root.isValid()) // always expand root
    expand = true;

  // Check children
  const auto count = m_model.rowCount(root);
  for (auto i = 0; i < count; ++i) {
    if (expandTree(m_model.index(i, 0, root)))
      expand = true;
  }

  // Apply to self
  if (expand)
    m_ui->projectComboBox->view()->expand(root);
  else
    m_ui->projectComboBox->view()->collapse(root);

  // if we are a high priority node, our *parent* needs to be expanded
  const auto tree = static_cast<AddNewTree*>(root.internalPointer());
  if (tree && tree->priority() >= 100)
    expand = true;

  return expand;
}

auto ProjectWizardPage::setBestNode(AddNewTree *tree) -> void
{
  auto index = tree ? m_model.indexForItem(tree) : QModelIndex();
  m_ui->projectComboBox->setCurrentIndex(index);

  while (index.isValid()) {
    m_ui->projectComboBox->view()->expand(index);
    index = index.parent();
  }
}

auto ProjectWizardPage::currentNode() const -> FolderNode*
{
  const QVariant v = m_ui->projectComboBox->currentData(Qt::UserRole);
  return v.isNull() ? nullptr : static_cast<FolderNode*>(v.value<void*>());
}

auto ProjectWizardPage::setAddingSubProject(bool addingSubProject) -> void
{
  m_ui->projectLabel->setText(addingSubProject ? tr("Add as a subproject to project:") : tr("Add to &project:"));
}

auto ProjectWizardPage::initializeVersionControls() -> void
{
  // Figure out version control situation:
  // 0) Check that any version control is available
  // 1) Directory is managed and VCS supports "Add" -> List it
  // 2) Directory is managed and VCS does not support "Add" -> None available
  // 3) Directory is not managed -> Offer all VCS that support "CreateRepository"

  const auto versionControls = VcsManager::versionControls();
  if (versionControls.isEmpty())
    hideVersionControlUiElements();

  IVersionControl *currentSelection = nullptr;
  const auto currentIdx = versionControlIndex() - 1;
  if (currentIdx >= 0 && currentIdx <= m_activeVersionControls.size() - 1)
    currentSelection = m_activeVersionControls.at(currentIdx);

  m_activeVersionControls.clear();

  auto versionControlChoices = QStringList(tr("<None>"));
  if (!m_commonDirectory.isEmpty()) {
    const auto managingControl = VcsManager::findVersionControlForDirectory(FilePath::fromString(m_commonDirectory));
    if (managingControl) {
      // Under VCS
      if (managingControl->supportsOperation(IVersionControl::AddOperation)) {
        versionControlChoices.append(managingControl->displayName());
        m_activeVersionControls.push_back(managingControl);
        m_repositoryExists = true;
      }
    } else {
      // Create
      foreach(IVersionControl *vc, VcsManager::versionControls()) {
        if (vc->supportsOperation(IVersionControl::CreateRepositoryOperation)) {
          versionControlChoices.append(vc->displayName());
          m_activeVersionControls.append(vc);
        }
      }
      m_repositoryExists = false;
    }
  } // has a common root.

  setVersionControls(versionControlChoices);
  // Enable adding to version control by default.
  if (m_repositoryExists && versionControlChoices.size() >= 2)
    setVersionControlIndex(1);
  if (!m_repositoryExists) {
    const int newIdx = m_activeVersionControls.indexOf(currentSelection) + 1;
    setVersionControlIndex(newIdx);
  }
}

auto ProjectWizardPage::runVersionControl(const QList<GeneratedFile> &files, QString *errorMessage) -> bool
{
  // Add files to  version control (Entry at 0 is 'None').
  const auto vcsIndex = versionControlIndex() - 1;
  if (vcsIndex < 0 || vcsIndex >= m_activeVersionControls.size())
    return true;
  QTC_ASSERT(!m_commonDirectory.isEmpty(), return false);

  const auto versionControl = m_activeVersionControls.at(vcsIndex);
  // Create repository?
  if (!m_repositoryExists) {
    QTC_ASSERT(versionControl->supportsOperation(IVersionControl::CreateRepositoryOperation), return false);
    if (!versionControl->vcsCreateRepository(FilePath::fromString(m_commonDirectory))) {
      *errorMessage = tr("A version control system repository could not be created in \"%1\".").arg(m_commonDirectory);
      return false;
    }
  }
  // Add files if supported.
  if (versionControl->supportsOperation(IVersionControl::AddOperation)) {
    foreach(const GeneratedFile &generatedFile, files) {
      if (!versionControl->vcsAdd(generatedFile.filePath())) {
        *errorMessage = tr("Failed to add \"%1\" to the version control system.").arg(generatedFile.path());
        return false;
      }
    }
  }
  return true;
}

auto ProjectWizardPage::initializeProjectTree(Node *context, const FilePaths &paths, IWizardFactory::WizardKind kind, ProjectAction action) -> void
{
  BestNodeSelector selector(m_commonDirectory, paths);

  const auto root = m_model.rootItem();
  root->removeChildren();
  for (const auto project : SessionManager::projects()) {
    if (const auto pn = project->rootProjectNode()) {
      if (kind == IWizardFactory::ProjectWizard) {
        if (const auto child = buildAddProjectTree(pn, paths.first(), context, &selector))
          root->appendChild(child);
      } else {
        if (const auto child = buildAddFilesTree(pn, paths, context, &selector))
          root->appendChild(child);
      }
    }
  }
  root->sortChildren([](const TreeItem *ti1, const TreeItem *ti2) {
    return compareNodes(static_cast<const AddNewTree*>(ti1)->node(), static_cast<const AddNewTree*>(ti2)->node());
  });
  root->prependChild(createNoneNode(&selector));

  // Set combobox to context node if that appears in the tree:
  auto predicate = [context](TreeItem *ti) { return static_cast<AddNewTree*>(ti)->node() == context; };
  const auto contextItem = root->findAnyChild(predicate);
  if (contextItem)
    m_ui->projectComboBox->setCurrentIndex(m_model.indexForItem(contextItem));

  setAdditionalInfo(selector.deployingProjects());
  setBestNode(selector.bestChoice());
  setAddingSubProject(action == AddSubProject);

  m_ui->projectComboBox->setEnabled(m_model.rowCount(QModelIndex()) > 1);
}

auto ProjectWizardPage::setNoneLabel(const QString &label) -> void
{
  m_ui->projectComboBox->setItemText(0, label);
}

auto ProjectWizardPage::setAdditionalInfo(const QString &text) -> void
{
  m_ui->additionalInfo->setText(text);
  m_ui->additionalInfo->setVisible(!text.isEmpty());
}

auto ProjectWizardPage::setVersionControls(const QStringList &vcs) -> void
{
  m_ui->addToVersionControlComboBox->clear();
  m_ui->addToVersionControlComboBox->addItems(vcs);
}

auto ProjectWizardPage::versionControlIndex() const -> int
{
  return m_ui->addToVersionControlComboBox->currentIndex();
}

auto ProjectWizardPage::setVersionControlIndex(int idx) -> void
{
  m_ui->addToVersionControlComboBox->setCurrentIndex(idx);
}

auto ProjectWizardPage::currentVersionControl() -> IVersionControl*
{
  const int index = m_ui->addToVersionControlComboBox->currentIndex() - 1; // Subtract "<None>"
  if (index < 0 || index > m_activeVersionControls.count())
    return nullptr; // <None>
  return m_activeVersionControls.at(index);
}

auto ProjectWizardPage::setFiles(const QStringList &fileNames) -> void
{
  if (fileNames.count() == 1)
    m_commonDirectory = QFileInfo(fileNames.first()).absolutePath();
  else
    m_commonDirectory = commonPath(fileNames);
  QString fileMessage;
  {
    QTextStream str(&fileMessage);
    str << "<qt>" << (m_commonDirectory.isEmpty() ? tr("Files to be added:") : tr("Files to be added in")) << "<pre>";

    QStringList formattedFiles;
    if (m_commonDirectory.isEmpty()) {
      formattedFiles = fileNames;
    } else {
      str << QDir::toNativeSeparators(m_commonDirectory) << ":\n\n";
      int prefixSize = m_commonDirectory.size();
      if (!m_commonDirectory.endsWith('/'))
        ++prefixSize;
      formattedFiles = transform(fileNames, [prefixSize](const QString &f) { return f.mid(prefixSize); });
    }
    // Alphabetically, and files in sub-directories first
    sort(formattedFiles, [](const QString &filePath1, const QString &filePath2) -> bool {
      const auto filePath1HasDir = filePath1.contains(QLatin1Char('/'));
      const auto filePath2HasDir = filePath2.contains(QLatin1Char('/'));

      if (filePath1HasDir == filePath2HasDir)
        return FilePath::fromString(filePath1) < FilePath::fromString(filePath2);
      return filePath1HasDir;
    });

    foreach(const QString &f, formattedFiles)
      str << QDir::toNativeSeparators(f) << '\n';

    str << "</pre>";
  }
  m_ui->filesLabel->setText(fileMessage);
}

auto ProjectWizardPage::setProjectToolTip(const QString &tt) -> void
{
  m_ui->projectComboBox->setToolTip(tt);
  m_ui->projectLabel->setToolTip(tt);
}

auto ProjectWizardPage::projectChanged(int index) -> void
{
  setProjectToolTip(index >= 0 && index < m_projectToolTips.size() ? m_projectToolTips.at(index) : QString());
  emit projectNodeChanged();
}

auto ProjectWizardPage::manageVcs() -> void
{
  ICore::showOptionsDialog(VcsBase::Constants::VCS_COMMON_SETTINGS_ID, this);
}

auto ProjectWizardPage::hideVersionControlUiElements() -> void
{
  m_ui->addToVersionControlLabel->hide();
  m_ui->vcsManageButton->hide();
  m_ui->addToVersionControlComboBox->hide();
}

auto ProjectWizardPage::setProjectUiVisible(bool visible) -> void
{
  m_ui->projectLabel->setVisible(visible);
  m_ui->projectComboBox->setVisible(visible);
}

} // namespace Internal
} // namespace ProjectExplorer
