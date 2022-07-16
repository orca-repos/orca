// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QByteArray>
#include <QHash>
#include <QVector>

namespace ProjectExplorer {

enum class MacroType {
  Invalid,
  Define,
  Undefine
};

class Macro;

using Macros = QVector<Macro>;

class PROJECTEXPLORER_EXPORT Macro {
public:
  Macro() = default;
  Macro(QByteArray key, QByteArray value, MacroType type = MacroType::Define) : key(key), value(value), type(type) {}
  Macro(QByteArray key, MacroType type = MacroType::Define) : key(key), type(type) {}

  auto isValid() const -> bool;
  auto toByteArray() const -> QByteArray;
  static auto toByteArray(const Macros &macros) -> QByteArray;
  static auto toByteArray(const QVector<Macros> &macross) -> QByteArray;
  static auto toMacros(const QByteArray &text) -> Macros;
  // define Foo will be converted to Foo=1
  static auto fromKeyValue(const QString &utf16text) -> Macro;
  static auto fromKeyValue(const QByteArray &text) -> Macro;
  auto toKeyValue(const QByteArray &prefix) const -> QByteArray;

  friend auto qHash(const Macro &macro)
  {
    using QT_PREPEND_NAMESPACE(qHash);
    return qHash(macro.key) ^ qHash(macro.value) ^ qHash(int(macro.type));
  }

  friend auto operator==(const Macro &first, const Macro &second) -> bool
  {
    return first.type == second.type && first.key == second.key && first.value == second.value;
  }

public:
  QByteArray key;
  QByteArray value;
  MacroType type = MacroType::Invalid;

private:
  static auto splitLines(const QByteArray &text) -> QList<QByteArray>;
  static auto removeNonsemanticSpaces(QByteArray line) -> QByteArray;
  static auto tokenizeLine(const QByteArray &line) -> QList<QByteArray>;
  static auto tokenizeLines(const QList<QByteArray> &lines) -> QList<QList<QByteArray>>;
  static auto tokensToMacro(const QList<QByteArray> &tokens) -> Macro;
  static auto tokensLinesToMacros(const QList<QList<QByteArray>> &tokensLines) -> Macros;
};

} // namespace ProjectExplorer
