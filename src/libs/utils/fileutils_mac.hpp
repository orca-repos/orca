// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QUrl>

namespace Utils {
namespace Internal {

QUrl filePathUrl(const QUrl &url);
QString normalizePathName(const QString &filePath);

} // Internal
} // Utils
