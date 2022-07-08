// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwizard.hpp"
#include "customwizardparameters.hpp"
#include "customwizardpage.hpp"
#include "customwizardscriptgenerator.hpp"

#include <projectexplorer/baseprojectwizarddialog.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/runconfiguration.hpp>

#include <core/icore.hpp>
#include <core/messagemanager.hpp>

#include <extensionsystem/pluginmanager.hpp>
#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QFile>
#include <QMap>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

using namespace Core;
using namespace Utils;

namespace ProjectExplorer {

constexpr char templatePathC[] = "templates/wizards";
constexpr char configFileC[] = "wizard.xml";

static auto enableLoadTemplateFiles() -> bool
{
#ifdef WITH_TESTS
    static bool value = qEnvironmentVariableIsEmpty("QTC_DISABLE_LOAD_TEMPLATES_FOR_TEST");
#else
    static auto value = true;
#endif
    return value;
}

static QList<ICustomWizardMetaFactory *> g_customWizardMetaFactories;

ICustomWizardMetaFactory::ICustomWizardMetaFactory(const QString &klass, IWizardFactory::WizardKind kind) : m_klass(klass), m_kind(kind)
{
  g_customWizardMetaFactories.append(this);
}

ICustomWizardMetaFactory::~ICustomWizardMetaFactory()
{
  g_customWizardMetaFactories.removeOne(this);
}

namespace Internal {
/*!
    \class ProjectExplorer::ICustomWizardFactory
    \brief The ICustomWizardFactory class implements a factory for creating
    custom wizards extending the base classes: CustomWizard and
    CustomProjectWizard.

    The factory can be registered under a name in CustomWizard. The name can
    be specified in the  \c <wizard class=''...> attribute in the \c wizard.xml file
    and thus allows for specifying a C++ derived wizard class.
    For example, this is currently used in Qt4ProjectManager to get Qt-specific
    aspects into the custom wizard.

    \sa ProjectExplorer::CustomWizard, ProjectExplorer::CustomProjectWizard
*/

class CustomWizardPrivate {
public:
  CustomWizardPrivate() : m_context(new CustomWizardContext) {}

  QSharedPointer<CustomWizardParameters> m_parameters;
  QSharedPointer<CustomWizardContext> m_context;
  static int verbose;
};

int CustomWizardPrivate::verbose = 0;

} // namespace Internal

using namespace Internal;

/*!
    \class ProjectExplorer::CustomWizard

    \brief The CustomWizard class is a base class for custom wizards based on
    file templates and an XML
    configuration file (\c share/qtcreator/templates/wizards).

    Presents CustomWizardDialog (fields page containing path control) for wizards
    of type "class" or "file". Serves as base class for project wizards.
*/

CustomWizard::CustomWizard() : d(new CustomWizardPrivate) {}

CustomWizard::~CustomWizard()
{
  delete d;
}

auto CustomWizard::setVerbose(int v) -> void
{
  CustomWizardPrivate::verbose = v;
}

auto CustomWizard::verbose() -> int
{
  return CustomWizardPrivate::verbose;
}

auto CustomWizard::setParameters(const CustomWizardParametersPtr &p) -> void
{
  QTC_ASSERT(p, return);

  d->m_parameters = p;

  setId(p->id);
  setSupportedProjectTypes((p->kind == FileWizard) ? QSet<Id>() : QSet<Id>() << "UNKNOWN_PROJECT");
  setIcon(p->icon);
  setDescription(p->description);
  setDisplayName(p->displayName);
  setCategory(p->category);
  setDisplayCategory(p->displayCategory);
  setRequiredFeatures(p->requiredFeatures);
  setFlags(p->flags);
}

auto CustomWizard::create(QWidget *parent, const WizardDialogParameters &p) const -> BaseFileWizard*
{
  QTC_ASSERT(!d->m_parameters.isNull(), return nullptr);
  const auto wizard = new BaseFileWizard(this, p.extraValues(), parent);

  d->m_context->reset();
  const auto customPage = new CustomWizardPage(d->m_context, parameters());
  customPage->setFilePath(p.defaultPath());
  if (parameters()->firstPageId >= 0)
    wizard->setPage(parameters()->firstPageId, customPage);
  else
    wizard->addPage(customPage);
  foreach(QWizardPage *ep, wizard->extensionPages())
    wizard->addPage(ep);
  if (CustomWizardPrivate::verbose)
    qDebug() << "initWizardDialog" << wizard << wizard->pageIds();

  return wizard;
}

// Read out files and store contents with field contents replaced.
static auto createFile(CustomWizardFile cwFile, const QString &sourceDirectory, const QString &targetDirectory, const CustomProjectWizard::FieldReplacementMap &fm, GeneratedFiles *files, QString *errorMessage) -> bool
{
  const QChar slash = QLatin1Char('/');
  const QString sourcePath = sourceDirectory + slash + cwFile.source;
  // Field replacement on target path
  CustomWizardContext::replaceFields(fm, &cwFile.target);
  const QString targetPath = targetDirectory + slash + cwFile.target;
  if (CustomWizardPrivate::verbose)
    qDebug() << "generating " << targetPath << sourcePath << fm;

  // Read contents of source file
  const auto openMode = cwFile.binary ? QIODevice::ReadOnly : (QIODevice::ReadOnly | QIODevice::Text);
  FileReader reader;
  if (!reader.fetch(FilePath::fromString(sourcePath), openMode, errorMessage))
    return false;

  GeneratedFile generatedFile;
  generatedFile.setPath(targetPath);
  if (cwFile.binary) {
    // Binary file: Set data.
    generatedFile.setBinary(true);
    generatedFile.setBinaryContents(reader.data());
  } else {
    // Template file: Preprocess.
    const auto contentsIn = QString::fromLocal8Bit(reader.data());
    generatedFile.setContents(CustomWizardContext::processFile(fm, contentsIn));
  }

  GeneratedFile::Attributes attributes;
  if (cwFile.openEditor)
    attributes |= GeneratedFile::OpenEditorAttribute;
  if (cwFile.openProject)
    attributes |= GeneratedFile::OpenProjectAttribute;
  generatedFile.setAttributes(attributes);
  files->push_back(generatedFile);
  return true;
}

// Helper to find a specific wizard page of a wizard by type.
template <class WizardPage>
auto findWizardPage(const QWizard *w) -> WizardPage*
{
  foreach(int pageId, w->pageIds()) if (auto wp = qobject_cast<WizardPage*>(w->page(pageId)))
    return wp;
  return nullptr;
}

// Determine where to run the generator script. The user may specify
// an expression subject to field replacement, default is the target path.
static auto scriptWorkingDirectory(const QSharedPointer<CustomWizardContext> &ctx, const QSharedPointer<CustomWizardParameters> &p) -> QString
{
  if (p->filesGeneratorScriptWorkingDirectory.isEmpty())
    return ctx->targetPath.toString();
  auto path = p->filesGeneratorScriptWorkingDirectory;
  CustomWizardContext::replaceFields(ctx->replacements, &path);
  return path;
}

auto CustomWizard::generateFiles(const QWizard *dialog, QString *errorMessage) const -> GeneratedFiles
{
  // Look for the Custom field page to find the path
  const CustomWizardPage *cwp = findWizardPage<CustomWizardPage>(dialog);
  QTC_ASSERT(cwp, return {});

  const auto ctx = context();
  ctx->path = ctx->targetPath = cwp->filePath();
  ctx->replacements = replacementMap(dialog);
  if (CustomWizardPrivate::verbose) {
    QString logText;
    QTextStream str(&logText);
    str << "CustomWizard::generateFiles: " << ctx->targetPath << '\n';
    const auto cend = context()->replacements.constEnd();
    for (auto it = context()->replacements.constBegin(); it != cend; ++it)
      str << "  '" << it.key() << "' -> '" << it.value() << "'\n";
    qWarning("%s", qPrintable(logText));
  }
  return generateWizardFiles(errorMessage);
}

auto CustomWizard::writeFiles(const GeneratedFiles &files, QString *errorMessage) const -> bool
{
  if (!BaseFileWizardFactory::writeFiles(files, errorMessage))
    return false;
  if (d->m_parameters->filesGeneratorScript.isEmpty())
    return true;
  // Prepare run of the custom script to generate. In the case of a
  // project wizard that is entirely created by a script,
  // the target project directory might not exist.
  // Known issue: By nature, the script does not honor
  // GeneratedFile::KeepExistingFileAttribute.
  const auto ctx = context();
  const auto scriptWorkingDir = scriptWorkingDirectory(ctx, d->m_parameters);
  const QDir scriptWorkingDirDir(scriptWorkingDir);
  if (!scriptWorkingDirDir.exists()) {
    if (CustomWizardPrivate::verbose)
      qDebug("Creating directory %s", qPrintable(scriptWorkingDir));
    if (!scriptWorkingDirDir.mkpath(scriptWorkingDir)) {
      *errorMessage = QString::fromLatin1("Unable to create the target directory \"%1\"").arg(scriptWorkingDir);
      return false;
    }
  }
  // Run the custom script to actually generate the files.
  if (!runCustomWizardGeneratorScript(scriptWorkingDir, d->m_parameters->filesGeneratorScript, d->m_parameters->filesGeneratorScriptArguments, ctx->replacements, errorMessage))
    return false;
  // Paranoia: Check on the files generated by the script:
  for (const GeneratedFile &generatedFile : files) {
    if (generatedFile.attributes() & GeneratedFile::CustomGeneratorAttribute)
      if (!QFileInfo(generatedFile.path()).isFile()) {
        *errorMessage = QString::fromLatin1("%1 failed to generate %2").arg(d->m_parameters->filesGeneratorScript.back(), generatedFile.path());
        return false;
      }
  }
  return true;
}

auto CustomWizard::generateWizardFiles(QString *errorMessage) const -> GeneratedFiles
{
  GeneratedFiles rc;
  const auto ctx = context();

  QTC_ASSERT(!ctx->targetPath.isEmpty(), return rc);

  if (CustomWizardPrivate::verbose)
    qDebug() << "CustomWizard::generateWizardFiles: in " << ctx->targetPath << ", using: " << ctx->replacements;

  // If generator script is non-empty, do a dry run to get it's files.
  if (!d->m_parameters->filesGeneratorScript.isEmpty()) {
    rc += dryRunCustomWizardGeneratorScript(scriptWorkingDirectory(ctx, d->m_parameters), d->m_parameters->filesGeneratorScript, d->m_parameters->filesGeneratorScriptArguments, ctx->replacements, errorMessage);
    if (rc.isEmpty())
      return rc;
  }
  // Add the template files specified by the <file> elements.
  for (const auto &file : qAsConst(d->m_parameters->files))
    if (!createFile(file, d->m_parameters->directory, ctx->targetPath.toString(), context()->replacements, &rc, errorMessage))
      return {};

  return rc;
}

// Create a replacement map of static base fields + wizard dialog fields
auto CustomWizard::replacementMap(const QWizard *w) const -> FieldReplacementMap
{
  return CustomWizardFieldPage::replacementMap(w, context(), d->m_parameters->fields);
}

auto CustomWizard::parameters() const -> CustomWizardParametersPtr
{
  return d->m_parameters;
}

auto CustomWizard::context() const -> CustomWizardContextPtr
{
  return d->m_context;
}

auto CustomWizard::createWizard(const CustomWizardParametersPtr &p) -> CustomWizard*
{
  const auto factory = findOrDefault(g_customWizardMetaFactories, [&p](ICustomWizardMetaFactory *factory) {
    return p->klass.isEmpty() ? (p->kind == factory->kind()) : (p->klass == factory->klass());
  });

  CustomWizard *rc = nullptr;
  if (factory)
    rc = factory->create();

  if (!rc) {
    qWarning("Unable to create custom wizard for class %s.", qPrintable(p->klass));
    return nullptr;
  }

  rc->setParameters(p);
  return rc;
}

/*!
    Reads \c share/qtcreator/templates/wizards and creates all custom wizards.

    As other plugins might register factories for derived
    classes, call it in extensionsInitialized().

    Scans the subdirectories of the template directory for directories
    containing valid configuration files and parse them into wizards.
*/

auto CustomWizard::createWizards() -> QList<IWizardFactory*>
{
  QString errorMessage;
  QString verboseLog;

  const auto templateDirName = ICore::resourcePath(templatePathC).toString();
  const auto userTemplateDirName = ICore::userResourcePath(templatePathC).toString();

  const QDir templateDir(templateDirName);
  if (CustomWizardPrivate::verbose)
    verboseLog += QString::fromLatin1("### CustomWizard: Checking \"%1\"\n").arg(templateDirName);
  if (!templateDir.exists()) {
    if (CustomWizardPrivate::verbose)
      qWarning("Custom project template path %s does not exist.", qPrintable(templateDir.absolutePath()));
    return {};
  }

  const QDir userTemplateDir(userTemplateDirName);
  if (CustomWizardPrivate::verbose)
    verboseLog += QString::fromLatin1("### CustomWizard: Checking \"%1\"\n").arg(userTemplateDirName);

  const auto filters = QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot;
  const auto sortflags = QDir::Name | QDir::IgnoreCase;
  QFileInfoList dirs;
  if (userTemplateDir.exists()) {
    if (CustomWizardPrivate::verbose)
      verboseLog += QString::fromLatin1("### CustomWizard: userTemplateDir \"%1\" found, adding\n").arg(userTemplateDirName);
    dirs += userTemplateDir.entryInfoList(filters, sortflags);
  }
  dirs += templateDir.entryInfoList(filters, sortflags);

  const QString configFile = QLatin1String(configFileC);
  // Check and parse config file in each directory.

  QList<CustomWizardParametersPtr> toCreate;

  while (enableLoadTemplateFiles() && !dirs.isEmpty()) {
    const auto dirFi = dirs.takeFirst();
    const QDir dir(dirFi.absoluteFilePath());
    if (CustomWizardPrivate::verbose)
      verboseLog += QString::fromLatin1("CustomWizard: Scanning %1\n").arg(dirFi.absoluteFilePath());
    if (dir.exists(configFile)) {
      CustomWizardParametersPtr parameters(new CustomWizardParameters);
      switch (parameters->parse(dir.absoluteFilePath(configFile), &errorMessage)) {
      case CustomWizardParameters::ParseOk:
        if (!contains(toCreate, [parameters](CustomWizardParametersPtr p) { return parameters->id == p->id; })) {
          parameters->directory = dir.absolutePath();
          toCreate.append(parameters);
        } else {
          verboseLog += QString::fromLatin1("Customwizard: Ignoring wizard in %1 due to duplicate Id %2.\n").arg(dir.absolutePath()).arg(parameters->id.toString());
        }
        break;
      case CustomWizardParameters::ParseDisabled:
        if (CustomWizardPrivate::verbose)
          qWarning("Ignoring disabled wizard %s...", qPrintable(dir.absolutePath()));
        break;
      case CustomWizardParameters::ParseFailed: qWarning("Failed to initialize custom project wizard in %s: %s", qPrintable(dir.absolutePath()), qPrintable(errorMessage));
        break;
      }
    } else {
      auto subDirs = dir.entryInfoList(filters, sortflags);
      if (!subDirs.isEmpty()) {
        // There is no QList::prepend(QList)...
        dirs.swap(subDirs);
        dirs.append(subDirs);
      } else if (CustomWizardPrivate::verbose) {
        verboseLog += QString::fromLatin1("CustomWizard: \"%1\" not found\n").arg(configFile);
      }
    }
  }

  QList<IWizardFactory*> rc;
  for (auto p : qAsConst(toCreate)) {
    if (const auto w = createWizard(p)) {
      rc.push_back(w);
    } else {
      qWarning("Custom wizard factory function failed for %s from %s.", qPrintable(p->id.toString()), qPrintable(p->directory));
    }
  }

  if (CustomWizardPrivate::verbose) {
    // Print to output pane for Windows.
    qWarning("%s", qPrintable(verboseLog));
    MessageManager::writeDisrupting(verboseLog);
  }
  return rc;
}

/*!
    \class ProjectExplorer::CustomProjectWizard
    \brief The CustomProjectWizard class provides a custom project wizard.

    Presents a CustomProjectWizardDialog (Project intro page and fields page)
    for wizards of type "project".
    Overwrites postGenerateFiles() to open the project files according to the
    file attributes. Also inserts \c '%ProjectName%' into the base
    replacement map once the intro page is left to have it available
    for QLineEdit-type fields' default text.
*/

CustomProjectWizard::CustomProjectWizard() = default;

/*!
    Can be reimplemented to create custom project wizards.

    initProjectWizardDialog() needs to be called.
*/

auto CustomProjectWizard::create(QWidget *parent, const WizardDialogParameters &parameters) const -> BaseFileWizard*
{
  const auto projectDialog = new BaseProjectWizardDialog(this, parent, parameters);
  initProjectWizardDialog(projectDialog, parameters.defaultPath(), projectDialog->extensionPages());
  return projectDialog;
}

auto CustomProjectWizard::initProjectWizardDialog(BaseProjectWizardDialog *w, const FilePath &defaultPath, const QList<QWizardPage*> &extensionPages) const -> void
{
  const auto pa = parameters();
  QTC_ASSERT(!pa.isNull(), return);

  const auto ctx = context();
  ctx->reset();

  if (!displayName().isEmpty())
    w->setWindowTitle(displayName());

  if (!pa->fields.isEmpty()) {
    if (parameters()->firstPageId >= 0)
      w->setPage(parameters()->firstPageId, new CustomWizardFieldPage(ctx, pa));
    else
      w->addPage(new CustomWizardFieldPage(ctx, pa));
  }
  for (const auto ep : extensionPages)
    w->addPage(ep);
  w->setFilePath(defaultPath);
  w->setProjectName(BaseProjectWizardDialog::uniqueProjectName(defaultPath));

  connect(w, &BaseProjectWizardDialog::projectParametersChanged, this, &CustomProjectWizard::projectParametersChanged);

  if (CustomWizardPrivate::verbose)
    qDebug() << "initProjectWizardDialog" << w << w->pageIds();
}

auto CustomProjectWizard::generateFiles(const QWizard *w, QString *errorMessage) const -> GeneratedFiles
{
  const auto *dialog = qobject_cast<const BaseProjectWizardDialog*>(w);
  QTC_ASSERT(dialog, return {});
  // Add project name as macro. Path is here under project directory
  const auto ctx = context();
  ctx->path = dialog->filePath();
  ctx->targetPath = ctx->path.pathAppended(dialog->projectName());
  auto fieldReplacementMap = replacementMap(dialog);
  fieldReplacementMap.insert(QLatin1String("ProjectName"), dialog->projectName());
  ctx->replacements = fieldReplacementMap;
  if (CustomWizardPrivate::verbose)
    qDebug() << "CustomProjectWizard::generateFiles" << dialog << ctx->targetPath << ctx->replacements;
  const GeneratedFiles generatedFiles = generateWizardFiles(errorMessage);
  return generatedFiles;
}

/*!
    Opens the projects and editors for the files that have
    the respective attributes set.
*/

auto CustomProjectWizard::postGenerateOpen(const GeneratedFiles &l, QString *errorMessage) -> bool
{
  // Post-Generate: Open the project and the editors as desired
  for (const GeneratedFile &file : l) {
    if (file.attributes() & GeneratedFile::OpenProjectAttribute) {
      const auto result = ProjectExplorerPlugin::openProject(file.filePath());
      if (!result) {
        if (errorMessage)
          *errorMessage = result.errorMessage();
        return false;
      }
    }
  }
  return BaseFileWizardFactory::postGenerateOpenEditors(l, errorMessage);
}

auto CustomProjectWizard::postGenerateFiles(const QWizard *, const GeneratedFiles &l, QString *errorMessage) const -> bool
{
  if (CustomWizardPrivate::verbose)
    qDebug() << "CustomProjectWizard::postGenerateFiles()";
  return CustomProjectWizard::postGenerateOpen(l, errorMessage);
}

auto CustomProjectWizard::projectParametersChanged(const QString &project, const QString &path) -> void
{
  // Make '%ProjectName%' available in base replacements.
  context()->baseReplacements.insert(QLatin1String("ProjectName"), project);

  emit projectLocationChanged(path + QLatin1Char('/') + project);
}

} // namespace ProjectExplorer
