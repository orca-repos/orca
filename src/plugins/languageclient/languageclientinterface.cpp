// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientinterface.hpp"

#include "languageclientsettings.hpp"

#include <QLoggingCategory>

using namespace LanguageServerProtocol;
using namespace Utils;

static Q_LOGGING_CATEGORY(LOGLSPCLIENTV, "qtc.languageclient.messages", QtWarningMsg);

namespace LanguageClient {

BaseClientInterface::BaseClientInterface()
{
  m_buffer.open(QIODevice::ReadWrite | QIODevice::Append);
}

BaseClientInterface::~BaseClientInterface()
{
  m_buffer.close();
}

auto BaseClientInterface::sendMessage(const BaseMessage &message) -> void
{
  sendData(message.toData());
}

auto BaseClientInterface::resetBuffer() -> void
{
  m_buffer.close();
  m_buffer.setData(nullptr);
  m_buffer.open(QIODevice::ReadWrite | QIODevice::Append);
}

auto BaseClientInterface::parseData(const QByteArray &data) -> void
{
  const auto preWritePosition = m_buffer.pos();
  qCDebug(parseLog) << "parse buffer pos: " << preWritePosition;
  qCDebug(parseLog) << "  data: " << data;
  if (!m_buffer.atEnd())
    m_buffer.seek(preWritePosition + m_buffer.bytesAvailable());
  m_buffer.write(data);
  m_buffer.seek(preWritePosition);
  while (!m_buffer.atEnd()) {
    QString parseError;
    BaseMessage::parse(&m_buffer, parseError, m_currentMessage);
    qCDebug(parseLog) << "  complete: " << m_currentMessage.isComplete();
    qCDebug(parseLog) << "  length: " << m_currentMessage.contentLength;
    qCDebug(parseLog) << "  content: " << m_currentMessage.content;
    if (!parseError.isEmpty()) emit error(parseError);
    if (!m_currentMessage.isComplete())
      break;
    emit messageReceived(m_currentMessage);
    m_currentMessage = BaseMessage();
  }
  if (m_buffer.atEnd()) {
    m_buffer.close();
    m_buffer.setData(nullptr);
    m_buffer.open(QIODevice::ReadWrite | QIODevice::Append);
  }
}

StdIOClientInterface::StdIOClientInterface() : m_process(ProcessMode::Writer)
{
  connect(&m_process, &QtcProcess::readyReadStandardError, this, &StdIOClientInterface::readError);
  connect(&m_process, &QtcProcess::readyReadStandardOutput, this, &StdIOClientInterface::readOutput);
  connect(&m_process, &QtcProcess::finished, this, &StdIOClientInterface::onProcessFinished);
}

StdIOClientInterface::~StdIOClientInterface()
{
  m_process.stopProcess();
}

auto StdIOClientInterface::start() -> bool
{
  m_process.start();
  if (!m_process.waitForStarted() || m_process.state() != QProcess::Running) {
    emit error(m_process.errorString());
    return false;
  }
  return true;
}

auto StdIOClientInterface::setCommandLine(const CommandLine &cmd) -> void
{
  m_process.setCommand(cmd);
}

auto StdIOClientInterface::setWorkingDirectory(const FilePath &workingDirectory) -> void
{
  m_process.setWorkingDirectory(workingDirectory);
}

auto StdIOClientInterface::sendData(const QByteArray &data) -> void
{
  if (m_process.state() != QProcess::Running) {
    emit error(tr("Cannot send data to unstarted server %1").arg(m_process.commandLine().toUserOutput()));
    return;
  }
  qCDebug(LOGLSPCLIENTV) << "StdIOClient send data:";
  qCDebug(LOGLSPCLIENTV).noquote() << data;
  m_process.write(data);
}

auto StdIOClientInterface::onProcessFinished() -> void
{
  if (m_process.exitStatus() == QProcess::CrashExit) emit error(tr("Crashed with exit code %1: %2").arg(m_process.exitCode()).arg(m_process.errorString()));
  emit finished();
}

auto StdIOClientInterface::readError() -> void
{
  qCDebug(LOGLSPCLIENTV) << "StdIOClient std err:\n";
  qCDebug(LOGLSPCLIENTV).noquote() << m_process.readAllStandardError();
}

auto StdIOClientInterface::readOutput() -> void
{
  const auto &out = m_process.readAllStandardOutput();
  qCDebug(LOGLSPCLIENTV) << "StdIOClient std out:\n";
  qCDebug(LOGLSPCLIENTV).noquote() << out;
  parseData(out);
}

} // namespace LanguageClient
