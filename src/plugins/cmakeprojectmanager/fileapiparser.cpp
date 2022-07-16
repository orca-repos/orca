// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileapiparser.hpp"

#include <app/app_version.hpp>
#include <core/core-message-manager.hpp>
#include <projectexplorer/rawprojectpart.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace CMakeProjectManager {
namespace Internal {

using namespace FileApiDetails;
using namespace Utils;

constexpr char CMAKE_RELATIVE_REPLY_PATH[] = ".cmake/api/v1/reply";
constexpr char CMAKE_RELATIVE_QUERY_PATH[] = ".cmake/api/v1/query";

static Q_LOGGING_CATEGORY(cmakeFileApi, "qtc.cmake.fileApi", QtWarningMsg);

const QStringList CMAKE_QUERY_FILENAMES = {"cache-v2", "codemodel-v2", "cmakeFiles-v1"};

// --------------------------------------------------------------------
// Helper:
// --------------------------------------------------------------------

static auto cmakeReplyDirectory(const FilePath &buildDirectory) -> FilePath
{
  return buildDirectory.pathAppended(CMAKE_RELATIVE_REPLY_PATH);
}

static auto reportFileApiSetupFailure() -> void
{
  Orca::Plugin::Core::MessageManager::writeFlashing(QCoreApplication::translate("CMakeProjectManager::Internal", "Failed to set up CMake file API support. %1 cannot " "extract project information.").arg(Orca::Plugin::Core::IDE_DISPLAY_NAME));
}

static auto cmakeVersion(const QJsonObject &obj) -> std::pair<int, int>
{
  const auto version = obj.value("version").toObject();
  const auto major = version.value("major").toInt(-1);
  const auto minor = version.value("minor").toInt(-1);
  return std::make_pair(major, minor);
}

static auto checkJsonObject(const QJsonObject &obj, const QString &kind, int major, int minor = -1) -> bool
{
  auto version = cmakeVersion(obj);
  if (major == -1)
    version.first = major;
  if (minor == -1)
    version.second = minor;
  return obj.value("kind").toString() == kind && version == std::make_pair(major, minor);
}

static auto nameValue(const QJsonObject &obj) -> std::pair<QString, QString>
{
  return std::make_pair(obj.value("name").toString(), obj.value("value").toString());
}

static auto readJsonFile(const FilePath &filePath) -> QJsonDocument
{
  qCDebug(cmakeFileApi) << "readJsonFile:" << filePath;
  QTC_ASSERT(!filePath.isEmpty(), return {});

  const auto doc = QJsonDocument::fromJson(filePath.fileContents());

  return doc;
}

auto indexList(const QJsonValue &v) -> std::vector<int>
{
  const auto &indexList = v.toArray();
  std::vector<int> result;
  result.reserve(static_cast<size_t>(indexList.count()));

  for (const QJsonValue &v : indexList) {
    result.push_back(v.toInt(-1));
  }
  return result;
}

// Reply file:

static auto readReplyFile(const FilePath &filePath, QString &errorMessage) -> ReplyFileContents
{
  const auto document = readJsonFile(filePath);
  static const auto msg = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid reply file created by CMake.");

  ReplyFileContents result;
  if (document.isNull() || document.isEmpty() || !document.isObject()) {
    errorMessage = msg;
    return result;
  }

  const auto rootObject = document.object();

  {
    const auto cmakeObject = rootObject.value("cmake").toObject();
    {
      const auto paths = cmakeObject.value("paths").toObject();
      {
        result.cmakeExecutable = paths.value("cmake").toString();
        result.ctestExecutable = paths.value("ctest").toString();
        result.cmakeRoot = paths.value("root").toString();
      }
      const auto generator = cmakeObject.value("generator").toObject();
      {
        result.generator = generator.value("name").toString();
        result.isMultiConfig = generator.value("multiConfig").toBool();
      }
      const auto version = cmakeObject.value("version").toObject();
      {
        auto major = version.value("major").toInt();
        auto minor = version.value("minor").toInt();
        auto patch = version.value("patch").toInt();
        result.cmakeVersion = QVersionNumber(major, minor, patch);
      }
    }
  }

  auto hadInvalidObject = false;
  {
    const auto objects = rootObject.value("objects").toArray();
    for (const QJsonValue &v : objects) {
      const auto object = v.toObject();
      {
        ReplyObject r;
        r.kind = object.value("kind").toString();
        r.file = object.value("jsonFile").toString();
        r.version = cmakeVersion(object);

        if (r.kind.isEmpty() || r.file.isEmpty() || r.version.first == -1 || r.version.second == -1)
          hadInvalidObject = true;
        else
          result.replies.append(r);
      }
    }
  }

  if (result.generator.isEmpty() || result.cmakeExecutable.isEmpty() || result.cmakeRoot.isEmpty() || result.replies.isEmpty() || hadInvalidObject)
    errorMessage = msg;

  return result;
}

// Cache file:

static auto readCacheFile(const FilePath &cacheFile, QString &errorMessage) -> CMakeConfig
{
  CMakeConfig result;

  const auto doc = readJsonFile(cacheFile);
  const auto root = doc.object();

  if (!checkJsonObject(root, "cache", 2)) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid cache file generated by CMake.");
    return {};
  }

  const auto entries = root.value("entries").toArray();
  for (const QJsonValue &v : entries) {
    CMakeConfigItem item;

    const auto entry = v.toObject();
    auto nv = nameValue(entry);
    item.key = nv.first.toUtf8();
    item.value = nv.second.toUtf8();

    item.type = CMakeConfigItem::typeStringToType(entry.value("type").toString().toUtf8());

    {
      const auto properties = entry.value("properties").toArray();
      for (const QJsonValue &v : properties) {
        const auto prop = v.toObject();
        auto nv = nameValue(prop);
        if (nv.first == "ADVANCED") {
          const auto boolValue = CMakeConfigItem::toBool(nv.second);
          item.isAdvanced = boolValue.has_value() && boolValue.value();
        } else if (nv.first == "HELPSTRING") {
          item.documentation = nv.second.toUtf8();
        } else if (nv.first == "STRINGS") {
          item.values = nv.second.split(';');
        }
      }
    }
    result.append(item);
  }
  return result;
}

// CMake Files:

static auto readCMakeFilesFile(const FilePath &cmakeFilesFile, QString &errorMessage) -> std::vector<CMakeFileInfo>
{
  std::vector<CMakeFileInfo> result;

  const auto doc = readJsonFile(cmakeFilesFile);
  const auto root = doc.object();

  if (!checkJsonObject(root, "cmakeFiles", 1)) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid cmakeFiles file generated by CMake.");
    return {};
  }

  const auto inputs = root.value("inputs").toArray();
  for (const QJsonValue &v : inputs) {
    CMakeFileInfo info;
    const auto input = v.toObject();
    info.path = cmakeFilesFile.withNewPath(input.value("path").toString());

    info.isCMake = input.value("isCMake").toBool();
    const auto filename = info.path.fileName();
    info.isCMakeListsDotTxt = (filename.compare("CMakeLists.txt", HostOsInfo::fileNameCaseSensitivity()) == 0);

    info.isGenerated = input.value("isGenerated").toBool();
    info.isExternal = input.value("isExternal").toBool();

    result.emplace_back(std::move(info));
  }
  return result;
}

// Codemodel file:

auto extractDirectories(const QJsonArray &directories, QString &errorMessage) -> std::vector<Directory>
{
  if (directories.isEmpty()) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: No directories.");
    return {};
  }

  std::vector<Directory> result;
  for (const QJsonValue &v : directories) {
    const auto obj = v.toObject();
    if (obj.isEmpty()) {
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Empty directory object.");
      continue;
    }
    Directory dir;
    dir.sourcePath = obj.value("source").toString();
    dir.buildPath = obj.value("build").toString();
    dir.parent = obj.value("parentIndex").toInt(-1);
    dir.project = obj.value("projectIndex").toInt(-1);
    dir.children = indexList(obj.value("childIndexes"));
    dir.targets = indexList(obj.value("targetIndexes"));
    dir.hasInstallRule = obj.value("hasInstallRule").toBool();

    result.emplace_back(std::move(dir));
  }
  return result;
}

static auto extractProjects(const QJsonArray &projects, QString &errorMessage) -> std::vector<Project>
{
  if (projects.isEmpty()) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: No projects.");
    return {};
  }

  std::vector<Project> result;
  for (const QJsonValue &v : projects) {
    const auto obj = v.toObject();
    if (obj.isEmpty()) {
      qCDebug(cmakeFileApi) << "Empty project skipped!";
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Empty project object.");
      continue;
    }
    Project project;
    project.name = obj.value("name").toString();
    project.parent = obj.value("parentIndex").toInt(-1);
    project.children = indexList(obj.value("childIndexes"));
    project.directories = indexList(obj.value("directoryIndexes"));
    project.targets = indexList(obj.value("targetIndexes"));

    if (project.directories.empty()) {
      qCDebug(cmakeFileApi) << "Invalid project skipped!";
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Broken project data.");
      continue;
    }

    qCDebug(cmakeFileApi) << "Project read:" << project.name << project.directories;
    result.emplace_back(std::move(project));
  }
  return result;
}

static auto extractTargets(const QJsonArray &targets, QString &errorMessage) -> std::vector<Target>
{
  std::vector<Target> result;
  for (const QJsonValue &v : targets) {
    const auto obj = v.toObject();
    if (obj.isEmpty()) {
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Empty target object.");
      continue;
    }
    Target target;
    target.name = obj.value("name").toString();
    target.id = obj.value("id").toString();
    target.directory = obj.value("directoryIndex").toInt(-1);
    target.project = obj.value("projectIndex").toInt(-1);
    target.jsonFile = obj.value("jsonFile").toString();

    if (target.name.isEmpty() || target.id.isEmpty() || target.jsonFile.isEmpty() || target.directory == -1 || target.project == -1) {
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Broken target data.");
      continue;
    }

    result.emplace_back(std::move(target));
  }
  return result;
}

static auto validateIndexes(const Configuration &config) -> bool
{
  const auto directoryCount = static_cast<int>(config.directories.size());
  const auto projectCount = static_cast<int>(config.projects.size());
  const auto targetCount = static_cast<int>(config.targets.size());

  auto topLevelCount = 0;
  for (const auto &d : config.directories) {
    if (d.parent == -1)
      ++topLevelCount;

    if (d.parent < -1 || d.parent >= directoryCount) {
      qCWarning(cmakeFileApi) << "Directory" << d.sourcePath << ": parent index" << d.parent << "is broken.";
      return false;
    }
    if (d.project < 0 || d.project >= projectCount) {
      qCWarning(cmakeFileApi) << "Directory" << d.sourcePath << ": project index" << d.project << "is broken.";
      return false;
    }
    if (contains(d.children, [directoryCount](int c) { return c < 0 || c >= directoryCount; })) {
      qCWarning(cmakeFileApi) << "Directory" << d.sourcePath << ": A child index" << d.children << "is broken.";
      return false;
    }
    if (contains(d.targets, [targetCount](int t) { return t < 0 || t >= targetCount; })) {
      qCWarning(cmakeFileApi) << "Directory" << d.sourcePath << ": A target index" << d.targets << "is broken.";
      return false;
    }
  }
  if (topLevelCount != 1) {
    qCWarning(cmakeFileApi) << "Directories: Invalid number of top level directories, " << topLevelCount << " (expected: 1).";
    return false;
  }

  topLevelCount = 0;
  for (const auto &p : config.projects) {
    if (p.parent == -1)
      ++topLevelCount;

    if (p.parent < -1 || p.parent >= projectCount) {
      qCWarning(cmakeFileApi) << "Project" << p.name << ": parent index" << p.parent << "is broken.";
      return false;
    }
    if (contains(p.children, [projectCount](int p) { return p < 0 || p >= projectCount; })) {
      qCWarning(cmakeFileApi) << "Project" << p.name << ": A child index" << p.children << "is broken.";
      return false;
    }
    if (contains(p.targets, [targetCount](int t) { return t < 0 || t >= targetCount; })) {
      qCWarning(cmakeFileApi) << "Project" << p.name << ": A target index" << p.targets << "is broken.";
      return false;
    }
    if (contains(p.directories, [directoryCount](int d) { return d < 0 || d >= directoryCount; })) {
      qCWarning(cmakeFileApi) << "Project" << p.name << ": A directory index" << p.directories << "is broken.";
      return false;
    }
  }
  if (topLevelCount != 1) {
    qCWarning(cmakeFileApi) << "Projects: Invalid number of top level projects, " << topLevelCount << " (expected: 1).";
    return false;
  }

  for (const auto &t : config.targets) {
    if (t.directory < 0 || t.directory >= directoryCount) {
      qCWarning(cmakeFileApi) << "Target" << t.name << ": directory index" << t.directory << "is broken.";
      return false;
    }
    if (t.project < 0 || t.project >= projectCount) {
      qCWarning(cmakeFileApi) << "Target" << t.name << ": project index" << t.project << "is broken.";
      return false;
    }
  }
  return true;
}

static auto extractConfigurations(const QJsonArray &configs, QString &errorMessage) -> std::vector<Configuration>
{
  if (configs.isEmpty()) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: No configurations.");
    return {};
  }

  std::vector<FileApiDetails::Configuration> result;
  for (const QJsonValue &v : configs) {
    const auto obj = v.toObject();
    if (obj.isEmpty()) {
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Empty configuration object.");
      continue;
    }
    Configuration config;
    config.name = obj.value("name").toString();

    config.directories = extractDirectories(obj.value("directories").toArray(), errorMessage);
    config.projects = extractProjects(obj.value("projects").toArray(), errorMessage);
    config.targets = extractTargets(obj.value("targets").toArray(), errorMessage);

    if (!validateIndexes(config)) {
      errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake: Broken " "indexes in directories, projects, or targets.");
      return {};
    }

    result.emplace_back(std::move(config));
  }
  return result;
}

static auto readCodemodelFile(const FilePath &codemodelFile, QString &errorMessage) -> std::vector<Configuration>
{
  const auto doc = readJsonFile(codemodelFile);
  const auto root = doc.object();

  if (!checkJsonObject(root, "codemodel", 2)) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid codemodel file generated by CMake.");
    return {};
  }

  return extractConfigurations(root.value("configurations").toArray(), errorMessage);
}

// TargetDetails:

static auto extractFragments(const QJsonObject &obj) -> std::vector<FileApiDetails::FragmentInfo>
{
  const auto fragments = obj.value("commandFragments").toArray();
  return transform<std::vector>(fragments, [](const QJsonValue &v) {
    const auto o = v.toObject();
    return FileApiDetails::FragmentInfo{o.value("fragment").toString(), o.value("role").toString()};
  });
}

static auto extractTargetDetails(const QJsonObject &root, QString &errorMessage) -> TargetDetails
{
  TargetDetails t;
  t.name = root.value("name").toString();
  t.id = root.value("id").toString();
  t.type = root.value("type").toString();

  if (t.name.isEmpty() || t.id.isEmpty() || t.type.isEmpty()) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid target file: Information is missing.");
    return {};
  }

  t.backtrace = root.value("backtrace").toInt(-1);
  {
    const auto folder = root.value("folder").toObject();
    t.folderTargetProperty = folder.value("name").toString();
  }
  {
    const auto paths = root.value("paths").toObject();
    t.sourceDir = FilePath::fromString(paths.value("source").toString());
    t.buildDir = FilePath::fromString(paths.value("build").toString());
  }
  t.nameOnDisk = root.value("nameOnDisk").toString();
  {
    const auto artifacts = root.value("artifacts").toArray();
    t.artifacts = transform<QList>(artifacts, [](const QJsonValue &v) {
      return FilePath::fromString(v.toObject().value("path").toString());
    });
  }
  t.isGeneratorProvided = root.value("isGeneratorProvided").toBool();
  {
    const auto install = root.value("install").toObject();
    t.installPrefix = install.value("prefix").toObject().value("path").toString();
    {
      const auto destinations = install.value("destinations").toArray();
      t.installDestination = transform<std::vector>(destinations, [](const QJsonValue &v) {
        const auto o = v.toObject();
        return InstallDestination{o.value("path").toString(), o.value("backtrace").toInt(-1)};
      });
    }
  }
  {
    const auto link = root.value("link").toObject();
    if (link.isEmpty()) {
      t.link = {};
    } else {
      LinkInfo info;
      info.language = link.value("language").toString();
      info.isLto = link.value("lto").toBool();
      info.sysroot = link.value("sysroot").toObject().value("path").toString();
      info.fragments = extractFragments(link);
      t.link = info;
    }
  }
  {
    const auto archive = root.value("archive").toObject();
    if (archive.isEmpty()) {
      t.archive = {};
    } else {
      ArchiveInfo info;
      info.isLto = archive.value("lto").toBool();
      info.fragments = extractFragments(archive);
      t.archive = info;
    }
  }
  {
    const auto dependencies = root.value("dependencies").toArray();
    t.dependencies = transform<std::vector>(dependencies, [](const QJsonValue &v) {
      const auto o = v.toObject();
      return DependencyInfo{o.value("id").toString(), o.value("backtrace").toInt(-1)};
    });
  }
  {
    const auto sources = root.value("sources").toArray();
    t.sources = transform<std::vector>(sources, [](const QJsonValue &v) {
      const auto o = v.toObject();
      return SourceInfo{o.value("path").toString(), o.value("compileGroupIndex").toInt(-1), o.value("sourceGroupIndex").toInt(-1), o.value("backtrace").toInt(-1), o.value("isGenerated").toBool()};
    });
  }
  {
    const auto sourceGroups = root.value("sourceGroups").toArray();
    t.sourceGroups = transform<std::vector>(sourceGroups, [](const QJsonValue &v) {
      const auto o = v.toObject();
      return o.value("name").toString();
    });
  }
  {
    const auto compileGroups = root.value("compileGroups").toArray();
    t.compileGroups = transform<std::vector>(compileGroups, [](const QJsonValue &v) {
      const auto o = v.toObject();
      return CompileInfo{
        transform<std::vector>(o.value("sourceIndexes").toArray(), [](const QJsonValue &v) { return v.toInt(-1); }),
        o.value("language").toString(),
        transform<QList>(o.value("compileCommandFragments").toArray(), [](const QJsonValue &v) {
          const auto o = v.toObject();
          return o.value("fragment").toString();
        }),
        transform<std::vector>(o.value("includes").toArray(), [](const QJsonValue &v) {
          const auto i = v.toObject();
          const auto path = i.value("path").toString();
          const auto isSystem = i.value("isSystem").toBool();
          const ProjectExplorer::HeaderPath hp(path, isSystem ? ProjectExplorer::HeaderPathType::System : ProjectExplorer::HeaderPathType::User);

          return IncludeInfo{ProjectExplorer::RawProjectPart::frameworkDetectionHeuristic(hp), i.value("backtrace").toInt(-1)};
        }),
        transform<std::vector>(o.value("defines").toArray(), [](const QJsonValue &v) {
          const auto d = v.toObject();
          return DefineInfo{ProjectExplorer::Macro::fromKeyValue(d.value("define").toString()), d.value("backtrace").toInt(-1),};
        }),
        o.value("sysroot").toString(),
      };
    });
  }
  {
    const auto backtraceGraph = root.value("backtraceGraph").toObject();
    t.backtraceGraph.files = transform<std::vector>(backtraceGraph.value("files").toArray(), [](const QJsonValue &v) {
      return v.toString();
    });
    t.backtraceGraph.commands = transform<std::vector>(backtraceGraph.value("commands").toArray(), [](const QJsonValue &v) { return v.toString(); });
    t.backtraceGraph.nodes = transform<std::vector>(backtraceGraph.value("nodes").toArray(), [](const QJsonValue &v) {
      const auto o = v.toObject();
      return BacktraceNode{o.value("file").toInt(-1), o.value("line").toInt(-1), o.value("command").toInt(-1), o.value("parent").toInt(-1),};
    });
  }

  return t;
}

static auto validateBacktraceGraph(const TargetDetails &t) -> int
{
  const auto backtraceFilesCount = static_cast<int>(t.backtraceGraph.files.size());
  const auto backtraceCommandsCount = static_cast<int>(t.backtraceGraph.commands.size());
  const auto backtraceNodeCount = static_cast<int>(t.backtraceGraph.nodes.size());

  auto topLevelNodeCount = 0;
  for (const auto &n : t.backtraceGraph.nodes) {
    if (n.parent == -1) {
      ++topLevelNodeCount;
    }
    if (n.file < 0 || n.file >= backtraceFilesCount) {
      qCWarning(cmakeFileApi) << "BacktraceNode: file index" << n.file << "is broken.";
      return -1;
    }
    if (n.command < -1 || n.command >= backtraceCommandsCount) {
      qCWarning(cmakeFileApi) << "BacktraceNode: command index" << n.command << "is broken.";
      return -1;
    }
    if (n.parent < -1 || n.parent >= backtraceNodeCount) {
      qCWarning(cmakeFileApi) << "BacktraceNode: parent index" << n.parent << "is broken.";
      return -1;
    }
  }

  if (topLevelNodeCount == 0 && backtraceNodeCount > 0) {
    // This is a forest, not a tree
    qCWarning(cmakeFileApi) << "BacktraceNode: Invalid number of top level nodes" << topLevelNodeCount;
    return -1;
  }

  return backtraceNodeCount;
}

static auto validateTargetDetails(const TargetDetails &t) -> bool
{
  // The part filled in by the codemodel file has already been covered!

  // Internal consistency of backtraceGraph:
  const auto backtraceCount = validateBacktraceGraph(t);
  if (backtraceCount < 0)
    return false;

  const auto sourcesCount = static_cast<int>(t.sources.size());
  const auto sourceGroupsCount = static_cast<int>(t.sourceGroups.size());
  const auto compileGroupsCount = static_cast<int>(t.compileGroups.size());

  if (t.backtrace < -1 || t.backtrace >= backtraceCount) {
    qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": backtrace index" << t.backtrace << "is broken.";
    return false;
  }
  for (const auto &id : t.installDestination) {
    if (id.backtrace < -1 || id.backtrace >= backtraceCount) {
      qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": backtrace index" << t.backtrace << "of install destination is broken.";
      return false;
    }
  }

  for (const auto &dep : t.dependencies) {
    if (dep.backtrace < -1 || dep.backtrace >= backtraceCount) {
      qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": backtrace index" << t.backtrace << "of dependency is broken.";
      return false;
    }
  }

  for (const auto &s : t.sources) {
    if (s.compileGroup < -1 || s.compileGroup >= compileGroupsCount) {
      qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": compile group index" << s.compileGroup << "of source info is broken.";
      return false;
    }
    if (s.sourceGroup < -1 || s.sourceGroup >= sourceGroupsCount) {
      qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": source group index" << s.sourceGroup << "of source info is broken.";
      return false;
    }
    if (s.backtrace < -1 || s.backtrace >= backtraceCount) {
      qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": backtrace index" << s.backtrace << "of source info is broken.";
      return false;
    }
  }

  for (const auto &cg : t.compileGroups) {
    for (auto s : cg.sources) {
      if (s < 0 || s >= sourcesCount) {
        qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": sources index" << s << "of compile group is broken.";
        return false;
      }
    }
    for (const auto &i : cg.includes) {
      if (i.backtrace < -1 || i.backtrace >= backtraceCount) {
        qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": includes/backtrace index" << i.backtrace << "of compile group is broken.";
        return false;
      }
    }
    for (const auto &d : cg.defines) {
      if (d.backtrace < -1 || d.backtrace >= backtraceCount) {
        qCWarning(cmakeFileApi) << "TargetDetails" << t.name << ": defines/backtrace index" << d.backtrace << "of compile group is broken.";
        return false;
      }
    }
  }

  return true;
}

static auto readTargetFile(const FilePath &targetFile, QString &errorMessage) -> TargetDetails
{
  const auto doc = readJsonFile(targetFile);
  const auto root = doc.object();

  auto result = extractTargetDetails(root, errorMessage);
  if (errorMessage.isEmpty() && !validateTargetDetails(result)) {
    errorMessage = QCoreApplication::translate("CMakeProjectManager::Internal", "Invalid target file generated by CMake: Broken indexes in target details.");
  }
  return result;
}

// --------------------------------------------------------------------
// ReplyFileContents:
// --------------------------------------------------------------------

auto FileApiDetails::ReplyFileContents::jsonFile(const QString &kind, const FilePath &replyDir) const -> FilePath
{
  const auto ro = findOrDefault(replies, equal(&ReplyObject::kind, kind));
  if (ro.file.isEmpty())
    return {};
  else
    return (replyDir / ro.file).absoluteFilePath();
}

// --------------------------------------------------------------------
// FileApi:
// --------------------------------------------------------------------

auto FileApiParser::setupCMakeFileApi(const FilePath &buildDirectory, Utils::FileSystemWatcher &watcher) -> bool
{
  // So that we have a directory to watch.
  buildDirectory.pathAppended(CMAKE_RELATIVE_REPLY_PATH).ensureWritableDir();

  auto queryDir = buildDirectory.pathAppended(CMAKE_RELATIVE_QUERY_PATH);
  queryDir.ensureWritableDir();

  if (!queryDir.exists()) {
    reportFileApiSetupFailure();
    return false;
  }
  QTC_ASSERT(queryDir.exists(), return false);

  auto failedBefore = false;
  for (const auto &filePath : cmakeQueryFilePaths(buildDirectory)) {
    const auto success = filePath.ensureExistingFile();
    if (!success && !failedBefore) {
      failedBefore = true;
      reportFileApiSetupFailure();
    }
  }

  watcher.addDirectory(cmakeReplyDirectory(buildDirectory).path(), FileSystemWatcher::WatchAllChanges);
  return true;
}

static auto uniqueTargetFiles(const Configuration &config) -> QStringList
{
  QSet<QString> knownIds;
  QStringList files;
  for (const auto &t : config.targets) {
    const int knownCount = knownIds.count();
    knownIds.insert(t.id);
    if (knownIds.count() > knownCount) {
      files.append(t.jsonFile);
    }
  }
  return files;
}

auto FileApiParser::parseData(QFutureInterface<std::shared_ptr<FileApiQtcData>> &fi, const FilePath &replyFilePath, const QString &cmakeBuildType, QString &errorMessage) -> FileApiData
{
  QTC_CHECK(errorMessage.isEmpty());
  QTC_CHECK(!replyFilePath.needsDevice());
  const auto replyDir = replyFilePath.parentDir();

  FileApiData result;

  const auto cancelCheck = [&fi, &errorMessage]() -> bool {
    if (fi.isCanceled()) {
      errorMessage = FileApiParser::tr("CMake parsing was cancelled.");
      return true;
    }
    return false;
  };

  result.replyFile = readReplyFile(replyFilePath, errorMessage);
  if (cancelCheck())
    return {};
  result.cache = readCacheFile(result.replyFile.jsonFile("cache", replyDir), errorMessage);
  if (cancelCheck())
    return {};
  result.cmakeFiles = readCMakeFilesFile(result.replyFile.jsonFile("cmakeFiles", replyDir), errorMessage);
  if (cancelCheck())
    return {};
  auto codeModels = readCodemodelFile(result.replyFile.jsonFile("codemodel", replyDir), errorMessage);

  if (codeModels.size() == 0) {
    errorMessage = "No CMake configuration found!";
    return result;
  }

  auto it = std::find_if(codeModels.cbegin(), codeModels.cend(), [cmakeBuildType](const Configuration &cfg) {
    return QString::compare(cfg.name, cmakeBuildType, Qt::CaseInsensitive) == 0;
  });
  if (it == codeModels.cend()) {
    QStringList buildTypes;
    for (const auto &cfg : codeModels)
      buildTypes << cfg.name;

    if (result.replyFile.isMultiConfig) {
      errorMessage = tr("No \"%1\" CMake configuration found. Available configurations: \"%2\".\n" "Make sure that CMAKE_CONFIGURATION_TYPES variable contains the \"Build type\" field.").arg(cmakeBuildType).arg(buildTypes.join(", "));
    } else {
      errorMessage = tr("No \"%1\" CMake configuration found. Available configuration: \"%2\".\n" "Make sure that CMAKE_BUILD_TYPE variable matches the \"Build type\" field.").arg(cmakeBuildType).arg(buildTypes.join(", "));
    }
    return result;
  }
  result.codemodel = std::move(*it);
  if (cancelCheck())
    return {};

  const auto targetFiles = uniqueTargetFiles(result.codemodel);

  for (const auto &targetFile : targetFiles) {
    if (cancelCheck())
      return {};
    QString targetErrorMessage;
    auto td = readTargetFile((replyDir / targetFile).absoluteFilePath(), targetErrorMessage);
    if (targetErrorMessage.isEmpty()) {
      result.targetDetails.emplace_back(std::move(td));
    } else {
      qWarning() << "Failed to retrieve target data from cmake fileapi:" << targetErrorMessage;
      errorMessage = targetErrorMessage;
    }
  }

  return result;
}

auto FileApiParser::scanForCMakeReplyFile(const FilePath &buildDirectory) -> FilePath
{
  const auto replyDir = cmakeReplyDirectory(buildDirectory);
  if (!replyDir.exists())
    return {};

  const auto entries = replyDir.dirEntries({{"index-*.json"}, QDir::Files}, QDir::Name);
  return entries.isEmpty() ? FilePath() : entries.first();
}

auto FileApiParser::cmakeQueryFilePaths(const FilePath &buildDirectory) -> FilePaths
{
  auto queryDir = buildDirectory / CMAKE_RELATIVE_QUERY_PATH;
  return transform(CMAKE_QUERY_FILENAMES, [&queryDir](const QString &name) {
    return queryDir.resolvePath(FilePath::fromString(name));
  });
}

} // namespace Internal
} // namespace CMakeProjectManager
