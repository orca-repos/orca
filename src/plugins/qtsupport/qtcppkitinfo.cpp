// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtcppkitinfo.hpp"

#include "baseqtversion.hpp"
#include "qtkitinformation.hpp"

namespace QtSupport {

CppKitInfo::CppKitInfo(ProjectExplorer::Kit *kit) : KitInfo(kit)
{
  if (kit && (qtVersion = QtKitAspect::qtVersion(kit))) {
    if (qtVersion->qtVersion() < QtVersionNumber(5, 0, 0))
      projectPartQtVersion = Utils::QtMajorVersion::Qt4;
    else if (qtVersion->qtVersion() < QtVersionNumber(6, 0, 0))
      projectPartQtVersion = Utils::QtMajorVersion::Qt5;
    else
      projectPartQtVersion = Utils::QtMajorVersion::Qt6;
  }
}

} // namespace QtSupport
