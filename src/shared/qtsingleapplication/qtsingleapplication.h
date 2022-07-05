// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QApplication>

QT_FORWARD_DECLARE_CLASS(QSharedMemory)

namespace SharedTools {

class QtLocalPeer;

class QtSingleApplication : public QApplication {
  Q_OBJECT

public:
  QtSingleApplication(const QString &id, int &argc, char **argv);
  ~QtSingleApplication();

  auto isRunning(qint64 pid = -1) -> bool;
  auto setActivationWindow(QWidget *aw, bool activateOnMessage = true) -> void;
  auto activationWindow() const -> QWidget*;
  auto event(QEvent *event) -> bool override;
  auto applicationId() const -> QString;
  auto setBlock(bool value) -> void;
  auto sendMessage(const QString &message, int timeout = 5000, qint64 pid = -1) -> bool;
  auto activateWindow() -> void;

Q_SIGNALS:
  auto messageReceived(const QString &message, QObject *socket) -> void;
  auto fileOpenRequest(const QString &file) -> void;

private:
  auto instancesFileName(const QString &appId) -> QString;

  qint64 firstPeer;
  QSharedMemory *instances;
  QtLocalPeer *pidPeer;
  QWidget *actWin;
  QString appId;
  bool block;
};

} // namespace SharedTools
