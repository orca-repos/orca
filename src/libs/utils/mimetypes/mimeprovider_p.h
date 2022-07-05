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

#include "mimedatabase_p.h"
#include "mimemagicrule_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qset.h>

namespace Utils {
namespace Internal {

class MimeMagicRuleMatcher;

class MimeProviderBase {
public:
  MimeProviderBase(MimeDatabasePrivate *db);
  virtual ~MimeProviderBase() {}

  virtual auto isValid() -> bool = 0;
  virtual auto mimeTypeForName(const QString &name) -> MimeType = 0;
  virtual auto findByFileName(const QString &fileName, QString *foundSuffix) -> QStringList = 0;
  virtual auto parents(const QString &mime) -> QStringList = 0;
  virtual auto resolveAlias(const QString &name) -> QString = 0;
  virtual auto listAliases(const QString &name) -> QStringList = 0;
  virtual auto findByMagic(const QByteArray &data, int *accuracyPtr) -> MimeType = 0;
  virtual auto allMimeTypes() -> QList<MimeType> = 0;
  virtual auto loadMimeTypePrivate(MimeTypePrivate &) -> void {}
  virtual auto loadIcon(MimeTypePrivate &) -> void {}
  virtual auto loadGenericIcon(MimeTypePrivate &) -> void {}

  // Orca additions
  virtual auto magicRulesForMimeType(const MimeType &mimeType) -> QMap<int, QList<MimeMagicRule>> = 0;
  virtual auto setGlobPatternsForMimeType(const MimeType &mimeType, const QStringList &patterns) -> void = 0;
  virtual auto setMagicRulesForMimeType(const MimeType &mimeType, const QMap<int, QList<MimeMagicRule>> &rules) -> void = 0;

  MimeDatabasePrivate *m_db;
protected:
  auto shouldCheck() -> bool;
  QDateTime m_lastCheck;
};

/*
   Parses the raw XML files (slower)
 */
class MimeXMLProvider : public MimeProviderBase {
public:
  MimeXMLProvider(MimeDatabasePrivate *db);

  auto isValid() -> bool override;
  auto mimeTypeForName(const QString &name) -> MimeType override;
  auto findByFileName(const QString &fileName, QString *foundSuffix) -> QStringList override;
  auto parents(const QString &mime) -> QStringList override;
  auto resolveAlias(const QString &name) -> QString override;
  auto listAliases(const QString &name) -> QStringList override;
  auto findByMagic(const QByteArray &data, int *accuracyPtr) -> MimeType override;
  auto allMimeTypes() -> QList<MimeType> override;
  auto load(const QString &fileName, QString *errorMessage) -> bool;

  // Called by the mimetype xml parser
  auto addMimeType(const MimeType &mt) -> void;
  auto addGlobPattern(const MimeGlobPattern &glob) -> void;
  auto addParent(const QString &child, const QString &parent) -> void;
  auto addAlias(const QString &alias, const QString &name) -> void;
  auto addMagicMatcher(const MimeMagicRuleMatcher &matcher) -> void;

  // Orca additions
  auto addData(const QString &id, const QByteArray &data) -> void;
  auto magicRulesForMimeType(const MimeType &mimeType) -> QMap<int, QList<MimeMagicRule>> override;
  auto setGlobPatternsForMimeType(const MimeType &mimeType, const QStringList &patterns) -> void override;
  auto setMagicRulesForMimeType(const MimeType &mimeType, const QMap<int, QList<MimeMagicRule>> &rules) -> void override;

private:
  using NameMimeTypeMap = QHash<QString, MimeType>;
  using AliasHash = QHash<QString, QString>;
  using ParentsHash = QHash<QString, QStringList>;

  auto ensureLoaded() -> void;
  auto load(const QString &fileName) -> void;
  bool m_loaded;
  NameMimeTypeMap m_nameMimeTypeMap;
  AliasHash m_aliases;
  ParentsHash m_parents;
  MimeAllGlobPatterns m_mimeTypeGlobs;
  QList<MimeMagicRuleMatcher> m_magicMatchers;
  QHash<QString, QByteArray> m_additionalData; // id -> data
};

} // Internal
}   // Utils
