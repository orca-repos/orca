// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "portlist.hpp"

#include <QPair>
#include <QString>
#include <QStringList>

#include <cctype>

namespace Utils {
namespace Internal {
namespace {

using Range = QPair<Port, Port>;

class PortsSpecParser {
  struct ParseException {
    ParseException(const char *error) : error(error) {}
    const char *const error;
  };

public:
  PortsSpecParser(const QString &portsSpec) : m_pos(0), m_portsSpec(portsSpec) { }

  /*
   * Grammar: Spec -> [ ElemList ]
   *          ElemList -> Elem [ ',' ElemList ]
   *          Elem -> Port [ '-' Port ]
   */
  auto parse() -> PortList
  {
    try {
      if (!atEnd())
        parseElemList();
    } catch (const ParseException &e) {
      qWarning("Malformed ports specification: %s", e.error);
    }
    return m_portList;
  }

private:
  auto parseElemList() -> void
  {
    if (atEnd())
      throw ParseException("Element list empty.");
    parseElem();
    if (atEnd())
      return;
    if (nextChar() != ',') {
      throw ParseException("Element followed by something else " "than a comma.");
    }
    ++m_pos;
    parseElemList();
  }

  auto parseElem() -> void
  {
    const Port startPort = parsePort();
    if (atEnd() || nextChar() != '-') {
      m_portList.addPort(startPort);
      return;
    }
    ++m_pos;
    const Port endPort = parsePort();
    if (endPort < startPort)
      throw ParseException("Invalid range (end < start).");
    m_portList.addRange(startPort, endPort);
  }

  auto parsePort() -> Port
  {
    if (atEnd())
      throw ParseException("Empty port string.");
    int port = 0;
    do {
      const char next = nextChar();
      if (!std::isdigit(next))
        break;
      port = 10 * port + next - '0';
      ++m_pos;
    } while (!atEnd());
    if (port == 0 || port >= 2 << 16)
      throw ParseException("Invalid port value.");
    return Port(port);
  }

  auto atEnd() const -> bool { return m_pos == m_portsSpec.length(); }
  auto nextChar() const -> char { return m_portsSpec.at(m_pos).toLatin1(); }

  PortList m_portList;
  int m_pos;
  const QString &m_portsSpec;
};

} // anonymous namespace

class PortListPrivate {
public:
  QList<Range> ranges;
};

} // namespace Internal

PortList::PortList() : d(new Internal::PortListPrivate) {}

PortList::PortList(const PortList &other) : d(new Internal::PortListPrivate(*other.d)) {}

PortList::~PortList()
{
  delete d;
}

auto PortList::operator=(const PortList &other) -> PortList&
{
  *d = *other.d;
  return *this;
}

auto PortList::fromString(const QString &portsSpec) -> PortList
{
  return Internal::PortsSpecParser(portsSpec).parse();
}

auto PortList::addPort(Port port) -> void { addRange(port, port); }

auto PortList::addRange(Port startPort, Port endPort) -> void
{
  d->ranges << Internal::Range(startPort, endPort);
}

auto PortList::hasMore() const -> bool { return !d->ranges.isEmpty(); }

auto PortList::contains(Port port) const -> bool
{
  for (const Internal::Range &r : qAsConst(d->ranges)) {
    if (port >= r.first && port <= r.second)
      return true;
  }
  return false;
}

auto PortList::count() const -> int
{
  int n = 0;
  for (const Internal::Range &r : qAsConst(d->ranges))
    n += r.second.number() - r.first.number() + 1;
  return n;
}

auto PortList::getNext() -> Port
{
  Q_ASSERT(!d->ranges.isEmpty());

  Internal::Range &firstRange = d->ranges.first();
  const Port next = firstRange.first;
  firstRange.first = Port(firstRange.first.number() + 1);
  if (firstRange.first > firstRange.second)
    d->ranges.removeFirst();
  return next;
}

auto PortList::toString() const -> QString
{
  QString stringRep;
  for (const Internal::Range &range : qAsConst(d->ranges)) {
    stringRep += QString::number(range.first.number());
    if (range.second != range.first)
      stringRep += QLatin1Char('-') + QString::number(range.second.number());
    stringRep += QLatin1Char(',');
  }
  if (!stringRep.isEmpty())
    stringRep.remove(stringRep.length() - 1, 1); // Trailing comma.
  return stringRep;
}

auto PortList::regularExpression() -> QString
{
  const QLatin1String portExpr("(\\d)+");
  const QString listElemExpr = QString::fromLatin1("%1(-%1)?").arg(portExpr);
  return QString::fromLatin1("((%1)(,%1)*)?").arg(listElemExpr);
}

} // namespace Utils
