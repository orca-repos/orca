// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "librarydetailscontroller.hpp"
#include "ui_librarydetailswidget.h"
#include "qmakebuildconfiguration.hpp"
#include "qmakeparsernodes.hpp"
#include "qmakeproject.hpp"

#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcprocess.hpp>

#include <QFileInfo>
#include <QDir>
#include <QTextStream>

using namespace ProjectExplorer;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

static auto fillLibraryPlatformTypes(QComboBox *comboBox) -> void
{
  comboBox->clear();
  comboBox->addItem("Windows (*.lib lib*.a)", int(OsTypeWindows));
  comboBox->addItem("Linux (lib*.so lib*.a)", int(OsTypeLinux));
  comboBox->addItem("macOS (*.dylib *.a *.framework)", int(OsTypeMac));
  const int currentIndex = comboBox->findData(int(HostOsInfo::hostOs()));
  comboBox->setCurrentIndex(std::max(0, currentIndex));
}

LibraryDetailsController::LibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : QObject(parent), m_proFile(proFile), m_libraryDetailsWidget(libraryDetails)
{
  fillLibraryPlatformTypes(m_libraryDetailsWidget->libraryTypeComboBox);
  setPlatformsVisible(true);
  setLinkageGroupVisible(true);
  setMacLibraryGroupVisible(true);
  setPackageLineEditVisible(false);
  const auto isMacOs = libraryPlatformType() == OsTypeMac;
  const auto isWindows = libraryPlatformType() == OsTypeWindows;
  setMacLibraryRadiosVisible(!isMacOs);
  setLinkageRadiosVisible(isWindows);

  connect(m_libraryDetailsWidget->includePathChooser, &PathChooser::rawPathChanged, this, &LibraryDetailsController::slotIncludePathChanged);
  connect(m_libraryDetailsWidget->frameworkRadio, &QAbstractButton::clicked, this, &LibraryDetailsController::slotMacLibraryTypeChanged);
  connect(m_libraryDetailsWidget->libraryRadio, &QAbstractButton::clicked, this, &LibraryDetailsController::slotMacLibraryTypeChanged);
  connect(m_libraryDetailsWidget->useSubfoldersCheckBox, &QAbstractButton::toggled, this, &LibraryDetailsController::slotUseSubfoldersChanged);
  connect(m_libraryDetailsWidget->addSuffixCheckBox, &QAbstractButton::toggled, this, &LibraryDetailsController::slotAddSuffixChanged);
  connect(m_libraryDetailsWidget->linCheckBox, &QAbstractButton::clicked, this, &LibraryDetailsController::slotPlatformChanged);
  connect(m_libraryDetailsWidget->macCheckBox, &QAbstractButton::clicked, this, &LibraryDetailsController::slotPlatformChanged);
  connect(m_libraryDetailsWidget->winCheckBox, &QAbstractButton::clicked, this, &LibraryDetailsController::slotPlatformChanged);
}

auto LibraryDetailsController::libraryDetailsWidget() const -> Ui::LibraryDetailsWidget*
{
  return m_libraryDetailsWidget;
}

auto LibraryDetailsController::platforms() const -> AddLibraryWizard::Platforms
{
  return m_platforms;
}

auto LibraryDetailsController::linkageType() const -> AddLibraryWizard::LinkageType
{
  return m_linkageType;
}

auto LibraryDetailsController::macLibraryType() const -> AddLibraryWizard::MacLibraryType
{
  return m_macLibraryType;
}

auto LibraryDetailsController::libraryPlatformType() const -> OsType
{
  return OsType(m_libraryDetailsWidget->libraryTypeComboBox->currentData().value<int>());
}

auto LibraryDetailsController::libraryPlatformFilter() const -> QString
{
  return m_libraryDetailsWidget->libraryTypeComboBox->currentText();
}

auto LibraryDetailsController::updateGui() -> void
{
  // read values from gui
  m_platforms = {};
  if (libraryDetailsWidget()->linCheckBox->isChecked())
    m_platforms |= AddLibraryWizard::LinuxPlatform;
  if (libraryDetailsWidget()->macCheckBox->isChecked())
    m_platforms |= AddLibraryWizard::MacPlatform;
  if (libraryDetailsWidget()->winCheckBox->isChecked())
    m_platforms |= AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform;

  auto macLibraryTypeUpdated = false;
  if (!m_linkageRadiosVisible) {
    m_linkageType = suggestedLinkageType();
    if (m_linkageType == AddLibraryWizard::StaticLinkage) {
      m_macLibraryType = AddLibraryWizard::LibraryType;
      macLibraryTypeUpdated = true;
    }
  } else {
    m_linkageType = AddLibraryWizard::DynamicLinkage; // the default
    if (libraryDetailsWidget()->staticRadio->isChecked())
      m_linkageType = AddLibraryWizard::StaticLinkage;
  }

  if (!macLibraryTypeUpdated) {
    if (!m_macLibraryRadiosVisible) {
      m_macLibraryType = suggestedMacLibraryType();
    } else {
      m_macLibraryType = AddLibraryWizard::LibraryType; // the default
      if (libraryDetailsWidget()->frameworkRadio->isChecked())
        m_macLibraryType = AddLibraryWizard::FrameworkType;
    }
  }

  // enable or disable some parts of gui
  libraryDetailsWidget()->macGroupBox->setEnabled(platforms() & AddLibraryWizard::MacPlatform);
  updateWindowsOptionsEnablement();
  const auto macRadiosEnabled = m_linkageRadiosVisible || linkageType() != AddLibraryWizard::StaticLinkage;
  libraryDetailsWidget()->libraryRadio->setEnabled(macRadiosEnabled);
  libraryDetailsWidget()->frameworkRadio->setEnabled(macRadiosEnabled);

  // update values in gui
  setIgnoreGuiSignals(true);

  showLinkageType(linkageType());
  showMacLibraryType(macLibraryType());
  if (!m_includePathChanged)
    libraryDetailsWidget()->includePathChooser->setPath(suggestedIncludePath());

  setIgnoreGuiSignals(false);

  // UGLY HACK BEGIN
  //
  // We need to invoke QWizardPrivate::updateLayout() method to properly
  // recalculate the new minimum size for the whole wizard.
  // This is done internally by QWizard e.g. when a new wizard page is being shown.
  // Unfortunately, QWizard doesn't expose this method currently.
  // Since the current implementation of QWizard::setTitleFormat() sets the
  // format and calls QWizardPrivate::updateLayout() unconditionally
  // we use it as a hacky solution to the above issue.
  // For reference please see: QTBUG-88666
  if (!m_wizard) {
    QWidget *widget = libraryDetailsWidget()->detailsLayout->parentWidget();
    while (widget) {
      auto wizard = qobject_cast<QWizard*>(widget);
      if (wizard) {
        m_wizard = wizard;
        break;
      }
      widget = widget->parentWidget();
    }
  }
  QTC_ASSERT(m_wizard, return);
  m_wizard->setTitleFormat(m_wizard->titleFormat());
  // UGLY HACK END
}

auto LibraryDetailsController::proFile() const -> FilePath
{
  return m_proFile;
}

auto LibraryDetailsController::isIncludePathChanged() const -> bool
{
  return m_includePathChanged;
}

auto LibraryDetailsController::setIgnoreGuiSignals(bool ignore) -> void
{
  m_ignoreGuiSignals = ignore;
}

auto LibraryDetailsController::guiSignalsIgnored() const -> bool
{
  return m_ignoreGuiSignals;
}

auto LibraryDetailsController::showLinkageType(AddLibraryWizard::LinkageType linkageType) -> void
{
  const auto linkage(tr("Linkage:"));
  QString linkageTitle;
  switch (linkageType) {
  case AddLibraryWizard::DynamicLinkage:
    libraryDetailsWidget()->dynamicRadio->setChecked(true);
    linkageTitle = tr("%1 Dynamic").arg(linkage);
    break;
  case AddLibraryWizard::StaticLinkage:
    libraryDetailsWidget()->staticRadio->setChecked(true);
    linkageTitle = tr("%1 Static").arg(linkage);
    break;
  default:
    libraryDetailsWidget()->dynamicRadio->setChecked(false);
    libraryDetailsWidget()->staticRadio->setChecked(false);
    linkageTitle = linkage;
    break;
  }
  libraryDetailsWidget()->linkageGroupBox->setTitle(linkageTitle);
}

auto LibraryDetailsController::showMacLibraryType(AddLibraryWizard::MacLibraryType libType) -> void
{
  const auto libraryType(tr("Mac:"));
  QString libraryTypeTitle;
  switch (libType) {
  case AddLibraryWizard::FrameworkType:
    libraryDetailsWidget()->frameworkRadio->setChecked(true);
    libraryTypeTitle = tr("%1 Framework").arg(libraryType);
    break;
  case AddLibraryWizard::LibraryType:
    libraryDetailsWidget()->libraryRadio->setChecked(true);
    libraryTypeTitle = tr("%1 Library").arg(libraryType);
    break;
  default:
    libraryDetailsWidget()->frameworkRadio->setChecked(false);
    libraryDetailsWidget()->libraryRadio->setChecked(false);
    libraryTypeTitle = libraryType;
    break;
  }
  libraryDetailsWidget()->macGroupBox->setTitle(libraryTypeTitle);
}

auto LibraryDetailsController::setPlatformsVisible(bool ena) -> void
{
  libraryDetailsWidget()->platformGroupBox->setVisible(ena);
}

auto LibraryDetailsController::setLinkageRadiosVisible(bool ena) -> void
{
  m_linkageRadiosVisible = ena;
  libraryDetailsWidget()->staticRadio->setVisible(ena);
  libraryDetailsWidget()->dynamicRadio->setVisible(ena);
}

auto LibraryDetailsController::setLinkageGroupVisible(bool ena) -> void
{
  setLinkageRadiosVisible(ena);
  libraryDetailsWidget()->linkageGroupBox->setVisible(ena);
}

auto LibraryDetailsController::setMacLibraryRadiosVisible(bool ena) -> void
{
  m_macLibraryRadiosVisible = ena;
  libraryDetailsWidget()->frameworkRadio->setVisible(ena);
  libraryDetailsWidget()->libraryRadio->setVisible(ena);
}

auto LibraryDetailsController::setMacLibraryGroupVisible(bool ena) -> void
{
  setMacLibraryRadiosVisible(ena);
  libraryDetailsWidget()->macGroupBox->setVisible(ena);
}

auto LibraryDetailsController::setLibraryPathChooserVisible(bool ena) -> void
{
  libraryDetailsWidget()->libraryTypeComboBox->setVisible(ena);
  libraryDetailsWidget()->libraryTypeLabel->setVisible(ena);
  libraryDetailsWidget()->libraryPathChooser->setVisible(ena);
  libraryDetailsWidget()->libraryFileLabel->setVisible(ena);
}

auto LibraryDetailsController::setLibraryComboBoxVisible(bool ena) -> void
{
  libraryDetailsWidget()->libraryComboBox->setVisible(ena);
  libraryDetailsWidget()->libraryLabel->setVisible(ena);
}

auto LibraryDetailsController::setPackageLineEditVisible(bool ena) -> void
{
  libraryDetailsWidget()->packageLineEdit->setVisible(ena);
  libraryDetailsWidget()->packageLabel->setVisible(ena);
}

auto LibraryDetailsController::setIncludePathVisible(bool ena) -> void
{
  m_includePathVisible = ena;
  libraryDetailsWidget()->includeLabel->setVisible(ena);
  libraryDetailsWidget()->includePathChooser->setVisible(ena);
}

auto LibraryDetailsController::setWindowsGroupVisible(bool ena) -> void
{
  m_windowsGroupVisible = ena;
  libraryDetailsWidget()->winGroupBox->setVisible(ena);
}

auto LibraryDetailsController::setRemoveSuffixVisible(bool ena) -> void
{
  libraryDetailsWidget()->removeSuffixCheckBox->setVisible(ena);
}

auto LibraryDetailsController::isMacLibraryRadiosVisible() const -> bool
{
  return m_macLibraryRadiosVisible;
}

auto LibraryDetailsController::isIncludePathVisible() const -> bool
{
  return m_includePathVisible;
}

auto LibraryDetailsController::isWindowsGroupVisible() const -> bool
{
  return m_windowsGroupVisible;
}

auto LibraryDetailsController::slotIncludePathChanged() -> void
{
  if (m_ignoreGuiSignals)
    return;
  m_includePathChanged = true;
}

auto LibraryDetailsController::slotPlatformChanged() -> void
{
  updateGui();
  emit completeChanged();
}

auto LibraryDetailsController::slotMacLibraryTypeChanged() -> void
{
  if (guiSignalsIgnored())
    return;

  if (m_linkageRadiosVisible && libraryDetailsWidget()->frameworkRadio->isChecked()) {
    setIgnoreGuiSignals(true);
    libraryDetailsWidget()->dynamicRadio->setChecked(true);
    setIgnoreGuiSignals(false);
  }

  updateGui();
}

auto LibraryDetailsController::slotUseSubfoldersChanged(bool ena) -> void
{
  if (ena) {
    libraryDetailsWidget()->addSuffixCheckBox->setChecked(false);
    libraryDetailsWidget()->removeSuffixCheckBox->setChecked(false);
  }
}

auto LibraryDetailsController::slotAddSuffixChanged(bool ena) -> void
{
  if (ena) {
    libraryDetailsWidget()->useSubfoldersCheckBox->setChecked(false);
    libraryDetailsWidget()->removeSuffixCheckBox->setChecked(false);
  }
}

// quote only when the string contains spaces
static auto smartQuote(const QString &aString) -> QString
{
  // The OS type is not important in that case, but use always the same
  // in order not to generate different quoting depending on host platform
  return ProcessArgs::quoteArg(aString, OsTypeLinux);
}

static auto appendSeparator(const QString &aString) -> QString
{
  if (aString.isEmpty())
    return aString;
  if (aString.at(aString.size() - 1) == QLatin1Char('/'))
    return aString;
  return aString + QLatin1Char('/');
}

static auto windowsScopes(AddLibraryWizard::Platforms scopes) -> QString
{
  QString scopesString;
  QTextStream str(&scopesString);
  auto windowsPlatforms = scopes & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
  if (windowsPlatforms == AddLibraryWizard::WindowsMinGWPlatform)
    str << "win32-g++"; // mingw only
  else if (windowsPlatforms == AddLibraryWizard::WindowsMSVCPlatform)
    str << "win32:!win32-g++"; // msvc only
  else if (windowsPlatforms)
    str << "win32"; // both mingw and msvc
  return scopesString;
}

static auto commonScopes(AddLibraryWizard::Platforms scopes, AddLibraryWizard::Platforms excludedScopes) -> QString
{
  QString scopesString;
  QTextStream str(&scopesString);
  auto common = scopes | excludedScopes;
  auto unixLikeScopes = false;
  if (scopes & ~QFlags<AddLibraryWizard::Platform>(AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform)) {
    unixLikeScopes = true;
    if (common & AddLibraryWizard::LinuxPlatform) {
      str << "unix";
      if (!(common & AddLibraryWizard::MacPlatform))
        str << ":!macx";
    } else {
      if (scopes & AddLibraryWizard::MacPlatform)
        str << "macx";
    }
  }
  auto windowsPlatforms = scopes & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
  if (windowsPlatforms) {
    if (unixLikeScopes)
      str << "|";
    str << windowsScopes(windowsPlatforms);
  }
  return scopesString;
}

static auto generateLibsSnippet(AddLibraryWizard::Platforms platforms, AddLibraryWizard::MacLibraryType macLibraryType, const QString &libName, const QString &targetRelativePath, const QString &pwd, bool useSubfolders, bool addSuffix, bool generateLibPath) -> QString
{
  const QDir targetRelativeDir(targetRelativePath);
  QString libraryPathSnippet;
  if (targetRelativeDir.isRelative()) {
    // it contains: $$[pwd]/
    libraryPathSnippet = QLatin1String("$$") + pwd + QLatin1Char('/');
  }

  auto commonPlatforms = platforms;
  if (macLibraryType == AddLibraryWizard::FrameworkType) // we will generate a separate -F -framework line
    commonPlatforms &= ~QFlags<AddLibraryWizard::Platform>(AddLibraryWizard::MacPlatform);
  if (useSubfolders || addSuffix) // we will generate a separate debug/release conditions
    commonPlatforms &= ~QFlags<AddLibraryWizard::Platform>(AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);

  auto diffPlatforms = platforms ^ commonPlatforms;
  AddLibraryWizard::Platforms generatedPlatforms;

  QString snippetMessage;
  QTextStream str(&snippetMessage);

  auto windowsPlatforms = diffPlatforms & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
  if (windowsPlatforms) {
    auto windowsString = windowsScopes(windowsPlatforms);
    str << windowsString << ":CONFIG(release, debug|release): LIBS += ";
    if (useSubfolders) {
      if (generateLibPath)
        str << "-L" << libraryPathSnippet << smartQuote(targetRelativePath + QLatin1String("release/")) << ' ';
      str << "-l" << libName << "\n";
    } else if (addSuffix) {
      if (generateLibPath)
        str << "-L" << libraryPathSnippet << smartQuote(targetRelativePath) << ' ';
      str << "-l" << libName << "\n";
    }

    str << "else:" << windowsString << ":CONFIG(debug, debug|release): LIBS += ";
    if (useSubfolders) {
      if (generateLibPath)
        str << "-L" << libraryPathSnippet << smartQuote(targetRelativePath + QLatin1String("debug/")) << ' ';
      str << "-l" << libName << "\n";
    } else if (addSuffix) {
      if (generateLibPath)
        str << "-L" << libraryPathSnippet << smartQuote(targetRelativePath) << ' ';
      str << "-l" << libName << "d\n";
    }
    generatedPlatforms |= windowsPlatforms;
  }
  if (diffPlatforms & AddLibraryWizard::MacPlatform) {
    if (generatedPlatforms)
      str << "else:";
    str << "mac: LIBS += ";
    if (generateLibPath)
      str << "-F" << libraryPathSnippet << smartQuote(targetRelativePath) << ' ';
    str << "-framework " << libName << "\n";
    generatedPlatforms |= AddLibraryWizard::MacPlatform;
  }

  if (commonPlatforms) {
    if (generatedPlatforms)
      str << "else:";
    str << commonScopes(commonPlatforms, generatedPlatforms) << ": LIBS += ";
    if (generateLibPath)
      str << "-L" << libraryPathSnippet << smartQuote(targetRelativePath) << ' ';
    str << "-l" << libName << "\n";
  }
  return snippetMessage;
}

static auto generateIncludePathSnippet(const QString &includeRelativePath) -> QString
{
  const QDir includeRelativeDir(includeRelativePath);
  QString includePathSnippet;
  if (includeRelativeDir.isRelative()) {
    includePathSnippet = QLatin1String("$$PWD/");
  }
  includePathSnippet += smartQuote(includeRelativePath) + QLatin1Char('\n');

  return QLatin1String("\nINCLUDEPATH += ") + includePathSnippet + QLatin1String("DEPENDPATH += ") + includePathSnippet;
}

static auto generatePreTargetDepsSnippet(AddLibraryWizard::Platforms platforms, AddLibraryWizard::LinkageType linkageType, const QString &libName, const QString &targetRelativePath, const QString &pwd, bool useSubfolders, bool addSuffix) -> QString
{
  if (linkageType != AddLibraryWizard::StaticLinkage)
    return QString();

  const QDir targetRelativeDir(targetRelativePath);

  QString preTargetDepsSnippet = QLatin1String("PRE_TARGETDEPS += ");
  if (targetRelativeDir.isRelative()) {
    // it contains: PRE_TARGETDEPS += $$[pwd]/
    preTargetDepsSnippet += QLatin1String("$$") + pwd + QLatin1Char('/');
  }

  QString snippetMessage;
  QTextStream str(&snippetMessage);
  str << "\n";
  AddLibraryWizard::Platforms generatedPlatforms;
  auto windowsPlatforms = platforms & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
  auto commonPlatforms = platforms;
  if (useSubfolders || addSuffix) // we will generate a separate debug/release conditions, otherwise mingw is unix like
    commonPlatforms &= ~QFlags<AddLibraryWizard::Platform>(AddLibraryWizard::WindowsMinGWPlatform);
  commonPlatforms &= ~QFlags<AddLibraryWizard::Platform>(AddLibraryWizard::WindowsMSVCPlatform); // this case is different from all platforms
  if (windowsPlatforms) {
    if (useSubfolders || addSuffix) {
      if (windowsPlatforms & AddLibraryWizard::WindowsMinGWPlatform) {
        str << "win32-g++:CONFIG(release, debug|release): " << preTargetDepsSnippet;
        if (useSubfolders)
          str << smartQuote(targetRelativePath + QLatin1String("release/lib") + libName + QLatin1String(".a")) << '\n';
        else if (addSuffix)
          str << smartQuote(targetRelativePath + QLatin1String("lib") + libName + QLatin1String(".a")) << '\n';

        str << "else:win32-g++:CONFIG(debug, debug|release): " << preTargetDepsSnippet;
        if (useSubfolders)
          str << smartQuote(targetRelativePath + QLatin1String("debug/lib") + libName + QLatin1String(".a")) << '\n';
        else if (addSuffix)
          str << smartQuote(targetRelativePath + QLatin1String("lib") + libName + QLatin1String("d.a")) << '\n';
      }
      if (windowsPlatforms & AddLibraryWizard::WindowsMSVCPlatform) {
        if (windowsPlatforms & AddLibraryWizard::WindowsMinGWPlatform)
          str << "else:";
        str << "win32:!win32-g++:CONFIG(release, debug|release): " << preTargetDepsSnippet;
        if (useSubfolders)
          str << smartQuote(targetRelativePath + QLatin1String("release/") + libName + QLatin1String(".lib")) << '\n';
        else if (addSuffix)
          str << smartQuote(targetRelativePath + libName + QLatin1String(".lib")) << '\n';

        str << "else:win32:!win32-g++:CONFIG(debug, debug|release): " << preTargetDepsSnippet;
        if (useSubfolders)
          str << smartQuote(targetRelativePath + QLatin1String("debug/") + libName + QLatin1String(".lib")) << '\n';
        else if (addSuffix)
          str << smartQuote(targetRelativePath + libName + QLatin1String("d.lib")) << '\n';
      }
      generatedPlatforms |= windowsPlatforms;
    } else {
      if (windowsPlatforms & AddLibraryWizard::WindowsMSVCPlatform) {
        str << "win32:!win32-g++: " << preTargetDepsSnippet << smartQuote(targetRelativePath + libName + QLatin1String(".lib")) << "\n";
        generatedPlatforms |= AddLibraryWizard::WindowsMSVCPlatform; // mingw will be handled with common scopes
      }
      // mingw not generated yet, will be joined with unix like
    }
  }
  if (commonPlatforms) {
    if (generatedPlatforms)
      str << "else:";
    str << commonScopes(commonPlatforms, generatedPlatforms) << ": " << preTargetDepsSnippet << smartQuote(targetRelativePath + QLatin1String("lib") + libName + QLatin1String(".a")) << "\n";
  }
  return snippetMessage;
}

NonInternalLibraryDetailsController::NonInternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : LibraryDetailsController(libraryDetails, proFile, parent)
{
  setLibraryComboBoxVisible(false);
  setLibraryPathChooserVisible(true);

  connect(libraryDetailsWidget()->libraryPathChooser, &PathChooser::validChanged, this, &LibraryDetailsController::completeChanged);
  connect(libraryDetailsWidget()->libraryPathChooser, &PathChooser::rawPathChanged, this, &NonInternalLibraryDetailsController::slotLibraryPathChanged);
  connect(libraryDetailsWidget()->removeSuffixCheckBox, &QAbstractButton::toggled, this, &NonInternalLibraryDetailsController::slotRemoveSuffixChanged);
  connect(libraryDetailsWidget()->dynamicRadio, &QAbstractButton::clicked, this, &NonInternalLibraryDetailsController::slotLinkageTypeChanged);
  connect(libraryDetailsWidget()->staticRadio, &QAbstractButton::clicked, this, &NonInternalLibraryDetailsController::slotLinkageTypeChanged);
  connect(libraryDetailsWidget()->libraryTypeComboBox, &QComboBox::currentTextChanged, this, &NonInternalLibraryDetailsController::slotLibraryTypeChanged);
  handleLibraryTypeChange();
}

auto NonInternalLibraryDetailsController::suggestedLinkageType() const -> AddLibraryWizard::LinkageType
{
  auto type = AddLibraryWizard::NoLinkage;
  if (libraryPlatformType() != OsTypeWindows) {
    if (libraryDetailsWidget()->libraryPathChooser->isValid()) {
      QFileInfo fi(libraryDetailsWidget()->libraryPathChooser->filePath().toString());
      if (fi.suffix() == QLatin1String("a"))
        type = AddLibraryWizard::StaticLinkage;
      else
        type = AddLibraryWizard::DynamicLinkage;
    }
  }
  return type;
}

auto NonInternalLibraryDetailsController::suggestedMacLibraryType() const -> AddLibraryWizard::MacLibraryType
{
  auto type = AddLibraryWizard::NoLibraryType;
  if (libraryPlatformType() == OsTypeMac) {
    if (libraryDetailsWidget()->libraryPathChooser->isValid()) {
      QFileInfo fi(libraryDetailsWidget()->libraryPathChooser->filePath().toString());
      if (fi.suffix() == QLatin1String("framework"))
        type = AddLibraryWizard::FrameworkType;
      else
        type = AddLibraryWizard::LibraryType;
    }
  }
  return type;
}

auto NonInternalLibraryDetailsController::suggestedIncludePath() const -> QString
{
  QString includePath;
  if (libraryDetailsWidget()->libraryPathChooser->isValid()) {
    QFileInfo fi(libraryDetailsWidget()->libraryPathChooser->filePath().toString());
    includePath = fi.absolutePath();
    QFileInfo dfi(includePath);
    // TODO: Win: remove debug or release folder first if appropriate
    if (dfi.fileName() == QLatin1String("lib")) {
      auto dir = dfi.absoluteDir();
      includePath = dir.absolutePath();
      QDir includeDir(dir.absoluteFilePath(QLatin1String("include")));
      if (includeDir.exists())
        includePath = includeDir.absolutePath();
    }
  }
  return includePath;
}

auto NonInternalLibraryDetailsController::updateWindowsOptionsEnablement() -> void
{
  bool ena = platforms() & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
  if (libraryPlatformType() == OsTypeWindows) {
    libraryDetailsWidget()->addSuffixCheckBox->setEnabled(ena);
    ena = true;
  }
  libraryDetailsWidget()->winGroupBox->setEnabled(ena);
}

auto NonInternalLibraryDetailsController::handleLinkageTypeChange() -> void
{
  if (isMacLibraryRadiosVisible() && libraryDetailsWidget()->staticRadio->isChecked()) {
    setIgnoreGuiSignals(true);
    libraryDetailsWidget()->libraryRadio->setChecked(true);
    setIgnoreGuiSignals(false);
  }
}

auto NonInternalLibraryDetailsController::slotLinkageTypeChanged() -> void
{
  if (guiSignalsIgnored())
    return;

  handleLinkageTypeChange();
  updateGui();
}

auto NonInternalLibraryDetailsController::slotRemoveSuffixChanged(bool ena) -> void
{
  if (ena) {
    libraryDetailsWidget()->useSubfoldersCheckBox->setChecked(false);
    libraryDetailsWidget()->addSuffixCheckBox->setChecked(false);
  }
}

auto NonInternalLibraryDetailsController::handleLibraryTypeChange() -> void
{
  libraryDetailsWidget()->libraryPathChooser->setPromptDialogFilter(libraryPlatformFilter());
  const auto isMacOs = libraryPlatformType() == OsTypeMac;
  const auto isWindows = libraryPlatformType() == OsTypeWindows;
  libraryDetailsWidget()->libraryPathChooser->setExpectedKind(isMacOs ? PathChooser::Any : PathChooser::File);
  setMacLibraryRadiosVisible(!isMacOs);
  setLinkageRadiosVisible(isWindows);
  setRemoveSuffixVisible(isWindows);
  handleLibraryPathChange();
  handleLinkageTypeChange();
}

auto NonInternalLibraryDetailsController::slotLibraryTypeChanged() -> void
{
  handleLibraryTypeChange();
  updateGui();
  emit completeChanged();
}

auto NonInternalLibraryDetailsController::handleLibraryPathChange() -> void
{
  if (libraryPlatformType() == OsTypeWindows) {
    auto subfoldersEnabled = true;
    auto removeSuffixEnabled = true;
    if (libraryDetailsWidget()->libraryPathChooser->isValid()) {
      QFileInfo fi(libraryDetailsWidget()->libraryPathChooser->filePath().toString());
      QFileInfo dfi(fi.absolutePath());
      const auto parentFolderName = dfi.fileName().toLower();
      if (parentFolderName != QLatin1String("debug") && parentFolderName != QLatin1String("release"))
        subfoldersEnabled = false;
      const auto baseName = fi.completeBaseName();

      if (baseName.isEmpty() || baseName.at(baseName.size() - 1).toLower() != QLatin1Char('d'))
        removeSuffixEnabled = false;

      if (subfoldersEnabled)
        libraryDetailsWidget()->useSubfoldersCheckBox->setChecked(true);
      else if (removeSuffixEnabled)
        libraryDetailsWidget()->removeSuffixCheckBox->setChecked(true);
      else
        libraryDetailsWidget()->addSuffixCheckBox->setChecked(true);
    }
  }
}

auto NonInternalLibraryDetailsController::slotLibraryPathChanged() -> void
{
  handleLibraryPathChange();
  updateGui();
  emit completeChanged();
}

auto NonInternalLibraryDetailsController::isComplete() const -> bool
{
  return libraryDetailsWidget()->libraryPathChooser->isValid() && platforms();
}

auto NonInternalLibraryDetailsController::snippet() const -> QString
{
  QString libPath = libraryDetailsWidget()->libraryPathChooser->filePath().toString();
  QFileInfo fi(libPath);
  QString libName;
  const bool removeSuffix = isWindowsGroupVisible() && libraryDetailsWidget()->removeSuffixCheckBox->isChecked();
  if (libraryPlatformType() == OsTypeWindows) {
    libName = fi.completeBaseName();
    if (removeSuffix && !libName.isEmpty()) // remove last letter which needs to be "d"
      libName = libName.left(libName.size() - 1);
    if (fi.completeSuffix() == QLatin1String("a")) // the mingw lib case
      libName = libName.mid(3);                    // cut the "lib" prefix
  } else if (libraryPlatformType() == OsTypeMac) {
    if (macLibraryType() == AddLibraryWizard::FrameworkType)
      libName = fi.completeBaseName();
    else
      libName = fi.completeBaseName().mid(3); // cut the "lib" prefix
  } else {
    libName = fi.completeBaseName().mid(3); // cut the "lib" prefix
  }

  auto useSubfolders = false;
  auto addSuffix = false;
  if (isWindowsGroupVisible()) {
    // when we are on Win but we don't generate the code for Win
    // we still need to remove "debug" or "release" subfolder
    const auto useSubfoldersCondition = (libraryPlatformType() == OsTypeWindows) ? true : platforms() & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform);
    if (useSubfoldersCondition)
      useSubfolders = libraryDetailsWidget()->useSubfoldersCheckBox->isChecked();
    if (platforms() & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform))
      addSuffix = libraryDetailsWidget()->addSuffixCheckBox->isChecked() || removeSuffix;
  }

  QString targetRelativePath;
  QString includeRelativePath;
  if (isIncludePathVisible()) {
    // generate also the path to lib
    auto pfi = proFile().toFileInfo();
    auto pdir = pfi.absoluteDir();
    auto absoluteLibraryPath = fi.absolutePath();
    if (libraryPlatformType() == OsTypeWindows && useSubfolders) {
      // drop last subfolder which needs to be "debug" or "release"
      QFileInfo libfi(absoluteLibraryPath);
      absoluteLibraryPath = libfi.absolutePath();
    }
    targetRelativePath = appendSeparator(pdir.relativeFilePath(absoluteLibraryPath));

    const QString includePath = libraryDetailsWidget()->includePathChooser->filePath().toString();
    if (!includePath.isEmpty())
      includeRelativePath = pdir.relativeFilePath(includePath);
  }

  QString snippetMessage;
  QTextStream str(&snippetMessage);
  str << "\n";
  str << generateLibsSnippet(platforms(), macLibraryType(), libName, targetRelativePath, QLatin1String("PWD"), useSubfolders, addSuffix, isIncludePathVisible());
  if (isIncludePathVisible()) {
    str << generateIncludePathSnippet(includeRelativePath);
    str << generatePreTargetDepsSnippet(platforms(), linkageType(), libName, targetRelativePath, QLatin1String("PWD"), useSubfolders, addSuffix);
  }
  return snippetMessage;
}

/////////////

PackageLibraryDetailsController::PackageLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : NonInternalLibraryDetailsController(libraryDetails, proFile, parent)
{
  setPlatformsVisible(false);
  setIncludePathVisible(false);
  setWindowsGroupVisible(false);
  setLinkageGroupVisible(false);
  setMacLibraryGroupVisible(false);
  setLibraryPathChooserVisible(false);
  setPackageLineEditVisible(true);

  connect(libraryDetailsWidget()->packageLineEdit, &QLineEdit::textChanged, this, &LibraryDetailsController::completeChanged);

  updateGui();
}

auto PackageLibraryDetailsController::isComplete() const -> bool
{
  return !libraryDetailsWidget()->packageLineEdit->text().isEmpty();
}

auto PackageLibraryDetailsController::snippet() const -> QString
{
  QString snippetMessage;
  QTextStream str(&snippetMessage);
  str << "\n";
  if (!isLinkPackageGenerated())
    str << "unix: CONFIG += link_pkgconfig\n";
  str << "unix: PKGCONFIG += " << libraryDetailsWidget()->packageLineEdit->text() << "\n";
  return snippetMessage;
}

auto PackageLibraryDetailsController::isLinkPackageGenerated() const -> bool
{
  const Project *project = SessionManager::projectForFile(proFile());
  if (!project)
    return false;

  const ProjectNode *projectNode = project->findNodeForBuildKey(proFile().toString());
  if (!projectNode)
    return false;

  auto currentProject = dynamic_cast<const QmakeProFileNode*>(projectNode);
  if (!currentProject)
    return false;

  const auto configVar = currentProject->variableValue(Variable::Config);
  if (configVar.contains(QLatin1String("link_pkgconfig")))
    return true;

  return false;
}

/////////////

SystemLibraryDetailsController::SystemLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : NonInternalLibraryDetailsController(libraryDetails, proFile, parent)
{
  setIncludePathVisible(false);
  setWindowsGroupVisible(false);

  updateGui();
}

/////////////

ExternalLibraryDetailsController::ExternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : NonInternalLibraryDetailsController(libraryDetails, proFile, parent)
{
  setIncludePathVisible(true);
  setWindowsGroupVisible(true);

  updateGui();
}

auto ExternalLibraryDetailsController::updateWindowsOptionsEnablement() -> void
{
  NonInternalLibraryDetailsController::updateWindowsOptionsEnablement();

  auto subfoldersEnabled = true;
  auto removeSuffixEnabled = true;
  if (libraryPlatformType() == OsTypeWindows && libraryDetailsWidget()->libraryPathChooser->isValid()) {
    QFileInfo fi(libraryDetailsWidget()->libraryPathChooser->filePath().toString());
    QFileInfo dfi(fi.absolutePath());
    const auto parentFolderName = dfi.fileName().toLower();
    if (parentFolderName != QLatin1String("debug") && parentFolderName != QLatin1String("release"))
      subfoldersEnabled = false;
    const auto baseName = fi.completeBaseName();

    if (baseName.isEmpty() || baseName.at(baseName.size() - 1).toLower() != QLatin1Char('d'))
      removeSuffixEnabled = false;
  }
  libraryDetailsWidget()->useSubfoldersCheckBox->setEnabled(subfoldersEnabled);
  libraryDetailsWidget()->removeSuffixCheckBox->setEnabled(removeSuffixEnabled);
}

/////////////

InternalLibraryDetailsController::InternalLibraryDetailsController(Ui::LibraryDetailsWidget *libraryDetails, const FilePath &proFile, QObject *parent) : LibraryDetailsController(libraryDetails, proFile, parent)
{
  setLinkageRadiosVisible(false);
  setLibraryPathChooserVisible(false);
  setLibraryComboBoxVisible(true);
  setIncludePathVisible(true);
  setWindowsGroupVisible(true);
  setRemoveSuffixVisible(false);

  if (HostOsInfo::isWindowsHost())
    libraryDetailsWidget()->useSubfoldersCheckBox->setEnabled(true);

  connect(libraryDetailsWidget()->libraryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InternalLibraryDetailsController::slotCurrentLibraryChanged);

  updateProFile();
  updateGui();
}

auto InternalLibraryDetailsController::suggestedLinkageType() const -> AddLibraryWizard::LinkageType
{
  const int currentIndex = libraryDetailsWidget()->libraryComboBox->currentIndex();
  auto type = AddLibraryWizard::NoLinkage;
  if (currentIndex >= 0) {
    auto proFile = m_proFiles.at(currentIndex);
    const auto configVar = proFile->variableValue(Variable::Config);
    if (configVar.contains(QLatin1String("staticlib")) || configVar.contains(QLatin1String("static")))
      type = AddLibraryWizard::StaticLinkage;
    else
      type = AddLibraryWizard::DynamicLinkage;
  }
  return type;
}

auto InternalLibraryDetailsController::suggestedMacLibraryType() const -> AddLibraryWizard::MacLibraryType
{
  const int currentIndex = libraryDetailsWidget()->libraryComboBox->currentIndex();
  auto type = AddLibraryWizard::NoLibraryType;
  if (currentIndex >= 0) {
    auto proFile = m_proFiles.at(currentIndex);
    const auto configVar = proFile->variableValue(Variable::Config);
    if (configVar.contains(QLatin1String("lib_bundle")))
      type = AddLibraryWizard::FrameworkType;
    else
      type = AddLibraryWizard::LibraryType;
  }
  return type;
}

auto InternalLibraryDetailsController::suggestedIncludePath() const -> QString
{
  const int currentIndex = libraryDetailsWidget()->libraryComboBox->currentIndex();
  if (currentIndex >= 0) {
    auto proFile = m_proFiles.at(currentIndex);
    return proFile->filePath().toFileInfo().absolutePath();
  }
  return QString();
}

auto InternalLibraryDetailsController::updateWindowsOptionsEnablement() -> void
{
  if (HostOsInfo::isWindowsHost())
    libraryDetailsWidget()->addSuffixCheckBox->setEnabled(true);
  libraryDetailsWidget()->winGroupBox->setEnabled(platforms() & (AddLibraryWizard::WindowsMinGWPlatform | AddLibraryWizard::WindowsMSVCPlatform));
}

auto InternalLibraryDetailsController::updateProFile() -> void
{
  m_rootProjectPath.clear();
  m_proFiles.clear();
  libraryDetailsWidget()->libraryComboBox->clear();

  const QmakeProject *project = dynamic_cast<QmakeProject*>(SessionManager::projectForFile(proFile()));
  if (!project)
    return;

  setIgnoreGuiSignals(true);

  m_rootProjectPath = project->projectDirectory().toString();

  auto t = project->activeTarget();
  auto bs = dynamic_cast<QmakeBuildSystem*>(t ? t->buildSystem() : nullptr);
  QTC_ASSERT(bs, return);

  QDir rootDir(m_rootProjectPath);
  foreach(QmakeProFile *proFile, bs->rootProFile()->allProFiles()) {
    auto type = proFile->projectType();
    if (type != ProjectType::SharedLibraryTemplate && type != ProjectType::StaticLibraryTemplate)
      continue;

    const auto configVar = proFile->variableValue(Variable::Config);
    if (!configVar.contains(QLatin1String("plugin"))) {
      const auto relProFilePath = rootDir.relativeFilePath(proFile->filePath().toString());
      auto targetInfo = proFile->targetInformation();
      const auto itemToolTip = QString::fromLatin1("%1 (%2)").arg(targetInfo.target).arg(relProFilePath);
      m_proFiles.append(proFile);

      libraryDetailsWidget()->libraryComboBox->addItem(targetInfo.target);
      libraryDetailsWidget()->libraryComboBox->setItemData(libraryDetailsWidget()->libraryComboBox->count() - 1, itemToolTip, Qt::ToolTipRole);
    }
  }

  setIgnoreGuiSignals(false);
}

auto InternalLibraryDetailsController::slotCurrentLibraryChanged() -> void
{
  const int currentIndex = libraryDetailsWidget()->libraryComboBox->currentIndex();
  if (currentIndex >= 0) {
    libraryDetailsWidget()->libraryComboBox->setToolTip(libraryDetailsWidget()->libraryComboBox->itemData(currentIndex, Qt::ToolTipRole).toString());
    auto proFile = m_proFiles.at(currentIndex);
    const auto configVar = proFile->variableValue(Variable::Config);
    if (HostOsInfo::isWindowsHost()) {
      auto useSubfolders = false;
      if (configVar.contains(QLatin1String("debug_and_release")) && configVar.contains(QLatin1String("debug_and_release_target")))
        useSubfolders = true;
      libraryDetailsWidget()->useSubfoldersCheckBox->setChecked(useSubfolders);
      libraryDetailsWidget()->addSuffixCheckBox->setChecked(!useSubfolders);
    }
  }

  if (guiSignalsIgnored())
    return;

  updateGui();

  emit completeChanged();
}

auto InternalLibraryDetailsController::isComplete() const -> bool
{
  return libraryDetailsWidget()->libraryComboBox->count() && platforms();
}

auto InternalLibraryDetailsController::snippet() const -> QString
{
  const int currentIndex = libraryDetailsWidget()->libraryComboBox->currentIndex();

  if (currentIndex < 0)
    return QString();

  if (m_rootProjectPath.isEmpty())
    return QString();

  // dir of the root project
  QDir rootDir(m_rootProjectPath);

  // relative path for the project for which we insert the snippet,
  // it's relative to the root project
  const auto proRelavitePath = rootDir.relativeFilePath(proFile().toString());

  // project for which we insert the snippet

  // the build directory of the active build configuration
  auto rootBuildDir = rootDir; // If the project is unconfigured use the project dir
  if (const Project *project = SessionManager::projectForFile(proFile())) {
    if (auto t = project->activeTarget())
      if (auto bc = t->activeBuildConfiguration())
        rootBuildDir.setPath(bc->buildDirectory().toString());
  }

  // the project for which we insert the snippet inside build tree
  QFileInfo pfi(rootBuildDir.filePath(proRelavitePath));
  // the project dir for which we insert the snippet inside build tree
  QDir projectBuildDir(pfi.absolutePath());

  // current project node from combobox
  auto fi = proFile().toFileInfo();
  QDir projectSrcDir(fi.absolutePath());

  // project node which we want to link against
  auto targetInfo = m_proFiles.at(currentIndex)->targetInformation();

  const auto targetRelativePath = appendSeparator(projectBuildDir.relativeFilePath(targetInfo.buildDir.toString()));
  const auto includeRelativePath = projectSrcDir.relativeFilePath(libraryDetailsWidget()->includePathChooser->filePath().toString());

  const bool useSubfolders = libraryDetailsWidget()->useSubfoldersCheckBox->isChecked();
  const bool addSuffix = libraryDetailsWidget()->addSuffixCheckBox->isChecked();

  QString snippetMessage;
  QTextStream str(&snippetMessage);
  str << "\n";

  // replace below to "PRI_OUT_PWD" when task QTBUG-13057 is done
  // (end enable adding libraries into .pri files as well).
  const QString outPwd = QLatin1String("OUT_PWD");
  str << generateLibsSnippet(platforms(), macLibraryType(), targetInfo.target, targetRelativePath, outPwd, useSubfolders, addSuffix, true);
  str << generateIncludePathSnippet(includeRelativePath);
  str << generatePreTargetDepsSnippet(platforms(), linkageType(), targetInfo.target, targetRelativePath, outPwd, useSubfolders, addSuffix);
  return snippetMessage;
}

} // Internal
} // QmakeProjectManager
