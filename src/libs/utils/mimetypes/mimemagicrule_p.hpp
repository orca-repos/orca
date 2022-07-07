// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <utils/utils_global.hpp>

#include <QtCore/qbytearray.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>

namespace Utils {

class MimeType;

namespace Internal {

class MimeMagicRulePrivate;

class ORCA_UTILS_EXPORT MimeMagicRule {
public:
  enum Type {
    Invalid = 0,
    String,
    RegExp,
    Host16,
    Host32,
    Big16,
    Big32,
    Little16,
    Little32,
    Byte
  };

  MimeMagicRule(Type type, const QByteArray &value, int startPos, int endPos, const QByteArray &mask = QByteArray(), QString *errorString = nullptr);
  MimeMagicRule(const MimeMagicRule &other);
  ~MimeMagicRule();

  auto operator=(const MimeMagicRule &other) -> MimeMagicRule&;
  auto operator==(const MimeMagicRule &other) const -> bool;
  auto type() const -> Type;
  auto value() const -> QByteArray;
  auto startPos() const -> int;
  auto endPos() const -> int;
  auto mask() const -> QByteArray;
  auto isValid() const -> bool;
  auto matches(const QByteArray &data) const -> bool;

  QList<MimeMagicRule> m_subMatches;

  static auto type(const QByteArray &type) -> Type;
  static auto typeName(Type type) -> QByteArray;
  static auto matchSubstring(const char *dataPtr, int dataSize, int rangeStart, int rangeLength, int valueLength, const char *valueData, const char *mask) -> bool;

private:
  const QScopedPointer<MimeMagicRulePrivate> d;
};

} // Internal
} // Utils

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(Utils::Internal::MimeMagicRule, Q_MOVABLE_TYPE);
QT_END_NAMESPACE
