// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/id.hpp>

#include <QObject>
#include <QVariantMap>

namespace ProjectExplorer {

class BuildStep;
class Target;

class PROJECTEXPLORER_EXPORT BuildStepList : public QObject {
  Q_OBJECT

public:
  explicit BuildStepList(QObject *parent, Utils::Id id);
  ~BuildStepList() override;

  auto clear() -> void;
  auto steps() const -> QList<BuildStep*>;

  template <class BS>
  auto firstOfType() -> BS*
  {
    BS *bs = nullptr;
    for (auto i = 0; i < count(); ++i) {
      bs = qobject_cast<BS*>(at(i));
      if (bs)
        return bs;
    }
    return nullptr;
  }

  auto firstStepWithId(Utils::Id id) const -> BuildStep*;
  auto count() const -> int;
  auto isEmpty() const -> bool;
  auto contains(Utils::Id id) const -> bool;
  auto insertStep(int position, BuildStep *step) -> void;
  auto insertStep(int position, Utils::Id id) -> void;
  auto appendStep(BuildStep *step) -> void { insertStep(count(), step); }
  auto appendStep(Utils::Id stepId) -> void { insertStep(count(), stepId); }

  struct StepCreationInfo {
    Utils::Id stepId;
    std::function<bool(Target *)> condition; // unset counts as unrestricted
  };

  auto removeStep(int position) -> bool;
  auto moveStepUp(int position) -> void;
  auto at(int position) -> BuildStep*;
  auto target() -> Target* { return m_target; }
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> bool;
  auto id() const -> Utils::Id { return m_id; }
  auto displayName() const -> QString;

signals:
  auto stepInserted(int position) -> void;
  auto aboutToRemoveStep(int position) -> void;
  auto stepRemoved(int position) -> void;
  auto stepMoved(int from, int to) -> void;

private:
  Target *m_target;
  Utils::Id m_id;
  QList<BuildStep*> m_steps;
};

} // namespace ProjectExplorer
