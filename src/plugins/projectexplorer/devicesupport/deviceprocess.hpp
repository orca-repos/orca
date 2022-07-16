// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <utils/qtcprocess.hpp>

#include <QSharedPointer>
#include <QStringList>

namespace ProjectExplorer {

class IDevice;
class Runnable;

class PROJECTEXPLORER_EXPORT DeviceProcess : public Utils::QtcProcess {
  Q_OBJECT

public:
  using QtcProcess::start;
  virtual auto start(const Runnable &runnable) -> void = 0;

  auto setRunInTerminal(bool term) -> void { m_runInTerminal = term; }
  auto runInTerminal() const -> bool { return m_runInTerminal; }

protected:
  explicit DeviceProcess(const QSharedPointer<const IDevice> &device, const Setup &setup, QObject *parent = nullptr);
  auto device() const -> QSharedPointer<const IDevice>;

private:
  const QSharedPointer<const IDevice> m_device;
  bool m_runInTerminal = false;
};

} // namespace ProjectExplorer
