// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectexplorerconstants.hpp"

#include <core/icore.hpp>

#include <QCoreApplication>
#include <QString>

namespace ProjectExplorer {
namespace Constants {

auto msgAutoDetected() -> QString
{
  return QCoreApplication::translate("ProjectExplorer", "Auto-detected");
}

auto msgAutoDetectedToolTip() -> QString
{
  return QCoreApplication::translate("ProjectExplorer", "Automatically managed by %1 or the installer.").arg(Core::ICore::ideDisplayName());
}

auto msgManual() -> QString
{
  return QCoreApplication::translate("ProjectExplorer", "Manual");
}

} // namespace Constants
} // namespace ProjectExplorer
