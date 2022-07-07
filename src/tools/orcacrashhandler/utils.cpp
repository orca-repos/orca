// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "utils.hpp"

#include <QDebug>
#include <QFile>

QByteArray fileContents(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Warning: Could not open '%s'.", qPrintable(filePath));
        return QByteArray();
    }
    return file.readAll();
}
