// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "externaleditors.hpp"

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcprocess.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/session.hpp>
#include <qtsupport/qtkitinformation.hpp>
#include <constants/designer/designerconstants.hpp>

#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>

using namespace ProjectExplorer;
using namespace Utils;

enum {
  debug = 0
};

namespace QmakeProjectManager {
namespace Internal {

// ------------ Messages
static auto msgStartFailed(const QString &binary, QStringList arguments) -> QString
{
  arguments.push_front(binary);
  return ExternalQtEditor::tr("Unable to start \"%1\"").arg(arguments.join(QLatin1Char(' ')));
}

static auto msgAppNotFound(const QString &id) -> QString
{
  return ExternalQtEditor::tr("The application \"%1\" could not be found.").arg(id);
}

// -- Commands and helpers
static auto linguistBinary(const QtSupport::QtVersion *qtVersion) -> QString
{
  if (qtVersion)
    return qtVersion->linguistFilePath().toString();
  return QLatin1String(Utils::HostOsInfo::isMacHost() ? "Linguist" : "linguist");
}

static auto designerBinary(const QtSupport::QtVersion *qtVersion) -> QString
{
  if (qtVersion)
    return qtVersion->designerFilePath().toString();
  return QLatin1String(Utils::HostOsInfo::isMacHost() ? "Designer" : "designer");
}

// Mac: Change the call 'Foo.app/Contents/MacOS/Foo <filelist>' to
// 'open -a Foo.app <filelist>'. doesn't support generic command line arguments
static auto createMacOpenCommand(const ExternalQtEditor::LaunchData &data) -> ExternalQtEditor::LaunchData
{
  auto openData = data;
  const int appFolderIndex = data.binary.lastIndexOf(QLatin1String("/Contents/MacOS/"));
  if (appFolderIndex != -1) {
    openData.binary = "open";
    openData.arguments = QStringList({QString("-a"), data.binary.left(appFolderIndex)}) + data.arguments;
  }
  return openData;
}

static const char designerIdC[] = "Qt.Designer";
static const char linguistIdC[] = "Qt.Linguist";

static const char designerDisplayName[] = QT_TRANSLATE_NOOP("OpenWith::Editors", "Qt Designer");
static const char linguistDisplayName[] = QT_TRANSLATE_NOOP("OpenWith::Editors", "Qt Linguist");

// -------------- ExternalQtEditor
ExternalQtEditor::ExternalQtEditor(Utils::Id id, const QString &displayName, const QString &mimetype, const CommandForQtVersion &commandForQtVersion) : m_commandForQtVersion(commandForQtVersion)
{
  setId(id);
  setDisplayName(displayName);
  setMimeTypes({mimetype});
}

auto ExternalQtEditor::createLinguistEditor() -> ExternalQtEditor*
{
  return new ExternalQtEditor(linguistIdC, QLatin1String(linguistDisplayName), QLatin1String(ProjectExplorer::Constants::LINGUIST_MIMETYPE), linguistBinary);
}

auto ExternalQtEditor::createDesignerEditor() -> ExternalQtEditor*
{
  if (Utils::HostOsInfo::isMacHost()) {
    return new ExternalQtEditor(designerIdC, QLatin1String(designerDisplayName), QLatin1String(ProjectExplorer::Constants::FORM_MIMETYPE), designerBinary);
  } else {
    return new DesignerExternalEditor;
  }
}

static auto findFirstCommand(QVector<QtSupport::QtVersion*> qtVersions, ExternalQtEditor::CommandForQtVersion command) -> QString
{
  foreach(QtSupport::QtVersion *qt, qtVersions) {
    if (qt) {
      const auto binary = command(qt);
      if (!binary.isEmpty())
        return binary;
    }
  }
  return QString();
}

auto ExternalQtEditor::getEditorLaunchData(const Utils::FilePath &filePath, LaunchData *data, QString *errorMessage) const -> bool
{
  // Check in order for Qt version with the binary:
  // - active kit of project
  // - any other of the project
  // - default kit
  // - any other kit
  // As fallback check PATH
  data->workingDirectory.clear();
  QVector<QtSupport::QtVersion*> qtVersionsToCheck; // deduplicated after being filled
  if (const Project *project = SessionManager::projectForFile(filePath)) {
    data->workingDirectory = project->projectDirectory();
    // active kit
    if (const Target *target = project->activeTarget()) {
      qtVersionsToCheck << QtSupport::QtKitAspect::qtVersion(target->kit());
    }
    // all kits of project
    qtVersionsToCheck += Utils::transform<QVector>(project->targets(), [](Target *t) {
      return QTC_GUARD(t) ? QtSupport::QtKitAspect::qtVersion(t->kit()) : nullptr;
    });
  }
  // default kit
  qtVersionsToCheck << QtSupport::QtKitAspect::qtVersion(KitManager::defaultKit());
  // all kits
  qtVersionsToCheck += Utils::transform<QVector>(KitManager::kits(), QtSupport::QtKitAspect::qtVersion);
  qtVersionsToCheck = Utils::filteredUnique(qtVersionsToCheck); // can still contain nullptr
  data->binary = findFirstCommand(qtVersionsToCheck, m_commandForQtVersion);
  // fallback
  if (data->binary.isEmpty())
    data->binary = Utils::QtcProcess::locateBinary(m_commandForQtVersion(nullptr));
  if (data->binary.isEmpty()) {
    *errorMessage = msgAppNotFound(id().toString());
    return false;
  }
  // Setup binary + arguments, use Mac Open if appropriate
  data->arguments.push_back(filePath.toString());
  if (Utils::HostOsInfo::isMacHost())
    *data = createMacOpenCommand(*data);
  if (debug)
    qDebug() << Q_FUNC_INFO << '\n' << data->binary << data->arguments;
  return true;
}

auto ExternalQtEditor::startEditor(const Utils::FilePath &filePath, QString *errorMessage) -> bool
{
  LaunchData data;
  return getEditorLaunchData(filePath, &data, errorMessage) && startEditorProcess(data, errorMessage);
}

auto ExternalQtEditor::startEditorProcess(const LaunchData &data, QString *errorMessage) -> bool
{
  if (debug)
    qDebug() << Q_FUNC_INFO << '\n' << data.binary << data.arguments << data.workingDirectory;
  qint64 pid = 0;
  if (!QtcProcess::startDetached({FilePath::fromString(data.binary), data.arguments}, data.workingDirectory, &pid)) {
    *errorMessage = msgStartFailed(data.binary, data.arguments);
    return false;
  }
  return true;
}

// --------------- DesignerExternalEditor with Designer Tcp remote control.
DesignerExternalEditor::DesignerExternalEditor() : ExternalQtEditor(designerIdC, QLatin1String(designerDisplayName), QLatin1String(Designer::Constants::FORM_MIMETYPE), designerBinary) {}

auto DesignerExternalEditor::processTerminated(const QString &binary) -> void
{
  const auto it = m_processCache.find(binary);
  if (it == m_processCache.end())
    return;
  // Make sure socket is closed and cleaned, remove from cache
  auto socket = it.value();
  m_processCache.erase(it); // Note that closing will cause the slot to be retriggered
  if (debug)
    qDebug() << Q_FUNC_INFO << '\n' << binary << socket->state();
  if (socket->state() == QAbstractSocket::ConnectedState)
    socket->close();
  socket->deleteLater();
}

auto DesignerExternalEditor::startEditor(const Utils::FilePath &filePath, QString *errorMessage) -> bool
{
  LaunchData data;
  // Find the editor binary
  if (!getEditorLaunchData(filePath, &data, errorMessage)) {
    return false;
  }
  // Known one?
  const auto it = m_processCache.find(data.binary);
  if (it != m_processCache.end()) {
    // Process is known, write to its socket to cause it to open the file
    if (debug)
      qDebug() << Q_FUNC_INFO << "\nWriting to socket:" << data.binary << filePath;
    auto socket = it.value();
    if (!socket->write(filePath.toString().toUtf8() + '\n')) {
      *errorMessage = tr("Qt Designer is not responding (%1).").arg(socket->errorString());
      return false;
    }
    return true;
  }
  // No process yet. Create socket & launch the process
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost)) {
    *errorMessage = tr("Unable to create server socket: %1").arg(server.errorString());
    return false;
  }
  const auto port = server.serverPort();
  if (debug)
    qDebug() << Q_FUNC_INFO << "\nLaunching server:" << port << data.binary << filePath;
  // Start first one with file and socket as '-client port file'
  // Wait for the socket listening
  data.arguments.push_front(QString::number(port));
  data.arguments.push_front(QLatin1String("-client"));

  if (!startEditorProcess(data, errorMessage))
    return false;
  // Insert into cache if socket is created, else try again next time
  if (server.waitForNewConnection(3000)) {
    auto socket = server.nextPendingConnection();
    socket->setParent(this);
    const auto binary = data.binary;
    m_processCache.insert(binary, socket);
    auto mapSlot = [this, binary] { processTerminated(binary); };
    connect(socket, &QAbstractSocket::disconnected, this, mapSlot);
    #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const auto errorOccurred = QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error);
    #else
    const auto errorOccurred = &QAbstractSocket::errorOccurred;
    #endif
    connect(socket, errorOccurred, this, mapSlot);
  }
  return true;
}

} // namespace Internal
} // namespace QmakeProjectManager
