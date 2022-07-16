// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <projectexplorer/runcontrol.hpp>

#include <utils/portlist.hpp>

namespace ProjectExplorer {

namespace Internal {
class DeviceUsedPortsGathererPrivate;
class SubChannelProvider;
} // Internal

class PROJECTEXPLORER_EXPORT DeviceUsedPortsGatherer : public QObject {
  Q_OBJECT

public:
  DeviceUsedPortsGatherer(QObject *parent = nullptr);
  ~DeviceUsedPortsGatherer() override;

  auto start(const IDevice::ConstPtr &device) -> void;
  auto stop() -> void;
  auto getNextFreePort(Utils::PortList *freePorts) const -> Utils::Port; // returns -1 if no more are left
  auto usedPorts() const -> QList<Utils::Port>;

signals:
  auto error(const QString &errMsg) -> void;
  auto portListReady() -> void;

private:
  auto handleRemoteStdOut() -> void;
  auto handleRemoteStdErr() -> void;
  auto handleProcessError() -> void;
  auto handleProcessFinished() -> void;
  auto setupUsedPorts() -> void;

  Internal::DeviceUsedPortsGathererPrivate *const d;
};

class PROJECTEXPLORER_EXPORT PortsGatherer : public RunWorker {
  Q_OBJECT

public:
  explicit PortsGatherer(RunControl *runControl);
  ~PortsGatherer() override;

  auto findEndPoint() -> QUrl;

protected:
  auto start() -> void override;
  auto stop() -> void override;

private:
  DeviceUsedPortsGatherer m_portsGatherer;
  Utils::PortList m_portList;
};

class PROJECTEXPLORER_EXPORT ChannelForwarder : public RunWorker {
  Q_OBJECT

public:
  explicit ChannelForwarder(RunControl *runControl);

  using UrlGetter = std::function<QUrl()>;

  auto setFromUrlGetter(const UrlGetter &urlGetter) -> void;
  auto fromUrl() const -> QUrl { return m_fromUrl; }
  auto toUrl() const -> QUrl { return m_toUrl; }

private:
  UrlGetter m_fromUrlGetter;
  QUrl m_fromUrl;
  QUrl m_toUrl;
};

class PROJECTEXPLORER_EXPORT ChannelProvider : public RunWorker {
  Q_OBJECT

public:
  ChannelProvider(RunControl *runControl, int requiredChannels = 1);
  ~ChannelProvider() override;

  auto channel(int i = 0) const -> QUrl;

private:
  QVector<Internal::SubChannelProvider*> m_channelProviders;
};

} // namespace ProjectExplorer
