// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "toolchain.hpp"

#include <utils/fileutils.hpp>

namespace ProjectExplorer {

class BuildInfo;
class Kit;
class Project;
class Target;
class ToolChain;

class PROJECTEXPLORER_EXPORT ProjectImporter : public QObject {
  Q_OBJECT

public:
  struct ToolChainData {
    QList<ToolChain*> tcs;
    bool areTemporary = false;
  };

  ProjectImporter(const Utils::FilePath &path);
  ~ProjectImporter() override;

  auto projectFilePath() const -> const Utils::FilePath { return m_projectPath; }
  auto projectDirectory() const -> const Utils::FilePath { return m_projectPath.parentDir(); }
  virtual auto import(const Utils::FilePath &importPath, bool silent = false) -> const QList<BuildInfo>;
  virtual auto importCandidates() -> QStringList = 0;
  virtual auto preferredTarget(const QList<Target*> &possibleTargets) -> Target*;
  auto isUpdating() const -> bool { return m_isUpdating; }
  auto makePersistent(Kit *k) const -> void;
  auto cleanupKit(Kit *k) const -> void;
  auto isTemporaryKit(Kit *k) const -> bool;
  auto addProject(Kit *k) const -> void;
  auto removeProject(Kit *k) const -> void;

protected:
  class UpdateGuard {
  public:
    UpdateGuard(const ProjectImporter &i) : m_importer(i)
    {
      m_wasUpdating = m_importer.isUpdating();
      m_importer.m_isUpdating = true;
    }

    ~UpdateGuard() { m_importer.m_isUpdating = m_wasUpdating; }

  private:
    const ProjectImporter &m_importer;
    bool m_wasUpdating;
  };

  // importPath is an existing directory at this point!
  virtual auto examineDirectory(const Utils::FilePath &importPath, QString *warningMessage) const -> QList<void*> = 0;
  // will get one of the results from examineDirectory
  virtual auto matchKit(void *directoryData, const Kit *k) const -> bool = 0;
  // will get one of the results from examineDirectory
  virtual auto createKit(void *directoryData) const -> Kit* = 0;
  // will get one of the results from examineDirectory
  virtual auto buildInfoList(void *directoryData) const -> const QList<BuildInfo> = 0;
  virtual auto deleteDirectoryData(void *directoryData) const -> void = 0;
  using KitSetupFunction = std::function<void(Kit *)>;
  auto createTemporaryKit(const KitSetupFunction &setup) const -> Kit*;
  // Handle temporary additions to Kits (Qt Versions, ToolChains, etc.)
  using CleanupFunction = std::function<void(Kit *, const QVariantList &)>;
  using PersistFunction = std::function<void(Kit *, const QVariantList &)>;
  auto useTemporaryKitAspect(Utils::Id id, CleanupFunction cleanup, PersistFunction persist) -> void;
  auto addTemporaryData(Utils::Id id, const QVariant &cleanupData, Kit *k) const -> void;
  // Does *any* kit feature the requested data yet?
  auto hasKitWithTemporaryData(Utils::Id id, const QVariant &data) const -> bool;
  auto findOrCreateToolChains(const ToolChainDescription &tcd) const -> ToolChainData;

private:
  auto markKitAsTemporary(Kit *k) const -> void;
  auto findTemporaryHandler(Utils::Id id) const -> bool;
  auto cleanupTemporaryToolChains(Kit *k, const QVariantList &vl) -> void;
  auto persistTemporaryToolChains(Kit *k, const QVariantList &vl) -> void;

  const Utils::FilePath m_projectPath;
  mutable bool m_isUpdating = false;

  class TemporaryInformationHandler {
  public:
    Utils::Id id;
    CleanupFunction cleanup;
    PersistFunction persist;
  };

  QList<TemporaryInformationHandler> m_temporaryHandlers;

  friend class UpdateGuard;
};

} // namespace ProjectExplorer
