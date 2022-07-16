// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-locator-filter-interface.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeTargetLocatorFilter : public Orca::Plugin::Core::ILocatorFilter {
  Q_OBJECT

public:
  CMakeTargetLocatorFilter();

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Orca::Plugin::Core::LocatorFilterEntry> final;

private:
  auto projectListUpdated() -> void;

  QList<Orca::Plugin::Core::LocatorFilterEntry> m_result;
};

class BuildCMakeTargetLocatorFilter : CMakeTargetLocatorFilter {
  Q_OBJECT

public:
  BuildCMakeTargetLocatorFilter();

  auto accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void final;
};

class OpenCMakeTargetLocatorFilter : CMakeTargetLocatorFilter {
  Q_OBJECT

public:
  OpenCMakeTargetLocatorFilter();

  auto accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void final;
};

} // namespace Internal
} // namespace CMakeProjectManager
