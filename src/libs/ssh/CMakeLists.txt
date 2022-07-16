add_qtc_library(QtcSsh
  DEPENDS Qt6::Core Qt6::Network Qt6::Widgets Utils
  SOURCES
    sftpdefs.cpp sftpdefs.h
    sftpfilesystemmodel.cpp sftpfilesystemmodel.h
    sftpsession.cpp sftpsession.h
    sftptransfer.cpp sftptransfer.h
    ssh.qrc
    ssh_global.h
    sshconnection.cpp sshconnection.h
    sshconnectionmanager.cpp sshconnectionmanager.h
    sshkeycreationdialog.cpp sshkeycreationdialog.h sshkeycreationdialog.ui
    sshlogging.cpp sshlogging_p.h
    sshprocess.cpp sshprocess.h
    sshconnection.cpp sshconnection.h
    sshconnectionmanager.cpp sshconnectionmanager.h
    sshkeycreationdialog.cpp sshkeycreationdialog.h sshkeycreationdialog.ui
    sshlogging.cpp sshlogging_p.h
    sshremoteprocess.cpp sshremoteprocess.h
    sshremoteprocessrunner.cpp sshremoteprocessrunner.h
    sshsettings.cpp sshsettings.h
)
