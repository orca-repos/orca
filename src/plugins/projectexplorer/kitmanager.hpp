// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "kit.hpp"

#include <core/featureprovider.hpp>

#include <utils/aspects.hpp>

#include <QObject>
#include <QPair>
#include <QSet>

#include <functional>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace Utils {
class Environment;
class FilePath;
class LayoutBuilder;
class MacroExpander;
class OutputLineParser;
} // namespace Utils

namespace ProjectExplorer {
class Task;
class KitAspectWidget;
class KitManager;

namespace Internal {
class KitManagerConfigWidget;
} // namespace Internal

/**
 * @brief The KitAspect class
 *
 * One piece of information stored in the kit.
 *
 * They auto-register with the \a KitManager for their life time
 */
class PROJECTEXPLORER_EXPORT KitAspect : public QObject {
  Q_OBJECT

public:
  using Item = QPair<QString, QString>;
  using ItemList = QList<Item>;

  auto id() const -> Utils::Id { return m_id; }
  auto priority() const -> int { return m_priority; }
  auto displayName() const -> QString { return m_displayName; }
  auto description() const -> QString { return m_description; }
  auto isEssential() const -> bool { return m_essential; }

  // called to find issues with the kit
  virtual auto validate(const Kit *) const -> Tasks = 0;
  // called after restoring a kit, so upgrading of kit information settings can be done
  virtual auto upgrade(Kit *) -> void { return; }
  // called to fix issues with this kitinformation. Does not modify the rest of the kit.
  virtual auto fix(Kit *) -> void { return; }
  // called on initial setup of a kit.
  virtual auto setup(Kit *) -> void { return; }
  virtual auto weight(const Kit *k) const -> int;
  virtual auto toUserOutput(const Kit *) const -> ItemList = 0;
  virtual auto createConfigWidget(Kit *) const -> KitAspectWidget* = 0;
  virtual auto addToBuildEnvironment(const Kit *k, Utils::Environment &env) const -> void;
  virtual auto addToRunEnvironment(const Kit *k, Utils::Environment &env) const -> void;
  virtual auto createOutputParsers(const Kit *k) const -> QList<Utils::OutputLineParser*>;
  virtual auto displayNamePostfix(const Kit *k) const -> QString;
  virtual auto supportedPlatforms(const Kit *k) const -> QSet<Utils::Id>;
  virtual auto availableFeatures(const Kit *k) const -> QSet<Utils::Id>;
  virtual auto addToMacroExpander(Kit *kit, Utils::MacroExpander *expander) const -> void;
  virtual auto isApplicableToKit(const Kit *) const -> bool { return true; }

protected:
  KitAspect();
  ~KitAspect();

  auto setId(Utils::Id id) -> void { m_id = id; }
  auto setDisplayName(const QString &name) -> void { m_displayName = name; }
  auto setDescription(const QString &desc) -> void { m_description = desc; }
  auto makeEssential() -> void { m_essential = true; }
  auto setPriority(int priority) -> void { m_priority = priority; }
  auto notifyAboutUpdate(Kit *k) -> void;

private:
  QString m_displayName;
  QString m_description;
  Utils::Id m_id;
  int m_priority = 0; // The higher the closer to the top.
  bool m_essential = false;
};

class PROJECTEXPLORER_EXPORT KitAspectWidget : public Utils::BaseAspect {
  Q_OBJECT

public:
  KitAspectWidget(Kit *kit, const KitAspect *ki);
  ~KitAspectWidget();

  virtual auto makeReadOnly() -> void = 0;
  virtual auto refresh() -> void = 0;
  auto addToLayoutWithLabel(QWidget *parent) -> void;
  static auto msgManage() -> QString;
  auto kit() const -> Kit* { return m_kit; }
  auto kitInformation() const -> const KitAspect* { return m_kitInformation; }
  auto mutableAction() const -> QAction* { return m_mutableAction; }
  auto addMutableAction(QWidget *child) -> void;
  auto createManageButton(Utils::Id pageId) -> QWidget*;

protected:
  Kit *m_kit;
  const KitAspect *m_kitInformation;
  QAction *m_mutableAction = nullptr;
};

class PROJECTEXPLORER_EXPORT KitManager : public QObject {
  Q_OBJECT

public:
  static auto instance() -> KitManager*;
  ~KitManager() override;

  static auto kits() -> const QList<Kit*>;
  static auto kit(const Kit::Predicate &predicate) -> Kit*;
  static auto kit(Utils::Id id) -> Kit*;
  static auto defaultKit() -> Kit*;
  static auto kitAspects() -> const QList<KitAspect*>;
  static auto irrelevantAspects() -> const QSet<Utils::Id>;
  static auto setIrrelevantAspects(const QSet<Utils::Id> &aspects) -> void;
  static auto registerKit(const std::function<void(Kit *)> &init, Utils::Id id = {}) -> Kit*;
  static auto deregisterKit(Kit *k) -> void;
  static auto setDefaultKit(Kit *k) -> void;
  static auto sortKits(const QList<Kit*> &kits) -> QList<Kit*>; // Avoid sorting whenever possible!
  static auto saveKits() -> void;
  static auto isLoaded() -> bool;

signals:
  auto kitAdded(Kit *) -> void;
  // Kit is still valid when this call happens!
  auto kitRemoved(Kit *) -> void;
  // Kit was updated.
  auto kitUpdated(Kit *) -> void;
  auto unmanagedKitUpdated(Kit *) -> void;
  // Default kit was changed.
  auto defaultkitChanged() -> void;
  // Something changed.
  auto kitsChanged() -> void;
  auto kitsLoaded() -> void;

private:
  KitManager();

  static auto destroy() -> void;
  static auto registerKitAspect(KitAspect *ki) -> void;
  static auto deregisterKitAspect(KitAspect *ki) -> void;
  static auto setBinaryForKit(const Utils::FilePath &binary) -> void;
  // Make sure the this is only called after all
  // KitAspects are registered!
  static auto restoreKits() -> void;
  static auto notifyAboutUpdate(Kit *k) -> void;
  static auto completeKit(Kit *k) -> void;

  friend class ProjectExplorerPlugin; // for constructor
  friend class Kit;
  friend class Internal::KitManagerConfigWidget;
  friend class KitAspect; // for notifyAboutUpdate and self-registration
};

} // namespace ProjectExplorer
