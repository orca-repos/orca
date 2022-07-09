// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/ilocatorfilter.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeTargetLocatorFilter : public Core::ILocatorFilter {
  Q_OBJECT

public:
  CMakeTargetLocatorFilter();

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry> final;

private:
  auto projectListUpdated() -> void;

  QList<Core::LocatorFilterEntry> m_result;
};

class BuildCMakeTargetLocatorFilter : CMakeTargetLocatorFilter {
  Q_OBJECT

public:
  BuildCMakeTargetLocatorFilter();

  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void final;
};

class OpenCMakeTargetLocatorFilter : CMakeTargetLocatorFilter {
  Q_OBJECT

public:
  OpenCMakeTargetLocatorFilter();

  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void final;
};

} // namespace Internal
} // namespace CMakeProjectManager
