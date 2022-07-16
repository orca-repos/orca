// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-external-editor-interface.hpp>
#include <utils/filepath.hpp>

#include <QStringList>
#include <QMap>

#include <functional>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

namespace QtSupport {
class QtVersion;
}

namespace QmakeProjectManager {
namespace Internal {

/* Convenience parametrizable base class for Qt editors/binaries
 * Provides convenience functions that
 * try to retrieve the binary of the editor from the Qt version
 * of the project the file belongs to, falling back to path search
 * if none is found. On Mac, the "open" mechanism can be optionally be used. */

class ExternalQtEditor : public Orca::Plugin::Core::IExternalEditor {
  Q_OBJECT

public:
  // Member function pointer for a QtVersion function return a string (command)
  using CommandForQtVersion = std::function<QString(const QtSupport::QtVersion *)>;

  static auto createLinguistEditor() -> ExternalQtEditor*;
  static auto createDesignerEditor() -> ExternalQtEditor*;

  auto startEditor(const Utils::FilePath &filePath, QString *errorMessage) -> bool override;

  // Data required to launch the editor
  struct LaunchData {
    QString binary;
    QStringList arguments;
    Utils::FilePath workingDirectory;
  };

protected:
  ExternalQtEditor(Utils::Id id, const QString &displayName, const QString &mimetype, const CommandForQtVersion &commandForQtVersion);

  // Try to retrieve the binary of the editor from the Qt version,
  // prepare arguments accordingly (Mac "open" if desired)
  auto getEditorLaunchData(const Utils::FilePath &filePath, LaunchData *data, QString *errorMessage) const -> bool;

  // Create and start a detached GUI process executing in the background.
  // Set the project environment if there is one.
  auto startEditorProcess(const LaunchData &data, QString *errorMessage) -> bool;

private:
  const CommandForQtVersion m_commandForQtVersion;
};

/* Qt Designer on the remaining platforms: Uses Designer's own
 * Tcp-based communication mechanism to ensure all files are opened
 * in one instance (per version). */

class DesignerExternalEditor : public ExternalQtEditor {
  Q_OBJECT public:
  DesignerExternalEditor();

  auto startEditor(const Utils::FilePath &filePath, QString *errorMessage) -> bool override;

private:
  auto processTerminated(const QString &binary) -> void;

  // A per-binary entry containing the socket
  using ProcessCache = QMap<QString, QTcpSocket*>;

  ProcessCache m_processCache;
};

} // namespace Internal
} // namespace QmakeProjectManager
