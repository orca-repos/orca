// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "task.hpp"

#include <core/featureprovider.hpp>

#include <QSet>
#include <QVariant>

#include <memory>

namespace Utils {
class Environment;
class MacroExpander;
class OutputLineParser;
} // namespace Utils

namespace ProjectExplorer {

namespace Internal {
class KitManagerPrivate;
class KitModel;
class KitPrivate;
} // namespace Internal

/**
 * @brief The Kit class
 *
 * The kit holds a set of values defining a system targeted by the software
 * under development.
 */
class PROJECTEXPLORER_EXPORT Kit {
public:
  using Predicate = std::function<bool(const Kit *)>;
  static auto defaultPredicate() -> Predicate;

  explicit Kit(Utils::Id id = Utils::Id());
  explicit Kit(const QVariantMap &data);
  ~Kit();

  // Do not trigger evaluations
  auto blockNotification() -> void;
  // Trigger evaluations again.
  auto unblockNotification() -> void;
  auto isValid() const -> bool;
  auto hasWarning() const -> bool;
  auto validate() const -> Tasks;
  auto fix() -> void; // Fix the individual kit information: Make sure it contains a valid value.
  // Fix will not look at other information in the kit!
  auto setup() -> void;   // Apply advanced magic(TM). Used only once on each kit during initial setup.
  auto upgrade() -> void; // Upgrade settings to new syntax (if appropriate).
  auto unexpandedDisplayName() const -> QString;
  auto displayName() const -> QString;
  auto setUnexpandedDisplayName(const QString &name) -> void;
  auto fileSystemFriendlyName() const -> QString;
  auto customFileSystemFriendlyName() const -> QString;
  auto setCustomFileSystemFriendlyName(const QString &fileSystemFriendlyName) -> void;
  auto isAutoDetected() const -> bool;
  auto autoDetectionSource() const -> QString;
  auto isSdkProvided() const -> bool;
  auto id() const -> Utils::Id;

  // The higher the weight, the more aspects have sensible values for this kit.
  // For instance, a kit where a matching debugger was found for the toolchain will have a
  // higher weight than one whose toolchain does not match a known debugger, assuming
  // all other aspects are equal.
  auto weight() const -> int;
  auto icon() const -> QIcon;        // Raw device icon, independent of warning or error.
  auto displayIcon() const -> QIcon; // Error or warning or device icon.
  auto iconPath() const -> Utils::FilePath;
  auto setIconPath(const Utils::FilePath &path) -> void;
  auto setDeviceTypeForIcon(Utils::Id deviceType) -> void;
  auto allKeys() const -> QList<Utils::Id>;
  auto value(Utils::Id key, const QVariant &unset = QVariant()) const -> QVariant;
  auto hasValue(Utils::Id key) const -> bool;
  auto setValue(Utils::Id key, const QVariant &value) -> void;
  auto setValueSilently(Utils::Id key, const QVariant &value) -> void;
  auto removeKey(Utils::Id key) -> void;
  auto removeKeySilently(Utils::Id key) -> void;
  auto isSticky(Utils::Id id) const -> bool;
  auto isDataEqual(const Kit *other) const -> bool;
  auto isEqual(const Kit *other) const -> bool;
  auto addToBuildEnvironment(Utils::Environment &env) const -> void;
  auto buildEnvironment() const -> Utils::Environment;
  auto addToRunEnvironment(Utils::Environment &env) const -> void;
  auto runEnvironment() const -> Utils::Environment;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*>;
  auto toHtml(const Tasks &additional = Tasks(), const QString &extraText = QString()) const -> QString;
  auto clone(bool keepName = false) const -> Kit*;
  auto copyFrom(const Kit *k) -> void;

  // Note: Stickyness is *not* saved!
  auto setAutoDetected(bool detected) -> void;
  auto setAutoDetectionSource(const QString &autoDetectionSource) -> void;
  auto makeSticky() -> void;
  auto setSticky(Utils::Id id, bool b) -> void;
  auto makeUnSticky() -> void;
  auto setMutable(Utils::Id id, bool b) -> void;
  auto isMutable(Utils::Id id) const -> bool;
  auto makeReplacementKit() -> void;
  auto isReplacementKit() const -> bool;
  auto setIrrelevantAspects(const QSet<Utils::Id> &irrelevant) -> void;
  auto irrelevantAspects() const -> QSet<Utils::Id>;
  auto supportedPlatforms() const -> QSet<Utils::Id>;
  auto availableFeatures() const -> QSet<Utils::Id>;
  auto hasFeatures(const QSet<Utils::Id> &features) const -> bool;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto newKitName(const QList<Kit*> &allKits) const -> QString;
  static auto newKitName(const QString &name, const QList<Kit*> &allKits) -> QString;

private:
  static auto copyKitCommon(Kit *target, const Kit *source) -> void;
  auto setSdkProvided(bool sdkProvided) -> void;

  Kit(const Kit &other) = delete;
  auto operator=(const Kit &other) -> void = delete;

  auto kitUpdated() -> void;
  auto toMap() const -> QVariantMap;

  const std::unique_ptr<Internal::KitPrivate> d;

  friend class KitAspect;
  friend class KitManager;
  friend class Internal::KitManagerPrivate;
  friend class Internal::KitModel; // needed for setAutoDetected() when cloning kits
};

class KitGuard {
public:
  KitGuard(Kit *k) : m_kit(k)
  {
    k->blockNotification();
  }

  ~KitGuard() { m_kit->unblockNotification(); }
private:
  Kit *m_kit;
};

using TasksGenerator = std::function<Tasks(const Kit *)>;

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Kit *)
