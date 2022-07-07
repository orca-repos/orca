// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environment.hpp"

#include "algorithm.hpp"
#include "qtcassert.hpp"
#include "stringutils.hpp"

#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QSet>
#include <QCoreApplication>

Q_GLOBAL_STATIC_WITH_ARGS(Utils::Environment, staticSystemEnvironment,
                          (QProcessEnvironment::systemEnvironment().toStringList()))

Q_GLOBAL_STATIC(QVector<Utils::EnvironmentProvider>, environmentProviders)

namespace Utils {

auto Environment::toProcessEnvironment() const -> QProcessEnvironment
{
  QProcessEnvironment result;
  for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
    if (it.value().second)
      result.insert(it.key().name, expandedValueForKey(key(it)));
  }
  return result;
}

auto Environment::appendOrSetPath(const FilePath &value) -> void
{
  QTC_CHECK(value.osType() == m_osType);
  if (value.isEmpty())
    return;
  appendOrSet("PATH", value.nativePath(), QString(OsSpecificAspects::pathListSeparator(m_osType)));
}

auto Environment::prependOrSetPath(const FilePath &value) -> void
{
  QTC_CHECK(value.osType() == m_osType);
  if (value.isEmpty())
    return;
  prependOrSet("PATH", value.nativePath(), QString(OsSpecificAspects::pathListSeparator(m_osType)));
}

auto Environment::appendOrSet(const QString &key, const QString &value, const QString &sep) -> void
{
  QTC_ASSERT(!key.contains('='), return);
  const auto it = findKey(key);
  if (it == m_values.end()) {
    m_values.insert(DictKey(key, nameCaseSensitivity()), qMakePair(value, true));
  } else {
    // Append unless it is already there
    const QString toAppend = sep + value;
    if (!it.value().first.endsWith(toAppend))
      it.value().first.append(toAppend);
  }
}

auto Environment::prependOrSet(const QString &key, const QString &value, const QString &sep) -> void
{
  QTC_ASSERT(!key.contains('='), return);
  const auto it = findKey(key);
  if (it == m_values.end()) {
    m_values.insert(DictKey(key, nameCaseSensitivity()), qMakePair(value, true));
  } else {
    // Prepend unless it is already there
    const QString toPrepend = value + sep;
    if (!it.value().first.startsWith(toPrepend))
      it.value().first.prepend(toPrepend);
  }
}

auto Environment::prependOrSetLibrarySearchPath(const FilePath &value) -> void
{
  QTC_CHECK(value.osType() == m_osType);
  switch (m_osType) {
  case OsTypeWindows: {
    const QChar sep = ';';
    prependOrSet("PATH", value.nativePath(), QString(sep));
    break;
  }
  case OsTypeMac: {
    const QString sep = ":";
    const QString nativeValue = value.nativePath();
    prependOrSet("DYLD_LIBRARY_PATH", nativeValue, sep);
    prependOrSet("DYLD_FRAMEWORK_PATH", nativeValue, sep);
    break;
  }
  case OsTypeLinux:
  case OsTypeOtherUnix: {
    const QChar sep = ':';
    prependOrSet("LD_LIBRARY_PATH", value.nativePath(), QString(sep));
    break;
  }
  default:
    break;
  }
}

auto Environment::prependOrSetLibrarySearchPaths(const FilePaths &values) -> void
{
  Utils::reverseForeach(values, [this](const FilePath &value) {
    prependOrSetLibrarySearchPath(value);
  });
}

auto Environment::systemEnvironment() -> Environment
{
  return *staticSystemEnvironment();
}

auto Environment::setupEnglishOutput() -> void
{
  set("LC_MESSAGES", "en_US.utf8");
  set("LANGUAGE", "en_US:en");
}

static auto searchInDirectory(const QStringList &execs, const FilePath &directory, QSet<FilePath> &alreadyChecked) -> FilePath
{
  const int checkedCount = alreadyChecked.count();
  alreadyChecked.insert(directory);

  if (directory.isEmpty() || alreadyChecked.count() == checkedCount)
    return FilePath();

  const QString dir = directory.toString();

  QFileInfo fi;
  for (const QString &exec : execs) {
    fi.setFile(dir, exec);
    if (fi.isFile() && fi.isExecutable())
      return FilePath::fromString(fi.absoluteFilePath());
  }
  return FilePath();
}

auto Environment::appendExeExtensions(const QString &executable) const -> QStringList
{
  QStringList execs(executable);
  const QFileInfo fi(executable);
  if (m_osType == OsTypeWindows) {
    // Check all the executable extensions on windows:
    // PATHEXT is only used if the executable has no extension
    if (fi.suffix().isEmpty()) {
      const QStringList extensions = expandedValueForKey("PATHEXT").split(';');

      for (const QString &ext : extensions)
        execs << executable + ext.toLower();
    }
  }
  return execs;
}

auto Environment::isSameExecutable(const QString &exe1, const QString &exe2) const -> bool
{
  const QStringList exe1List = appendExeExtensions(exe1);
  const QStringList exe2List = appendExeExtensions(exe2);
  for (const QString &i1 : exe1List) {
    for (const QString &i2 : exe2List) {
      const FilePath f1 = FilePath::fromString(i1);
      const FilePath f2 = FilePath::fromString(i2);
      if (f1 == f2)
        return true;
      if (f1.needsDevice() != f2.needsDevice() || f1.scheme() != f2.scheme())
        return false;
      if (f1.resolveSymlinks() == f2.resolveSymlinks())
        return true;
      if (FileUtils::fileId(f1) == FileUtils::fileId(f2))
        return true;
    }
  }
  return false;
}

auto Environment::expandedValueForKey(const QString &key) const -> QString
{
  return expandVariables(value(key));
}

static auto searchInDirectoriesHelper(const Environment &env, const QString &executable, const FilePaths &dirs, const Environment::PathFilter &func, bool usePath) -> FilePath
{
  if (executable.isEmpty())
    return FilePath();

  const QString exec = QDir::cleanPath(env.expandVariables(executable));
  const QFileInfo fi(exec);

  const QStringList execs = env.appendExeExtensions(exec);

  if (fi.isAbsolute()) {
    for (const QString &path : execs) {
      QFileInfo pfi = QFileInfo(path);
      if (pfi.isFile() && pfi.isExecutable())
        return FilePath::fromString(path);
    }
    return FilePath::fromString(exec);
  }

  QSet<FilePath> alreadyChecked;
  for (const FilePath &dir : dirs) {
    FilePath tmp = searchInDirectory(execs, dir, alreadyChecked);
    if (!tmp.isEmpty() && (!func || func(tmp)))
      return tmp;
  }

  if (usePath) {
    if (executable.contains('/'))
      return FilePath();

    for (const FilePath &p : env.path()) {
      FilePath tmp = searchInDirectory(execs, p, alreadyChecked);
      if (!tmp.isEmpty() && (!func || func(tmp)))
        return tmp;
    }
  }
  return FilePath();
}

auto Environment::searchInDirectories(const QString &executable, const FilePaths &dirs) const -> FilePath
{
  return searchInDirectoriesHelper(*this, executable, dirs, {}, false);
}

auto Environment::searchInPath(const QString &executable, const FilePaths &additionalDirs, const PathFilter &func) const -> FilePath
{
  return searchInDirectoriesHelper(*this, executable, additionalDirs, func, true);
}

auto Environment::findAllInPath(const QString &executable, const FilePaths &additionalDirs, const Environment::PathFilter &func) const -> FilePaths
{
  if (executable.isEmpty())
    return {};

  const QString exec = QDir::cleanPath(expandVariables(executable));
  const QFileInfo fi(exec);

  const QStringList execs = appendExeExtensions(exec);

  if (fi.isAbsolute()) {
    for (const QString &path : execs) {
      QFileInfo pfi = QFileInfo(path);
      if (pfi.isFile() && pfi.isExecutable())
        return {FilePath::fromString(path)};
    }
    return {FilePath::fromString(exec)};
  }

  QSet<FilePath> result;
  QSet<FilePath> alreadyChecked;
  for (const FilePath &dir : additionalDirs) {
    FilePath tmp = searchInDirectory(execs, dir, alreadyChecked);
    if (!tmp.isEmpty() && (!func || func(tmp)))
      result << tmp;
  }

  if (!executable.contains('/')) {
    for (const FilePath &p : path()) {
      FilePath tmp = searchInDirectory(execs, p, alreadyChecked);
      if (!tmp.isEmpty() && (!func || func(tmp)))
        result << tmp;
    }
  }
  return result.values();
}

auto Environment::path() const -> FilePaths
{
  return pathListValue("PATH");
}

auto Environment::pathListValue(const QString &varName) const -> FilePaths
{
  const QStringList pathComponents = expandedValueForKey(varName).split(OsSpecificAspects::pathListSeparator(m_osType), SkipEmptyParts);
  return transform(pathComponents, &FilePath::fromUserInput);
}

auto Environment::modifySystemEnvironment(const EnvironmentItems &list) -> void
{
  staticSystemEnvironment->modify(list);
}

auto Environment::setSystemEnvironment(const Environment &environment) -> void
{
  *staticSystemEnvironment = environment;
}

/** Expand environment variables in a string.
 *
 * Environment variables are accepted in the following forms:
 * $SOMEVAR, ${SOMEVAR} on Unix and %SOMEVAR% on Windows.
 * No escapes and quoting are supported.
 * If a variable is not found, it is not substituted.
 */
auto Environment::expandVariables(const QString &input) const -> QString
{
  QString result = input;

  if (m_osType == OsTypeWindows) {
    for (int vStart = -1, i = 0; i < result.length();) {
      if (result.at(i++) == '%') {
        if (vStart > 0) {
          const auto it = findKey(result.mid(vStart, i - vStart - 1));
          if (it != m_values.constEnd()) {
            result.replace(vStart - 1, i - vStart + 1, it->first);
            i = vStart - 1 + it->first.length();
            vStart = -1;
          } else {
            vStart = i;
          }
        } else {
          vStart = i;
        }
      }
    }
  } else {
    enum {
      BASE,
      OPTIONALVARIABLEBRACE,
      VARIABLE,
      BRACEDVARIABLE
    } state = BASE;
    int vStart = -1;

    for (int i = 0; i < result.length();) {
      QChar c = result.at(i++);
      if (state == BASE) {
        if (c == '$')
          state = OPTIONALVARIABLEBRACE;
      } else if (state == OPTIONALVARIABLEBRACE) {
        if (c == '{') {
          state = BRACEDVARIABLE;
          vStart = i;
        } else if (c.isLetterOrNumber() || c == '_') {
          state = VARIABLE;
          vStart = i - 1;
        } else {
          state = BASE;
        }
      } else if (state == BRACEDVARIABLE) {
        if (c == '}') {
          const_iterator it = constFind(result.mid(vStart, i - 1 - vStart));
          if (it != constEnd()) {
            result.replace(vStart - 2, i - vStart + 2, it->first);
            i = vStart - 2 + it->first.length();
          }
          state = BASE;
        }
      } else if (state == VARIABLE) {
        if (!c.isLetterOrNumber() && c != '_') {
          const_iterator it = constFind(result.mid(vStart, i - vStart - 1));
          if (it != constEnd()) {
            result.replace(vStart - 1, i - vStart, it->first);
            i = vStart - 1 + it->first.length();
          }
          state = BASE;
        }
      }
    }
    if (state == VARIABLE) {
      const_iterator it = constFind(result.mid(vStart));
      if (it != constEnd())
        result.replace(vStart - 1, result.length() - vStart + 1, it->first);
    }
  }
  return result;
}

auto Environment::expandVariables(const FilePath &variables) const -> FilePath
{
  return FilePath::fromString(expandVariables(variables.toString()));
}

auto Environment::expandVariables(const QStringList &variables) const -> QStringList
{
  return Utils::transform(variables, [this](const QString &i) { return expandVariables(i); });
}

auto EnvironmentProvider::addProvider(EnvironmentProvider &&provider) -> void
{
  environmentProviders->append(std::move(provider));
}

auto EnvironmentProvider::providers() -> const QVector<EnvironmentProvider>
{
  return *environmentProviders;
}

auto EnvironmentProvider::provider(const QByteArray &id) -> optional<EnvironmentProvider>
{
  const int index = indexOf(*environmentProviders, equal(&EnvironmentProvider::id, id));
  if (index >= 0)
    return make_optional(environmentProviders->at(index));
  return nullopt;
}

auto EnvironmentChange::addSetValue(const QString &key, const QString &value) -> void
{
  m_changeItems.append([key, value](Environment &env) { env.set(key, value); });
}

auto EnvironmentChange::addUnsetValue(const QString &key) -> void
{
  m_changeItems.append([key](Environment &env) { env.unset(key); });
}

auto EnvironmentChange::addPrependToPath(const FilePaths &values) -> void
{
  for (int i = values.size(); --i >= 0;) {
    const FilePath value = values.at(i);
    m_changeItems.append([value](Environment &env) { env.prependOrSetPath(value); });
  }
}

auto EnvironmentChange::addAppendToPath(const FilePaths &values) -> void
{
  for (const FilePath &value : values)
    m_changeItems.append([value](Environment &env) { env.appendOrSetPath(value); });
}

auto EnvironmentChange::addModify(const NameValueItems &items) -> void
{
  m_changeItems.append([items](Environment &env) { env.modify(items); });
}

auto EnvironmentChange::fromFixedEnvironment(const Environment &fixedEnv) -> EnvironmentChange
{
  EnvironmentChange change;
  change.m_changeItems.append([fixedEnv](Environment &env) { env = fixedEnv; });
  return change;
}

auto EnvironmentChange::applyToEnvironment(Environment &env) const -> void
{
  for (const Item &item : m_changeItems)
    item(env);
}

} // namespace Utils
