// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-base-file-filter.hpp>

#include <QFutureInterface>

namespace ProjectExplorer {

class Project;

namespace Internal {

class CurrentProjectFilter : public Orca::Plugin::Core::BaseFileFilter {
  Q_OBJECT

public:
  CurrentProjectFilter();

  auto refresh(QFutureInterface<void> &future) -> void override;
  auto prepareSearch(const QString &entry) -> void override;

private:
  auto currentProjectChanged() -> void;
  auto markFilesAsOutOfDate() -> void;

  Project *m_project = nullptr;
};

} // namespace Internal
} // namespace ProjectExplorer
