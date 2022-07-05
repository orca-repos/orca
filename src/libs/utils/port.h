// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "qtcassert.h"

#include <QMetaType>
#include <QString>

#include <limits>

namespace Utils {

class ORCA_UTILS_EXPORT Port {
public:
  Port() = default;
  explicit Port(quint16 port) : m_port(port) {}
  explicit Port(int port) : m_port((port < 0 || port > std::numeric_limits<quint16>::max()) ? -1 : port) { }
  explicit Port(uint port) : m_port(port > std::numeric_limits<quint16>::max() ? -1 : port) { }

  auto number() const -> quint16
  {
    QTC_ASSERT(isValid(), return -1);
    return quint16(m_port);
  }

  auto isValid() const -> bool { return m_port != -1; }
  auto toString() const -> QString { return QString::number(m_port); }

private:
  int m_port = -1;
};

inline auto operator<(const Port &p1, const Port &p2) -> bool { return p1.number() < p2.number(); }
inline auto operator<=(const Port &p1, const Port &p2) -> bool { return p1.number() <= p2.number(); }
inline auto operator>(const Port &p1, const Port &p2) -> bool { return p1.number() > p2.number(); }
inline auto operator>=(const Port &p1, const Port &p2) -> bool { return p1.number() >= p2.number(); }

inline auto operator==(const Port &p1, const Port &p2) -> bool
{
  return p1.isValid() ? (p2.isValid() && p1.number() == p2.number()) : !p2.isValid();
}

inline auto operator!=(const Port &p1, const Port &p2) -> bool
{
  return p1.isValid() ? (!p2.isValid() || p1.number() != p2.number()) : p2.isValid();
}

} // Utils

Q_DECLARE_METATYPE(Utils::Port)
