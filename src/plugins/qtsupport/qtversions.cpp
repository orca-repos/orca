// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtversions.hpp"

#include "baseqtversion.hpp"
#include "qtsupportconstants.hpp"

#include <projectexplorer/abi.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <constants/remotelinux/remotelinux_constants.hpp>

#include <core/core-feature-provider.hpp>

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

#include <QCoreApplication>
#include <QFileInfo>

namespace QtSupport {
namespace Internal {

class DesktopQtVersion : public QtVersion {
public:
  DesktopQtVersion() = default;

  auto warningReason() const -> QStringList override;
  auto description() const -> QString override;
  auto availableFeatures() const -> QSet<Utils::Id> override;
  auto targetDeviceTypes() const -> QSet<Utils::Id> override;
};

auto DesktopQtVersion::warningReason() const -> QStringList
{
  auto ret = QtVersion::warningReason();
  if (qtVersion() >= QtVersionNumber(5, 0, 0)) {
    if (qmlRuntimeFilePath().isEmpty())
      ret << QCoreApplication::translate("QtVersion", "No QML utility installed.");
  }
  return ret;
}

auto DesktopQtVersion::description() const -> QString
{
  return QCoreApplication::translate("QtVersion", "Desktop", "Qt Version is meant for the desktop");
}

auto DesktopQtVersion::availableFeatures() const -> QSet<Utils::Id>
{
  auto features = QtVersion::availableFeatures();
  features.insert(Constants::FEATURE_DESKTOP);
  features.insert(Constants::FEATURE_QMLPROJECT);
  return features;
}

auto DesktopQtVersion::targetDeviceTypes() const -> QSet<Utils::Id>
{
  QSet<Utils::Id> result = {ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE};
  if (Utils::contains(qtAbis(), [](const ProjectExplorer::Abi a) { return a.os() == ProjectExplorer::Abi::LinuxOS; }))
    result.insert(RemoteLinux::Constants::GenericLinuxOsType);
  return result;
}

// Factory

DesktopQtVersionFactory::DesktopQtVersionFactory()
{
  setQtVersionCreator([] { return new DesktopQtVersion; });
  setSupportedType(Constants::DESKTOPQT);
  setPriority(0); // Lowest of all, we want to be the fallback
  // No further restrictions. We are the fallback :) so we don't care what kind of qt it is.
}

// EmbeddedLinuxQtVersion

constexpr char EMBEDDED_LINUX_QT[] = "RemoteLinux.EmbeddedLinuxQt";

class EmbeddedLinuxQtVersion : public QtVersion {
public:
  EmbeddedLinuxQtVersion() = default;

  auto description() const -> QString override
  {
    return QCoreApplication::translate("QtVersion", "Embedded Linux", "Qt Version is used for embedded Linux development");
  }

  auto targetDeviceTypes() const -> QSet<Utils::Id> override
  {
    return {RemoteLinux::Constants::GenericLinuxOsType};
  }
};

EmbeddedLinuxQtVersionFactory::EmbeddedLinuxQtVersionFactory()
{
  setQtVersionCreator([] { return new EmbeddedLinuxQtVersion; });
  setSupportedType(EMBEDDED_LINUX_QT);
  setPriority(10);

  setRestrictionChecker([](const SetupData &) { return false; });
}

} // Internal
} // QtSupport
