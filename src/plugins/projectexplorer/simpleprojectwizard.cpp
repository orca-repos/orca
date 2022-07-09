// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "simpleprojectwizard.hpp"

#include "projectexplorerconstants.hpp"

#include <app/app_version.hpp>

#include <core/basefilewizard.hpp>
#include <core/icore.hpp>

#include <cmakeprojectmanager/cmakeprojectconstants.h>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectexplorericons.hpp>
#include <projectexplorer/customwizard/customwizard.hpp>
#include <projectexplorer/selectablefilesmodel.hpp>
#include <qmakeprojectmanager/qmakeprojectmanagerconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/filewizardpage.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/wizard.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>
#include <QWizardPage>

using namespace Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class SimpleProjectWizardDialog;

class FilesSelectionWizardPage : public QWizardPage {
  Q_OBJECT

public:
  FilesSelectionWizardPage(SimpleProjectWizardDialog *simpleProjectWizard);

  auto isComplete() const -> bool override { return m_filesWidget->hasFilesSelected(); }
  auto initializePage() -> void override;
  auto cleanupPage() -> void override { m_filesWidget->cancelParsing(); }
  auto selectedFiles() const -> FilePaths { return m_filesWidget->selectedFiles(); }
  auto selectedPaths() const -> FilePaths { return m_filesWidget->selectedPaths(); }
  auto qtModules() const -> QString { return m_qtModules; }
  auto buildSystem() const -> QString { return m_buildSystem; }

private:
  SimpleProjectWizardDialog *m_simpleProjectWizardDialog;
  SelectableFilesWidget *m_filesWidget;
  QString m_qtModules;
  QString m_buildSystem;
};

FilesSelectionWizardPage::FilesSelectionWizardPage(SimpleProjectWizardDialog *simpleProjectWizard) : m_simpleProjectWizardDialog(simpleProjectWizard), m_filesWidget(new SelectableFilesWidget(this))
{
  const auto layout = new QVBoxLayout(this);
  {
    const auto hlayout = new QHBoxLayout;
    hlayout->addWidget(new QLabel("Qt modules", this));
    auto lineEdit = new QLineEdit("core gui widgets", this);
    connect(lineEdit, &QLineEdit::editingFinished, this, [this, lineEdit] {
      m_qtModules = lineEdit->text();
    });
    m_qtModules = lineEdit->text();
    hlayout->addWidget(lineEdit);
    layout->addLayout(hlayout);
  }

  {
    const auto hlayout = new QHBoxLayout;
    hlayout->addWidget(new QLabel("Build system", this));
    const auto comboBox = new QComboBox(this);
    connect(comboBox, &QComboBox::currentTextChanged, this, [this](const QString &bs) {
      m_buildSystem = bs;
    });
    comboBox->addItems(QStringList() << "qmake" << "cmake");
    comboBox->setEditable(false);
    comboBox->setCurrentText("qmake");
    hlayout->addWidget(comboBox);
    layout->addLayout(hlayout);
  }

  layout->addWidget(m_filesWidget);
  m_filesWidget->setBaseDirEditable(false);
  m_filesWidget->enableFilterHistoryCompletion(Constants::ADD_FILES_DIALOG_FILTER_HISTORY_KEY);
  connect(m_filesWidget, &SelectableFilesWidget::selectedFilesChanged, this, &FilesSelectionWizardPage::completeChanged);

  setProperty(SHORT_TITLE_PROPERTY, tr("Files"));
}

class SimpleProjectWizardDialog : public BaseFileWizard {
  Q_OBJECT public:
  SimpleProjectWizardDialog(const BaseFileWizardFactory *factory, QWidget *parent) : BaseFileWizard(factory, QVariantMap(), parent)
  {
    setWindowTitle(tr("Import Existing Project"));

    m_firstPage = new FileWizardPage;
    m_firstPage->setTitle(tr("Project Name and Location"));
    m_firstPage->setFileNameLabel(tr("Project name:"));
    m_firstPage->setPathLabel(tr("Location:"));
    addPage(m_firstPage);

    m_secondPage = new FilesSelectionWizardPage(this);
    m_secondPage->setTitle(tr("File Selection"));
    addPage(m_secondPage);
  }

  auto projectDir() const -> FilePath { return m_firstPage->filePath(); }
  auto setProjectDir(const FilePath &path) -> void { m_firstPage->setFilePath(path); }
  auto selectedFiles() const -> FilePaths { return m_secondPage->selectedFiles(); }
  auto selectedPaths() const -> FilePaths { return m_secondPage->selectedPaths(); }
  auto qtModules() const -> QString { return m_secondPage->qtModules(); }
  auto buildSystem() const -> QString { return m_secondPage->buildSystem(); }
  auto projectName() const -> QString { return m_firstPage->fileName(); }

  FileWizardPage *m_firstPage;
  FilesSelectionWizardPage *m_secondPage;
};

auto FilesSelectionWizardPage::initializePage() -> void
{
  m_filesWidget->resetModel(m_simpleProjectWizardDialog->projectDir(), FilePaths());
}

SimpleProjectWizard::SimpleProjectWizard()
{
  setSupportedProjectTypes({QmakeProjectManager::Constants::QMAKEPROJECT_ID, CMakeProjectManager::Constants::CMAKE_PROJECT_ID});
  setIcon(Icons::WIZARD_IMPORT_AS_PROJECT.icon());
  setDisplayName(tr("Import as qmake or cmake Project (Limited Functionality)"));
  setId("Z.DummyProFile");
  setDescription(tr("Imports existing projects that do not use qmake, CMake, Qbs, Meson, or Autotools.<p>" "This creates a project file that allows you to use %1 as a code editor " "and as a launcher for debugging and analyzing tools. " "If you want to build the project, you might need to edit the generated project file.").arg(Core::Constants::IDE_DISPLAY_NAME));
  setCategory(Constants::IMPORT_WIZARD_CATEGORY);
  setDisplayCategory(Constants::IMPORT_WIZARD_CATEGORY_DISPLAY);
  setFlags(PlatformIndependent);
}

auto SimpleProjectWizard::create(QWidget *parent, const WizardDialogParameters &parameters) const -> BaseFileWizard*
{
  const auto wizard = new SimpleProjectWizardDialog(this, parent);
  wizard->setProjectDir(parameters.defaultPath());

  for (const auto p : wizard->extensionPages())
    wizard->addPage(p);

  return wizard;
}

auto generateQmakeFiles(const SimpleProjectWizardDialog *wizard, QString *errorMessage) -> GeneratedFiles
{
  Q_UNUSED(errorMessage)
  const auto projectPath = wizard->projectDir().toString();
  const QDir dir(projectPath);
  const auto projectName = wizard->projectName();
  const auto proFileName = FilePath::fromString(QFileInfo(dir, projectName + ".pro").absoluteFilePath());
  const auto paths = transform(wizard->selectedPaths(), &FilePath::toString);

  const auto headerType = mimeTypeForName("text/x-chdr");

  const auto nameFilters = headerType.globPatterns();

  QString proIncludes = "INCLUDEPATH = \\\n";
  for (const auto &path : paths) {
    QFileInfo fileInfo(path);
    QDir thisDir(fileInfo.absoluteFilePath());
    if (!thisDir.entryList(nameFilters, QDir::Files).isEmpty()) {
      auto relative = dir.relativeFilePath(path);
      if (!relative.isEmpty())
        proIncludes.append("    $$PWD/" + relative + " \\\n");
    }
  }

  QString proSources = "SOURCES = \\\n";
  QString proHeaders = "HEADERS = \\\n";

  for (const auto &fileName : wizard->selectedFiles()) {
    auto source = dir.relativeFilePath(fileName.toString());
    auto mimeType = mimeTypeForFile(fileName.toFileInfo());
    if (mimeType.matchesName("text/x-chdr") || mimeType.matchesName("text/x-c++hdr"))
      proHeaders += "   $$PWD/" + source + " \\\n";
    else
      proSources += "   $$PWD/" + source + " \\\n";
  }

  proHeaders.chop(3);
  proSources.chop(3);
  proIncludes.chop(3);

  GeneratedFile generatedProFile(proFileName);
  generatedProFile.setAttributes(GeneratedFile::OpenProjectAttribute);
  generatedProFile.setContents("# Created by and for " + QLatin1String(Core::Constants::IDE_DISPLAY_NAME) + " This file was created for editing the project sources only.\n" "# You may attempt to use it for building too, by modifying this file here.\n\n" "#TARGET = " + projectName + "\n\n" "QT = " + wizard->qtModules() + "\n\n" + proHeaders + "\n\n" + proSources + "\n\n" + proIncludes + "\n\n" "#DEFINES = \n\n");

  return GeneratedFiles{generatedProFile};
}

auto generateCmakeFiles(const SimpleProjectWizardDialog *wizard, QString *errorMessage) -> GeneratedFiles
{
  Q_UNUSED(errorMessage)
  const QDir dir(wizard->projectDir().toString());
  const auto projectName = wizard->projectName();
  const auto projectFileName = FilePath::fromString(QFileInfo(dir, "CMakeLists.txt").absoluteFilePath());
  const auto paths = transform(wizard->selectedPaths(), &FilePath::toString);

  const auto headerType = mimeTypeForName("text/x-chdr");

  const auto nameFilters = headerType.globPatterns();

  QString includes = "include_directories(\n";
  auto haveIncludes = false;
  for (const auto &path : paths) {
    QFileInfo fileInfo(path);
    QDir thisDir(fileInfo.absoluteFilePath());
    if (!thisDir.entryList(nameFilters, QDir::Files).isEmpty()) {
      auto relative = dir.relativeFilePath(path);
      if (!relative.isEmpty()) {
        includes.append("    " + relative + "\n");
        haveIncludes = true;
      }
    }
  }
  if (haveIncludes)
    includes += ")";
  else
    includes.clear();

  QString srcs = "set (SRCS\n";
  for (const auto &fileName : wizard->selectedFiles())
    srcs += "    " + dir.relativeFilePath(fileName.toString()) + "\n";
  srcs += ")\n";

  QString components = "find_package(Qt5 COMPONENTS";
  QString libs = "target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE";
  auto haveQtModules = false;
  for (auto c : wizard->qtModules().split(' ')) {
    if (c.isEmpty())
      continue;
    c[0] = c[0].toUpper();
    libs += " Qt5::" + c;
    components += " " + c;
    haveQtModules = true;
  }
  if (haveQtModules) {
    libs += ")\n";
    components += " REQUIRED)";
  } else {
    libs.clear();
    components.clear();
  }

  GeneratedFile generatedProFile(projectFileName);
  generatedProFile.setAttributes(GeneratedFile::OpenProjectAttribute);
  generatedProFile.setContents("# Created by and for " + QLatin1String(Core::Constants::IDE_DISPLAY_NAME) + " This file was created for editing the project sources only.\n" "# You may attempt to use it for building too, by modifying this file here.\n\n" "cmake_minimum_required(VERSION 3.5)\n" "project(" + projectName + ")\n\n" "set(CMAKE_INCLUDE_CURRENT_DIR ON)\n" "set(CMAKE_AUTOUIC ON)\n" "set(CMAKE_AUTOMOC ON)\n" "set(CMAKE_AUTORCC ON)\n" "set(CMAKE_CXX_STANDARD 11)\n" "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n" + components + "\n\n" + includes + "\n\n" + srcs + "\n\n" "add_executable(${CMAKE_PROJECT_NAME} ${SRCS})\n\n" + libs);
  return GeneratedFiles{generatedProFile};
}

auto SimpleProjectWizard::generateFiles(const QWizard *w, QString *errorMessage) const -> GeneratedFiles
{
  Q_UNUSED(errorMessage)

  const auto wizard = qobject_cast<const SimpleProjectWizardDialog*>(w);
  if (wizard->buildSystem() == "qmake")
    return generateQmakeFiles(wizard, errorMessage);
  else if (wizard->buildSystem() == "cmake")
    return generateCmakeFiles(wizard, errorMessage);

  if (errorMessage)
    *errorMessage = tr("Unknown build system \"%1\"").arg(wizard->buildSystem());
  return {};
}

auto SimpleProjectWizard::postGenerateFiles(const QWizard *w, const GeneratedFiles &l, QString *errorMessage) const -> bool
{
  Q_UNUSED(w)
  return CustomProjectWizard::postGenerateOpen(l, errorMessage);
}

} // namespace Internal
} // namespace GenericProjectManager

#include "simpleprojectwizard.moc"
