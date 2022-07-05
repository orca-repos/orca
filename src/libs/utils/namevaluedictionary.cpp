// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "algorithm.h"
#include "namevaluedictionary.h"
#include "qtcassert.h"

#include <QDir>

namespace Utils {

NameValueDictionary::NameValueDictionary(const QStringList &env, OsType osType) : m_osType(osType)
{
  for (const QString &s : env) {
    int i = s.indexOf('=', 1);
    if (i >= 0) {
      const QString key = s.left(i);
      if (!key.contains('=')) {
        const QString value = s.mid(i + 1);
        set(key, value);
      }
    }
  }
}

NameValueDictionary::NameValueDictionary(const NameValuePairs &nameValues)
{
  for (const auto &nameValue : nameValues)
    set(nameValue.first, nameValue.second);
}

auto NameValueDictionary::findKey(const QString &key) -> NameValueMap::iterator
{
  for (auto it = m_values.begin(); it != m_values.end(); ++it) {
    if (key.compare(it.key().name, nameCaseSensitivity()) == 0)
      return it;
  }
  return m_values.end();
}

auto NameValueDictionary::findKey(const QString &key) const -> NameValueMap::const_iterator
{
  for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
    if (key.compare(it.key().name, nameCaseSensitivity()) == 0)
      return it;
  }
  return m_values.constEnd();
}

auto NameValueDictionary::toStringList() const -> QStringList
{
  QStringList result;
  for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
    if (it.value().second)
      result.append(it.key().name + '=' + it.value().first);
  }
  return result;
}

auto NameValueDictionary::set(const QString &key, const QString &value, bool enabled) -> void
{
  QTC_ASSERT(!key.contains('='), return);
  const auto it = findKey(key);
  const auto valuePair = qMakePair(value, enabled);
  if (it == m_values.end())
    m_values.insert(DictKey(key, nameCaseSensitivity()), valuePair);
  else
    it.value() = valuePair;
}

auto NameValueDictionary::unset(const QString &key) -> void
{
  QTC_ASSERT(!key.contains('='), return);
  const auto it = findKey(key);
  if (it != m_values.end())
    m_values.erase(it);
}

auto NameValueDictionary::clear() -> void
{
  m_values.clear();
}

auto NameValueDictionary::value(const QString &key) const -> QString
{
  const auto it = findKey(key);
  return it != m_values.end() && it.value().second ? it.value().first : QString();
}

auto NameValueDictionary::constFind(const QString &name) const -> NameValueDictionary::const_iterator
{
  return findKey(name);
}

auto NameValueDictionary::size() const -> int
{
  return m_values.size();
}

auto NameValueDictionary::modify(const NameValueItems &items) -> void
{
  NameValueDictionary resultKeyValueDictionary = *this;
  for (const NameValueItem &item : items)
    item.apply(&resultKeyValueDictionary);
  *this = resultKeyValueDictionary;
}

auto NameValueDictionary::diff(const NameValueDictionary &other, bool checkAppendPrepend) const -> NameValueItems
{
  NameValueMap::const_iterator thisIt = constBegin();
  NameValueMap::const_iterator otherIt = other.constBegin();

  NameValueItems result;
  while (thisIt != constEnd() || otherIt != other.constEnd()) {
    if (thisIt == constEnd()) {
      result.append({other.key(otherIt), other.value(otherIt), otherIt.value().second ? NameValueItem::SetEnabled : NameValueItem::SetDisabled});
      ++otherIt;
    } else if (otherIt == other.constEnd()) {
      result.append(NameValueItem(key(thisIt), QString(), NameValueItem::Unset));
      ++thisIt;
    } else if (thisIt.key() < otherIt.key()) {
      result.append(NameValueItem(key(thisIt), QString(), NameValueItem::Unset));
      ++thisIt;
    } else if (thisIt.key() > otherIt.key()) {
      result.append({other.key(otherIt), otherIt.value().first, otherIt.value().second ? NameValueItem::SetEnabled : NameValueItem::SetDisabled});
      ++otherIt;
    } else {
      const QString &oldValue = thisIt.value().first;
      const QString &newValue = otherIt.value().first;
      const bool oldEnabled = thisIt.value().second;
      const bool newEnabled = otherIt.value().second;
      if (oldValue != newValue || oldEnabled != newEnabled) {
        if (checkAppendPrepend && newValue.startsWith(oldValue) && oldEnabled == newEnabled) {
          QString appended = newValue.right(newValue.size() - oldValue.size());
          if (appended.startsWith(OsSpecificAspects::pathListSeparator(osType())))
            appended.remove(0, 1);
          result.append(NameValueItem(other.key(otherIt), appended, NameValueItem::Append));
        } else if (checkAppendPrepend && newValue.endsWith(oldValue) && oldEnabled == newEnabled) {
          QString prepended = newValue.left(newValue.size() - oldValue.size());
          if (prepended.endsWith(OsSpecificAspects::pathListSeparator(osType())))
            prepended.chop(1);
          result.append(NameValueItem(other.key(otherIt), prepended, NameValueItem::Prepend));
        } else {
          result.append({other.key(otherIt), newValue, newEnabled ? NameValueItem::SetEnabled : NameValueItem::SetDisabled});
        }
      }
      ++otherIt;
      ++thisIt;
    }
  }
  return result;
}

auto NameValueDictionary::hasKey(const QString &key) const -> bool
{
  return findKey(key) != constEnd();
}

auto NameValueDictionary::osType() const -> OsType
{
  return m_osType;
}

auto NameValueDictionary::nameCaseSensitivity() const -> Qt::CaseSensitivity
{
  return OsSpecificAspects::envVarCaseSensitivity(osType());
}

auto NameValueDictionary::userName() const -> QString
{
  return value(QString::fromLatin1(m_osType == OsTypeWindows ? "USERNAME" : "USER"));
}

} // namespace Utils
