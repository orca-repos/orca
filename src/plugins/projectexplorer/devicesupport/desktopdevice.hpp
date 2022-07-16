// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include "idevice.hpp"
#include "idevicefactory.hpp"

#include <QApplication>

namespace ProjectExplorer {
class ProjectExplorerPlugin;

namespace Internal {
class DesktopDeviceFactory;
}

class PROJECTEXPLORER_EXPORT DesktopDevice : public IDevice {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::DesktopDevice)
public:
  auto deviceInformation() const -> DeviceInfo override;
  auto createWidget() -> IDeviceWidget* override;
  auto canAutoDetectPorts() const -> bool override;
  auto canCreateProcessModel() const -> bool override;
  auto createProcessListModel(QObject *parent) const -> DeviceProcessList* override;
  auto canCreateProcess() const -> bool override { return true; }
  auto portsGatheringMethod() const -> PortsGatheringMethod::Ptr override;
  auto createProcess(QObject *parent) const -> DeviceProcess* override;
  auto signalOperation() const -> DeviceProcessSignalOperation::Ptr override;
  auto environmentFetcher() const -> DeviceEnvironmentFetcher::Ptr override;
  auto toolControlChannel(const ControlChannelHint &) const -> QUrl override;
  auto handlesFile(const Utils::FilePath &filePath) const -> bool override;
  auto systemEnvironment() const -> Utils::Environment override;
  auto isExecutableFile(const Utils::FilePath &filePath) const -> bool override;
  auto isReadableFile(const Utils::FilePath &filePath) const -> bool override;
  auto isWritableFile(const Utils::FilePath &filePath) const -> bool override;
  auto isReadableDirectory(const Utils::FilePath &filePath) const -> bool override;
  auto isWritableDirectory(const Utils::FilePath &filePath) const -> bool override;
  auto isFile(const Utils::FilePath &filePath) const -> bool override;
  auto isDirectory(const Utils::FilePath &filePath) const -> bool override;
  auto ensureExistingFile(const Utils::FilePath &filePath) const -> bool override;
  auto createDirectory(const Utils::FilePath &filePath) const -> bool override;
  auto exists(const Utils::FilePath &filePath) const -> bool override;
  auto removeFile(const Utils::FilePath &filePath) const -> bool override;
  auto removeRecursively(const Utils::FilePath &filePath) const -> bool override;
  auto copyFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const -> bool override;
  auto renameFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const -> bool override;
  auto lastModified(const Utils::FilePath &filePath) const -> QDateTime override;
  auto symLinkTarget(const Utils::FilePath &filePath) const -> Utils::FilePath override;
  auto iterateDirectory(const Utils::FilePath &filePath, const std::function<bool(const Utils::FilePath &)> &callBack, const Utils::FileFilter &filter) const -> void override;
  auto fileContents(const Utils::FilePath &filePath, qint64 limit, qint64 offset) const -> QByteArray override;
  auto writeFileContents(const Utils::FilePath &filePath, const QByteArray &data) const -> bool override;
  auto fileSize(const Utils::FilePath &filePath) const -> qint64 override;
  auto permissions(const Utils::FilePath &filePath) const -> QFile::Permissions override;
  auto setPermissions(const Utils::FilePath &filePath, QFile::Permissions) const -> bool override;

protected:
  DesktopDevice();

  friend class ProjectExplorerPlugin;
  friend class Internal::DesktopDeviceFactory;
};

} // namespace ProjectExplorer
