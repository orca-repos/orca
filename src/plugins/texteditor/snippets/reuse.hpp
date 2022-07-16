// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <QString>

namespace TextEditor {
namespace Internal {

const QLatin1String kTrue("true");
const QLatin1String kFalse("false");

inline auto toBool(const QString &s) -> bool
{
  if (s == kTrue)
    return true;
  return false;
}

inline auto fromBool(bool b) -> QString
{
  if (b)
    return kTrue;
  return kFalse;
}

} // Internal
} // TextEditor
