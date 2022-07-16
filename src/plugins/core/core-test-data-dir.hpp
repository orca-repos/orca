// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QString>

#define QTC_DECLARE_MYTESTDATADIR(PATH)                                          \
    class MyTestDataDir : public Orca::Plugin::Core::Tests::TestDataDir                        \
    {                                                                            \
    public:                                                                      \
        MyTestDataDir(const QString &testDataDirectory = QString())              \
            : TestDataDir(QLatin1String(SRCDIR "/" PATH) + testDataDirectory) {} \
    };

namespace Orca::Plugin::Core {

class CORE_EXPORT TestDataDir
{
public:
    TestDataDir(const QString &directory);

    QString file(const QString &fileName) const;
    QString directory(const QString &subdir = QString(), bool clean = true) const;

    QString path() const;

private:
    QString m_directory;
};

} // namespace Orca::Plugin::Core
