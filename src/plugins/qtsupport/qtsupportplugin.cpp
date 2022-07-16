// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtsupportplugin.hpp"

#include "codegenerator.hpp"
#include "codegensettingspage.hpp"
#include "gettingstartedwelcomepage.hpp"
#include "profilereader.hpp"
#include "qscxmlcgenerator.hpp"
#include "qtkitinformation.hpp"
#include "qtoptionspage.hpp"
#include "qtoutputformatter.hpp"
#include "qtsupportconstants.hpp"
#include "qtversionmanager.hpp"
#include "qtversions.hpp"
#include "translationwizardpage.hpp"
#include "uicgenerator.hpp"

#include <core/core-interface.hpp>
#include <core/core-js-expander.hpp>

#include <projectexplorer/jsonwizard/jsonwizardfactory.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/runcontrol.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>

#include <utils/infobar.hpp>
#include <utils/macroexpander.hpp>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;

namespace QtSupport {
namespace Internal {

class QtSupportPluginPrivate {
public:
  QtVersionManager qtVersionManager;
  DesktopQtVersionFactory desktopQtVersionFactory;
  EmbeddedLinuxQtVersionFactory embeddedLinuxQtVersionFactory;
  CodeGenSettingsPage codeGenSettingsPage;
  QtOptionsPage qtOptionsPage;
  ExamplesWelcomePage examplesPage{true};
  ExamplesWelcomePage tutorialPage{false};
  QtKitAspect qtKiAspect;
  QtOutputFormatterFactory qtOutputFormatterFactory;
  UicGeneratorFactory uicGeneratorFactory;
  QScxmlcGeneratorFactory qscxmlcGeneratorFactory;
};

QtSupportPlugin::~QtSupportPlugin()
{
  delete d;
}

auto QtSupportPlugin::initialize(const QStringList &arguments, QString *errorMessage) -> bool
{
  Q_UNUSED(arguments)
  Q_UNUSED(errorMessage)
  QMakeParser::initialize();
  ProFileEvaluator::initialize();
  new ProFileCacheManager(this);

  JsExpander::registerGlobalObject<CodeGenerator>("QtSupport");
  JsonWizardFactory::registerPageFactory(new TranslationWizardPageFactory);
  ProjectExplorerPlugin::showQtSettings();

  d = new QtSupportPluginPrivate;

  QtVersionManager::initialized();

  return true;
}

const char kLinkWithQtInstallationSetting[] = "LinkWithQtInstallation";

static auto askAboutQtInstallation() -> void
{
  // if the install settings exist, the Qt Creator installation is (probably) already linked to
  // a Qt installation, so don't ask
  if (!QtOptionsPage::canLinkWithQt() || QtOptionsPage::isLinkedWithQt() || !ICore::infoBar()->canInfoBeAdded(kLinkWithQtInstallationSetting))
    return;

  Utils::InfoBarEntry info(kLinkWithQtInstallationSetting, QtSupportPlugin::tr("Link with a Qt installation to automatically register Qt versions and kits? To do " "this later, select Options > Kits > Qt Versions > Link with Qt."), Utils::InfoBarEntry::GlobalSuppression::Enabled);
  info.addCustomButton(QtSupportPlugin::tr("Link with Qt"), [] {
    ICore::infoBar()->removeInfo(kLinkWithQtInstallationSetting);
    QTimer::singleShot(0, ICore::dialogParent(), &QtOptionsPage::linkWithQt);
  });
  ICore::infoBar()->addInfo(info);
}

auto QtSupportPlugin::extensionsInitialized() -> void
{
  const auto expander = Utils::globalMacroExpander();

  static const auto currentQtVersion = []() -> const QtVersion* {
    const auto project = ProjectTree::currentProject();
    if (!project || !project->activeTarget())
      return nullptr;
    return QtKitAspect::qtVersion(project->activeTarget()->kit());
  };
  static const char kCurrentHostBins[] = "CurrentDocument:Project:QT_HOST_BINS";
  expander->registerVariable(kCurrentHostBins, tr("Full path to the host bin directory of the Qt version in the active kit " "of the project containing the current document."), []() {
    const auto qt = currentQtVersion();
    return qt ? qt->hostBinPath().toUserOutput() : QString();
  });

  expander->registerVariable("CurrentDocument:Project:QT_INSTALL_BINS", tr("Full path to the target bin directory of the Qt version in the active kit " "of the project containing the current document.<br>You probably want %1 instead.").arg(QString::fromLatin1(kCurrentHostBins)), []() {
    const auto qt = currentQtVersion();
    return qt ? qt->binPath().toUserOutput() : QString();
  });

  expander->registerVariable("CurrentDocument:Project:QT_HOST_LIBEXECS", tr("Full path to the host libexec directory of the Qt version in the active kit " "of the project containing the current document."), []() {
    const auto qt = currentQtVersion();
    return qt ? qt->hostLibexecPath().toUserOutput() : QString();
  });

  static const auto activeQtVersion = []() -> const QtVersion* {
    const auto project = SessionManager::startupProject();
    if (!project || !project->activeTarget())
      return nullptr;
    return QtKitAspect::qtVersion(project->activeTarget()->kit());
  };
  static const char kActiveHostBins[] = "ActiveProject:QT_HOST_BINS";
  expander->registerVariable(kActiveHostBins, tr("Full path to the host bin directory of the Qt version in the active kit " "of the active project."), []() {
    const auto qt = activeQtVersion();
    return qt ? qt->hostBinPath().toUserOutput() : QString();
  });

  expander->registerVariable("ActiveProject:QT_INSTALL_BINS", tr("Full path to the target bin directory of the Qt version in the active kit " "of the active project.<br>You probably want %1 instead.").arg(QString::fromLatin1(kActiveHostBins)), []() {
    const auto qt = activeQtVersion();
    return qt ? qt->binPath().toUserOutput() : QString();
  });

  expander->registerVariable("ActiveProject::QT_HOST_LIBEXECS", tr("Full path to the libexec bin directory of the Qt version in the active kit " "of the active project."), []() {
    const auto qt = activeQtVersion();
    return qt ? qt->hostLibexecPath().toUserOutput() : QString();
  });

  askAboutQtInstallation();
}

} // Internal
} // QtSupport
