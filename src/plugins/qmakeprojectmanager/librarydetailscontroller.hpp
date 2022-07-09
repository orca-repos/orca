// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "addlibrarywizard.hpp"

namespace QmakeProjectManager {

class QmakeProFile;

namespace Internal {

namespace Ui {
class LibraryDetailsWidget;
}

class LibraryDetailsController : public QObject {
  Q_OBJECT

public:
  explicit LibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);

  virtual auto isComplete() const -> bool = 0;
  virtual auto snippet() const -> QString = 0;

signals:
  auto completeChanged() -> void;

protected:
  auto libraryDetailsWidget() const -> Ui::LibraryDetailsWidget*;
  auto platforms() const -> AddLibraryWizard::Platforms;
  auto linkageType() const -> AddLibraryWizard::LinkageType;
  auto macLibraryType() const -> AddLibraryWizard::MacLibraryType;
  auto libraryPlatformType() const -> Utils::OsType;
  auto libraryPlatformFilter() const -> QString;
  auto proFile() const -> Utils::FilePath;
  auto isIncludePathChanged() const -> bool;
  auto guiSignalsIgnored() const -> bool;
  auto updateGui() -> void;

  virtual auto suggestedLinkageType() const -> AddLibraryWizard::LinkageType = 0;
  virtual auto suggestedMacLibraryType() const -> AddLibraryWizard::MacLibraryType = 0;
  virtual auto suggestedIncludePath() const -> QString = 0;
  virtual auto updateWindowsOptionsEnablement() -> void = 0;

  auto setIgnoreGuiSignals(bool ignore) -> void;
  auto setPlatformsVisible(bool ena) -> void;
  auto setLinkageRadiosVisible(bool ena) -> void;
  auto setLinkageGroupVisible(bool ena) -> void;
  auto setMacLibraryRadiosVisible(bool ena) -> void;
  auto setMacLibraryGroupVisible(bool ena) -> void;
  auto setLibraryPathChooserVisible(bool ena) -> void;
  auto setLibraryComboBoxVisible(bool ena) -> void;
  auto setPackageLineEditVisible(bool ena) -> void;
  auto setIncludePathVisible(bool ena) -> void;
  auto setWindowsGroupVisible(bool ena) -> void;
  auto setRemoveSuffixVisible(bool ena) -> void;
  auto isMacLibraryRadiosVisible() const -> bool;
  auto isIncludePathVisible() const -> bool;
  auto isWindowsGroupVisible() const -> bool;

private:
  auto slotIncludePathChanged() -> void;
  auto slotPlatformChanged() -> void;
  auto slotMacLibraryTypeChanged() -> void;
  auto slotUseSubfoldersChanged(bool ena) -> void;
  auto slotAddSuffixChanged(bool ena) -> void;
  auto showLinkageType(AddLibraryWizard::LinkageType linkageType) -> void;
  auto showMacLibraryType(AddLibraryWizard::MacLibraryType libType) -> void;

  AddLibraryWizard::Platforms m_platforms = AddLibraryWizard::LinuxPlatform | AddLibraryWizard::MacPlatform | AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform;
  AddLibraryWizard::LinkageType m_linkageType = AddLibraryWizard::NoLinkage;
  AddLibraryWizard::MacLibraryType m_macLibraryType = AddLibraryWizard::NoLibraryType;
  Utils::FilePath m_proFile;
  bool m_ignoreGuiSignals = false;
  bool m_includePathChanged = false;
  bool m_linkageRadiosVisible = true;
  bool m_macLibraryRadiosVisible = true;
  bool m_includePathVisible = true;
  bool m_windowsGroupVisible = true;
  Ui::LibraryDetailsWidget *m_libraryDetailsWidget;
  QWizard *m_wizard = nullptr;
};

class NonInternalLibraryDetailsController : public LibraryDetailsController {
  Q_OBJECT

public:
  explicit NonInternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);
  auto isComplete() const -> bool override;
  auto snippet() const -> QString override;

protected:
  auto suggestedLinkageType() const -> AddLibraryWizard::LinkageType override final;
  auto suggestedMacLibraryType() const -> AddLibraryWizard::MacLibraryType override final;
  auto suggestedIncludePath() const -> QString override final;
  auto updateWindowsOptionsEnablement() -> void override;

private:
  auto handleLinkageTypeChange() -> void;
  auto handleLibraryTypeChange() -> void;
  auto handleLibraryPathChange() -> void;
  auto slotLinkageTypeChanged() -> void;
  auto slotRemoveSuffixChanged(bool ena) -> void;
  auto slotLibraryTypeChanged() -> void;
  auto slotLibraryPathChanged() -> void;
};

class PackageLibraryDetailsController : public NonInternalLibraryDetailsController {
  Q_OBJECT

public:
  explicit PackageLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);
  auto isComplete() const -> bool override;
  auto snippet() const -> QString override;

protected:
  auto updateWindowsOptionsEnablement() -> void override final
  {
    NonInternalLibraryDetailsController::updateWindowsOptionsEnablement();
  }

private:
  auto isLinkPackageGenerated() const -> bool;
};

class SystemLibraryDetailsController : public NonInternalLibraryDetailsController {
  Q_OBJECT

public:
  explicit SystemLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);

protected:
  auto updateWindowsOptionsEnablement() -> void override final
  {
    NonInternalLibraryDetailsController::updateWindowsOptionsEnablement();
  }
};

class ExternalLibraryDetailsController : public NonInternalLibraryDetailsController {
  Q_OBJECT

public:
  explicit ExternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);

protected:
  auto updateWindowsOptionsEnablement() -> void override final;
};

class InternalLibraryDetailsController : public LibraryDetailsController {
  Q_OBJECT

public:
  explicit InternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const Utils::FilePath &proFile, QObject *parent = nullptr);
  auto isComplete() const -> bool override;
  auto snippet() const -> QString override;

protected:
  auto suggestedLinkageType() const -> AddLibraryWizard::LinkageType override final;
  auto suggestedMacLibraryType() const -> AddLibraryWizard::MacLibraryType override final;
  auto suggestedIncludePath() const -> QString override final;
  auto updateWindowsOptionsEnablement() -> void override final;

private:
  auto slotCurrentLibraryChanged() -> void;
  auto updateProFile() -> void;

  QString m_rootProjectPath;
  QVector<QmakeProFile*> m_proFiles;
};

} // namespace Internal
} // namespace QmakeProjectManager
