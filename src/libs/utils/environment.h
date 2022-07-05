// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.h"
#include "hostosinfo.h"
#include "namevaluedictionary.h"
#include "namevalueitem.h"
#include "optional.h"

#include <QStringList>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QProcessEnvironment)

namespace Utils {

class ORCA_UTILS_EXPORT Environment final : public NameValueDictionary {
public:
  using NameValueDictionary::NameValueDictionary;
  using PathFilter = std::function<bool(const FilePath &)>;

  static auto systemEnvironment() -> Environment;
  auto toProcessEnvironment() const -> QProcessEnvironment;
  auto appendOrSet(const QString &key, const QString &value, const QString &sep = QString()) -> void;
  auto prependOrSet(const QString &key, const QString &value, const QString &sep = QString()) -> void;
  auto appendOrSetPath(const Utils::FilePath &value) -> void;
  auto prependOrSetPath(const Utils::FilePath &value) -> void;
  auto prependOrSetLibrarySearchPath(const Utils::FilePath &value) -> void;
  auto prependOrSetLibrarySearchPaths(const Utils::FilePaths &values) -> void;
  auto setupEnglishOutput() -> void;
  auto searchInPath(const QString &executable, const FilePaths &additionalDirs = FilePaths(), const PathFilter &func = PathFilter()) const -> FilePath;
  auto searchInDirectories(const QString &executable, const FilePaths &dirs) const -> FilePath;
  auto findAllInPath(const QString &executable, const FilePaths &additionalDirs = FilePaths(), const PathFilter &func = PathFilter()) const -> FilePaths;
  auto path() const -> FilePaths;
  auto pathListValue(const QString &varName) const -> FilePaths;
  auto appendExeExtensions(const QString &executable) const -> QStringList;
  auto isSameExecutable(const QString &exe1, const QString &exe2) const -> bool;
  auto expandedValueForKey(const QString &key) const -> QString;
  auto expandVariables(const QString &input) const -> QString;
  auto expandVariables(const FilePath &input) const -> FilePath;
  auto expandVariables(const QStringList &input) const -> QStringList;
  static auto modifySystemEnvironment(const EnvironmentItems &list) -> void; // use with care!!!
  static auto setSystemEnvironment(const Environment &environment) -> void;  // don't use at all!!!
};

class ORCA_UTILS_EXPORT EnvironmentChange final {
public:
  using Item = std::function<void(Environment &)>;

  EnvironmentChange() = default;

  static auto fromFixedEnvironment(const Environment &fixedEnv) -> EnvironmentChange;
  auto applyToEnvironment(Environment &) const -> void;
  auto addSetValue(const QString &key, const QString &value) -> void;
  auto addUnsetValue(const QString &key) -> void;
  auto addPrependToPath(const Utils::FilePaths &values) -> void;
  auto addAppendToPath(const Utils::FilePaths &values) -> void;
  auto addModify(const NameValueItems &items) -> void;
  auto addChange(const Item &item) -> void { m_changeItems.append(item); }

private:
  QList<Item> m_changeItems;
};

class ORCA_UTILS_EXPORT EnvironmentProvider {
public:
  QByteArray id;
  QString displayName;
  std::function<Environment()> environment;

  static auto addProvider(EnvironmentProvider &&provider) -> void;
  static auto providers() -> const QVector<EnvironmentProvider>;
  static auto provider(const QByteArray &id) -> optional<EnvironmentProvider>;
};

} // namespace Utils
