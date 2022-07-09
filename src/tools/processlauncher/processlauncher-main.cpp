// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0


#include "launcherlogging.hpp"
#include "launchersockethandler.hpp"
#include "singleton.hpp"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qscopeguard.h>
#include <QtCore/qtimer.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>

auto WINAPI consoleCtrlHandler(DWORD) -> BOOL
{
    // Ignore Ctrl-C / Ctrl-Break. QtCreator will tell us to exit gracefully.
    return TRUE;
}
#endif

auto main(int argc, char *argv[]) -> int
{
#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

    QCoreApplication app(argc, argv);
    if (QCoreApplication::arguments().size() != 2) {
        Utils::Internal::logError("Need exactly one argument (path to socket)");
        return 1;
    }

    auto cleanup = qScopeGuard([] { Utils::Singleton::deleteAll(); });

    const Utils::Internal::LauncherSocketHandler launcher(QCoreApplication::arguments().constLast());
    QTimer::singleShot(0, &launcher, &Utils::Internal::LauncherSocketHandler::start);
    return QCoreApplication::exec();
}
