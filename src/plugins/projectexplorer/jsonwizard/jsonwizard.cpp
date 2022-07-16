// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizard.hpp"

#include "jsonwizardfactory.hpp"
#include "jsonwizardgeneratorfactory.hpp"

#include "../project.hpp"
#include "../projectexplorer.hpp"
#include "../projectexplorerconstants.hpp"
#include "../projecttree.hpp"
#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-message-manager.hpp>

#include <utils/algorithm.hpp>
#include <utils/itemviews.hpp>
#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>
#include <utils/wizardpage.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#ifdef WITH_TESTS
#include "jsonwizard_test.cpp"
#endif

namespace ProjectExplorer {

namespace Internal {

class ProjectFileTreeItem : public Utils::TreeItem {
public:
  ProjectFileTreeItem(JsonWizard::GeneratorFile *candidate) : m_candidate(candidate)
  {
    toggleProjectFileStatus(false);
  }

  auto toggleProjectFileStatus(bool on) -> void
  {
    m_candidate->file.setAttributes(m_candidate->file.attributes().setFlag(Orca::Plugin::Core::GeneratedFile::OpenProjectAttribute, on));
  }

private:
  auto data(int column, int role) const -> QVariant override
  {
    if (column != 0 || role != Qt::DisplayRole)
      return QVariant();
    return QDir::toNativeSeparators(m_candidate->file.path());
  }

  JsonWizard::GeneratorFile *const m_candidate;
};

class ProjectFilesModel : public Utils::TreeModel<Utils::TreeItem, ProjectFileTreeItem> {
public:
  ProjectFilesModel(const QList<JsonWizard::GeneratorFile*> &candidates, QObject *parent) : TreeModel(parent)
  {
    setHeader({QCoreApplication::translate("ProjectExplorer::JsonWizard", "Project File")});
    for (const auto candidate : candidates)
      rootItem()->appendChild(new ProjectFileTreeItem(candidate));
  }
};

class ProjectFileChooser : public QDialog {
public:
  ProjectFileChooser(const QList<JsonWizard::GeneratorFile*> &candidates, QWidget *parent) : QDialog(parent), m_view(new Utils::TreeView(this))
  {
    setWindowTitle(QCoreApplication::translate("ProjectExplorer::JsonWizard", "Choose Project File"));
    const auto model = new ProjectFilesModel(candidates, this);
    m_view->setSelectionMode(Utils::TreeView::ExtendedSelection);
    m_view->setSelectionBehavior(Utils::TreeView::SelectRows);
    m_view->setModel(model);
    const auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    const auto updateOkButton = [buttonBox, this] {
      buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_view->selectionModel()->hasSelection());
    };
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateOkButton);
    updateOkButton();
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    const auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QCoreApplication::translate("ProjectExplorer::JsonWizard", "The project contains more than one project file. " "Select the one you would like to use.")));
    layout->addWidget(m_view);
    layout->addWidget(buttonBox);
  }

private:
  auto accept() -> void override
  {
    const auto selected = m_view->selectionModel()->selectedRows();
    const auto *const model = static_cast<ProjectFilesModel*>(m_view->model());
    for (const auto &index : selected) {
      const auto item = static_cast<ProjectFileTreeItem*>(model->itemForIndex(index));
      QTC_ASSERT(item, continue);
      item->toggleProjectFileStatus(true);
    }
    QDialog::accept();
  }

  Utils::TreeView *const m_view;
};

} // namespace Internal

JsonWizard::JsonWizard(QWidget *parent) : Utils::Wizard(parent)
{
  setMinimumSize(800, 500);
  m_expander.registerExtraResolver([this](const QString &name, QString *ret) -> bool {
    *ret = stringValue(name);
    return !ret->isNull();
  });
  m_expander.registerPrefix("Exists", tr("Check whether a variable exists.<br>" "Returns \"true\" if it does and an empty string if not."), [this](const QString &value) -> QString {
    const QString key = QString::fromLatin1("%{") + value + QLatin1Char('}');
    return m_expander.expand(key) == key ? QString() : QLatin1String("true");
  });
  // override default JS macro by custom one that adds Wizard specific features
  m_jsExpander.registerObject("Wizard", new Internal::JsonWizardJsExtension(this));
  m_jsExpander.engine().evaluate("var value = Wizard.value");
  m_jsExpander.registerForExpander(&m_expander);
}

JsonWizard::~JsonWizard()
{
  qDeleteAll(m_generators);
}

auto JsonWizard::addGenerator(JsonWizardGenerator *gen) -> void
{
  QTC_ASSERT(gen, return);
  QTC_ASSERT(!m_generators.contains(gen), return);

  m_generators.append(gen);
}

auto JsonWizard::expander() -> Utils::MacroExpander*
{
  return &m_expander;
}

auto JsonWizard::generateFileList() -> JsonWizard::GeneratorFiles
{
  QString errorMessage;
  GeneratorFiles list;

  const auto targetPath = stringValue(QLatin1String("TargetPath"));
  if (targetPath.isEmpty())
    errorMessage = tr("Could not determine target path. \"TargetPath\" was not set on any page.");

  if (m_files.isEmpty() && errorMessage.isEmpty()) {
    emit preGenerateFiles();
    foreach(JsonWizardGenerator *gen, m_generators) {
      auto tmp = gen->fileList(&m_expander, stringValue(QStringLiteral("WizardDir")), targetPath, &errorMessage);
      if (!errorMessage.isEmpty())
        break;
      list.append(Utils::transform(tmp, [&gen](const Orca::Plugin::Core::GeneratedFile &f) { return JsonWizard::GeneratorFile(f, gen); }));
    }
  }

  if (!errorMessage.isEmpty()) {
    QMessageBox::critical(this, tr("File Generation Failed"), tr("The wizard failed to generate files.<br>" "The error message was: \"%1\".").arg(errorMessage));
    reject();
    return GeneratorFiles();
  }

  QList<GeneratorFile*> projectFiles;
  for (auto &f : list) {
    if (f.file.attributes().testFlag(Orca::Plugin::Core::GeneratedFile::OpenProjectAttribute))
      projectFiles << &f;
  }
  if (projectFiles.count() > 1)
    Internal::ProjectFileChooser(projectFiles, this).exec();

  return list;
}

auto JsonWizard::commitToFileList(const JsonWizard::GeneratorFiles &list) -> void
{
  m_files = list;
  emit postGenerateFiles(m_files);
}

auto JsonWizard::stringValue(const QString &n) const -> QString
{
  const auto v = value(n);
  if (!v.isValid())
    return QString();

  if (v.type() == QVariant::String) {
    auto tmp = m_expander.expand(v.toString());
    if (tmp.isEmpty())
      tmp = QString::fromLatin1(""); // Make sure isNull() is *not* true.
    return tmp;
  }

  if (v.type() == QVariant::StringList)
    return stringListToArrayString(v.toStringList(), &m_expander);

  return v.toString();
}

auto JsonWizard::setValue(const QString &key, const QVariant &value) -> void
{
  setProperty(key.toUtf8(), value);
}

auto JsonWizard::parseOptions(const QVariant &v, QString *errorMessage) -> QList<JsonWizard::OptionDefinition>
{
  QTC_ASSERT(errorMessage, return { });

  QList<JsonWizard::OptionDefinition> result;
  if (!v.isNull()) {
    const auto optList = JsonWizardFactory::objectOrList(v, errorMessage);
    foreach(const QVariant &o, optList) {
      auto optionObject = o.toMap();
      JsonWizard::OptionDefinition odef;
      odef.m_key = optionObject.value(QLatin1String("key")).toString();
      odef.m_value = optionObject.value(QLatin1String("value")).toString();
      odef.m_condition = optionObject.value(QLatin1String("condition"), true);
      odef.m_evaluate = optionObject.value(QLatin1String("evaluate"), false);

      if (odef.m_key.isEmpty()) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::Internal::JsonWizardFileGenerator", "No 'key' in options object.");
        result.clear();
        break;
      }
      result.append(odef);
    }
  }

  QTC_ASSERT(errorMessage->isEmpty() || (!errorMessage->isEmpty() && result.isEmpty()), return result);
  return result;
}

auto JsonWizard::value(const QString &n) const -> QVariant
{
  auto v = property(n.toUtf8());
  if (v.isValid())
    return v;
  if (hasField(n))
    return field(n); // Cannot contain macros!
  return QVariant();
}

auto JsonWizard::boolFromVariant(const QVariant &v, Utils::MacroExpander *expander) -> bool
{
  if (v.type() == QVariant::String) {
    const auto tmp = expander->expand(v.toString());
    return !(tmp.isEmpty() || tmp == QLatin1String("false"));
  }
  return v.toBool();
}

auto JsonWizard::stringListToArrayString(const QStringList &list, const Utils::MacroExpander *expander) -> QString
{
  // Todo: Handle ' embedded in the strings better.
  if (list.isEmpty())
    return QString();

  const auto tmp = Utils::transform(list, [expander](const QString &i) {
    return expander->expand(i).replace(QLatin1Char('\''), QLatin1String("\\'"));
  });

  QString result;
  result.append(QLatin1Char('\''));
  result.append(tmp.join(QLatin1String("', '")));
  result.append(QLatin1Char('\''));

  return result;
}

auto JsonWizard::removeAttributeFromAllFiles(Orca::Plugin::Core::GeneratedFile::Attribute a) -> void
{
  for (auto i = 0; i < m_files.count(); ++i) {
    if (m_files.at(i).file.attributes() & a)
      m_files[i].file.setAttributes(m_files.at(i).file.attributes() ^ a);
  }
}

auto JsonWizard::variables() const -> QHash<QString, QVariant>
{
  auto result = Wizard::variables();
  foreach(const QByteArray &p, dynamicPropertyNames()) {
    auto key = QString::fromUtf8(p);
    result.insert(key, value(key));
  }
  return result;
}

auto JsonWizard::accept() -> void
{
  const auto page = qobject_cast<Utils::WizardPage*>(currentPage());
  if (page && page->handleAccept())
    return;

  Utils::Wizard::accept();

  QString errorMessage;
  if (m_files.isEmpty()) {
    commitToFileList(generateFileList()); // The Summary page does this for us, but a wizard
    // does not need to have one.
  }
  QTC_ASSERT(!m_files.isEmpty(), return);

  emit prePromptForOverwrite(m_files);
  const auto overwrite = JsonWizardGenerator::promptForOverwrite(&m_files, &errorMessage);
  if (overwrite != JsonWizardGenerator::OverwriteOk) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Overwrite Files"), errorMessage);
    return;
  }

  emit preFormatFiles(m_files);
  if (!JsonWizardGenerator::formatFiles(this, &m_files, &errorMessage)) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Format Files"), errorMessage);
    return;
  }

  emit preWriteFiles(m_files);
  if (!JsonWizardGenerator::writeFiles(this, &m_files, &errorMessage)) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Write Files"), errorMessage);
    return;
  }

  emit postProcessFiles(m_files);
  if (!JsonWizardGenerator::postWrite(this, &m_files, &errorMessage)) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Post-Process Files"), errorMessage);
    return;
  }
  emit filesReady(m_files);
  if (!JsonWizardGenerator::polish(this, &m_files, &errorMessage)) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Polish Files"), errorMessage);
    return;
  }
  emit filesPolished(m_files);
  if (!JsonWizardGenerator::allDone(this, &m_files, &errorMessage)) {
    if (!errorMessage.isEmpty())
      QMessageBox::warning(this, tr("Failed to Open Files"), errorMessage);
    return;
  }
  emit allDone(m_files);

  openFiles(m_files);

  const auto node = static_cast<ProjectExplorer::Node*>(value(ProjectExplorer::Constants::PREFERRED_PROJECT_NODE).value<void*>());
  if (node && ProjectTree::hasNode(node)) // PREFERRED_PROJECT_NODE is not set for newly created projects
    openProjectForNode(node);
}

auto JsonWizard::reject() -> void
{
  const auto page = qobject_cast<Utils::WizardPage*>(currentPage());
  if (page && page->handleReject())
    return;

  Utils::Wizard::reject();
}

auto JsonWizard::handleNewPages(int pageId) -> void
{
  const auto wp = qobject_cast<Utils::WizardPage*>(page(pageId));
  if (!wp)
    return;

  connect(wp, &Utils::WizardPage::reportError, this, &JsonWizard::handleError);
}

auto JsonWizard::handleError(const QString &message) -> void
{
  Orca::Plugin::Core::MessageManager::writeDisrupting(message);
}

auto JsonWizard::stringify(const QVariant &v) const -> QString
{
  if (v.type() == QVariant::StringList)
    return stringListToArrayString(v.toStringList(), &m_expander);
  return Wizard::stringify(v);
}

auto JsonWizard::evaluate(const QVariant &v) const -> QString
{
  return m_expander.expand(stringify(v));
}

auto JsonWizard::openFiles(const JsonWizard::GeneratorFiles &files) -> void
{
  QString errorMessage;
  auto openedSomething = false;
  foreach(const JsonWizard::GeneratorFile &f, files) {
    const auto &file = f.file;
    if (!QFileInfo::exists(file.path())) {
      errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"%1\" does not exist in the file system.").arg(file.filePath().toUserOutput());
      break;
    }
    if (file.attributes() & Orca::Plugin::Core::GeneratedFile::OpenProjectAttribute) {
      auto result = ProjectExplorerPlugin::openProject(file.filePath());
      if (!result) {
        errorMessage = result.errorMessage();
        if (errorMessage.isEmpty()) {
          errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "Failed to open \"%1\" as a project.").arg(file.filePath().toUserOutput());
        }
        break;
      }
      result.project()->setNeedsInitialExpansion(true);
      openedSomething = true;
    }
    if (file.attributes() & Orca::Plugin::Core::GeneratedFile::OpenEditorAttribute) {
      const auto editor = Orca::Plugin::Core::EditorManager::openEditor(Utils::FilePath::fromString(file.path()), file.editorId());
      if (!editor) {
        errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "Failed to open an editor for \"%1\".").arg(file.filePath().toUserOutput());
        break;
      } else if (file.attributes() & Orca::Plugin::Core::GeneratedFile::TemporaryFile) {
        editor->document()->setTemporary(true);
      }
      openedSomething = true;
    }
  }

  const auto path = QDir::toNativeSeparators(m_expander.expand(value(QLatin1String("TargetPath")).toString()));

  // Now try to find the project file and open
  if (!openedSomething) {
    errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "No file to open found in \"%1\".").arg(path);
  }

  if (!errorMessage.isEmpty()) {
    const auto text = path.isEmpty() ? tr("Failed to open project.") : tr("Failed to open project in \"%1\".").arg(path);
    QMessageBox msgBox(QMessageBox::Warning, tr("Cannot Open Project"), text);
    msgBox.setDetailedText(errorMessage);
    msgBox.addButton(QMessageBox::Ok);
    msgBox.exec();
  }
}

auto JsonWizard::openProjectForNode(Node *node) -> void
{
  using namespace Utils;

  const ProjectNode *projNode = node->asProjectNode();
  if (!projNode) {
    if (const auto cn = node->asContainerNode())
      projNode = cn->rootProjectNode();
    else
      projNode = node->parentProjectNode();
  }
  QTC_ASSERT(projNode, return);

  const auto projFilePath = projNode->visibleAfterAddFileAction();

  if (projFilePath && !Orca::Plugin::Core::EditorManager::openEditor(projFilePath.value())) {
    const auto errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "Failed to open an editor for \"%1\".").arg(QDir::toNativeSeparators(projFilePath.value().toString()));
    QMessageBox::warning(nullptr, tr("Cannot Open Project"), errorMessage);
  }
}

auto JsonWizard::OptionDefinition::value(Utils::MacroExpander &expander) const -> QString
{
  if (JsonWizard::boolFromVariant(m_evaluate, &expander))
    return expander.expand(m_value);
  return m_value;
}

auto JsonWizard::OptionDefinition::condition(Utils::MacroExpander &expander) const -> bool
{
  return JsonWizard::boolFromVariant(m_condition, &expander);
}

namespace Internal {

JsonWizardJsExtension::JsonWizardJsExtension(JsonWizard *wizard) : m_wizard(wizard) {}

auto JsonWizardJsExtension::value(const QString &name) const -> QVariant
{
  return m_wizard->expander()->expandVariant(m_wizard->value(name));
}

} // namespace Internal
} // namespace ProjectExplorer
