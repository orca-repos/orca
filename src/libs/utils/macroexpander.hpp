// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <functional>

#include <QList>
#include <QVector>
#include <QCoreApplication>

namespace Utils {

namespace Internal { class MacroExpanderPrivate; }

class FilePath;
class MacroExpander;
using MacroExpanderProvider = std::function<MacroExpander *()>;
using MacroExpanderProviders = QVector<MacroExpanderProvider>;

class ORCA_UTILS_EXPORT MacroExpander {
  Q_DECLARE_TR_FUNCTIONS(Utils::MacroExpander)
  Q_DISABLE_COPY(MacroExpander)

public:
  explicit MacroExpander();
  ~MacroExpander();

  auto resolveMacro(const QString &name, QString *ret) const -> bool;
  auto value(const QByteArray &variable, bool *found = nullptr) const -> QString;
  auto expand(const QString &stringWithVariables) const -> QString;
  auto expand(const FilePath &fileNameWithVariables) const -> FilePath;
  auto expand(const QByteArray &stringWithVariables) const -> QByteArray;
  auto expandVariant(const QVariant &v) const -> QVariant;
  auto expandProcessArgs(const QString &argsWithVariables) const -> QString;

  using PrefixFunction = std::function<QString(QString)>;
  using ResolverFunction = std::function<bool(QString, QString *)>;
  using StringFunction = std::function<QString()>;
  using FileFunction = std::function<FilePath()>;
  using IntFunction = std::function<int()>;

  auto registerPrefix(const QByteArray &prefix, const QString &description, const PrefixFunction &value, bool visible = true) -> void;
  auto registerVariable(const QByteArray &variable, const QString &description, const StringFunction &value, bool visibleInChooser = true) -> void;
  auto registerIntVariable(const QByteArray &variable, const QString &description, const IntFunction &value) -> void;
  auto registerFileVariables(const QByteArray &prefix, const QString &heading, const FileFunction &value, bool visibleInChooser = true) -> void;
  auto registerExtraResolver(const ResolverFunction &value) -> void;
  auto visibleVariables() const -> QList<QByteArray>;
  auto variableDescription(const QByteArray &variable) const -> QString;
  auto isPrefixVariable(const QByteArray &variable) const -> bool;
  auto subProviders() const -> MacroExpanderProviders;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &displayName) -> void;
  auto registerSubProvider(const MacroExpanderProvider &provider) -> void;
  auto isAccumulating() const -> bool;
  auto setAccumulating(bool on) -> void;

private:
  friend class Internal::MacroExpanderPrivate;
  Internal::MacroExpanderPrivate *d;
};

ORCA_UTILS_EXPORT auto globalMacroExpander() -> MacroExpander*;

} // namespace Utils
