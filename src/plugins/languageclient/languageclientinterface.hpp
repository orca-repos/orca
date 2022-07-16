// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/basemessage.h>

#include <utils/qtcprocess.hpp>

#include <QBuffer>

namespace LanguageClient {

class StdIOSettings;

class LANGUAGECLIENT_EXPORT BaseClientInterface : public QObject {
  Q_OBJECT

public:
  BaseClientInterface();
  ~BaseClientInterface() override;

  auto sendMessage(const LanguageServerProtocol::BaseMessage &message) -> void;
  virtual auto start() -> bool { return true; }
  auto resetBuffer() -> void;

signals:
  auto messageReceived(LanguageServerProtocol::BaseMessage message) -> void;
  auto finished() -> void;
  auto error(const QString &message) -> void;

protected:
  virtual auto sendData(const QByteArray &data) -> void = 0;
  auto parseData(const QByteArray &data) -> void;

private:
  QBuffer m_buffer;
  LanguageServerProtocol::BaseMessage m_currentMessage;
};

class LANGUAGECLIENT_EXPORT StdIOClientInterface : public BaseClientInterface {
  Q_OBJECT

public:
  StdIOClientInterface();
  ~StdIOClientInterface() override;
  StdIOClientInterface(const StdIOClientInterface &) = delete;
  StdIOClientInterface(StdIOClientInterface &&) = delete;

  auto operator=(const StdIOClientInterface &) -> StdIOClientInterface& = delete;
  auto operator=(StdIOClientInterface &&) -> StdIOClientInterface& = delete;
  auto start() -> bool override;

  // These functions only have an effect if they are called before start
  auto setCommandLine(const Utils::CommandLine &cmd) -> void;
  auto setWorkingDirectory(const Utils::FilePath &workingDirectory) -> void;

protected:
  auto sendData(const QByteArray &data) -> void final;
  Utils::QtcProcess m_process;

private:
  auto readError() -> void;
  auto readOutput() -> void;
  auto onProcessFinished() -> void;
};

} // namespace LanguageClient
