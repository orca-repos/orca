// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectpanelfactory.hpp"

#include <QCoreApplication>

#include <memory>

namespace ProjectExplorer {

class Target;

namespace Internal {

class TargetItem;
class TargetGroupItemPrivate;

// Second level: Special case for the Build & Run item (with per-kit subItems)
class TargetGroupItem : public Utils::TypedTreeItem<TargetItem /*, ProjectItem */> {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelItem)

public:
  TargetGroupItem(const QString &displayName, Project *project);
  ~TargetGroupItem() override;

  auto data(int column, int role) const -> QVariant override;
  auto setData(int column, const QVariant &data, int role) -> bool override;
  auto flags(int) const -> Qt::ItemFlags override;
  auto currentTargetItem() const -> TargetItem*;
  auto targetItem(Target *target) const -> TargetItem*;

private:
  const std::unique_ptr<TargetGroupItemPrivate> d;
};

} // namespace Internal
} // namespace ProjectExplorer
