import qbs 1.0

Project {
    name: "QtcSsh"

    QtcLibrary {
        cpp.defines: {
            var defines = base;
            defines.push("QTCSSH_LIBRARY");
            if (project.withAutotests && !defines.contains("WITH_TESTS"))
                defines.push("WITH_TESTS");
            return defines;
        }
        cpp.enableExceptions: true

        Depends { name: "Qt"; submodules: ["widgets", "network" ] }
        Depends { name: "Utils" }

        files: [
            "sftpdefs.cpp",
            "sftpdefs.h",
            "sftpfilesystemmodel.cpp",
            "sftpfilesystemmodel.h",
            "sftpsession.cpp",
            "sftpsession.h",
            "sftptransfer.cpp",
            "sftptransfer.h",
            "ssh.qrc",
            "sshconnection.h",
            "sshconnection.cpp",
            "sshconnectionmanager.cpp",
            "sshconnectionmanager.h",
            "sshkeycreationdialog.cpp",
            "sshkeycreationdialog.h",
            "sshkeycreationdialog.ui",
            "sshlogging.cpp",
            "sshlogging_p.h",
            "sshprocess.cpp",
            "sshprocess.h",
            "sshremoteprocess.cpp",
            "sshremoteprocess.h",
            "sshremoteprocessrunner.cpp",
            "sshremoteprocessrunner.h",
            "sshsettings.cpp",
            "sshsettings.h",
        ]

        Export { Depends { name: "Qt.network" } }
    }
}
