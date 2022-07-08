import qbs 1.0

Project {
    name: "ProjectExplorer"

    QtcPlugin {
        Depends { name: "Qt"; submodules: ["widgets", "xml", "network", "qml"] }
        Depends { name: "Aggregation" }
        Depends { name: "QtcSsh" }
        Depends { name: "Utils" }

        Depends { name: "Core" }
        Depends { name: "TextEditor" }
        Depends { name: "app_version_header" }

        Depends { name: "libclang"; required: false }
        Depends { name: "clang_defines" }

        pluginTestDepends: ["GenericProjectManager"]

        Group {
            name: "General"
            files: [
                "abi.cpp", "abi.hpp",
                "abiwidget.cpp", "abiwidget.hpp",
                "abstractprocessstep.cpp", "abstractprocessstep.hpp",
                "addrunconfigdialog.cpp", "addrunconfigdialog.hpp",
                "allprojectsfilter.cpp", "allprojectsfilter.hpp",
                "allprojectsfind.cpp", "allprojectsfind.hpp",
                "applicationlauncher.cpp", "applicationlauncher.hpp",
                "appoutputpane.cpp", "appoutputpane.hpp",
                "baseprojectwizarddialog.cpp", "baseprojectwizarddialog.hpp",
                "buildaspects.cpp", "buildaspects.hpp",
                "buildconfiguration.cpp", "buildconfiguration.hpp",
                "buildinfo.cpp", "buildinfo.hpp",
                "buildmanager.cpp", "buildmanager.hpp",
                "buildprogress.cpp", "buildprogress.hpp",
                "buildpropertiessettings.cpp", "buildpropertiessettings.hpp",
                "buildsettingspropertiespage.cpp", "buildsettingspropertiespage.hpp",
                "buildstep.cpp", "buildstep.hpp",
                "buildsteplist.cpp", "buildsteplist.hpp",
                "buildstepspage.cpp", "buildstepspage.hpp",
                "buildsystem.cpp", "buildsystem.hpp",
                "buildtargetinfo.hpp",
                "buildtargettype.hpp",
                "clangparser.cpp", "clangparser.hpp",
                "codestylesettingspropertiespage.cpp", "codestylesettingspropertiespage.hpp", "codestylesettingspropertiespage.ui",
                "compileoutputwindow.cpp", "compileoutputwindow.hpp",
                "configtaskhandler.cpp", "configtaskhandler.hpp",
                "copytaskhandler.cpp", "copytaskhandler.hpp",
                "currentprojectfilter.cpp", "currentprojectfilter.hpp",
                "currentprojectfind.cpp", "currentprojectfind.hpp",
                "customexecutablerunconfiguration.cpp", "customexecutablerunconfiguration.hpp",
                "customparser.cpp", "customparser.hpp",
                "customparserconfigdialog.cpp", "customparserconfigdialog.hpp", "customparserconfigdialog.ui",
                "customparserssettingspage.cpp", "customparserssettingspage.hpp",
                "customtoolchain.cpp", "customtoolchain.hpp",
                "dependenciespanel.cpp", "dependenciespanel.hpp",
                "deployablefile.cpp", "deployablefile.hpp",
                "deployconfiguration.cpp", "deployconfiguration.hpp",
                "deploymentdata.cpp",
                "deploymentdata.hpp",
                "deploymentdataview.cpp",
                "deploymentdataview.hpp",
                "desktoprunconfiguration.cpp", "desktoprunconfiguration.hpp",
                "editorconfiguration.cpp", "editorconfiguration.hpp",
                "editorsettingspropertiespage.cpp", "editorsettingspropertiespage.hpp", "editorsettingspropertiespage.ui",
                "environmentaspect.cpp", "environmentaspect.hpp",
                "environmentaspectwidget.cpp", "environmentaspectwidget.hpp",
                "environmentwidget.cpp", "environmentwidget.hpp",
                "expanddata.cpp", "expanddata.hpp",
                "extraabi.cpp", "extraabi.hpp",
                "extracompiler.cpp", "extracompiler.hpp",
                "fileinsessionfinder.cpp", "fileinsessionfinder.hpp",
                "filesinallprojectsfind.cpp", "filesinallprojectsfind.hpp",
                "filterkitaspectsdialog.cpp", "filterkitaspectsdialog.hpp",
                "gccparser.cpp", "gccparser.hpp",
                "gcctoolchain.cpp", "gcctoolchain.hpp",
                "gnumakeparser.cpp", "gnumakeparser.hpp",
                "headerpath.hpp",
                "importwidget.cpp", "importwidget.hpp",
                "ioutputparser.cpp", "ioutputparser.hpp",
                "ipotentialkit.hpp",
                "itaskhandler.hpp",
                "kit.cpp", "kit.hpp",
                "kitchooser.cpp", "kitchooser.hpp",
                "kitfeatureprovider.hpp",
                "kitinformation.cpp", "kitinformation.hpp",
                "kitmanager.cpp", "kitmanager.hpp",
                "kitmanagerconfigwidget.cpp", "kitmanagerconfigwidget.hpp",
                "kitmodel.cpp", "kitmodel.hpp",
                "kitoptionspage.cpp", "kitoptionspage.hpp",
                "ldparser.cpp", "ldparser.hpp",
                "lldparser.cpp", "lldparser.hpp",
                "linuxiccparser.cpp", "linuxiccparser.hpp",
                "localenvironmentaspect.cpp", "localenvironmentaspect.hpp",
                "makestep.cpp", "makestep.hpp",
                "miniprojecttargetselector.cpp", "miniprojecttargetselector.hpp",
                "msvcparser.cpp", "msvcparser.hpp",
                "namedwidget.cpp", "namedwidget.hpp",
                "osparser.cpp", "osparser.hpp",
                "panelswidget.cpp", "panelswidget.hpp",
                "parseissuesdialog.cpp", "parseissuesdialog.hpp",
                "processparameters.cpp", "processparameters.hpp",
                "processstep.cpp", "processstep.hpp",
                "project.cpp", "project.hpp",
                "projectconfiguration.cpp", "projectconfiguration.hpp",
                "projectconfigurationmodel.cpp", "projectconfigurationmodel.hpp",
                "projectexplorer.cpp", "projectexplorer.hpp",
                "projectexplorer.qrc",
                "projectexplorer_export.hpp",
                "projectexplorerconstants.cpp",
                "projectexplorerconstants.hpp",
                "projectexplorericons.hpp", "projectexplorericons.cpp",
                "projectexplorersettings.hpp",
                "projectexplorersettingspage.cpp", "projectexplorersettingspage.hpp", "projectexplorersettingspage.ui",
                "projectfilewizardextension.cpp", "projectfilewizardextension.hpp",
                "projectimporter.cpp", "projectimporter.hpp",
                "projectmacro.cpp", "projectmacro.hpp",
                "projectmanager.hpp",
                "projectmodels.cpp", "projectmodels.hpp",
                "projectnodes.cpp", "projectnodes.hpp",
                "projectpanelfactory.cpp", "projectpanelfactory.hpp",
                "projecttree.cpp",
                "projecttree.hpp",
                "projecttreewidget.cpp", "projecttreewidget.hpp",
                "projectwindow.cpp", "projectwindow.hpp",
                "projectwizardpage.cpp", "projectwizardpage.hpp", "projectwizardpage.ui",
                "rawprojectpart.cpp", "rawprojectpart.hpp",
                "removetaskhandler.cpp", "removetaskhandler.hpp",
                "runconfiguration.cpp", "runconfiguration.hpp",
                "runcontrol.cpp", "runcontrol.hpp",
                "runconfigurationaspects.cpp", "runconfigurationaspects.hpp",
                "runsettingspropertiespage.cpp", "runsettingspropertiespage.hpp",
                "selectablefilesmodel.cpp", "selectablefilesmodel.hpp",
                "session.cpp", "session.hpp",
                "sessionmodel.cpp", "sessionmodel.hpp",
                "sessionview.cpp", "sessionview.hpp",
                "sessiondialog.cpp", "sessiondialog.hpp", "sessiondialog.ui",
                "showineditortaskhandler.cpp", "showineditortaskhandler.hpp",
                "showoutputtaskhandler.cpp", "showoutputtaskhandler.hpp",
                "simpleprojectwizard.cpp", "simpleprojectwizard.hpp",
                "target.cpp", "target.hpp",
                "targetsettingspanel.cpp", "targetsettingspanel.hpp",
                "targetsetuppage.cpp", "targetsetuppage.hpp",
                "targetsetupwidget.cpp", "targetsetupwidget.hpp",
                "task.cpp", "task.hpp",
                "taskhub.cpp", "taskhub.hpp",
                "taskmodel.cpp", "taskmodel.hpp",
                "taskwindow.cpp", "taskwindow.hpp",
                "toolchain.cpp", "toolchain.hpp",
                "toolchaincache.hpp",
                "toolchainconfigwidget.cpp", "toolchainconfigwidget.hpp",
                "toolchainmanager.cpp", "toolchainmanager.hpp",
                "toolchainoptionspage.cpp", "toolchainoptionspage.hpp",
                "toolchainsettingsaccessor.cpp", "toolchainsettingsaccessor.hpp",
                "treescanner.cpp", "treescanner.hpp",
                "userfileaccessor.cpp", "userfileaccessor.hpp",
                "vcsannotatetaskhandler.cpp", "vcsannotatetaskhandler.hpp",
                "waitforstopdialog.cpp", "waitforstopdialog.hpp",
                "xcodebuildparser.cpp", "xcodebuildparser.hpp"
            ]
        }

        Group {
            name: "Project Welcome Page"
            files: [
                "projectwelcomepage.cpp",
                "projectwelcomepage.hpp"
            ]
        }

        Group {
            name: "JsonWizard"
            prefix: "jsonwizard/"
            files: [
                "jsonfieldpage.cpp", "jsonfieldpage_p.hpp", "jsonfieldpage.hpp",
                "jsonfilepage.cpp", "jsonfilepage.hpp",
                "jsonkitspage.cpp", "jsonkitspage.hpp",
                "jsonprojectpage.cpp", "jsonprojectpage.hpp",
                "jsonsummarypage.cpp", "jsonsummarypage.hpp",
                "jsonwizard.cpp", "jsonwizard.hpp",
                "jsonwizardfactory.cpp", "jsonwizardfactory.hpp",
                "jsonwizardfilegenerator.cpp", "jsonwizardfilegenerator.hpp",
                "jsonwizardgeneratorfactory.cpp", "jsonwizardgeneratorfactory.hpp",
                "jsonwizardpagefactory.cpp", "jsonwizardpagefactory.hpp",
                "jsonwizardpagefactory_p.cpp", "jsonwizardpagefactory_p.hpp",
                "jsonwizardscannergenerator.cpp", "jsonwizardscannergenerator.hpp",
                "wizarddebug.hpp"
            ]
        }

        Group {
            name: "CustomWizard"
            prefix: "customwizard/"
            files: [
                "customwizard.cpp", "customwizard.hpp",
                "customwizardpage.cpp", "customwizardpage.hpp",
                "customwizardparameters.cpp", "customwizardparameters.hpp",
                "customwizardscriptgenerator.cpp", "customwizardscriptgenerator.hpp"
            ]
        }

        Group {
            name: "Device Support"
            prefix: "devicesupport/"
            files: [
                "desktopdevice.cpp", "desktopdevice.hpp",
                "desktopdevicefactory.cpp", "desktopdevicefactory.hpp",
                "devicecheckbuildstep.cpp", "devicecheckbuildstep.hpp",
                "devicefactoryselectiondialog.cpp", "devicefactoryselectiondialog.hpp", "devicefactoryselectiondialog.ui",
                "devicemanager.cpp", "devicemanager.hpp",
                "devicemanagermodel.cpp", "devicemanagermodel.hpp",
                "deviceprocess.cpp", "deviceprocess.hpp",
                "deviceprocessesdialog.cpp", "deviceprocessesdialog.hpp",
                "deviceprocesslist.cpp", "deviceprocesslist.hpp",
                "devicesettingspage.cpp", "devicesettingspage.hpp",
                "devicesettingswidget.cpp", "devicesettingswidget.hpp", "devicesettingswidget.ui",
                "devicetestdialog.cpp", "devicetestdialog.hpp", "devicetestdialog.ui",
                "deviceusedportsgatherer.cpp", "deviceusedportsgatherer.hpp",
                "idevice.cpp", "idevice.hpp",
                "idevicefactory.cpp", "idevicefactory.hpp",
                "idevicewidget.hpp",
                "desktopdeviceprocess.cpp", "desktopdeviceprocess.hpp",
                "localprocesslist.cpp", "localprocesslist.hpp",
                "sshdeviceprocess.cpp", "sshdeviceprocess.hpp",
                "sshdeviceprocesslist.cpp", "sshdeviceprocesslist.hpp",
                "sshsettingspage.cpp", "sshsettingspage.hpp",
                "desktopprocesssignaloperation.cpp", "desktopprocesssignaloperation.hpp"
            ]
        }

        Group {
            name: "Images"
            prefix: "images/"
            files: ["*.png"]
        }

        Group {
            name: "WindowsToolChains"
            condition: qbs.targetOS.contains("windows") || qtc.testsEnabled
            files: [
                "msvctoolchain.cpp",
                "msvctoolchain.hpp",
                "windebuginterface.cpp",
                "windebuginterface.hpp",
            ]
        }

        Group {
            name: "Tests"
            condition: qtc.testsEnabled
            files: ["outputparser_test.hpp", "outputparser_test.cpp"]
        }

        Group {
            name: "Test resources"
            condition: qtc.testsEnabled
            files: ["testdata/**"]
            fileTags: ["qt.core.resource_data"]
            Qt.core.resourcePrefix: "/projectexplorer"
            Qt.core.resourceSourceBase: path
        }

        Export {
            Depends { name: "Qt.network" }
        }
    }
}
