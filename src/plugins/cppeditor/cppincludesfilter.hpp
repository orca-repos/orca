// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/basefilefilter.hpp>

namespace CppEditor::Internal {

class CppIncludesFilter : public Core::BaseFileFilter {
  Q_OBJECT

public:
  CppIncludesFilter();

  // ILocatorFilter interface
  auto prepareSearch(const QString &entry) -> void override;
  auto refresh(QFutureInterface<void> &future) -> void override;

private:
  auto markOutdated() -> void;
  bool m_needsUpdate = true;
};

} // namespace CppEditor::Internal
