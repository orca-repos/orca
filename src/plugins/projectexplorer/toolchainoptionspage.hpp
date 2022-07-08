// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

#include <QCoreApplication>

namespace ProjectExplorer {
namespace Internal {

class ToolChainOptionsPage final : public Core::IOptionsPage {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::ToolChainOptionsPage)
public:
  ToolChainOptionsPage();
};

} // namespace Internal
} // namespace ProjectExplorer
