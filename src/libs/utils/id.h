// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "porting.h"

#include <QList>
#include <QMetaType>
#include <QString>

QT_BEGIN_NAMESPACE
class QDataStream;
class QVariant;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT Id {
public:
  Id() = default;
  Id(const char *name); // Good to use.
  Id(const QLatin1String &) = delete;

  auto withSuffix(int suffix) const -> Id;
  auto withSuffix(const char *suffix) const -> Id;
  auto withSuffix(const QString &suffix) const -> Id;
  auto withPrefix(const char *prefix) const -> Id;
  auto name() const -> QByteArray;
  auto toString() const -> QString;   // Avoid.
  auto toSetting() const -> QVariant; // Good to use.
  auto suffixAfter(Id baseId) const -> QString;
  auto isValid() const -> bool { return m_id; }
  auto operator==(Id id) const -> bool { return m_id == id.m_id; }
  auto operator==(const char *name) const -> bool;
  auto operator!=(Id id) const -> bool { return m_id != id.m_id; }
  auto operator!=(const char *name) const -> bool { return !operator==(name); }
  auto operator<(Id id) const -> bool { return m_id < id.m_id; }
  auto operator>(Id id) const -> bool { return m_id > id.m_id; }
  auto alphabeticallyBefore(Id other) const -> bool;
  auto uniqueIdentifier() const -> quintptr { return m_id; } // Avoid.
  static auto fromString(const QString &str) -> Id;          // FIXME: avoid.
  static auto fromName(const QByteArray &ba) -> Id;          // FIXME: avoid.
  static auto fromSetting(const QVariant &variant) -> Id;    // Good to use.
  static auto versionedId(const QByteArray &prefix, int major, int minor = -1) -> Id;
  static auto fromStringList(const QStringList &list) -> QSet<Id>;
  static auto toStringList(const QSet<Id> &ids) -> QStringList;

  friend auto qHash(Id id) -> QHashValueType { return static_cast<QHashValueType>(id.uniqueIdentifier()); }
  friend ORCA_UTILS_EXPORT auto operator<<(QDataStream &ds, Utils::Id id) -> QDataStream&;
  friend ORCA_UTILS_EXPORT auto operator>>(QDataStream &ds, Utils::Id &id) -> QDataStream&;
  friend ORCA_UTILS_EXPORT auto operator<<(QDebug dbg, const Utils::Id &id) -> QDebug;

private:
  explicit Id(quintptr uid) : m_id(uid) {}

  quintptr m_id = 0;
};

} // namespace Utils

Q_DECLARE_METATYPE(Utils::Id)
Q_DECLARE_METATYPE(QList<Utils::Id>)
