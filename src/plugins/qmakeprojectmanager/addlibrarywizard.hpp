// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/wizard.hpp>
#include <utils/pathchooser.hpp>

QT_BEGIN_NAMESPACE
class QRadioButton;
class QLabel;
QT_END_NAMESPACE

namespace QmakeProjectManager {
namespace Internal {

class LibraryDetailsWidget;
class LibraryDetailsController;
class LibraryTypePage;
class DetailsPage;
class SummaryPage;

namespace Ui {
class LibraryDetailsWidget;
}

class AddLibraryWizard : public Utils::Wizard {
  Q_OBJECT

public:
  enum LibraryKind {
    InternalLibrary,
    ExternalLibrary,
    SystemLibrary,
    PackageLibrary
  };

  enum LinkageType {
    DynamicLinkage,
    StaticLinkage,
    NoLinkage
  };

  enum MacLibraryType {
    FrameworkType,
    LibraryType,
    NoLibraryType
  };

  enum Platform {
    LinuxPlatform = 0x01,
    MacPlatform = 0x02,
    WindowsMinGWPlatform = 0x04,
    WindowsMSVCPlatform = 0x08
  };

  Q_DECLARE_FLAGS(Platforms, Platform)

  explicit AddLibraryWizard(const Utils::FilePath &proFile, QWidget *parent = nullptr);
  ~AddLibraryWizard() override;

  auto libraryKind() const -> LibraryKind;
  auto proFile() const -> Utils::FilePath;
  auto snippet() const -> QString;

private:
  LibraryTypePage *m_libraryTypePage = nullptr;
  DetailsPage *m_detailsPage = nullptr;
  SummaryPage *m_summaryPage = nullptr;
  Utils::FilePath m_proFile;
};

class LibraryTypePage : public QWizardPage {
  Q_OBJECT

public:
  LibraryTypePage(AddLibraryWizard *parent);
  auto libraryKind() const -> AddLibraryWizard::LibraryKind;

private:
  QRadioButton *m_internalRadio = nullptr;
  QRadioButton *m_externalRadio = nullptr;
  QRadioButton *m_systemRadio = nullptr;
  QRadioButton *m_packageRadio = nullptr;
};

class DetailsPage : public QWizardPage {
  Q_OBJECT public:
  DetailsPage(AddLibraryWizard *parent);
  auto initializePage() -> void override;
  auto isComplete() const -> bool override;
  auto snippet() const -> QString;

private:
  AddLibraryWizard *m_libraryWizard;
  Ui::LibraryDetailsWidget *m_libraryDetailsWidget = nullptr;
  LibraryDetailsController *m_libraryDetailsController = nullptr;
};

class SummaryPage : public QWizardPage {
  Q_OBJECT

public:
  SummaryPage(AddLibraryWizard *parent);

  auto initializePage() -> void override;
  auto snippet() const -> QString;

private:
  AddLibraryWizard *m_libraryWizard = nullptr;
  QLabel *m_summaryLabel = nullptr;
  QLabel *m_snippetLabel = nullptr;
  QString m_snippet;
};

} // namespace Internal
} // namespace QmakeProjectManager

Q_DECLARE_OPERATORS_FOR_FLAGS(QmakeProjectManager::Internal::AddLibraryWizard::Platforms)
