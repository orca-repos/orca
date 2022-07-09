// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace RemoteLinux {
namespace Constants {

constexpr char GenericLinuxOsType[] = "GenericLinuxOsType";
constexpr char CheckForFreeDiskSpaceId[] = "RemoteLinux.CheckForFreeDiskSpaceStep";
constexpr char DirectUploadStepId[] = "RemoteLinux.DirectUploadStep";
constexpr char MakeInstallStepId[] = "RemoteLinux.MakeInstall";
constexpr char TarPackageCreationStepId[]  = "MaemoTarPackageCreationStep";
constexpr char UploadAndInstallTarPackageStepId[] = "MaemoUploadAndInstallTarPackageStep";
constexpr char RsyncDeployStepId[] = "RemoteLinux.RsyncDeployStep";
constexpr char CustomCommandDeployStepId[] = "RemoteLinux.GenericRemoteLinuxCustomCommandDeploymentStep";
constexpr char KillAppStepId[] = "RemoteLinux.KillAppStep";
constexpr char SupportsRSync[] =  "RemoteLinux.SupportsRSync";

} // Constants
} // RemoteLinux
