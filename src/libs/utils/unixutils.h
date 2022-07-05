// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT UnixUtils {
public:
  static auto defaultFileBrowser() -> QString;
  static auto fileBrowser(const QSettings *settings) -> QString;
  static auto setFileBrowser(QSettings *settings, const QString &term) -> void;
  static auto fileBrowserHelpText() -> QString;
  static auto substituteFileBrowserParameters(const QString &command, const QString &file) -> QString;
};

}
