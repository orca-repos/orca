// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "runconfiguration.hpp"

#include <utils/aspects.hpp>
#include <utils/environment.hpp>

#include <QList>
#include <QVariantMap>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT EnvironmentAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  using EnvironmentModifier = std::function<void(Utils::Environment &)>;

  EnvironmentAspect();

  // The environment including the user's modifications.
  auto environment() const -> Utils::Environment;

  // Environment including modifiers, but without explicit user changes.
  auto modifiedBaseEnvironment() const -> Utils::Environment;
  auto baseEnvironmentBase() const -> int;
  auto setBaseEnvironmentBase(int base) -> void;
  auto userEnvironmentChanges() const -> Utils::EnvironmentItems { return m_userChanges; }
  auto setUserEnvironmentChanges(const Utils::EnvironmentItems &diff) -> void;
  auto addSupportedBaseEnvironment(const QString &displayName, const std::function<Utils::Environment()> &getter) -> void;
  auto addPreferredBaseEnvironment(const QString &displayName, const std::function<Utils::Environment()> &getter) -> void;
  auto currentDisplayName() const -> QString;
  auto displayNames() const -> const QStringList;
  auto addModifier(const EnvironmentModifier &) -> void;
  auto isLocal() const -> bool { return m_isLocal; }

signals:
  auto baseEnvironmentChanged() -> void;
  auto userEnvironmentChangesChanged(const Utils::EnvironmentItems &diff) -> void;
  auto environmentChanged() -> void;

protected:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;
  auto setIsLocal(bool local) -> void { m_isLocal = local; }

private:
  // One possible choice in the Environment aspect.
  struct BaseEnvironment {
    auto unmodifiedBaseEnvironment() const -> Utils::Environment;

    std::function<Utils::Environment()> getter;
    QString displayName;
  };

  Utils::EnvironmentItems m_userChanges;
  QList<EnvironmentModifier> m_modifiers;
  QList<BaseEnvironment> m_baseEnvironments;
  int m_base = -1;
  bool m_isLocal = false;
};

} // namespace ProjectExplorer
