// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "port.h"

QT_FORWARD_DECLARE_CLASS(QString)

namespace Utils {
namespace Internal { class PortListPrivate; }

class ORCA_UTILS_EXPORT PortList {
public:
  PortList();
  PortList(const PortList &other);
  auto operator=(const PortList &other) -> PortList&;
  ~PortList();

  auto addPort(Port port) -> void;
  auto addRange(Port startPort, Port endPort) -> void;
  auto hasMore() const -> bool;
  auto contains(Port port) const -> bool;
  auto count() const -> int;
  auto getNext() -> Port;
  auto toString() const -> QString;

  static auto fromString(const QString &portsSpec) -> PortList;
  static auto regularExpression() -> QString;

private:
  Internal::PortListPrivate *const d;
};

} // namespace Utils
