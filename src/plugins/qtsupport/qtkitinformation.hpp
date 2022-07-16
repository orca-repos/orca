// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include "baseqtversion.hpp"

#include <projectexplorer/kitinformation.hpp>

namespace Utils {
class MacroExpander;
}

namespace QtSupport {

class QTSUPPORT_EXPORT QtKitAspect : public ProjectExplorer::KitAspect {
  Q_OBJECT

public:
  QtKitAspect();

  auto setup(ProjectExplorer::Kit *k) -> void override;
  auto validate(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks override;
  auto fix(ProjectExplorer::Kit *) -> void override;
  auto createConfigWidget(ProjectExplorer::Kit *k) const -> ProjectExplorer::KitAspectWidget* override;
  auto displayNamePostfix(const ProjectExplorer::Kit *k) const -> QString override;
  auto toUserOutput(const ProjectExplorer::Kit *k) const -> ItemList override;
  auto addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const -> void override;
  auto createOutputParsers(const ProjectExplorer::Kit *k) const -> QList<Utils::OutputLineParser*> override;
  auto addToMacroExpander(ProjectExplorer::Kit *kit, Utils::MacroExpander *expander) const -> void override;

  static auto id() -> Utils::Id;
  static auto qtVersionId(const ProjectExplorer::Kit *k) -> int;
  static auto setQtVersionId(ProjectExplorer::Kit *k, const int id) -> void;
  static auto qtVersion(const ProjectExplorer::Kit *k) -> QtVersion*;
  static auto setQtVersion(ProjectExplorer::Kit *k, const QtVersion *v) -> void;
  static auto addHostBinariesToPath(const ProjectExplorer::Kit *k, Utils::Environment &env) -> void;
  static auto platformPredicate(Utils::Id availablePlatforms) -> ProjectExplorer::Kit::Predicate;
  static auto qtVersionPredicate(const QSet<Utils::Id> &required = QSet<Utils::Id>(), const QtVersionNumber &min = QtVersionNumber(0, 0, 0), const QtVersionNumber &max = QtVersionNumber(INT_MAX, INT_MAX, INT_MAX)) -> ProjectExplorer::Kit::Predicate;

  auto supportedPlatforms(const ProjectExplorer::Kit *k) const -> QSet<Utils::Id> override;
  auto availableFeatures(const ProjectExplorer::Kit *k) const -> QSet<Utils::Id> override;

private:
  auto weight(const ProjectExplorer::Kit *k) const -> int override;
  auto qtVersionsChanged(const QList<int> &addedIds, const QList<int> &removedIds, const QList<int> &changedIds) -> void;
  auto kitsWereLoaded() -> void;
};

class QTSUPPORT_EXPORT SuppliesQtQuickImportPath {
public:
  static auto id() -> Utils::Id;
};

class QTSUPPORT_EXPORT KitQmlImportPath {
public:
  static auto id() -> Utils::Id;
};

class QTSUPPORT_EXPORT KitHasMergedHeaderPathsWithQmlImportPaths {
public:
  static auto id() -> Utils::Id;
};

} // namespace QtSupport
