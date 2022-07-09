// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "dynamiccapabilities.hpp"

#include <QTime>
#include <QWidget>

#include <languageserverprotocol/basemessage.h>
#include <languageserverprotocol/servercapabilities.h>

#include <list>

namespace LanguageClient {

class LspLogMessage {
public:
  enum MessageSender {
    ClientMessage,
    ServerMessage
  } sender = ClientMessage;

  LspLogMessage();
  LspLogMessage(MessageSender sender, const QTime &time, const LanguageServerProtocol::BaseMessage &message);

  QTime time;
  LanguageServerProtocol::BaseMessage message;

  auto id() const -> LanguageServerProtocol::MessageId;
  auto displayText() const -> QString;
  auto json() const -> QJsonObject&;

private:
  mutable Utils::optional<LanguageServerProtocol::MessageId> m_id;
  mutable Utils::optional<QString> m_displayText;
  mutable Utils::optional<QJsonObject> m_json;
};

struct Capabilities {
  LanguageServerProtocol::ServerCapabilities capabilities;
  DynamicCapabilities dynamicCapabilities;
};

class LspInspector : public QObject {
  Q_OBJECT

public:
  LspInspector() {}

  auto createWidget(const QString &defaultClient = {}) -> QWidget*;
  auto log(const LspLogMessage::MessageSender sender, const QString &clientName, const LanguageServerProtocol::BaseMessage &message) -> void;
  auto clientInitialized(const QString &clientName, const LanguageServerProtocol::ServerCapabilities &capabilities) -> void;
  auto updateCapabilities(const QString &clientName, const DynamicCapabilities &dynamicCapabilities) -> void;
  auto messages(const QString &clientName) const -> std::list<LspLogMessage>;
  auto capabilities(const QString &clientName) const -> Capabilities;
  auto clients() const -> QList<QString>;
  auto clear() -> void { m_logs.clear(); }

signals:
  auto newMessage(const QString &clientName, const LspLogMessage &message) -> void;
  auto capabilitiesUpdated(const QString &clientName) -> void;

private:
  QMap<QString, std::list<LspLogMessage>> m_logs;
  QMap<QString, Capabilities> m_capabilities;
  int m_logSize = 100; // default log size if no widget is currently visible
};

} // namespace LanguageClient
