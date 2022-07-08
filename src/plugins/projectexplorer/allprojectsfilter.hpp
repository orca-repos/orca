// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/basefilefilter.hpp>

#include <QFutureInterface>

namespace ProjectExplorer {
namespace Internal {

class AllProjectsFilter : public Core::BaseFileFilter {
  Q_OBJECT

public:
  AllProjectsFilter();

  auto refresh(QFutureInterface<void> &future) -> void override;
  auto prepareSearch(const QString &entry) -> void override;

private:
  auto markFilesAsOutOfDate() -> void;
};

} // namespace Internal
} // namespace ProjectExplorer
