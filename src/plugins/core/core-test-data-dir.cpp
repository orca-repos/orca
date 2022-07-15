// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-test-data-dir.hpp"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QTest>

namespace Orca::Plugin::Core {

TestDataDir::TestDataDir(const QString &directory)
    : m_directory(directory)
{
    QFileInfo fi(m_directory);
    QVERIFY(fi.exists());
    QVERIFY(fi.isDir());
}

QString TestDataDir::file(const QString &fileName) const
{
    return directory() + QLatin1Char('/') + fileName;
}

QString TestDataDir::path() const
{
    return m_directory;
}

QString TestDataDir::directory(const QString &subdir, bool clean) const
{
    QString path = m_directory;
    if (!subdir.isEmpty())
        path += QLatin1Char('/') + subdir;
    if (clean)
        path = QDir::cleanPath(path);
    return path;
}

} // namespace Orca::Plugin::Core