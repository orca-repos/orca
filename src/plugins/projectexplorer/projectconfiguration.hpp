// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/aspects.hpp>
#include <utils/displayname.hpp>
#include <utils/id.hpp>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

namespace ProjectExplorer {

class Kit;
class Project;
class Target;

class PROJECTEXPLORER_EXPORT ProjectConfiguration : public QObject {
  Q_OBJECT

protected:
  explicit ProjectConfiguration(QObject *parent, Utils::Id id);

public:
  ~ProjectConfiguration() override;

  auto id() const -> Utils::Id;
  auto displayName() const -> QString { return m_displayName.value(); }
  auto expandedDisplayName() const -> QString;
  auto usesDefaultDisplayName() const -> bool { return m_displayName.usesDefaultValue(); }
  auto setDisplayName(const QString &name) -> void;
  auto setDefaultDisplayName(const QString &name) -> void;
  auto setToolTip(const QString &text) -> void;
  auto toolTip() const -> QString;
  // Note: Make sure subclasses call the superclasses' fromMap() function!
  virtual auto fromMap(const QVariantMap &map) -> bool;
  // Note: Make sure subclasses call the superclasses' toMap() function!
  virtual auto toMap() const -> QVariantMap;
  auto target() const -> Target*;
  auto project() const -> Project*;
  auto kit() const -> Kit*;
  static auto settingsIdKey() -> QString;

  template <class Aspect, typename ...Args>
  auto addAspect(Args && ...args) -> Aspect*
  {
    return m_aspects.addAspect<Aspect>(std::forward<Args>(args)...);
  }

  auto aspects() const -> const Utils::AspectContainer& { return m_aspects; }
  auto aspect(Utils::Id id) const -> Utils::BaseAspect*;
  template <typename T>
  auto aspect() const -> T* { return m_aspects.aspect<T>(); }
  auto acquaintAspects() -> void;
  auto mapFromBuildDeviceToGlobalPath(const Utils::FilePath &path) const -> Utils::FilePath;
  auto addPostInit(const std::function<void()> &fixup) -> void { m_postInit.append(fixup); }
  auto doPostInit() -> void;

signals:
  auto displayNameChanged() -> void;
  auto toolTipChanged() -> void;

protected:
  Utils::AspectContainer m_aspects;

private:
  QPointer<Target> m_target;
  const Utils::Id m_id;
  Utils::DisplayName m_displayName;
  QString m_toolTip;
  QList<std::function<void()>> m_postInit;
};

// helper function:
PROJECTEXPLORER_EXPORT auto idFromMap(const QVariantMap &map) -> Utils::Id;

} // namespace ProjectExplorer
