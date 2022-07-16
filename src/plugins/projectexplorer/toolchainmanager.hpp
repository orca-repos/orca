// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "toolchain.hpp"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

namespace Utils {
class FilePath;
}

namespace ProjectExplorer {

class ProjectExplorerPlugin;
class Abi;

class ToolchainDetectionSettings {
public:
  bool detectX64AsX32 = false;
};

// --------------------------------------------------------------------------
// ToolChainManager
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT ToolChainManager : public QObject {
  Q_OBJECT

public:
  ~ToolChainManager() override;

  static auto instance() -> ToolChainManager*;
  static auto toolchains() -> const Toolchains&;
  static auto toolchains(const ToolChain::Predicate &predicate) -> Toolchains;
  static auto toolChain(const ToolChain::Predicate &predicate) -> ToolChain*;
  static auto findToolChains(const Abi &abi) -> QList<ToolChain*>;
  static auto findToolChain(const QByteArray &id) -> ToolChain*;
  static auto isLoaded() -> bool;
  static auto registerToolChain(ToolChain *tc) -> bool;
  static auto deregisterToolChain(ToolChain *tc) -> void;
  static auto allLanguages() -> QList<Utils::Id>;
  static auto registerLanguage(const Utils::Id &language, const QString &displayName) -> bool;
  static auto displayNameOfLanguageId(const Utils::Id &id) -> QString;
  static auto isLanguageSupported(const Utils::Id &id) -> bool;
  static auto aboutToShutdown() -> void;
  static auto detectionSettings() -> ToolchainDetectionSettings;
  static auto setDetectionSettings(const ToolchainDetectionSettings &settings) -> void;
  static auto resetBadToolchains() -> void;
  static auto isBadToolchain(const Utils::FilePath &toolchain) -> bool;
  static auto addBadToolchain(const Utils::FilePath &toolchain) -> void;
  auto saveToolChains() -> void;

signals:
  auto toolChainAdded(ToolChain *) -> void;
  // Tool chain is still valid when this call happens!
  auto toolChainRemoved(ToolChain *) -> void;
  // Tool chain was updated.
  auto toolChainUpdated(ToolChain *) -> void;
  // Something changed.
  auto toolChainsChanged() -> void;
  //
  auto toolChainsLoaded() -> void;

private:
  explicit ToolChainManager(QObject *parent = nullptr);

  // Make sure the this is only called after all toolchain factories are registered!
  static auto restoreToolChains() -> void;
  static auto notifyAboutUpdate(ToolChain *) -> void;

  friend class ProjectExplorerPlugin; // for constructor
  friend class ToolChain;
};

} // namespace ProjectExplorer
