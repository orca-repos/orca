// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/utils_global.h>

#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE
class QFileinfo;
QT_END_NAMESPACE

namespace Utils {

namespace Internal {
class MimeTypeParserBase;
class MimeTypeMapEntry;
class MimeDatabasePrivate;
class MimeXMLProvider;
class MimeBinaryProvider;
class MimeTypePrivate;
}

class ORCA_UTILS_EXPORT MimeType {
public:
  MimeType();
  MimeType(const MimeType &other);
  explicit MimeType(const Internal::MimeTypePrivate &dd);
  ~MimeType();

  auto operator=(const MimeType &other) -> MimeType&;
  auto operator==(const MimeType &other) const -> bool;

  auto operator!=(const MimeType &other) const -> bool
  {
    return !operator==(other);
  }

  auto isValid() const -> bool;
  auto isDefault() const -> bool;
  auto name() const -> QString;
  auto comment() const -> QString;
  auto genericIconName() const -> QString;
  auto iconName() const -> QString;
  auto globPatterns() const -> QStringList;
  auto parentMimeTypes() const -> QStringList;
  auto allAncestors() const -> QStringList;
  auto aliases() const -> QStringList;
  auto suffixes() const -> QStringList;
  auto preferredSuffix() const -> QString;
  auto inherits(const QString &mimeTypeName) const -> bool;
  auto filterString() const -> QString;

  // Orca additions
  auto matchesName(const QString &nameOrAlias) const -> bool;
  auto setPreferredSuffix(const QString &suffix) -> void;

  friend auto qHash(const MimeType &mime) { return qHash(mime.name()); }

protected:
  friend class Internal::MimeTypeParserBase;
  friend class Internal::MimeTypeMapEntry;
  friend class Internal::MimeDatabasePrivate;
  friend class Internal::MimeXMLProvider;
  friend class Internal::MimeBinaryProvider;
  friend class Internal::MimeTypePrivate;

  QExplicitlySharedDataPointer<Internal::MimeTypePrivate> d;
};

} // Utils

//Q_DECLARE_SHARED(Utils::MimeType)

#ifndef QT_NO_DEBUG_STREAM
QT_BEGIN_NAMESPACE
class QDebug;
ORCA_UTILS_EXPORT auto operator<<(QDebug debug, const Utils::MimeType &mime) -> QDebug;
QT_END_NAMESPACE
#endif

