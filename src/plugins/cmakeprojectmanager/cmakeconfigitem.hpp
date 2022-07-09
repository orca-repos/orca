// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <utils/porting.hpp>
#include <utils/optional.hpp>

#include <QByteArray>
#include <QStringList>

namespace Utils {
class FilePath;
class MacroExpander;
} // namespace Utils

namespace ProjectExplorer {
class Kit;
}

namespace CMakeProjectManager {

class CMAKE_EXPORT CMakeConfigItem {
public:
  enum Type {
    FILEPATH,
    PATH,
    BOOL,
    STRING,
    INTERNAL,
    STATIC,
    UNINITIALIZED
  };

  CMakeConfigItem();
  CMakeConfigItem(const QByteArray &k, Type t, const QByteArray &d, const QByteArray &v, const QStringList &s = {});
  CMakeConfigItem(const QByteArray &k, Type t, const QByteArray &v);
  CMakeConfigItem(const QByteArray &k, const QByteArray &v);

  static auto cmakeSplitValue(const QString &in, bool keepEmpty = false) -> QStringList;
  static auto typeStringToType(const QByteArray &typeString) -> Type;
  static auto typeToTypeString(const Type t) -> QString;
  static auto toBool(const QString &value) -> Utils::optional<bool>;
  auto isNull() const -> bool { return key.isEmpty(); }
  auto expandedValue(const ProjectExplorer::Kit *k) const -> QString;
  auto expandedValue(const Utils::MacroExpander *expander) const -> QString;
  static auto less(const CMakeConfigItem &a, const CMakeConfigItem &b) -> bool;
  static auto fromString(const QString &s) -> CMakeConfigItem;
  auto toString(const Utils::MacroExpander *expander = nullptr) const -> QString;
  auto toArgument() const -> QString;
  auto toArgument(const Utils::MacroExpander *expander) const -> QString;
  auto toCMakeSetLine(const Utils::MacroExpander *expander = nullptr) const -> QString;
  auto operator==(const CMakeConfigItem &o) const -> bool;
  friend auto qHash(const CMakeConfigItem &it) -> Utils::QHashValueType; // needed for MSVC

  QByteArray key;
  Type type = STRING;
  bool isAdvanced = false;
  bool inCMakeCache = false;
  bool isUnset = false;
  bool isInitial = false;
  QByteArray value; // converted to string as needed
  QByteArray documentation;
  QStringList values;
};

class CMAKE_EXPORT CMakeConfig : public QList<CMakeConfigItem> {
public:
  CMakeConfig() = default;
  CMakeConfig(const QList<CMakeConfigItem> &items) : QList<CMakeConfigItem>(items) {}
  CMakeConfig(std::initializer_list<CMakeConfigItem> items) : QList<CMakeConfigItem>(items) {}

  auto toList() const -> const QList<CMakeConfigItem>& { return *this; }
  static auto fromArguments(const QStringList &list, QStringList &unknownOptions) -> CMakeConfig;
  static auto fromFile(const Utils::FilePath &input, QString *errorMessage) -> CMakeConfig;
  auto valueOf(const QByteArray &key) const -> QByteArray;
  auto stringValueOf(const QByteArray &key) const -> QString;
  auto filePathValueOf(const QByteArray &key) const -> Utils::FilePath;
  auto expandedValueOf(const ProjectExplorer::Kit *k, const QByteArray &key) const -> QString;
};

} // namespace CMakeProjectManager
