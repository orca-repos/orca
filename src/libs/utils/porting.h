// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#ifdef QT_GUI_LIB
#include <QEnterEvent>
#endif
#include <QString>

namespace Utils {

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
// Keep the support code for lower Qt versions for sdktool
constexpr QString::SplitBehavior SkipEmptyParts = QString::SkipEmptyParts;
#else
constexpr Qt::SplitBehaviorFlags SkipEmptyParts = Qt::SkipEmptyParts;
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using QHashValueType = uint;
#else
using QHashValueType = size_t;
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using QtSizeType = int;
#else
using QtSizeType = qsizetype;
#endif

// StringView - either QStringRef or QStringView
// Can be used where it is not possible to completely switch to QStringView
// For example where QString::splitRef / QStringView::split is essential.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using StringView = QStringRef;
#else
using StringView = QStringView;
#endif

inline auto make_stringview(const QString &s) -> StringView
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QStringRef(&s);
#else
    return QStringView(s);
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
// QStringView::mid in Qt5 does not do bounds checking, in Qt6 it does
inline auto midView(const QString &s, int offset, int length) -> QStringView
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const int size = s.size();
    if (offset > size)
        return {};
    if (offset < 0) {
        if (length < 0 || length + offset >= size)
            return QStringView(s);
        if (length + offset <= 0)
            return {};
        return QStringView(s).left(length + offset);
    } else if (length > size - offset)
        return QStringView(s).mid(offset);
    return QStringView(s).mid(offset, length);
#else
    return QStringView(s).mid(offset, length);
#endif
}
#endif

#ifdef QT_GUI_LIB
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using EnterEvent = QEvent;
#else
using EnterEvent = QEnterEvent;
#endif
#endif

} // namespace Utils
