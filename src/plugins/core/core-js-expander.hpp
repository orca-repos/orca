// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <functional>

QT_BEGIN_NAMESPACE
class QJSEngine;
class QObject;
class QString;
QT_END_NAMESPACE

namespace Utils {
class MacroExpander;
}

namespace Orca::Plugin::Core {

class MainWindow;
class JsExpanderPrivate;

class CORE_EXPORT JsExpander {
public:
  using ObjectFactory = std::function<QObject *()>;

  JsExpander();
  ~JsExpander();

  template <class T>
  static auto registerGlobalObject(const QString &name) -> void
  {
    registerGlobalObject(name, [] { return new T; });
  }

  static auto registerGlobalObject(const QString &name, const ObjectFactory &factory) -> void;
  auto registerObject(const QString &name, QObject *obj) const -> void;
  auto evaluate(const QString &expression, QString *error_message = nullptr) const -> QString;
  auto engine() const -> QJSEngine&;
  auto registerForExpander(Utils::MacroExpander *macro_expander) const -> void;

private:
  static auto createGlobalJsExpander() -> JsExpander*;

  JsExpanderPrivate *d;
  friend class MainWindow;
};

} // namespace Orca::Plugin::Core
