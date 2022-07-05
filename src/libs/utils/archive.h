// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "fileutils.h"

#include <QObject>

namespace Utils {

class QtcProcess;

class ORCA_UTILS_EXPORT Archive : public QObject
{
    Q_OBJECT
public:
    static auto supportsFile(const FilePath &filePath, QString *reason = nullptr) -> bool;
    static auto unarchive(const FilePath &src, const FilePath &dest, QWidget *parent) -> bool;
    static auto unarchive(const FilePath &src, const FilePath &dest) -> Archive*;

    auto cancel() -> void;

signals:
    auto outputReceived(const QString &output) -> void;
    auto finished(bool success) -> void;

private:
    Archive() = default;

    QtcProcess *m_process = nullptr;
};

} // namespace Utils
