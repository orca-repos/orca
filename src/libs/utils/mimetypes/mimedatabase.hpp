// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "mimetype.hpp"
#include "mimemagicrule_p.hpp"

#include <utils/utils_global.hpp>

QT_BEGIN_NAMESPACE
class QFileInfo;
QT_END_NAMESPACE

namespace Utils {

class FilePath;

// Wrapped QMimeDataBase functions
ORCA_UTILS_EXPORT auto mimeTypeForName(const QString &nameOrAlias) -> MimeType;

enum class MimeMatchMode {
  MatchDefault = 0x0,
  MatchExtension = 0x1,
  MatchContent = 0x2
};

ORCA_UTILS_EXPORT auto mimeTypeForFile(const QString &fileName, MimeMatchMode mode = MimeMatchMode::MatchDefault) -> MimeType;
ORCA_UTILS_EXPORT auto mimeTypeForFile(const QFileInfo &fileInfo, MimeMatchMode mode = MimeMatchMode::MatchDefault) -> MimeType;
ORCA_UTILS_EXPORT auto mimeTypeForFile(const FilePath &filePath, MimeMatchMode mode = MimeMatchMode::MatchDefault) -> MimeType;
ORCA_UTILS_EXPORT auto mimeTypesForFileName(const QString &fileName) -> QList<MimeType>;
ORCA_UTILS_EXPORT auto mimeTypeForData(const QByteArray &data) -> MimeType;
ORCA_UTILS_EXPORT auto allMimeTypes() -> QList<MimeType>;

// Orca additions
// For debugging purposes.
enum class MimeStartupPhase {
  BeforeInitialize,
  PluginsLoading,
  PluginsInitializing,
  // Register up to here.
  PluginsDelayedInitializing,
  // Use from here on.
  UpAndRunning
};

ORCA_UTILS_EXPORT auto setMimeStartupPhase(MimeStartupPhase) -> void;
ORCA_UTILS_EXPORT auto addMimeTypes(const QString &id, const QByteArray &data) -> void;
ORCA_UTILS_EXPORT auto allFiltersString(QString *allFilesFilter = nullptr) -> QString;
ORCA_UTILS_EXPORT auto allFilesFilterString() -> QString;
ORCA_UTILS_EXPORT auto allGlobPatterns() -> QStringList;
ORCA_UTILS_EXPORT auto magicRulesForMimeType(const MimeType &mimeType) -> QMap<int, QList<Internal::MimeMagicRule>>; // priority -> rules
ORCA_UTILS_EXPORT auto setGlobPatternsForMimeType(const MimeType &mimeType, const QStringList &patterns) -> void;
ORCA_UTILS_EXPORT auto setMagicRulesForMimeType(const MimeType &mimeType, const QMap<int, QList<Internal::MimeMagicRule>> &rules) -> void; // priority -> rules

} // Utils
