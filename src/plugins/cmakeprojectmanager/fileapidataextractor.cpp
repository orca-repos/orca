// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileapidataextractor.hpp"

#include "fileapiparser.hpp"
#include "projecttreehelper.hpp"

#include <cppeditor/cppeditorconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/utilsicons.hpp>

#include <projectexplorer/projecttree.hpp>

#include <QDir>

using namespace ProjectExplorer;
using namespace Utils;

namespace {

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using namespace CMakeProjectManager::Internal::FileApiDetails;

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

class CMakeFileResult {
public:
  QSet<CMakeFileInfo> cmakeFiles;

  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesSource;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesBuild;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesOther;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeListNodes;
};

auto extractCMakeFilesData(const std::vector<CMakeFileInfo> &cmakefiles, const FilePath &sourceDirectory, const FilePath &buildDirectory) -> CMakeFileResult
{
  CMakeFileResult result;

  for (const auto &info : cmakefiles) {
    const auto sfn = sourceDirectory.resolvePath(info.path);
    const int oldCount = result.cmakeFiles.count();
    auto absolute(info);
    absolute.path = sfn;
    result.cmakeFiles.insert(absolute);
    if (oldCount < result.cmakeFiles.count()) {
      if (info.isCMake && !info.isCMakeListsDotTxt) {
        // Skip files that cmake considers to be part of the installation -- but include
        // CMakeLists.txt files. This fixes cmake binaries running from their own
        // build directory.
        continue;
      }

      auto node = std::make_unique<FileNode>(sfn, FileType::Project);
      node->setIsGenerated(info.isGenerated && !info.isCMakeListsDotTxt); // CMakeLists.txt are never
      // generated, independent
      // what cmake thinks:-)

      if (info.isCMakeListsDotTxt) {
        result.cmakeListNodes.emplace_back(std::move(node));
      } else if (sfn.isChildOf(sourceDirectory)) {
        result.cmakeNodesSource.emplace_back(std::move(node));
      } else if (sfn.isChildOf(buildDirectory)) {
        result.cmakeNodesBuild.emplace_back(std::move(node));
      } else {
        result.cmakeNodesOther.emplace_back(std::move(node));
      }
    }
  }

  return result;
}

class PreprocessedData {
public:
  CMakeProjectManager::CMakeConfig cache;

  QSet<CMakeFileInfo> cmakeFiles;

  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesSource;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesBuild;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeNodesOther;
  std::vector<std::unique_ptr<ProjectExplorer::FileNode>> cmakeListNodes;

  Configuration codemodel;
  std::vector<TargetDetails> targetDetails;
};

auto preprocess(FileApiData &data, const FilePath &sourceDirectory, const FilePath &buildDirectory, QString &errorMessage) -> PreprocessedData
{
  Q_UNUSED(errorMessage)

  PreprocessedData result;

  result.cache = std::move(data.cache); // Make sure this is available, even when nothing else is

  result.codemodel = std::move(data.codemodel);

  auto cmakeFileResult = extractCMakeFilesData(data.cmakeFiles, sourceDirectory, buildDirectory);

  result.cmakeFiles = std::move(cmakeFileResult.cmakeFiles);
  result.cmakeNodesSource = std::move(cmakeFileResult.cmakeNodesSource);
  result.cmakeNodesBuild = std::move(cmakeFileResult.cmakeNodesBuild);
  result.cmakeNodesOther = std::move(cmakeFileResult.cmakeNodesOther);
  result.cmakeListNodes = std::move(cmakeFileResult.cmakeListNodes);

  result.targetDetails = std::move(data.targetDetails);

  return result;
}

auto extractBacktraceInformation(const BacktraceInfo &backtraces, const QDir &sourceDir, int backtraceIndex, unsigned int locationInfoPriority) -> QVector<FolderNode::LocationInfo>
{
  QVector<FolderNode::LocationInfo> info;
  // Set up a default target path:
  while (backtraceIndex != -1) {
    const auto bi = static_cast<size_t>(backtraceIndex);
    QTC_ASSERT(bi < backtraces.nodes.size(), break);
    const auto &btNode = backtraces.nodes[bi];
    backtraceIndex = btNode.parent; // advance to next node

    const auto fileIndex = static_cast<size_t>(btNode.file);
    QTC_ASSERT(fileIndex < backtraces.files.size(), break);
    const auto path = FilePath::fromString(sourceDir.absoluteFilePath(backtraces.files[fileIndex]));

    if (btNode.command < 0) {
      // No command, skip: The file itself is already covered:-)
      continue;
    }

    const auto commandIndex = static_cast<size_t>(btNode.command);
    QTC_ASSERT(commandIndex < backtraces.commands.size(), break);

    const auto command = backtraces.commands[commandIndex];

    info.append(FolderNode::LocationInfo(command, path, btNode.line, locationInfoPriority));
  }
  return info;
}

static auto isChildOf(const FilePath &path, const QStringList &prefixes) -> bool
{
  for (const auto &prefix : prefixes)
    if (path.isChildOf(FilePath::fromString(prefix)))
      return true;
  return false;
}

auto generateBuildTargets(const PreprocessedData &input, const FilePath &sourceDirectory, const FilePath &buildDirectory, bool haveLibrariesRelativeToBuildDirectory) -> QList<CMakeBuildTarget>
{
  QDir sourceDir(sourceDirectory.toString());

  const auto result = transform<QList>(input.targetDetails, [&sourceDir, &sourceDirectory, &buildDirectory, &haveLibrariesRelativeToBuildDirectory](const TargetDetails &t) {
    const auto currentBuildDir = buildDirectory.resolvePath(t.buildDir);

    CMakeBuildTarget ct;
    ct.title = t.name;
    if (!t.artifacts.isEmpty())
      ct.executable = buildDirectory.resolvePath(t.artifacts.at(0));
    auto type = UtilityType;
    if (t.type == "EXECUTABLE")
      type = ExecutableType;
    else if (t.type == "STATIC_LIBRARY")
      type = StaticLibraryType;
    else if (t.type == "OBJECT_LIBRARY")
      type = ObjectLibraryType;
    else if (t.type == "MODULE_LIBRARY" || t.type == "SHARED_LIBRARY")
      type = DynamicLibraryType;
    else
      type = UtilityType;
    ct.targetType = type;
    ct.workingDirectory = ct.executable.isEmpty() ? currentBuildDir.absolutePath() : ct.executable.parentDir();
    ct.sourceDirectory = sourceDirectory.resolvePath(t.sourceDir);

    ct.backtrace = extractBacktraceInformation(t.backtraceGraph, sourceDir, t.backtrace, 0);

    for (const auto &d : t.dependencies) {
      ct.dependencyDefinitions.append(extractBacktraceInformation(t.backtraceGraph, sourceDir, d.backtrace, 100));
    }
    for (const auto &si : t.sources) {
      ct.sourceDefinitions.append(extractBacktraceInformation(t.backtraceGraph, sourceDir, si.backtrace, 200));
    }
    for (const auto &ci : t.compileGroups) {
      for (const auto &ii : ci.includes) {
        ct.includeDefinitions.append(extractBacktraceInformation(t.backtraceGraph, sourceDir, ii.backtrace, 300));
      }
      for (const auto &di : ci.defines) {
        ct.defineDefinitions.append(extractBacktraceInformation(t.backtraceGraph, sourceDir, di.backtrace, 400));
      }
    }
    for (const auto &id : t.installDestination) {
      ct.installDefinitions.append(extractBacktraceInformation(t.backtraceGraph, sourceDir, id.backtrace, 500));
    }

    if (ct.targetType == ExecutableType) {
      Utils::FilePaths librarySeachPaths;
      // Is this a GUI application?
      ct.linksToQtGui = Utils::contains(t.link.value().fragments, [](const FragmentInfo &f) {
        return f.role == "libraries" && (f.fragment.contains("QtGui") || f.fragment.contains("Qt5Gui") || f.fragment.contains("Qt6Gui"));
      });

      ct.qtcRunnable = t.folderTargetProperty == "qtc_runnable";

      // Extract library directories for executables:
      for (const auto &f : t.link.value().fragments) {
        if (f.role == "flags") // ignore all flags fragments
          continue;

        // CMake sometimes mixes several shell-escaped pieces into one fragment. Disentangle that again:
        const auto parts = ProcessArgs::splitArgs(f.fragment);
        for (auto part : parts) {
          // Library search paths that are added with target_link_directories are added as
          // -LIBPATH:... (Windows/MSVC), or
          // -L... (Unix/GCC)
          // with role "libraryPath"
          if (f.role == "libraryPath") {
            if (part.startsWith("-LIBPATH:"))
              part = part.mid(9);
            else if (part.startsWith("-L"))
              part = part.mid(2);
          }

          // Some projects abuse linking to libraries to pass random flags to the linker, so ignore
          // flags mixed into a fragment
          if (part.startsWith("-"))
            continue;

          const auto buildDir = haveLibrariesRelativeToBuildDirectory ? buildDirectory : currentBuildDir;
          auto tmp = buildDir.resolvePath(FilePath::fromUserInput(part));

          if (f.role == "libraries")
            tmp = tmp.parentDir();

          if (!tmp.isEmpty() && tmp.isDir()) {
            // f.role is libraryPath or frameworkPath
            // On Linux, exclude sub-paths from "/lib(64)", "/usr/lib(64)" and
            // "/usr/local/lib" since these are usually in the standard search
            // paths. There probably are more, but the naming schemes are arbitrary
            // so we'd need to ask the linker ("ld --verbose | grep SEARCH_DIR").
            if (!HostOsInfo::isLinuxHost() || !isChildOf(tmp, {"/lib", "/lib64", "/usr/lib", "/usr/lib64", "/usr/local/lib"})) {
              librarySeachPaths.append(tmp);
              // Libraries often have their import libs in ../lib and the
              // actual dll files in ../bin on windows. Qt is one example of that.
              if (tmp.fileName() == "lib" && HostOsInfo::isWindowsHost()) {
                const auto path = tmp.parentDir().pathAppended("bin");

                if (path.isDir())
                  librarySeachPaths.append(path);
              }
            }
          }
        }
      }
      ct.libraryDirectories = filteredUnique(librarySeachPaths);
    }

    return ct;
  });
  return result;
}

static auto splitFragments(const QStringList &fragments) -> QStringList
{
  QStringList result;
  for (const auto &f : fragments) {
    result += ProcessArgs::splitArgs(f);
  }
  return result;
}

auto isPchFile(const FilePath &buildDirectory, const FilePath &path) -> bool
{
  return path.isChildOf(buildDirectory) && path.fileName().startsWith("cmake_pch");
}

auto generateRawProjectParts(const PreprocessedData &input, const FilePath &sourceDirectory, const FilePath &buildDirectory) -> RawProjectParts
{
  RawProjectParts rpps;

  auto counter = 0;
  for (const auto &t : input.targetDetails) {
    QDir sourceDir(sourceDirectory.toString());
    auto needPostfix = t.compileGroups.size() > 1;
    auto count = 1;
    for (const auto &ci : t.compileGroups) {
      if (ci.language != "C" && ci.language != "CXX" && ci.language != "CUDA")
        continue; // No need to bother the C++ codemodel

      // CMake users worked around Creator's inability of listing header files by creating
      // custom targets with all the header files. This target breaks the code model, so
      // keep quiet about it:-)
      if (ci.defines.empty() && ci.includes.empty() && allOf(ci.sources, [t](const int sid) {
        const auto &source = t.sources[static_cast<size_t>(sid)];
        return Node::fileTypeForFileName(FilePath::fromString(source.path)) == FileType::Header;
      })) {
        qWarning() << "Not reporting all-header compilegroup of target" << t.name << "to code model.";
        continue;
      }

      QString ending;
      QString qtcPchFile;
      if (ci.language == "C") {
        ending = "/cmake_pch.hpp";
        qtcPchFile = "qtc_cmake_pch.hpp";
      } else if (ci.language == "CXX") {
        ending = "/cmake_pch.hxx";
        qtcPchFile = "qtc_cmake_pch.hxx";
      }

      ++counter;
      RawProjectPart rpp;
      rpp.setProjectFileLocation(t.sourceDir.pathAppended("CMakeLists.txt").toString());
      rpp.setBuildSystemTarget(t.name);
      const auto postfix = needPostfix ? "_cg" + QString::number(count) : QString();
      rpp.setDisplayName(t.id + postfix);
      rpp.setMacros(transform<QVector>(ci.defines, &DefineInfo::define));
      rpp.setHeaderPaths(transform<QVector>(ci.includes, &IncludeInfo::path));

      auto fragments = splitFragments(ci.fragments);

      // Get all sources from the compiler group, except generated sources
      QStringList sources;
      for (auto idx : ci.sources) {
        auto si = t.sources.at(idx);
        if (si.isGenerated)
          continue;
        sources.push_back(sourceDir.absoluteFilePath(si.path));
      }

      // If we are not in a pch compiler group, add all the headers that are not generated
      const auto hasPchSource = anyOf(sources, [buildDirectory](const QString &path) {
        return isPchFile(buildDirectory, FilePath::fromString(path));
      });
      if (!hasPchSource) {
        QString headerMimeType;
        if (ci.language == "C")
          headerMimeType = CppEditor::Constants::C_HEADER_MIMETYPE;
        else if (ci.language == "CXX")
          headerMimeType = CppEditor::Constants::CPP_HEADER_MIMETYPE;

        for (const auto &si : t.sources) {
          if (si.isGenerated)
            continue;
          const auto mimeTypes = Utils::mimeTypesForFileName(si.path);
          for (auto mime : mimeTypes)
            if (mime.name() == headerMimeType)
              sources.push_back(sourceDir.absoluteFilePath(si.path));
        }
      }

      // Set project files except pch files
      rpp.setFiles(Utils::filtered(sources, [buildDirectory](const QString &path) {
        return !isPchFile(buildDirectory, FilePath::fromString(path));
      }));

      auto precompiled_header = FilePath::fromString(findOrDefault(t.sources, [&ending](const SourceInfo &si) {
        return si.path.endsWith(ending);
      }).path);
      if (!precompiled_header.isEmpty()) {
        if (precompiled_header.toFileInfo().isRelative()) {
          const auto parentDir = FilePath::fromString(sourceDir.absolutePath());
          precompiled_header = parentDir.pathAppended(precompiled_header.toString());
        }

        // Remove the CMake PCH usage command line options in order to avoid the case
        // when the build system would produce a .pch/.gch file that would be treated
        // by the Clang code model as its own and fail.
        auto remove = [&](const QStringList &args) {
          auto foundPos = std::search(fragments.begin(), fragments.end(), args.begin(), args.end());
          if (foundPos != fragments.end())
            fragments.erase(foundPos, std::next(foundPos, args.size()));
        };

        remove({"-Xclang", "-include-pch", "-Xclang", precompiled_header.toString() + ".gch"});
        remove({"-Xclang", "-include-pch", "-Xclang", precompiled_header.toString() + ".pch"});
        remove({"-Xclang", "-include", "-Xclang", precompiled_header.toString()});
        remove({"-include", precompiled_header.toString()});
        remove({"/FI", precompiled_header.toString()});

        // Make a copy of the CMake PCH header and use it instead
        auto qtc_precompiled_header = precompiled_header.parentDir().pathAppended(qtcPchFile);
        FileUtils::copyIfDifferent(precompiled_header, qtc_precompiled_header);

        rpp.setPreCompiledHeaders({qtc_precompiled_header.toString()});
      }

      RawProjectPartFlags cProjectFlags;
      cProjectFlags.commandLineFlags = fragments;
      rpp.setFlagsForC(cProjectFlags);

      RawProjectPartFlags cxxProjectFlags;
      cxxProjectFlags.commandLineFlags = cProjectFlags.commandLineFlags;
      rpp.setFlagsForCxx(cxxProjectFlags);

      const auto isExecutable = t.type == "EXECUTABLE";
      rpp.setBuildTargetType(isExecutable ? ProjectExplorer::BuildTargetType::Executable : ProjectExplorer::BuildTargetType::Library);
      rpps.append(rpp);
      ++count;
    }
  }

  return rpps;
}

auto directorySourceDir(const Configuration &c, const FilePath &sourceDir, int directoryIndex) -> FilePath
{
  const auto di = static_cast<size_t>(directoryIndex);
  QTC_ASSERT(di < c.directories.size(), return FilePath());

  return sourceDir.resolvePath(c.directories[di].sourcePath).cleanPath();
}

auto directoryBuildDir(const Configuration &c, const FilePath &buildDir, int directoryIndex) -> FilePath
{
  const auto di = static_cast<size_t>(directoryIndex);
  QTC_ASSERT(di < c.directories.size(), return FilePath());

  return buildDir.resolvePath(c.directories[di].buildPath).cleanPath();
}

auto addProjects(const QHash<Utils::FilePath, ProjectNode*> &cmakeListsNodes, const Configuration &config, const FilePath &sourceDir) -> void
{
  for (const auto &p : config.projects) {
    if (p.parent == -1)
      continue; // Top-level project has already been covered
    auto dir = directorySourceDir(config, sourceDir, p.directories[0]);
    createProjectNode(cmakeListsNodes, dir, p.name);
  }
}

auto createSourceGroupNode(const QString &sourceGroupName, const FilePath &sourceDirectory, FolderNode *targetRoot) -> FolderNode*
{
  auto currentNode = targetRoot;

  if (!sourceGroupName.isEmpty()) {
    const auto parts = sourceGroupName.split("\\");

    for (const auto &p : parts) {
      auto existingNode = Utils::findOrDefault(currentNode->folderNodes(), [&p](const FolderNode *fn) {
        return fn->displayName() == p;
      });

      if (!existingNode) {
        auto node = createCMakeVFolder(sourceDirectory, Node::DefaultFolderPriority + 5, p);
        node->setListInProject(false);
        node->setIcon([] { return QIcon::fromTheme("edit-copy", ::Utils::Icons::COPY.icon()); });

        existingNode = node.get();

        currentNode->addNode(std::move(node));
      }

      currentNode = existingNode;
    }
  }
  return currentNode;
}

auto addCompileGroups(ProjectNode *targetRoot, const Utils::FilePath &topSourceDirectory, const Utils::FilePath &sourceDirectory, const Utils::FilePath &buildDirectory, const TargetDetails &td) -> void
{
  const auto inSourceBuild = (sourceDirectory == buildDirectory);

  std::vector<std::unique_ptr<FileNode>> toList;
  QSet<Utils::FilePath> alreadyListed;

  // Files already added by other configurations:
  targetRoot->forEachGenericNode([&alreadyListed](const Node *n) { alreadyListed.insert(n->filePath()); });

  std::vector<std::unique_ptr<FileNode>> buildFileNodes;
  std::vector<std::unique_ptr<FileNode>> otherFileNodes;
  std::vector<std::vector<std::unique_ptr<FileNode>>> sourceGroupFileNodes{td.sourceGroups.size()};

  for (const auto &si : td.sources) {
    const auto sourcePath = topSourceDirectory.resolvePath(si.path).cleanPath();

    // Filter out already known files:
    const int count = alreadyListed.count();
    alreadyListed.insert(sourcePath);
    if (count == alreadyListed.count())
      continue;

    // Create FileNodes from the file
    auto node = std::make_unique<FileNode>(sourcePath, Node::fileTypeForFileName(sourcePath));
    node->setIsGenerated(si.isGenerated);

    // CMake pch files are generated at configured time, but not marked as generated
    // so that a "clean" step won't remove them and at a subsequent build they won't exist.
    if (isPchFile(buildDirectory, sourcePath))
      node->setIsGenerated(true);

    // Where does the file node need to go?
    if (sourcePath.isChildOf(buildDirectory) && !inSourceBuild) {
      buildFileNodes.emplace_back(std::move(node));
    } else if (sourcePath.isChildOf(sourceDirectory)) {
      sourceGroupFileNodes[si.sourceGroup].emplace_back(std::move(node));
    } else {
      otherFileNodes.emplace_back(std::move(node));
    }
  }

  // Calculate base directory for source groups:
  for (size_t i = 0; i < sourceGroupFileNodes.size(); ++i) {
    auto &current = sourceGroupFileNodes[i];
    FilePath baseDirectory;
    // All the sourceGroupFileNodes are below sourceDirectory, so this is safe:
    for (const auto &fn : current) {
      if (baseDirectory.isEmpty()) {
        baseDirectory = fn->filePath().parentDir();
      } else {
        baseDirectory = Utils::FileUtils::commonPath(baseDirectory, fn->filePath());
      }
    }

    auto insertNode = createSourceGroupNode(td.sourceGroups[i], baseDirectory, targetRoot);
    insertNode->addNestedNodes(std::move(current), baseDirectory);
  }

  addCMakeVFolder(targetRoot, buildDirectory, 100, QCoreApplication::translate("CMakeProjectManager::Internal::FileApi", "<Build Directory>"), std::move(buildFileNodes));
  addCMakeVFolder(targetRoot, Utils::FilePath(), 10, QCoreApplication::translate("CMakeProjectManager::Internal::FileApi", "<Other Locations>"), std::move(otherFileNodes));
}

auto addTargets(const QHash<Utils::FilePath, ProjectExplorer::ProjectNode*> &cmakeListsNodes, const Configuration &config, const std::vector<TargetDetails> &targetDetails, const FilePath &sourceDir, const FilePath &buildDir) -> void
{
  QHash<QString, const TargetDetails*> targetDetailsHash;
  for (const auto &t : targetDetails)
    targetDetailsHash.insert(t.id, &t);
  const TargetDetails defaultTargetDetails;
  auto getTargetDetails = [&targetDetailsHash, &defaultTargetDetails](const QString &id) -> const TargetDetails& {
    auto it = targetDetailsHash.constFind(id);
    if (it != targetDetailsHash.constEnd())
      return *it.value();
    return defaultTargetDetails;
  };

  for (const auto &t : config.targets) {
    const auto &td = getTargetDetails(t.id);

    const auto dir = directorySourceDir(config, sourceDir, t.directory);

    auto tNode = createTargetNode(cmakeListsNodes, dir, t.name);
    QTC_ASSERT(tNode, continue);

    tNode->setTargetInformation(td.artifacts, td.type);
    tNode->setBuildDirectory(directoryBuildDir(config, buildDir, t.directory));

    addCompileGroups(tNode, sourceDir, dir, tNode->buildDirectory(), td);
  }
}

auto generateRootProjectNode(PreprocessedData &data, const FilePath &sourceDirectory, const FilePath &buildDirectory) -> std::unique_ptr<CMakeProjectNode>
{
  auto result = std::make_unique<CMakeProjectNode>(sourceDirectory);

  const auto topLevelProject = findOrDefault(data.codemodel.projects, equal(&FileApiDetails::Project::parent, -1));
  if (!topLevelProject.name.isEmpty())
    result->setDisplayName(topLevelProject.name);
  else
    result->setDisplayName(sourceDirectory.fileName());

  auto cmakeListsNodes = addCMakeLists(result.get(), std::move(data.cmakeListNodes));
  data.cmakeListNodes.clear(); // Remove all the nullptr in the vector...

  addProjects(cmakeListsNodes, data.codemodel, sourceDirectory);

  addTargets(cmakeListsNodes, data.codemodel, data.targetDetails, sourceDirectory, buildDirectory);

  if (!data.cmakeNodesSource.empty() || !data.cmakeNodesBuild.empty() || !data.cmakeNodesOther.empty())
    addCMakeInputs(result.get(), sourceDirectory, buildDirectory, std::move(data.cmakeNodesSource), std::move(data.cmakeNodesBuild), std::move(data.cmakeNodesOther));

  data.cmakeNodesSource.clear(); // Remove all the nullptr in the vector...
  data.cmakeNodesBuild.clear();  // Remove all the nullptr in the vector...
  data.cmakeNodesOther.clear();  // Remove all the nullptr in the vector...

  return result;
}

auto setupLocationInfoForTargets(CMakeProjectNode *rootNode, const QList<CMakeBuildTarget> &targets) -> void
{
  const auto titles = Utils::transform<QSet>(targets, &CMakeBuildTarget::title);
  QHash<QString, FolderNode*> buildKeyToNode;
  rootNode->forEachGenericNode([&buildKeyToNode, &titles](Node *node) {
    auto folderNode = node->asFolderNode();
    const auto &buildKey = node->buildKey();
    if (folderNode && titles.contains(buildKey))
      buildKeyToNode.insert(buildKey, folderNode);
  });
  for (const auto &t : targets) {
    auto folderNode = buildKeyToNode.value(t.title);
    if (folderNode) {
      QSet<std::pair<FilePath, int>> locations;
      auto dedup = [&locations](const Backtrace &bt) {
        QVector<FolderNode::LocationInfo> result;
        for (const auto &i : bt) {
          int count = locations.count();
          locations.insert(std::make_pair(i.path, i.line));
          if (count != locations.count()) {
            result.append(i);
          }
        }
        return result;
      };

      auto result = dedup(t.backtrace);
      auto dedupMulti = [&dedup](const Backtraces &bts) {
        QVector<FolderNode::LocationInfo> result;
        for (const auto &bt : bts) {
          result.append(dedup(bt));
        }
        return result;
      };
      result += dedupMulti(t.dependencyDefinitions);
      result += dedupMulti(t.includeDefinitions);
      result += dedupMulti(t.defineDefinitions);
      result += dedupMulti(t.sourceDefinitions);
      result += dedupMulti(t.installDefinitions);

      folderNode->setLocationInfo(result);
    }
  }
}

} // namespace

namespace CMakeProjectManager {
namespace Internal {

using namespace FileApiDetails;

// --------------------------------------------------------------------
// extractData:
// --------------------------------------------------------------------

auto extractData(FileApiData &input, const FilePath &sourceDirectory, const FilePath &buildDirectory) -> FileApiQtcData
{
    FileApiQtcData result;

    // Preprocess our input:
    auto data = preprocess(input, sourceDirectory, buildDirectory, result.errorMessage);
    result.cache = std::move(data.cache); // Make sure this is available, even when nothing else is
    if (!result.errorMessage.isEmpty()) {
        return {};
    }

    // Ninja generator from CMake version 3.20.5 has libraries relative to build directory
    const auto haveLibrariesRelativeToBuildDirectory =
            input.replyFile.generator.startsWith("Ninja")
         && input.replyFile.cmakeVersion >= QVersionNumber(3, 20, 5);

    result.buildTargets = generateBuildTargets(data, sourceDirectory, buildDirectory, haveLibrariesRelativeToBuildDirectory);
    result.cmakeFiles = std::move(data.cmakeFiles);
    result.projectParts = generateRawProjectParts(data, sourceDirectory, buildDirectory);

    auto rootProjectNode = generateRootProjectNode(data, sourceDirectory, buildDirectory);
    ProjectTree::applyTreeManager(rootProjectNode.get(), ProjectTree::AsyncPhase); // QRC nodes
    result.rootProjectNode = std::move(rootProjectNode);

    setupLocationInfoForTargets(result.rootProjectNode.get(), result.buildTargets);

    result.ctestPath = input.replyFile.ctestExecutable;
    result.isMultiConfig = input.replyFile.isMultiConfig;
    if (input.replyFile.isMultiConfig && input.replyFile.generator != "Ninja Multi-Config")
        result.usesAllCapsTargets = true;

    return result;
}

} // namespace Internal
} // namespace CMakeProjectManager
