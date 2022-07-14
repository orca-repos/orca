// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-js-expander.hpp"

#include "core-js-extensions.hpp"

#include <utils/macroexpander.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QJSEngine>

#include <unordered_map>

using ExtensionMap = std::unordered_map<QString, Orca::Plugin::Core::JsExpander::ObjectFactory>;
Q_GLOBAL_STATIC(ExtensionMap, globalJsExtensions);
static Orca::Plugin::Core::JsExpander *global_expander = nullptr;

namespace Orca::Plugin::Core {

class JsExpanderPrivate {
public:
  QJSEngine m_engine;
};

auto JsExpander::registerGlobalObject(const QString &name, const ObjectFactory &factory) -> void
{
  globalJsExtensions->insert({name, factory});

  if (global_expander)
    global_expander->registerObject(name, factory());
}

auto JsExpander::registerObject(const QString &name, QObject *obj) const -> void
{
  const auto js_obj = d->m_engine.newQObject(obj);
  d->m_engine.globalObject().setProperty(name, js_obj);
}

auto JsExpander::evaluate(const QString &expression, QString *error_message) const -> QString
{
  const auto value = d->m_engine.evaluate(expression);

  if (value.isError()) {
    const auto msg = QCoreApplication::translate("Core::JsExpander", "Error in \"%1\": %2").arg(expression, value.toString());
    if (error_message)
      *error_message = msg;
    return {};
  }

  // Try to convert to bool, be that an int or whatever.
  if (value.isBool())
    return value.toString();

  if (value.isNumber())
    return QString::number(value.toNumber());

  if (value.isString())
    return value.toString();

  const auto msg = QCoreApplication::translate("Core::JsExpander", "Cannot convert result of \"%1\" to string.").arg(expression);

  if (error_message)
    *error_message = msg;

  return {};
}

auto JsExpander::engine() const -> QJSEngine&
{
  return d->m_engine;
}

auto JsExpander::registerForExpander(Utils::MacroExpander *macro_expander) const -> void
{
  macro_expander->registerPrefix("JS", QCoreApplication::translate("Core::JsExpander", "Evaluate simple JavaScript statements.<br>" "Literal '}' characters must be escaped as \"\\}\", " "'\\' characters must be escaped as \"\\\\\", " "and \"%{\" must be escaped as \"%\\{\"."), [this](const QString &in) -> QString {
    QString error_message;
    auto result = evaluate(in, &error_message);

    if (!error_message.isEmpty()) {
      qWarning() << error_message;
      return error_message;
    }

    return result;
  });
}

auto JsExpander::createGlobalJsExpander() -> JsExpander*
{
  global_expander = new JsExpander();
  registerGlobalObject<UtilsJsExtension>("Util");
  global_expander->registerForExpander(Utils::globalMacroExpander());
  return global_expander;
}

JsExpander::JsExpander()
{
  d = new JsExpanderPrivate;
  for (const auto & [fst, snd] : *globalJsExtensions)
    registerObject(fst, snd());
}

JsExpander::~JsExpander()
{
  delete d;
  d = nullptr;
}

} // namespace Orca::Plugin::Core
