// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "macroexpander.h"

#include <QString>

QT_FORWARD_DECLARE_CLASS(QJSEngine);

namespace Utils {

class ORCA_UTILS_EXPORT TemplateEngine {
public:
  static auto preprocessText(const QString &input, QString *output, QString *errorMessage) -> bool;
  static auto processText(MacroExpander *expander, const QString &input, QString *errorMessage) -> QString;
  static auto evaluateBooleanJavaScriptExpression(QJSEngine &engine, const QString &expression, bool *result, QString *errorMessage) -> bool;
};

} // namespace Utils
