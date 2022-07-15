// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-locator-filter-interface.hpp"

#include <QTest>

namespace Orca::Plugin::Core {

/// Runs a locator filter for a search text and returns the results.
class CORE_EXPORT BasicLocatorFilterTest
{
public:
    BasicLocatorFilterTest(ILocatorFilter *filter);
    virtual ~BasicLocatorFilterTest();

    QList<LocatorFilterEntry> matchesFor(const QString &searchText = QString());

private:
    virtual void doBeforeLocatorRun() {}
    virtual void doAfterLocatorRun() {}

    ILocatorFilter *m_filter = nullptr;
};

class CORE_EXPORT ResultData
{
public:
    using ResultDataList = QList<ResultData>;

    ResultData();
    ResultData(const QString &textColumn1, const QString &textColumn2,
               const QString &highlightPositions = QString());

    bool operator==(const ResultData &other) const;

    static ResultDataList fromFilterEntryList(const QList<LocatorFilterEntry> &entries);

    /// For debugging and creating reference data
    static void printFilterEntries(const ResultDataList &entries, const QString &msg = QString());

    QString textColumn1;
    QString textColumn2;
    QString highlight;
    LocatorFilterEntry::HighlightInfo::DataType dataType;
};

using ResultDataList = ResultData::ResultDataList;

} // namespace Orca::Plugin::Core

Q_DECLARE_METATYPE(Orca::Plugin::Core::Tests::ResultData)
Q_DECLARE_METATYPE(Orca::Plugin::Core::Tests::ResultDataList)

QT_BEGIN_NAMESPACE
namespace QTest {

template<> inline char *toString(const Core::Tests::ResultData &data)
{
    const QByteArray ba = "\n\"" + data.textColumn1.toUtf8() + "\", \"" + data.textColumn2.toUtf8()
            + "\"\n\"" + data.highlight.toUtf8() + "\"";
    return qstrdup(ba.data());
}

} // namespace QTest
QT_END_NAMESPACE