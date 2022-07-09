// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakeparsernodes.hpp"

#include "qmakeproject.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakebuildconfiguration.hpp"

#include <android/androidconstants.h>
#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/icore.hpp>
#include <core/iversioncontrol.hpp>
#include <core/vcsmanager.hpp>
#include <cppeditor/cppeditorconstants.hpp>
#include <projectexplorer/editorconfiguration.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/taskhub.hpp>
#include <qtsupport/profilereader.hpp>
#include <texteditor/icodestylepreferences.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <utils/algorithm.hpp>
#include <utils/filesystemwatcher.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/stringutils.hpp>
#include <utils/temporarydirectory.hpp>
#include <utils/QtConcurrentTools>

#include <QLoggingCategory>
#include <QMessageBox>
#include <QTextCodec>

using namespace Core;
using namespace ProjectExplorer;
using namespace QmakeProjectManager;
using namespace QmakeProjectManager::Internal;
using namespace QMakeInternal;
using namespace Utils;

namespace QmakeProjectManager {

static Q_LOGGING_CATEGORY(qmakeParse, "qtc.qmake.parsing", QtWarningMsg);

auto qHash(Variable key, uint seed) -> Utils::QHashValueType { return ::qHash(static_cast<int>(key), seed); }
auto qHash(FileOrigin fo) -> Utils::QHashValueType { return ::qHash(int(fo)); }

namespace Internal {

Q_LOGGING_CATEGORY(qmakeNodesLog, "qtc.qmake.nodes", QtWarningMsg)

class QmakeEvalInput {
public:
  QString projectDir;
  FilePath projectFilePath;
  FilePath buildDirectory;
  FilePath sysroot;
  QtSupport::ProFileReader *readerExact;
  QtSupport::ProFileReader *readerCumulative;
  QMakeGlobals *qmakeGlobals;
  QMakeVfs *qmakeVfs;
  QSet<FilePath> parentFilePaths;
  bool includedInExcactParse;
};

class QmakePriFileEvalResult {
public:
  QSet<FilePath> folders;
  QSet<FilePath> recursiveEnumerateFiles;
  QMap<FileType, QSet<FilePath>> foundFilesExact;
  QMap<FileType, QSet<FilePath>> foundFilesCumulative;
};

class QmakeIncludedPriFile {
public:
  ProFile *proFile;
  Utils::FilePath name;
  QmakePriFileEvalResult result;
  QMap<Utils::FilePath, QmakeIncludedPriFile*> children;

  ~QmakeIncludedPriFile()
  {
    qDeleteAll(children);
  }
};

class QmakeEvalResult {
public:
  ~QmakeEvalResult() { qDeleteAll(directChildren); }

  enum EvalResultState {
    EvalAbort,
    EvalFail,
    EvalPartial,
    EvalOk
  };

  EvalResultState state;
  ProjectType projectType;

  QStringList subProjectsNotToDeploy;
  QSet<FilePath> exactSubdirs;
  QmakeIncludedPriFile includedFiles;
  TargetInformation targetInformation;
  InstallsList installsList;
  QHash<Variable, QStringList> newVarValues;
  QStringList errors;
  QSet<QString> directoriesWithWildcards;
  QList<QmakePriFile*> directChildren;
  QList<QPair<QmakePriFile*, QmakePriFileEvalResult>> priFiles;
  QList<QmakeProFile*> proFiles;
};

} // namespace Internal

QmakePriFile::QmakePriFile(QmakeBuildSystem *buildSystem, QmakeProFile *qmakeProFile, const FilePath &filePath) : m_filePath(filePath)
{
  finishInitialization(buildSystem, qmakeProFile);
}

QmakePriFile::QmakePriFile(const FilePath &filePath) : m_filePath(filePath) { }

auto QmakePriFile::finishInitialization(QmakeBuildSystem *buildSystem, QmakeProFile *qmakeProFile) -> void
{
  QTC_ASSERT(buildSystem, return);
  m_buildSystem = buildSystem;
  m_qmakeProFile = qmakeProFile;
}

auto QmakePriFile::filePath() const -> FilePath
{
  return m_filePath;
}

auto QmakePriFile::directoryPath() const -> FilePath
{
  return filePath().parentDir();
}

auto QmakePriFile::displayName() const -> QString
{
  return filePath().completeBaseName();
}

auto QmakePriFile::parent() const -> QmakePriFile*
{
  return m_parent;
}

auto QmakePriFile::project() const -> QmakeProject*
{
  return static_cast<QmakeProject*>(m_buildSystem->project());
}

auto QmakePriFile::children() const -> QVector<QmakePriFile*>
{
  return m_children;
}

auto QmakePriFile::findPriFile(const FilePath &fileName) -> QmakePriFile*
{
  if (fileName == filePath())
    return this;
  for (auto n : qAsConst(m_children)) {
    if (auto result = n->findPriFile(fileName))
      return result;
  }
  return nullptr;
}

auto QmakePriFile::findPriFile(const FilePath &fileName) const -> const QmakePriFile*
{
  if (fileName == filePath())
    return this;
  for (const QmakePriFile *n : qAsConst(m_children)) {
    if (auto result = n->findPriFile(fileName))
      return result;
  }
  return nullptr;
}

auto QmakePriFile::makeEmpty() -> void
{
  qDeleteAll(m_children);
  m_children.clear();
}

auto QmakePriFile::files(const FileType &type) const -> SourceFiles
{
  return m_files.value(type);
}

auto QmakePriFile::collectFiles(const FileType &type) const -> const QSet<FilePath>
{
  auto allFiles = transform(files(type), [](const SourceFile &sf) { return sf.first; });
  for (const QmakePriFile *const priFile : qAsConst(m_children)) {
    if (!dynamic_cast<const QmakeProFile*>(priFile))
      allFiles.unite(priFile->collectFiles(type));
  }
  return allFiles;
}

QmakePriFile::~QmakePriFile()
{
  watchFolders({});
  qDeleteAll(m_children);
}

auto QmakePriFile::scheduleUpdate() -> void
{
  QTC_ASSERT(m_buildSystem, return);
  QtSupport::ProFileCacheManager::instance()->discardFile(filePath().toString(), m_buildSystem->qmakeVfs());
  m_qmakeProFile->scheduleUpdate(QmakeProFile::ParseLater);
}

auto QmakePriFile::baseVPaths(QtSupport::ProFileReader *reader, const QString &projectDir, const QString &buildDir) -> QStringList
{
  QStringList result;
  if (!reader)
    return result;
  result += reader->absolutePathValues(QLatin1String("VPATH"), projectDir);
  result << projectDir; // QMAKE_ABSOLUTE_SOURCE_PATH
  result << buildDir;
  result.removeDuplicates();
  return result;
}

auto QmakePriFile::fullVPaths(const QStringList &baseVPaths, QtSupport::ProFileReader *reader, const QString &qmakeVariable, const QString &projectDir) -> QStringList
{
  QStringList vPaths;
  if (!reader)
    return vPaths;
  vPaths = reader->absolutePathValues(QLatin1String("VPATH_") + qmakeVariable, projectDir);
  vPaths += baseVPaths;
  vPaths.removeDuplicates();
  return vPaths;
}

auto QmakePriFile::recursiveEnumerate(const QString &folder) -> QSet<FilePath>
{
  QSet<FilePath> result;
  QDir dir(folder);
  dir.setFilter(dir.filter() | QDir::NoDotAndDotDot);
  foreach(const QFileInfo &file, dir.entryInfoList()) {
    if (file.isDir() && !file.isSymLink())
      result += recursiveEnumerate(file.absoluteFilePath());
    else if (!Core::EditorManager::isAutoSaveFile(file.fileName()))
      result += FilePath::fromFileInfo(file);
  }
  return result;
}

static auto fileListForVar(const QHash<QString, QVector<ProFileEvaluator::SourceFile>> &sourceFiles, const QString &varName) -> QStringList
{
  const auto &sources = sourceFiles[varName];
  QStringList result;
  result.reserve(sources.size());
  foreach(const ProFileEvaluator::SourceFile &sf, sources)
    result << sf.fileName;
  return result;
}

auto QmakePriFile::extractSources(QHash<int, QmakePriFileEvalResult*> proToResult, QmakePriFileEvalResult *fallback, QVector<ProFileEvaluator::SourceFile> sourceFiles, FileType type, bool cumulative) -> void
{
  foreach(const ProFileEvaluator::SourceFile &source, sourceFiles) {
    auto *result = proToResult.value(source.proFileId);
    if (!result)
      result = fallback;
    auto &foundFiles = cumulative ? result->foundFilesCumulative : result->foundFilesExact;
    foundFiles[type].insert(FilePath::fromString(source.fileName));
  }
}

auto QmakePriFile::extractInstalls(QHash<int, QmakePriFileEvalResult*> proToResult, QmakePriFileEvalResult *fallback, const InstallsList &installList) -> void
{
  for (const auto &item : installList.items) {
    for (const ProFileEvaluator::SourceFile &source : item.files) {
      auto *result = proToResult.value(source.proFileId);
      if (!result)
        result = fallback;
      result->folders.insert(FilePath::fromString(source.fileName));
    }
  }
}

auto QmakePriFile::processValues(QmakePriFileEvalResult &result) -> void
{
  // Remove non existing items and non folders
  auto it = result.folders.begin();
  while (it != result.folders.end()) {
    auto fi((*it).toFileInfo());
    if (fi.exists()) {
      if (fi.isDir()) {
        result.recursiveEnumerateFiles += recursiveEnumerate((*it).toString());
        // keep directories
        ++it;
      } else {
        // move files directly to recursiveEnumerateFiles
        result.recursiveEnumerateFiles += (*it);
        it = result.folders.erase(it);
      }
    } else {
      // do remove non exsting stuff
      it = result.folders.erase(it);
    }
  }

  for (auto i = 0; i < static_cast<int>(FileType::FileTypeSize); ++i) {
    auto type = static_cast<FileType>(i);
    for (const auto foundFiles : {&result.foundFilesExact[type], &result.foundFilesCumulative[type]}) {
      result.recursiveEnumerateFiles.subtract(*foundFiles);
      auto newFilePaths = filterFilesProVariables(type, *foundFiles);
      newFilePaths += filterFilesRecursiveEnumerata(type, result.recursiveEnumerateFiles);
      *foundFiles = newFilePaths;
    }
  }
}

auto QmakePriFile::update(const Internal::QmakePriFileEvalResult &result) -> void
{
  m_recursiveEnumerateFiles = result.recursiveEnumerateFiles;
  watchFolders(result.folders);

  for (auto i = 0; i < static_cast<int>(FileType::FileTypeSize); ++i) {
    const auto type = static_cast<FileType>(i);
    auto &files = m_files[type];
    files.clear();
    const auto exactFps = result.foundFilesExact.value(type);
    for (const auto &exactFp : exactFps)
      files << qMakePair(exactFp, FileOrigin::ExactParse);
    for (const auto &cumulativeFp : result.foundFilesCumulative.value(type)) {
      if (!exactFps.contains(cumulativeFp))
        files << qMakePair(cumulativeFp, FileOrigin::CumulativeParse);
    }
  }
}

auto QmakePriFile::watchFolders(const QSet<FilePath> &folders) -> void
{
  const auto folderStrings = Utils::transform(folders, &FilePath::toString);
  auto toUnwatch = m_watchedFolders;
  toUnwatch.subtract(folderStrings);

  auto toWatch = folderStrings;
  toWatch.subtract(m_watchedFolders);

  if (m_buildSystem) {
    // Check needed on early exit of QmakeProFile::applyEvaluate?
    m_buildSystem->unwatchFolders(Utils::toList(toUnwatch), this);
    m_buildSystem->watchFolders(Utils::toList(toWatch), this);
  }

  m_watchedFolders = folderStrings;
}

auto QmakePriFile::continuationIndent() const -> QString
{
  const EditorConfiguration *editorConf = project()->editorConfiguration();
  const auto &tabSettings = editorConf->useGlobalSettings() ? TextEditor::TextEditorSettings::codeStyle()->tabSettings() : editorConf->codeStyle()->tabSettings();
  if (tabSettings.m_continuationAlignBehavior == TextEditor::TabSettings::ContinuationAlignWithIndent && tabSettings.m_tabPolicy == TextEditor::TabSettings::TabsOnlyTabPolicy) {
    return QString("\t");
  }
  return QString(tabSettings.m_indentSize, ' ');
}

auto QmakePriFile::buildSystem() const -> QmakeBuildSystem*
{
  return m_buildSystem;
}

auto QmakePriFile::knowsFile(const FilePath &filePath) const -> bool
{
  return m_recursiveEnumerateFiles.contains(filePath);
}

auto QmakePriFile::folderChanged(const QString &changedFolder, const QSet<FilePath> &newFiles) -> bool
{
  qCDebug(qmakeParse()) << "QmakePriFile::folderChanged";

  auto addedFiles = newFiles;
  addedFiles.subtract(m_recursiveEnumerateFiles);

  auto removedFiles = m_recursiveEnumerateFiles;
  removedFiles.subtract(newFiles);

  foreach(const FilePath &file, removedFiles) {
    if (!file.isChildOf(FilePath::fromString(changedFolder)))
      removedFiles.remove(file);
  }

  if (addedFiles.isEmpty() && removedFiles.isEmpty())
    return false;

  m_recursiveEnumerateFiles = newFiles;

  // Apply the differences per file type
  for (auto i = 0; i < static_cast<int>(FileType::FileTypeSize); ++i) {
    auto type = static_cast<FileType>(i);
    auto add = filterFilesRecursiveEnumerata(type, addedFiles);
    auto remove = filterFilesRecursiveEnumerata(type, removedFiles);

    if (!add.isEmpty() || !remove.isEmpty()) {
      qCDebug(qmakeParse()) << "For type" << static_cast<int>(type) << "\n" << "added files" << add << "\n" << "removed files" << remove;
      auto &currentFiles = m_files[type];
      for (const auto &fp : add) {
        if (!contains(currentFiles, [&fp](const SourceFile &sf) { return sf.first == fp; }))
          currentFiles.insert(qMakePair(fp, FileOrigin::ExactParse));
      }
      for (const auto &fp : remove) {
        const auto it = std::find_if(currentFiles.begin(), currentFiles.end(), [&fp](const SourceFile &sf) {
          return sf.first == fp;
        });
        if (it != currentFiles.end())
          currentFiles.erase(it);
      }
    }
  }
  return true;
}

auto QmakePriFile::deploysFolder(const QString &folder) const -> bool
{
  auto f = folder;
  const QChar slash = QLatin1Char('/');
  if (!f.endsWith(slash))
    f.append(slash);

  foreach(const QString &wf, m_watchedFolders) {
    if (f.startsWith(wf) && (wf.endsWith(slash) || (wf.length() < f.length() && f.at(wf.length()) == slash)))
      return true;
  }
  return false;
}

auto QmakePriFile::subPriFilesExact() const -> QVector<QmakePriFile*>
{
  return Utils::filtered(m_children, &QmakePriFile::includedInExactParse);
}

auto QmakePriFile::proFile() const -> QmakeProFile*
{
  return m_qmakeProFile;
}

auto QmakePriFile::includedInExactParse() const -> bool
{
  return m_includedInExactParse;
}

auto QmakePriFile::setIncludedInExactParse(bool b) -> void
{
  m_includedInExactParse = b;
}

auto QmakePriFile::canAddSubProject(const FilePath &proFilePath) const -> bool
{
  return proFilePath.suffix() == "pro" || proFilePath.suffix() == "pri";
}

static auto simplifyProFilePath(const FilePath &proFilePath) -> FilePath
{
  // if proFilePath is like: _path_/projectName/projectName.pro
  // we simplify it to: _path_/projectName
  auto fi = proFilePath.toFileInfo(); // FIXME
  const auto parentPath = fi.absolutePath();
  QFileInfo parentFi(parentPath);
  if (parentFi.fileName() == fi.completeBaseName())
    return FilePath::fromString(parentPath);
  return proFilePath;
}

auto QmakePriFile::addSubProject(const FilePath &proFile) -> bool
{
  FilePaths uniqueProFilePaths;
  if (!m_recursiveEnumerateFiles.contains(proFile))
    uniqueProFilePaths.append(simplifyProFilePath(proFile));

  FilePaths failedFiles;
  changeFiles(QLatin1String(Constants::PROFILE_MIMETYPE), uniqueProFilePaths, &failedFiles, AddToProFile);

  return failedFiles.isEmpty();
}

auto QmakePriFile::removeSubProjects(const FilePath &proFilePath) -> bool
{
  FilePaths failedOriginalFiles;
  changeFiles(QLatin1String(Constants::PROFILE_MIMETYPE), {proFilePath}, &failedOriginalFiles, RemoveFromProFile);

  auto simplifiedProFiles = Utils::transform(failedOriginalFiles, &simplifyProFilePath);

  FilePaths failedSimplifiedFiles;
  changeFiles(QLatin1String(Constants::PROFILE_MIMETYPE), simplifiedProFiles, &failedSimplifiedFiles, RemoveFromProFile);

  return failedSimplifiedFiles.isEmpty();
}

auto QmakePriFile::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  // If a file is already referenced in the .pro file then we don't add them.
  // That ignores scopes and which variable was used to reference the file
  // So it's obviously a bit limited, but in those cases you need to edit the
  // project files manually anyway.

  using TypeFileMap = QMap<QString, FilePaths>;
  // Split into lists by file type and bulk-add them.
  TypeFileMap typeFileMap;
  for (const auto &file : filePaths) {
    const auto mt = Utils::mimeTypeForFile(file);
    typeFileMap[mt.name()] << file;
  }

  FilePaths failedFiles;
  foreach(const QString &type, typeFileMap.keys()) {
    const auto typeFiles = typeFileMap.value(type);
    FilePaths qrcFiles; // the list of qrc files referenced from ui files
    if (type == QLatin1String(ProjectExplorer::Constants::RESOURCE_MIMETYPE)) {
      for (const auto &formFile : typeFiles) {
        const auto resourceFiles = formResources(formFile);
        for (const auto &resourceFile : resourceFiles)
          if (!qrcFiles.contains(resourceFile))
            qrcFiles.append(resourceFile);
      }
    }

    FilePaths uniqueQrcFiles;
    for (const auto &file : qAsConst(qrcFiles)) {
      if (!m_recursiveEnumerateFiles.contains(file))
        uniqueQrcFiles.append(file);
    }

    FilePaths uniqueFilePaths;
    for (const auto &file : typeFiles) {
      if (!m_recursiveEnumerateFiles.contains(file))
        uniqueFilePaths.append(file);
    }
    FilePath::sort(uniqueFilePaths);

    changeFiles(type, uniqueFilePaths, &failedFiles, AddToProFile);
    if (notAdded)
      *notAdded += failedFiles;
    changeFiles(QLatin1String(ProjectExplorer::Constants::RESOURCE_MIMETYPE), uniqueQrcFiles, &failedFiles, AddToProFile);
    if (notAdded)
      *notAdded += failedFiles;
  }
  return failedFiles.isEmpty();
}

auto QmakePriFile::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> bool
{
  FilePaths failedFiles;
  using TypeFileMap = QMap<QString, FilePaths>;
  // Split into lists by file type and bulk-add them.
  TypeFileMap typeFileMap;
  for (const auto &file : filePaths) {
    const auto mt = Utils::mimeTypeForFile(file);
    typeFileMap[mt.name()] << file;
  }
  foreach(const QString &type, typeFileMap.keys()) {
    const auto typeFiles = typeFileMap.value(type);
    changeFiles(type, typeFiles, &failedFiles, RemoveFromProFile);
    if (notRemoved)
      *notRemoved = failedFiles;
  }
  return failedFiles.isEmpty();
}

auto QmakePriFile::deleteFiles(const FilePaths &filePaths) -> bool
{
  removeFiles(filePaths);
  return true;
}

auto QmakePriFile::canRenameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (newFilePath.isEmpty())
    return false;

  auto changeProFileOptional = deploysFolder(oldFilePath.absolutePath().toString());
  if (changeProFileOptional)
    return true;

  return renameFile(oldFilePath, newFilePath, Change::TestOnly);
}

auto QmakePriFile::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (newFilePath.isEmpty())
    return false;

  auto changeProFileOptional = deploysFolder(oldFilePath.absolutePath().toString());
  if (renameFile(oldFilePath, newFilePath, Change::Save))
    return true;
  return changeProFileOptional;
}

auto QmakePriFile::addDependencies(const QStringList &dependencies) -> bool
{
  if (dependencies.isEmpty())
    return true;
  if (!prepareForChange())
    return false;

  auto qtDependencies = filtered(dependencies, [](const QString &dep) {
    return dep.length() > 3 && dep.startsWith("Qt.");
  });
  qtDependencies = transform(qtDependencies, [](const QString &dep) {
    return dep.mid(3);
  });
  qtDependencies.removeOne("core");
  if (qtDependencies.isEmpty())
    return true;

  const QPair<ProFile*, QStringList> pair = readProFile();
  ProFile *const includeFile = pair.first;
  if (!includeFile)
    return false;
  QStringList lines = pair.second;

  const auto indent = continuationIndent();
  const ProWriter::PutFlags appendFlags(ProWriter::AppendValues | ProWriter::AppendOperator);
  if (!proFile()->variableValue(Variable::Config).contains("qt")) {
    if (lines.removeAll("CONFIG -= qt") == 0) {
      ProWriter::putVarValues(includeFile, &lines, {"qt"}, "CONFIG", appendFlags, QString(), indent);
    }
  }

  const auto currentQtDependencies = proFile()->variableValue(Variable::Qt);
  qtDependencies = filtered(qtDependencies, [currentQtDependencies](const QString &dep) {
    return !currentQtDependencies.contains(dep);
  });
  if (!qtDependencies.isEmpty()) {
    ProWriter::putVarValues(includeFile, &lines, qtDependencies, "QT", appendFlags, QString(), indent);
  }

  save(lines);
  includeFile->deref();
  return true;
}

auto QmakePriFile::saveModifiedEditors() -> bool
{
  auto document = Core::DocumentModel::documentForFilePath(filePath());
  if (!document || !document->isModified())
    return true;

  if (!Core::DocumentManager::saveDocument(document))
    return false;

  // force instant reload of ourselves
  QtSupport::ProFileCacheManager::instance()->discardFile(filePath().toString(), m_buildSystem->qmakeVfs());

  m_buildSystem->notifyChanged(filePath());
  return true;
}

auto QmakePriFile::formResources(const FilePath &formFile) const -> FilePaths
{
  QStringList resourceFiles;
  QFile file(formFile.toString());
  if (!file.open(QIODevice::ReadOnly))
    return {};

  QXmlStreamReader reader(&file);

  QFileInfo fi(formFile.toString());
  auto formDir = fi.absoluteDir();
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.name() == QLatin1String("iconset")) {
        const auto attributes = reader.attributes();
        if (attributes.hasAttribute(QLatin1String("resource")))
          resourceFiles.append(QDir::cleanPath(formDir.absoluteFilePath(attributes.value(QLatin1String("resource")).toString())));
      } else if (reader.name() == QLatin1String("include")) {
        const auto attributes = reader.attributes();
        if (attributes.hasAttribute(QLatin1String("location")))
          resourceFiles.append(QDir::cleanPath(formDir.absoluteFilePath(attributes.value(QLatin1String("location")).toString())));

      }
    }
  }

  if (reader.hasError())
    qWarning() << "Could not read form file:" << formFile;

  return Utils::transform(resourceFiles, &FilePath::fromString);
}

auto QmakePriFile::ensureWriteableProFile(const QString &file) -> bool
{
  // Ensure that the file is not read only
  QFileInfo fi(file);
  if (!fi.isWritable()) {
    // Try via vcs manager
    auto versionControl = Core::VcsManager::findVersionControlForDirectory(FilePath::fromString(fi.absolutePath()));
    if (!versionControl || !versionControl->vcsOpen(FilePath::fromString(file))) {
      auto makeWritable = QFile::setPermissions(file, fi.permissions() | QFile::WriteUser);
      if (!makeWritable) {
        QMessageBox::warning(Core::ICore::dialogParent(), QCoreApplication::translate("QmakePriFile", "Failed"), QCoreApplication::translate("QmakePriFile", "Could not write project file %1.").arg(file));
        return false;
      }
    }
  }
  return true;
}

auto QmakePriFile::readProFile() -> QPair<ProFile*, QStringList>
{
  QStringList lines;
  ProFile *includeFile = nullptr;
  {
    QString contents;
    {
      QString errorMsg;
      if (TextFileFormat::readFile(filePath(), Core::EditorManager::defaultTextCodec(), &contents, &m_textFormat, &errorMsg) != TextFileFormat::ReadSuccess) {
        QmakeBuildSystem::proFileParseError(errorMsg, filePath());
        return qMakePair(includeFile, lines);
      }
      lines = contents.split('\n');
    }

    QMakeVfs vfs;
    QtSupport::ProMessageHandler handler;
    QMakeParser parser(nullptr, &vfs, &handler);
    includeFile = parser.parsedProBlock(Utils::make_stringview(contents), 0, filePath().toString(), 1);
  }
  return qMakePair(includeFile, lines);
}

auto QmakePriFile::prepareForChange() -> bool
{
  return saveModifiedEditors() && ensureWriteableProFile(filePath().toString());
}

auto QmakePriFile::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath, Change mode) -> bool
{
  if (!prepareForChange())
    return false;

  QPair<ProFile*, QStringList> pair = readProFile();
  ProFile *includeFile = pair.first;
  QStringList lines = pair.second;

  if (!includeFile)
    return false;

  auto priFileDir = QDir(m_qmakeProFile->directoryPath().toString());
  ProWriter::VarLocations removedLocations;
  const QStringList notChanged = ProWriter::removeFiles(includeFile, &lines, priFileDir, {oldFilePath.toString()}, varNamesForRemoving(), &removedLocations);

  includeFile->deref();
  if (!notChanged.isEmpty())
    return false;
  QTC_ASSERT(!removedLocations.isEmpty(), return false);

  int endLine = lines.count();
  reverseForeach(removedLocations, [this, &newFilePath, &lines, &endLine](const ProWriter::VarLocation &loc) {
    QStringList currentLines = lines.mid(loc.second, endLine - loc.second);
    const QString currentContents = currentLines.join('\n');

    // Reparse necessary due to changed contents.
    QMakeParser parser(nullptr, nullptr, nullptr);
    ProFile *const proFile = parser.parsedProBlock(Utils::make_stringview(currentContents), 0, filePath().toString(), 1, QMakeParser::FullGrammar);
    QTC_ASSERT(proFile, return); // The file should still be valid after what we did.

    ProWriter::addFiles(proFile, &currentLines, {newFilePath.toString()}, loc.first, continuationIndent());
    lines = lines.mid(0, loc.second) + currentLines + lines.mid(endLine);
    endLine = loc.second;
    proFile->deref();
  });

  if (mode == Change::Save)
    save(lines);
  return true;
}

auto QmakePriFile::changeFiles(const QString &mimeType, const FilePaths &filePaths, FilePaths *notChanged, ChangeType change, Change mode) -> void
{
  if (filePaths.isEmpty())
    return;

  *notChanged = filePaths;

  // Check for modified editors
  if (!prepareForChange())
    return;

  QPair<ProFile*, QStringList> pair = readProFile();
  ProFile *includeFile = pair.first;
  QStringList lines = pair.second;

  if (!includeFile)
    return;

  qCDebug(qmakeNodesLog) << Q_FUNC_INFO << "mime type:" << mimeType << "file paths:" << filePaths << "change type:" << int(change) << "mode:" << int(mode);
  if (change == AddToProFile) {
    // Use the first variable for adding.
    ProWriter::addFiles(includeFile, &lines, Utils::transform(filePaths, &FilePath::toString), varNameForAdding(mimeType), continuationIndent());
    notChanged->clear();
  } else {
    // RemoveFromProFile
    auto priFileDir = QDir(m_qmakeProFile->directoryPath().toString());
    *notChanged = Utils::transform(ProWriter::removeFiles(includeFile, &lines, priFileDir, Utils::transform(filePaths, &FilePath::toString), varNamesForRemoving()), &FilePath::fromString);
  }

  // save file
  if (mode == Change::Save)
    save(lines);
  includeFile->deref();
}

auto QmakePriFile::addChild(QmakePriFile *pf) -> void
{
  QTC_ASSERT(!m_children.contains(pf), return);
  QTC_ASSERT(!pf->parent(), return);
  m_children.append(pf);
  pf->setParent(this);
}

auto QmakePriFile::setParent(QmakePriFile *p) -> void
{
  QTC_ASSERT(!m_parent, return);
  m_parent = p;
}

auto QmakePriFile::setProVariable(const QString &var, const QStringList &values, const QString &scope, int flags) -> bool
{
  if (!prepareForChange())
    return false;

  QPair<ProFile*, QStringList> pair = readProFile();
  ProFile *includeFile = pair.first;
  QStringList lines = pair.second;

  if (!includeFile)
    return false;

  ProWriter::putVarValues(includeFile, &lines, values, var, ProWriter::PutFlags(flags), scope, continuationIndent());

  save(lines);
  includeFile->deref();
  return true;
}

auto QmakePriFile::save(const QStringList &lines) -> void
{
  {
    QTC_ASSERT(m_textFormat.codec, return);
    FileChangeBlocker changeGuard(filePath());
    QString errorMsg;
    if (!m_textFormat.writeFile(filePath(), lines.join('\n'), &errorMsg)) {
      QMessageBox::critical(Core::ICore::dialogParent(), QCoreApplication::translate("QmakePriFile", "File Error"), errorMsg);
    }
  }

  // This is a hack.
  // We are saving twice in a very short timeframe, once the editor and once the ProFile.
  // So the modification time might not change between those two saves.
  // We manually tell each editor to reload it's file.
  // (The .pro files are notified by the file system watcher.)
  QStringList errorStrings;
  auto document = Core::DocumentModel::documentForFilePath(filePath());
  if (document) {
    QString errorString;
    if (!document->reload(&errorString, Core::IDocument::FlagReload, Core::IDocument::TypeContents))
      errorStrings << errorString;
  }
  if (!errorStrings.isEmpty())
    QMessageBox::warning(Core::ICore::dialogParent(), QCoreApplication::translate("QmakePriFile", "File Error"), errorStrings.join(QLatin1Char('\n')));
}

auto QmakePriFile::varNames(FileType type, QtSupport::ProFileReader *readerExact) -> QStringList
{
  QStringList vars;
  switch (type) {
  case FileType::Header:
    vars << "HEADERS" << "OBJECTIVE_HEADERS" << "PRECOMPILED_HEADER";
    break;
  case FileType::Source: {
    vars << QLatin1String("SOURCES");
    auto listOfExtraCompilers = readerExact->values(QLatin1String("QMAKE_EXTRA_COMPILERS"));
    foreach(const QString &var, listOfExtraCompilers) {
      auto inputs = readerExact->values(var + QLatin1String(".input"));
      foreach(const QString &input, inputs)
      // FORMS, RESOURCES, and STATECHARTS are handled below, HEADERS and SOURCES above
        if (input != "FORMS" && input != "STATECHARTS" && input != "RESOURCES" && input != "SOURCES" && input != "HEADERS" && input != "OBJECTIVE_HEADERS" && input != "PRECOMPILED_HEADER") {
          vars << input;
        }
    }
    break;
  }
  case FileType::Resource:
    vars << QLatin1String("RESOURCES");
    break;
  case FileType::Form:
    vars << QLatin1String("FORMS");
    break;
  case FileType::StateChart:
    vars << QLatin1String("STATECHARTS");
    break;
  case FileType::Project:
    vars << QLatin1String("SUBDIRS");
    break;
  case FileType::QML:
    vars << QLatin1String("OTHER_FILES");
    vars << QLatin1String("DISTFILES");
    break;
  default:
    vars << "DISTFILES" << "ICON" << "OTHER_FILES" << "QMAKE_INFO_PLIST" << "TRANSLATIONS";
    break;
  }
  return vars;
}

//!
//! \brief QmakePriFile::varNames
//! \param mimeType
//! \return the qmake variable name for the mime type
//! Note: Only used for adding.
//!
auto QmakePriFile::varNameForAdding(const QString &mimeType) -> QString
{
  if (mimeType == QLatin1String(ProjectExplorer::Constants::CPP_HEADER_MIMETYPE) || mimeType == QLatin1String(ProjectExplorer::Constants::C_HEADER_MIMETYPE)) {
    return QLatin1String("HEADERS");
  }

  if (mimeType == QLatin1String(ProjectExplorer::Constants::CPP_SOURCE_MIMETYPE) || mimeType == QLatin1String(CppEditor::Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE) || mimeType == QLatin1String(ProjectExplorer::Constants::C_SOURCE_MIMETYPE)) {
    return QLatin1String("SOURCES");
  }

  if (mimeType == QLatin1String(ProjectExplorer::Constants::RESOURCE_MIMETYPE))
    return QLatin1String("RESOURCES");

  if (mimeType == QLatin1String(ProjectExplorer::Constants::FORM_MIMETYPE))
    return QLatin1String("FORMS");

  if (mimeType == QLatin1String(ProjectExplorer::Constants::QML_MIMETYPE) || mimeType == QLatin1String(ProjectExplorer::Constants::QMLUI_MIMETYPE)) {
    return QLatin1String("DISTFILES");
  }

  if (mimeType == QLatin1String(ProjectExplorer::Constants::SCXML_MIMETYPE))
    return QLatin1String("STATECHARTS");

  if (mimeType == QLatin1String(Constants::PROFILE_MIMETYPE))
    return QLatin1String("SUBDIRS");

  return QLatin1String("DISTFILES");
}

//!
//! \brief QmakePriFile::varNamesForRemoving
//! \return all qmake variables which are displayed in the project tree
//! Note: Only used for removing.
//!
auto QmakePriFile::varNamesForRemoving() -> QStringList
{
  QStringList vars;
  vars << QLatin1String("HEADERS");
  vars << QLatin1String("OBJECTIVE_HEADERS");
  vars << QLatin1String("PRECOMPILED_HEADER");
  vars << QLatin1String("SOURCES");
  vars << QLatin1String("OBJECTIVE_SOURCES");
  vars << QLatin1String("RESOURCES");
  vars << QLatin1String("FORMS");
  vars << QLatin1String("OTHER_FILES");
  vars << QLatin1String("SUBDIRS");
  vars << QLatin1String("DISTFILES");
  vars << QLatin1String("ICON");
  vars << QLatin1String("QMAKE_INFO_PLIST");
  vars << QLatin1String("STATECHARTS");
  return vars;
}

auto QmakePriFile::filterFilesProVariables(FileType fileType, const QSet<FilePath> &files) -> QSet<FilePath>
{
  if (fileType != FileType::QML && fileType != FileType::Unknown)
    return files;
  QSet<FilePath> result;
  if (fileType == FileType::QML) {
    foreach(const FilePath &file, files) if (file.toString().endsWith(QLatin1String(".qml")))
      result << file;
  } else {
    foreach(const FilePath &file, files) if (!file.toString().endsWith(QLatin1String(".qml")))
      result << file;
  }
  return result;
}

auto QmakePriFile::filterFilesRecursiveEnumerata(FileType fileType, const QSet<FilePath> &files) -> QSet<FilePath>
{
  QSet<FilePath> result;
  if (fileType != FileType::QML && fileType != FileType::Unknown)
    return result;
  if (fileType == FileType::QML) {
    foreach(const FilePath &file, files) if (file.toString().endsWith(QLatin1String(".qml")))
      result << file;
  } else {
    foreach(const FilePath &file, files) if (!file.toString().endsWith(QLatin1String(".qml")))
      result << file;
  }
  return result;
}

} // namespace QmakeProjectManager

static auto proFileTemplateTypeToProjectType(ProFileEvaluator::TemplateType type) -> ProjectType
{
  switch (type) {
  case ProFileEvaluator::TT_Unknown:
  case ProFileEvaluator::TT_Application:
    return ProjectType::ApplicationTemplate;
  case ProFileEvaluator::TT_StaticLibrary:
    return ProjectType::StaticLibraryTemplate;
  case ProFileEvaluator::TT_SharedLibrary:
    return ProjectType::SharedLibraryTemplate;
  case ProFileEvaluator::TT_Script:
    return ProjectType::ScriptTemplate;
  case ProFileEvaluator::TT_Aux:
    return ProjectType::AuxTemplate;
  case ProFileEvaluator::TT_Subdirs:
    return ProjectType::SubDirsTemplate;
  default:
    return ProjectType::Invalid;
  }
}

auto QmakeProFile::findProFile(const FilePath &fileName) -> QmakeProFile*
{
  return static_cast<QmakeProFile*>(findPriFile(fileName));
}

auto QmakeProFile::findProFile(const FilePath &fileName) const -> const QmakeProFile*
{
  return static_cast<const QmakeProFile*>(findPriFile(fileName));
}

auto QmakeProFile::cxxDefines() const -> QByteArray
{
  QByteArray result;
  foreach(const QString &def, variableValue(Variable::Defines)) {
    // 'def' is shell input, so interpret it.
    auto error = ProcessArgs::SplitOk;
    const auto args = ProcessArgs::splitArgs(def, HostOsInfo::hostOs(), false, &error);
    if (error != ProcessArgs::SplitOk || args.size() == 0)
      continue;

    result += "#define ";
    const auto defInterpreted = args.first();
    const int index = defInterpreted.indexOf(QLatin1Char('='));
    if (index == -1) {
      result += defInterpreted.toLatin1();
      result += " 1\n";
    } else {
      const auto name = defInterpreted.left(index);
      const auto value = defInterpreted.mid(index + 1);
      result += name.toLatin1();
      result += ' ';
      result += value.toLocal8Bit();
      result += '\n';
    }
  }
  return result;
}

/*!
  \class QmakeProFile
  Implements abstract ProjectNode class
  */
QmakeProFile::QmakeProFile(QmakeBuildSystem *buildSystem, const FilePath &filePath) : QmakePriFile(buildSystem, this, filePath) {}

QmakeProFile::QmakeProFile(const FilePath &filePath) : QmakePriFile(filePath) { }

QmakeProFile::~QmakeProFile()
{
  qDeleteAll(m_extraCompilers);
  cleanupFutureWatcher();
  cleanupProFileReaders();
}

auto QmakeProFile::cleanupFutureWatcher() -> void
{
  if (!m_parseFutureWatcher)
    return;

  m_parseFutureWatcher->disconnect();
  m_parseFutureWatcher->cancel();
  m_parseFutureWatcher->waitForFinished();
  m_parseFutureWatcher->deleteLater();
  m_parseFutureWatcher = nullptr;
  m_buildSystem->decrementPendingEvaluateFutures();
}

auto QmakeProFile::setupFutureWatcher() -> void
{
  QTC_ASSERT(!m_parseFutureWatcher, return);

  m_parseFutureWatcher = new QFutureWatcher<Internal::QmakeEvalResultPtr>;
  QObject::connect(m_parseFutureWatcher, &QFutureWatcherBase::finished, [this]() {
    applyEvaluate(m_parseFutureWatcher->result());
    cleanupFutureWatcher();
  });
  m_buildSystem->incrementPendingEvaluateFutures();
}

auto QmakeProFile::isParent(QmakeProFile *node) -> bool
{
  while ((node = dynamic_cast<QmakeProFile*>(node->parent()))) {
    if (node == this)
      return true;
  }
  return false;
}

auto QmakeProFile::displayName() const -> QString
{
  if (!m_displayName.isEmpty())
    return m_displayName;
  return QmakePriFile::displayName();
}

auto QmakeProFile::allProFiles() -> QList<QmakeProFile*>
{
  QList<QmakeProFile*> result = {this};
  for (auto c : qAsConst(m_children)) {
    auto proC = dynamic_cast<QmakeProFile*>(c);
    if (proC)
      result.append(proC->allProFiles());
  }
  return result;
}

auto QmakeProFile::projectType() const -> ProjectType
{
  return m_projectType;
}

auto QmakeProFile::variableValue(const Variable var) const -> QStringList
{
  return m_varValues.value(var);
}

auto QmakeProFile::singleVariableValue(const Variable var) const -> QString
{
  const auto &values = variableValue(var);
  return values.isEmpty() ? QString() : values.first();
}

auto QmakeProFile::setParseInProgressRecursive(bool b) -> void
{
  setParseInProgress(b);
  foreach(QmakePriFile *c, children()) {
    if (auto node = dynamic_cast<QmakeProFile*>(c))
      node->setParseInProgressRecursive(b);
  }
}

auto QmakeProFile::setParseInProgress(bool b) -> void
{
  m_parseInProgress = b;
}

// Do note the absence of signal emission, always set validParse
// before setParseInProgress, as that will emit the signals
auto QmakeProFile::setValidParseRecursive(bool b) -> void
{
  m_validParse = b;
  foreach(QmakePriFile *c, children()) {
    if (auto *node = dynamic_cast<QmakeProFile*>(c))
      node->setValidParseRecursive(b);
  }
}

auto QmakeProFile::validParse() const -> bool
{
  return m_validParse;
}

auto QmakeProFile::parseInProgress() const -> bool
{
  return m_parseInProgress;
}

auto QmakeProFile::scheduleUpdate(QmakeProFile::AsyncUpdateDelay delay) -> void
{
  setParseInProgressRecursive(true);
  m_buildSystem->scheduleAsyncUpdateFile(this, delay);
}

auto QmakeProFile::asyncUpdate() -> void
{
  cleanupFutureWatcher();
  setupFutureWatcher();
  setupReader();
  if (!includedInExactParse())
    m_readerExact->setExact(false);
  auto input = evalInput();
  auto future = Utils::runAsync(ProjectExplorerPlugin::sharedThreadPool(), QThread::LowestPriority, &QmakeProFile::asyncEvaluate, this, input);
  m_parseFutureWatcher->setFuture(future);
}

auto QmakeProFile::isFileFromWildcard(const QString &filePath) const -> bool
{
  const QFileInfo fileInfo(filePath);
  const auto directoryIterator = m_wildcardDirectoryContents.constFind(fileInfo.path());
  return (directoryIterator != m_wildcardDirectoryContents.end() && directoryIterator.value().contains(fileInfo.fileName()));
}

auto QmakeProFile::evalInput() const -> QmakeEvalInput
{
  QmakeEvalInput input;
  input.projectDir = directoryPath().toString();
  input.projectFilePath = filePath();
  input.buildDirectory = m_buildSystem->buildDir(m_filePath);
  input.sysroot = FilePath::fromString(m_buildSystem->qmakeSysroot());
  input.readerExact = m_readerExact;
  input.readerCumulative = m_readerCumulative;
  input.qmakeGlobals = m_buildSystem->qmakeGlobals();
  input.qmakeVfs = m_buildSystem->qmakeVfs();
  input.includedInExcactParse = includedInExactParse();
  for (const QmakePriFile *pri = this; pri; pri = pri->parent())
    input.parentFilePaths.insert(pri->filePath());
  return input;
}

auto QmakeProFile::setupReader() -> void
{
  Q_ASSERT(!m_readerExact);
  Q_ASSERT(!m_readerCumulative);

  m_readerExact = m_buildSystem->createProFileReader(this);

  m_readerCumulative = m_buildSystem->createProFileReader(this);
  m_readerCumulative->setCumulative(true);
}

static auto evaluateOne(const QmakeEvalInput &input, ProFile *pro, QtSupport::ProFileReader *reader, bool cumulative, QtSupport::ProFileReader **buildPassReader) -> bool
{
  if (!reader->accept(pro, QMakeEvaluator::LoadAll))
    return false;

  auto builds = reader->values(QLatin1String("BUILDS"));
  if (builds.isEmpty()) {
    *buildPassReader = reader;
  } else {
    auto build = builds.first();
    QHash<QString, QStringList> basevars;
    auto basecfgs = reader->values(build + QLatin1String(".CONFIG"));
    basecfgs += build;
    basecfgs += QLatin1String("build_pass");
    basecfgs += "qtc_run";
    basevars[QLatin1String("BUILD_PASS")] = QStringList(build);
    auto buildname = reader->values(build + QLatin1String(".name"));
    basevars[QLatin1String("BUILD_NAME")] = (buildname.isEmpty() ? QStringList(build) : buildname);

    // We don't increase/decrease m_qmakeGlobalsRefCnt here, because the outer profilereaders keep m_qmakeGlobals alive anyway
    auto bpReader = new QtSupport::ProFileReader(input.qmakeGlobals, input.qmakeVfs); // needs to access m_qmakeGlobals, m_qmakeVfs

    // FIXME: Currently intentional.
    // Core parts of the ProParser hard-assert on non-local items.
    bpReader->setOutputDir(input.buildDirectory.path());
    bpReader->setCumulative(cumulative);
    bpReader->setExtraVars(basevars);
    bpReader->setExtraConfigs(basecfgs);

    if (bpReader->accept(pro, QMakeEvaluator::LoadAll))
      *buildPassReader = bpReader;
    else
      delete bpReader;
  }

  return true;
}

auto QmakeProFile::evaluate(const QmakeEvalInput &input) -> QmakeEvalResultPtr
{
  QmakeEvalResultPtr result(new QmakeEvalResult);
  QtSupport::ProFileReader *exactBuildPassReader = nullptr;
  QtSupport::ProFileReader *cumulativeBuildPassReader = nullptr;
  ProFile *pro;
  if ((pro = input.readerExact->parsedProFile(input.projectFilePath.toString()))) {
    auto exactOk = evaluateOne(input, pro, input.readerExact, false, &exactBuildPassReader);
    auto cumulOk = evaluateOne(input, pro, input.readerCumulative, true, &cumulativeBuildPassReader);
    pro->deref();
    result->state = exactOk ? QmakeEvalResult::EvalOk : cumulOk ? QmakeEvalResult::EvalPartial : QmakeEvalResult::EvalFail;
  } else {
    result->state = QmakeEvalResult::EvalFail;
  }

  if (result->state == QmakeEvalResult::EvalFail)
    return result;

  result->includedFiles.proFile = pro;
  result->includedFiles.name = input.projectFilePath;

  QHash<int, QmakePriFileEvalResult*> proToResult;

  result->projectType = proFileTemplateTypeToProjectType((result->state == QmakeEvalResult::EvalOk ? input.readerExact : input.readerCumulative)->templateType());
  if (result->state == QmakeEvalResult::EvalOk) {
    if (result->projectType == ProjectType::SubDirsTemplate) {
      QStringList errors;
      auto subDirs = subDirsPaths(input.readerExact, input.projectDir, &result->subProjectsNotToDeploy, &errors);
      result->errors.append(errors);

      foreach(const Utils::FilePath &subDirName, subDirs) {
        auto subDir = new QmakeIncludedPriFile;
        subDir->proFile = nullptr;
        subDir->name = subDirName;
        result->includedFiles.children.insert(subDirName, subDir);
      }

      result->exactSubdirs = Utils::toSet(subDirs);
    }

    // Convert ProFileReader::includeFiles to IncludedPriFile structure
    auto includeFiles = input.readerExact->includeFiles();
    QList<QmakeIncludedPriFile*> toBuild = {&result->includedFiles};
    while (!toBuild.isEmpty()) {
      auto current = toBuild.takeFirst();
      if (!current->proFile)
        continue; // Don't attempt to map subdirs here
      auto children = includeFiles.value(current->proFile);
      foreach(ProFile *child, children) {
        const auto childName = Utils::FilePath::fromString(child->fileName());
        auto it = current->children.find(childName);
        if (it == current->children.end()) {
          auto childTree = new QmakeIncludedPriFile;
          childTree->proFile = child;
          childTree->name = childName;
          current->children.insert(childName, childTree);
          proToResult[child->id()] = &childTree->result;
        }
      }
      toBuild.append(current->children.values());
    }
  }

  if (result->projectType == ProjectType::SubDirsTemplate) {
    auto subDirs = subDirsPaths(input.readerCumulative, input.projectDir, nullptr, nullptr);
    foreach(const Utils::FilePath &subDirName, subDirs) {
      auto it = result->includedFiles.children.find(subDirName);
      if (it == result->includedFiles.children.end()) {
        auto subDir = new QmakeIncludedPriFile;
        subDir->proFile = nullptr;
        subDir->name = subDirName;
        result->includedFiles.children.insert(subDirName, subDir);
      }
    }
  }

  // Add ProFileReader::includeFiles information from cumulative parse to IncludedPriFile structure
  auto includeFiles = input.readerCumulative->includeFiles();
  QList<QmakeIncludedPriFile*> toBuild = {&result->includedFiles};
  while (!toBuild.isEmpty()) {
    auto current = toBuild.takeFirst();
    if (!current->proFile)
      continue; // Don't attempt to map subdirs here
    auto children = includeFiles.value(current->proFile);
    foreach(ProFile *child, children) {
      const auto childName = Utils::FilePath::fromString(child->fileName());
      auto it = current->children.find(childName);
      if (it == current->children.end()) {
        auto childTree = new QmakeIncludedPriFile;
        childTree->proFile = child;
        childTree->name = childName;
        current->children.insert(childName, childTree);
        proToResult[child->id()] = &childTree->result;
      }
    }
    toBuild.append(current->children.values());
  }

  auto exactReader = exactBuildPassReader ? exactBuildPassReader : input.readerExact;
  auto cumulativeReader = cumulativeBuildPassReader ? cumulativeBuildPassReader : input.readerCumulative;

  QHash<QString, QVector<ProFileEvaluator::SourceFile>> exactSourceFiles;
  QHash<QString, QVector<ProFileEvaluator::SourceFile>> cumulativeSourceFiles;

  const auto baseVPathsExact = baseVPaths(exactReader, input.projectDir, input.buildDirectory.toString());
  const auto baseVPathsCumulative = baseVPaths(cumulativeReader, input.projectDir, input.buildDirectory.toString());

  for (auto i = 0; i < static_cast<int>(FileType::FileTypeSize); ++i) {
    const auto type = static_cast<FileType>(i);
    const auto qmakeVariables = varNames(type, exactReader);
    foreach(const QString &qmakeVariable, qmakeVariables) {
      QHash<ProString, bool> handled;
      if (result->state == QmakeEvalResult::EvalOk) {
        const auto vPathsExact = fullVPaths(baseVPathsExact, exactReader, qmakeVariable, input.projectDir);
        auto sourceFiles = exactReader->absoluteFileValues(qmakeVariable, input.projectDir, vPathsExact, &handled, result->directoriesWithWildcards);
        exactSourceFiles[qmakeVariable] = sourceFiles;
        extractSources(proToResult, &result->includedFiles.result, sourceFiles, type, false);
      }
      const auto vPathsCumulative = fullVPaths(baseVPathsCumulative, cumulativeReader, qmakeVariable, input.projectDir);
      auto sourceFiles = cumulativeReader->absoluteFileValues(qmakeVariable, input.projectDir, vPathsCumulative, &handled, result->directoriesWithWildcards);
      cumulativeSourceFiles[qmakeVariable] = sourceFiles;
      extractSources(proToResult, &result->includedFiles.result, sourceFiles, type, true);
    }
  }

  // This is used for two things:
  // - Actual deployment, in which case we need exact values.
  // - The project tree, in which case we also want exact values to avoid recursively
  //   watching bogus paths. However, we accept the values even if the evaluation
  //   failed, to at least have a best-effort result.
  result->installsList = installsList(exactBuildPassReader, input.projectFilePath.toString(), input.projectDir, input.buildDirectory.toString());
  extractInstalls(proToResult, &result->includedFiles.result, result->installsList);

  if (result->state == QmakeEvalResult::EvalOk) {
    result->targetInformation = targetInformation(input.readerExact, exactBuildPassReader, input.buildDirectory, input.projectFilePath);

    // update other variables
    result->newVarValues[Variable::Defines] = exactReader->values(QLatin1String("DEFINES"));
    result->newVarValues[Variable::IncludePath] = includePaths(exactReader, input.sysroot, input.buildDirectory, input.projectDir);
    result->newVarValues[Variable::CppFlags] = exactReader->values(QLatin1String("QMAKE_CXXFLAGS"));
    result->newVarValues[Variable::CFlags] = exactReader->values(QLatin1String("QMAKE_CFLAGS"));
    result->newVarValues[Variable::ExactSource] = fileListForVar(exactSourceFiles, QLatin1String("SOURCES")) + fileListForVar(exactSourceFiles, QLatin1String("HEADERS")) + fileListForVar(exactSourceFiles, QLatin1String("OBJECTIVE_HEADERS"));
    result->newVarValues[Variable::CumulativeSource] = fileListForVar(cumulativeSourceFiles, QLatin1String("SOURCES")) + fileListForVar(cumulativeSourceFiles, QLatin1String("HEADERS")) + fileListForVar(cumulativeSourceFiles, QLatin1String("OBJECTIVE_HEADERS"));
    result->newVarValues[Variable::UiDir] = QStringList() << uiDirPath(exactReader, input.buildDirectory);
    result->newVarValues[Variable::HeaderExtension] = QStringList() << exactReader->value(QLatin1String("QMAKE_EXT_H"));
    result->newVarValues[Variable::CppExtension] = QStringList() << exactReader->value(QLatin1String("QMAKE_EXT_CPP"));
    result->newVarValues[Variable::MocDir] = QStringList() << mocDirPath(exactReader, input.buildDirectory);
    result->newVarValues[Variable::ExactResource] = fileListForVar(exactSourceFiles, QLatin1String("RESOURCES"));
    result->newVarValues[Variable::CumulativeResource] = fileListForVar(cumulativeSourceFiles, QLatin1String("RESOURCES"));
    result->newVarValues[Variable::PkgConfig] = exactReader->values(QLatin1String("PKGCONFIG"));
    result->newVarValues[Variable::PrecompiledHeader] = ProFileEvaluator::sourcesToFiles(exactReader->fixifiedValues(QLatin1String("PRECOMPILED_HEADER"), input.projectDir, input.buildDirectory.toString(), false));
    result->newVarValues[Variable::LibDirectories] = libDirectories(exactReader);
    result->newVarValues[Variable::Config] = exactReader->values(QLatin1String("CONFIG"));
    result->newVarValues[Variable::QmlImportPath] = exactReader->absolutePathValues(QLatin1String("QML_IMPORT_PATH"), input.projectDir);
    result->newVarValues[Variable::QmlDesignerImportPath] = exactReader->absolutePathValues(QLatin1String("QML_DESIGNER_IMPORT_PATH"), input.projectDir);
    result->newVarValues[Variable::Makefile] = exactReader->values(QLatin1String("MAKEFILE"));
    result->newVarValues[Variable::Qt] = exactReader->values(QLatin1String("QT"));
    result->newVarValues[Variable::ObjectExt] = exactReader->values(QLatin1String("QMAKE_EXT_OBJ"));
    result->newVarValues[Variable::ObjectsDir] = exactReader->values(QLatin1String("OBJECTS_DIR"));
    result->newVarValues[Variable::Version] = exactReader->values(QLatin1String("VERSION"));
    result->newVarValues[Variable::TargetExt] = exactReader->values(QLatin1String("TARGET_EXT"));
    result->newVarValues[Variable::TargetVersionExt] = exactReader->values(QLatin1String("TARGET_VERSION_EXT"));
    result->newVarValues[Variable::StaticLibExtension] = exactReader->values(QLatin1String("QMAKE_EXTENSION_STATICLIB"));
    result->newVarValues[Variable::ShLibExtension] = exactReader->values(QLatin1String("QMAKE_EXTENSION_SHLIB"));
    result->newVarValues[Variable::AndroidAbi] = exactReader->values(QLatin1String(Android::Constants::ANDROID_TARGET_ARCH));
    result->newVarValues[Variable::AndroidDeploySettingsFile] = exactReader->values(QLatin1String(Android::Constants::ANDROID_DEPLOYMENT_SETTINGS_FILE));
    result->newVarValues[Variable::AndroidPackageSourceDir] = exactReader->values(QLatin1String(Android::Constants::ANDROID_PACKAGE_SOURCE_DIR));
    result->newVarValues[Variable::AndroidAbis] = exactReader->values(QLatin1String(Android::Constants::ANDROID_ABIS));
    result->newVarValues[Variable::AndroidApplicationArgs] = exactReader->values(QLatin1String(Android::Constants::ANDROID_APPLICATION_ARGUMENTS));
    result->newVarValues[Variable::AndroidExtraLibs] = exactReader->values(QLatin1String(Android::Constants::ANDROID_EXTRA_LIBS));
    result->newVarValues[Variable::AppmanPackageDir] = exactReader->values(QLatin1String("AM_PACKAGE_DIR"));
    result->newVarValues[Variable::AppmanManifest] = exactReader->values(QLatin1String("AM_MANIFEST"));
    result->newVarValues[Variable::IsoIcons] = exactReader->values(QLatin1String("ISO_ICONS"));
    result->newVarValues[Variable::QmakeProjectName] = exactReader->values(QLatin1String("QMAKE_PROJECT_NAME"));
    result->newVarValues[Variable::QmakeCc] = exactReader->values("QMAKE_CC");
    result->newVarValues[Variable::QmakeCxx] = exactReader->values("QMAKE_CXX");
  }

  if (result->state == QmakeEvalResult::EvalOk || result->state == QmakeEvalResult::EvalPartial) {

    QList<QmakeIncludedPriFile*> toExtract = {&result->includedFiles};
    while (!toExtract.isEmpty()) {
      auto current = toExtract.takeFirst();
      processValues(current->result);
      toExtract.append(current->children.values());
    }
  }

  if (exactBuildPassReader && exactBuildPassReader != input.readerExact)
    delete exactBuildPassReader;
  if (cumulativeBuildPassReader && cumulativeBuildPassReader != input.readerCumulative)
    delete cumulativeBuildPassReader;

  QList<QPair<QmakePriFile*, QmakeIncludedPriFile*>> toCompare;
  toCompare.append(qMakePair(nullptr, &result->includedFiles));
  while (!toCompare.isEmpty()) {
    auto pn = toCompare.first().first;
    auto tree = toCompare.first().second;
    toCompare.pop_front();

    // Loop prevention: Make sure that exact same node is not in our parent chain
    for (auto priFile : qAsConst(tree->children)) {
      auto loop = input.parentFilePaths.contains(priFile->name);
      for (const QmakePriFile *n = pn; n && !loop; n = n->parent()) {
        if (n->filePath() == priFile->name)
          loop = true;
      }
      if (loop)
        continue; // Do nothing

      if (priFile->proFile) {
        auto *qmakePriFileNode = new QmakePriFile(priFile->name);
        if (pn)
          pn->addChild(qmakePriFileNode);
        else
          result->directChildren << qmakePriFileNode;
        qmakePriFileNode->setIncludedInExactParse(input.includedInExcactParse && result->state == QmakeEvalResult::EvalOk);
        result->priFiles.append(qMakePair(qmakePriFileNode, priFile->result));
        toCompare.append(qMakePair(qmakePriFileNode, priFile));
      } else {
        auto *qmakeProFileNode = new QmakeProFile(priFile->name);
        if (pn)
          pn->addChild(qmakeProFileNode);
        else
          result->directChildren << qmakeProFileNode;
        qmakeProFileNode->setIncludedInExactParse(input.includedInExcactParse && result->exactSubdirs.contains(qmakeProFileNode->filePath()));
        qmakeProFileNode->setParseInProgress(true);
        result->proFiles << qmakeProFileNode;
      }
    }
  }

  return result;
}

auto QmakeProFile::asyncEvaluate(QFutureInterface<QmakeEvalResultPtr> &fi, QmakeEvalInput input) -> void
{
  fi.reportResult(evaluate(input));
}

auto sortByParserNodes(Node *a, Node *b) -> bool
{
  return a->filePath() < b->filePath();
}

auto QmakeProFile::applyEvaluate(const QmakeEvalResultPtr &result) -> void
{
  if (!m_readerExact)
    return;

  if (m_buildSystem->asyncUpdateState() == QmakeBuildSystem::ShuttingDown) {
    cleanupProFileReaders();
    return;
  }

  foreach(const QString &error, result->errors)
    QmakeBuildSystem::proFileParseError(error, filePath());

  // we are changing what is executed in that case
  if (result->state == QmakeEvalResult::EvalFail || m_buildSystem->wasEvaluateCanceled()) {
    m_validParse = false;
    cleanupProFileReaders();
    setValidParseRecursive(false);
    setParseInProgressRecursive(false);

    if (result->state == QmakeEvalResult::EvalFail) {
      QmakeBuildSystem::proFileParseError(QCoreApplication::translate("QmakeProFile", "Error while parsing file %1. Giving up.").arg(filePath().toUserOutput()), filePath());
      if (m_projectType == ProjectType::Invalid)
        return;

      makeEmpty();

      m_projectType = ProjectType::Invalid;
    }
    return;
  }

  qCDebug(qmakeParse()) << "QmakeProFile - updating files for file " << filePath();

  if (result->projectType != m_projectType) {
    // probably all subfiles/projects have changed anyway
    // delete files && folders && projects
    foreach(QmakePriFile *c, children()) {
      if (auto qmakeProFile = dynamic_cast<QmakeProFile*>(c)) {
        qmakeProFile->setValidParseRecursive(false);
        qmakeProFile->setParseInProgressRecursive(false);
      }
    }

    makeEmpty();
    m_projectType = result->projectType;
  }

  //
  // Add/Remove pri files, sub projects
  //
  auto buildDirectory = m_buildSystem->buildDir(m_filePath);
  makeEmpty();
  for (const auto toAdd : qAsConst(result->directChildren))
    addChild(toAdd);
  result->directChildren.clear();

  for (const auto &priFiles : qAsConst(result->priFiles)) {
    priFiles.first->finishInitialization(m_buildSystem, this);
    priFiles.first->update(priFiles.second);
  }

  for (const auto proFile : qAsConst(result->proFiles)) {
    proFile->finishInitialization(m_buildSystem, proFile);
    proFile->asyncUpdate();
  }
  QmakePriFile::update(result->includedFiles.result);

  m_validParse = (result->state == QmakeEvalResult::EvalOk);
  if (m_validParse) {
    // update TargetInformation
    m_qmakeTargetInformation = result->targetInformation;

    m_subProjectsNotToDeploy = Utils::transform(result->subProjectsNotToDeploy, [](const QString &s) { return FilePath::fromString(s); });
    m_installsList = result->installsList;

    if (m_varValues != result->newVarValues)
      m_varValues = result->newVarValues;

    m_displayName = singleVariableValue(Variable::QmakeProjectName);
    m_featureRoots = m_readerExact->featureRoots();
  } // result == EvalOk

  if (!result->directoriesWithWildcards.isEmpty()) {
    if (!m_wildcardWatcher) {
      m_wildcardWatcher = std::make_unique<Utils::FileSystemWatcher>();
      QObject::connect(m_wildcardWatcher.get(), &Utils::FileSystemWatcher::directoryChanged, [this](QString path) {
        auto directoryContents = QDir(path).entryList();
        if (m_wildcardDirectoryContents.value(path) != directoryContents) {
          m_wildcardDirectoryContents.insert(path, directoryContents);
          scheduleUpdate();
        }
      });
    }
    const auto directoriesToAdd = Utils::filtered<QStringList>(Utils::toList(result->directoriesWithWildcards), [this](const QString &path) {
      return !m_wildcardWatcher->watchesDirectory(path);
    });
    for (const auto &path : directoriesToAdd)
      m_wildcardDirectoryContents.insert(path, QDir(path).entryList());
    m_wildcardWatcher->addDirectories(directoriesToAdd, Utils::FileSystemWatcher::WatchModifiedDate);
  }
  if (m_wildcardWatcher) {
    if (result->directoriesWithWildcards.isEmpty()) {
      m_wildcardWatcher.reset();
      m_wildcardDirectoryContents.clear();
    } else {
      const auto directoriesToRemove = Utils::filtered<QStringList>(m_wildcardWatcher->directories(), [&result](const QString &path) {
        return !result->directoriesWithWildcards.contains(path);
      });
      m_wildcardWatcher->removeDirectories(directoriesToRemove);
      for (const auto &path : directoriesToRemove)
        m_wildcardDirectoryContents.remove(path);
    }
  }

  setParseInProgress(false);

  updateGeneratedFiles(buildDirectory);

  cleanupProFileReaders();
}

auto QmakeProFile::cleanupProFileReaders() -> void
{
  if (m_readerExact)
    m_buildSystem->destroyProFileReader(m_readerExact);
  if (m_readerCumulative)
    m_buildSystem->destroyProFileReader(m_readerCumulative);

  m_readerExact = nullptr;
  m_readerCumulative = nullptr;
}

auto QmakeProFile::uiDirPath(QtSupport::ProFileReader *reader, const FilePath &buildDir) -> QString
{
  auto path = reader->value(QLatin1String("UI_DIR"));
  if (QFileInfo(path).isRelative())
    path = QDir::cleanPath(buildDir.toString() + QLatin1Char('/') + path);
  return path;
}

auto QmakeProFile::mocDirPath(QtSupport::ProFileReader *reader, const FilePath &buildDir) -> QString
{
  auto path = reader->value(QLatin1String("MOC_DIR"));
  if (QFileInfo(path).isRelative())
    path = QDir::cleanPath(buildDir.toString() + QLatin1Char('/') + path);
  return path;
}

auto QmakeProFile::sysrootify(const QString &path, const QString &sysroot, const QString &baseDir, const QString &outputDir) -> QString
{
  #ifdef Q_OS_WIN
  auto cs = Qt::CaseInsensitive;
  #else
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
  #endif
  if (sysroot.isEmpty() || path.startsWith(sysroot, cs) || path.startsWith(baseDir, cs) || path.startsWith(outputDir, cs)) {
    return path;
  }
  auto sysrooted = QDir::cleanPath(sysroot + path);
  return !IoUtils::exists(sysrooted) ? path : sysrooted;
}

auto QmakeProFile::includePaths(QtSupport::ProFileReader *reader, const FilePath &sysroot, const FilePath &buildDir, const QString &projectDir) -> QStringList
{
  QStringList paths;
  auto nextIsAnIncludePath = false;
  foreach(const QString &cxxflags, reader->values(QLatin1String("QMAKE_CXXFLAGS"))) {
    if (nextIsAnIncludePath) {
      nextIsAnIncludePath = false;
      paths.append(cxxflags);
    } else if (cxxflags.startsWith(QLatin1String("-I"))) {
      paths.append(cxxflags.mid(2));
    } else if (cxxflags.startsWith(QLatin1String("-isystem"))) {
      nextIsAnIncludePath = true;
    }
  }

  auto tryUnfixified = false;

  // These paths should not be checked for existence, to ensure consistent include path lists
  // before and after building.
  const auto mocDir = mocDirPath(reader, buildDir);
  const auto uiDir = uiDirPath(reader, buildDir);

  foreach(const ProFileEvaluator::SourceFile &el, reader->fixifiedValues(QLatin1String("INCLUDEPATH"), projectDir, buildDir.toString(), false)) {
    const auto sysrootifiedPath = sysrootify(el.fileName, sysroot.toString(), projectDir, buildDir.toString());
    if (IoUtils::isAbsolutePath(sysrootifiedPath) && (IoUtils::exists(sysrootifiedPath) || sysrootifiedPath == mocDir || sysrootifiedPath == uiDir)) {
      paths << sysrootifiedPath;
    } else {
      tryUnfixified = true;
    }
  }

  // If sysrootifying a fixified path does not yield a valid path, try again with the
  // unfixified value. This can be necessary for cross-building; see QTCREATORBUG-21164.
  if (tryUnfixified) {
    const auto rawValues = reader->values("INCLUDEPATH");
    for (const auto &p : rawValues) {
      const auto sysrootifiedPath = sysrootify(QDir::cleanPath(p), sysroot.toString(), projectDir, buildDir.toString());
      if (IoUtils::isAbsolutePath(sysrootifiedPath) && IoUtils::exists(sysrootifiedPath))
        paths << sysrootifiedPath;
    }
  }

  paths.removeDuplicates();
  return paths;
}

auto QmakeProFile::libDirectories(QtSupport::ProFileReader *reader) -> QStringList
{
  QStringList result;
  foreach(const QString &str, reader->values(QLatin1String("LIBS"))) {
    if (str.startsWith(QLatin1String("-L")))
      result.append(str.mid(2));
  }
  return result;
}

auto QmakeProFile::subDirsPaths(QtSupport::ProFileReader *reader, const QString &projectDir, QStringList *subProjectsNotToDeploy, QStringList *errors) -> FilePaths
{
  FilePaths subProjectPaths;

  const auto subDirVars = reader->values(QLatin1String("SUBDIRS"));

  foreach(const QString &subDirVar, subDirVars) {
    // Special case were subdir is just an identifier:
    //   "SUBDIR = subid
    //    subid.subdir = realdir"
    // or
    //   "SUBDIR = subid
    //    subid.file = realdir/realfile.pro"

    QString realDir;
    const QString subDirKey = subDirVar + QLatin1String(".subdir");
    const QString subDirFileKey = subDirVar + QLatin1String(".file");
    if (reader->contains(subDirKey))
      realDir = reader->value(subDirKey);
    else if (reader->contains(subDirFileKey))
      realDir = reader->value(subDirFileKey);
    else
      realDir = subDirVar;
    QFileInfo info(realDir);
    if (!info.isAbsolute())
      info.setFile(projectDir + QLatin1Char('/') + realDir);
    realDir = info.filePath();

    QString realFile;
    if (info.isDir())
      realFile = QString::fromLatin1("%1/%2.pro").arg(realDir, info.fileName());
    else
      realFile = realDir;

    if (QFile::exists(realFile)) {
      realFile = QDir::cleanPath(realFile);
      subProjectPaths << FilePath::fromString(realFile);
      if (subProjectsNotToDeploy && !subProjectsNotToDeploy->contains(realFile) && reader->values(subDirVar + QLatin1String(".CONFIG")).contains(QLatin1String("no_default_target"))) {
        subProjectsNotToDeploy->append(realFile);
      }
    } else {
      if (errors)
        errors->append(QCoreApplication::translate("QmakeProFile", "Could not find .pro file for subdirectory \"%1\" in \"%2\".").arg(subDirVar).arg(realDir));
    }
  }

  return Utils::filteredUnique(subProjectPaths);
}

auto QmakeProFile::targetInformation(QtSupport::ProFileReader *reader, QtSupport::ProFileReader *readerBuildPass, const FilePath &buildDir, const FilePath &projectFilePath) -> TargetInformation
{
  TargetInformation result;
  if (!reader || !readerBuildPass)
    return result;

  auto builds = reader->values(QLatin1String("BUILDS"));
  if (!builds.isEmpty()) {
    auto build = builds.first();
    result.buildTarget = reader->value(build + QLatin1String(".target"));
  }

  // BUILD DIR
  result.buildDir = buildDir;

  if (readerBuildPass->contains(QLatin1String("DESTDIR")))
    result.destDir = FilePath::fromString(readerBuildPass->value(QLatin1String("DESTDIR")));

  // Target
  result.target = readerBuildPass->value(QLatin1String("TARGET"));
  if (result.target.isEmpty())
    result.target = projectFilePath.baseName();

  result.valid = true;

  return result;
}

auto QmakeProFile::targetInformation() const -> TargetInformation
{
  return m_qmakeTargetInformation;
}

auto QmakeProFile::installsList(const QtSupport::ProFileReader *reader, const QString &projectFilePath, const QString &projectDir, const QString &buildDir) -> InstallsList
{
  InstallsList result;
  if (!reader)
    return result;
  const auto &itemList = reader->values(QLatin1String("INSTALLS"));
  if (itemList.isEmpty())
    return result;

  const QStringList installPrefixVars{"QT_INSTALL_PREFIX", "QT_INSTALL_EXAMPLES"};
  QList<QPair<QString, QString>> installPrefixValues;
  for (const auto &installPrefix : installPrefixVars) {
    installPrefixValues << qMakePair(reader->propertyValue(installPrefix), reader->propertyValue(installPrefix + "/dev"));
  }

  foreach(const QString &item, itemList) {
    const auto config = reader->values(item + ".CONFIG");
    const auto active = !config.contains("no_default_install");
    const auto executable = config.contains("executable");
    const QString pathVar = item + QLatin1String(".path");
    const auto &itemPaths = reader->values(pathVar);
    if (itemPaths.count() != 1) {
      qDebug("Invalid RHS: Variable '%s' has %d values.", qPrintable(pathVar), int(itemPaths.count()));
      if (itemPaths.isEmpty()) {
        qDebug("%s: Ignoring INSTALLS item '%s', because it has no path.", qPrintable(projectFilePath), qPrintable(item));
        continue;
      }
    }

    auto itemPath = itemPaths.last();
    for (const auto &prefixValuePair : qAsConst(installPrefixValues)) {
      if (prefixValuePair.first == prefixValuePair.second || !itemPath.startsWith(prefixValuePair.first)) {
        continue;
      }
      // This is a hack for projects which install into $$[QT_INSTALL_*],
      // in particular Qt itself, examples being most relevant.
      // Projects which implement their own install path policy must
      // parametrize their INSTALLS themselves depending on the intended
      // installation/deployment mode.
      itemPath.replace(0, prefixValuePair.first.length(), prefixValuePair.second);
      break;
    }
    if (item == QLatin1String("target")) {
      if (active)
        result.targetPath = itemPath;
    } else {
      const auto &itemFiles = reader->fixifiedValues(item + QLatin1String(".files"), projectDir, buildDir, true);
      result.items << InstallsItem(itemPath, itemFiles, active, executable);
    }
  }
  return result;
}

auto QmakeProFile::installsList() const -> InstallsList
{
  return m_installsList;
}

auto QmakeProFile::sourceDir() const -> FilePath
{
  return directoryPath();
}

auto QmakeProFile::generatedFiles(const FilePath &buildDir, const FilePath &sourceFile, const FileType &sourceFileType) const -> FilePaths
{
  // The mechanism for finding the file names is rather crude, but as we
  // cannot parse QMAKE_EXTRA_COMPILERS and qmake has facilities to put
  // ui_*.h files into a special directory, or even change the .h suffix, we
  // cannot help doing this here.

  if (sourceFileType == FileType::Form) {
    FilePath location;
    auto it = m_varValues.constFind(Variable::UiDir);
    if (it != m_varValues.constEnd() && !it.value().isEmpty())
      location = FilePath::fromString(it.value().front());
    else
      location = buildDir;
    if (location.isEmpty())
      return {};
    location = location.pathAppended("ui_" + sourceFile.completeBaseName() + singleVariableValue(Variable::HeaderExtension));
    return {Utils::FilePath::fromString(QDir::cleanPath(location.toString()))};
  } else if (sourceFileType == FileType::StateChart) {
    if (buildDir.isEmpty())
      return {};
    const auto location = buildDir.pathAppended(sourceFile.completeBaseName());
    return {location.stringAppended(singleVariableValue(Variable::HeaderExtension)), location.stringAppended(singleVariableValue(Variable::CppExtension))};
  }
  return {};
}

auto QmakeProFile::extraCompilers() const -> QList<ExtraCompiler*>
{
  return m_extraCompilers;
}

auto QmakeProFile::setupExtraCompiler(const FilePath &buildDir, const FileType &fileType, ExtraCompilerFactory *factory) -> void
{
  for (const auto &fn : collectFiles(fileType)) {
    const auto generated = generatedFiles(buildDir, fn, fileType);
    if (!generated.isEmpty())
      m_extraCompilers.append(factory->create(m_buildSystem->project(), fn, generated));
  }
}

auto QmakeProFile::updateGeneratedFiles(const FilePath &buildDir) -> void
{
  // We can do this because other plugins are not supposed to keep the compilers around.
  qDeleteAll(m_extraCompilers);
  m_extraCompilers.clear();

  // Only those project types can have generated files for us
  if (m_projectType != ProjectType::ApplicationTemplate && m_projectType != ProjectType::SharedLibraryTemplate && m_projectType != ProjectType::StaticLibraryTemplate) {
    return;
  }

  const auto factories = ProjectExplorer::ExtraCompilerFactory::extraCompilerFactories();

  auto formFactory = Utils::findOrDefault(factories, Utils::equal(&ExtraCompilerFactory::sourceType, FileType::Form));
  if (formFactory)
    setupExtraCompiler(buildDir, FileType::Form, formFactory);
  auto scxmlFactory = Utils::findOrDefault(factories, Utils::equal(&ExtraCompilerFactory::sourceType, FileType::StateChart));
  if (scxmlFactory)
    setupExtraCompiler(buildDir, FileType::StateChart, scxmlFactory);
}
