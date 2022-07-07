// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ifindsupport.hpp"

#include <utils/multitextcursor.hpp>

#include <QRegularExpression>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QTextEdit;
class QTextCursor;
QT_END_NAMESPACE

namespace Core {
struct BaseTextFindPrivate;

class CORE_EXPORT BaseTextFind : public IFindSupport
{
    Q_OBJECT

public:
    explicit BaseTextFind(QPlainTextEdit *editor);
    explicit BaseTextFind(QTextEdit *editor);
    ~BaseTextFind() override;

    using cursor_provider = std::function<Utils::MultiTextCursor()>;

    auto supportsReplace() const -> bool override;
    auto supportedFindFlags() const -> FindFlags override;
    auto resetIncrementalSearch() -> void override;
    auto clearHighlights() -> void override;
    auto currentFindString() const -> QString override;
    auto completedFindString() const -> QString override;
    auto findIncremental(const QString &txt, FindFlags find_flags) -> Result override;
    auto findStep(const QString &txt, FindFlags find_flags) -> Result override;
    auto replace(const QString &before, const QString &after, FindFlags find_flags) -> void override;
    auto replaceStep(const QString &before, const QString &after, FindFlags find_flags) -> bool override;
    auto replaceAll(const QString &before, const QString &after, FindFlags find_flags) -> int override;
    auto defineFindScope() -> void override;
    auto clearFindScope() -> void override;
    auto highlightAll(const QString &txt, FindFlags find_flags) -> void override;
    auto setMultiTextCursorProvider(const cursor_provider &provider) const -> void;
    auto inScope(const QTextCursor &candidate) const -> bool;
    static auto regularExpression(const QString &txt, FindFlags flags) -> QRegularExpression;

signals:
    auto highlightAllRequested(const QString &txt, FindFlags find_flags) -> void;
    auto findScopeChanged(const Utils::MultiTextCursor &cursor) -> void;

private:
    auto find(const QString &txt, FindFlags find_flags, QTextCursor start, bool *wrapped) const -> bool;
    auto replaceInternal(const QString &before, const QString &after, FindFlags find_flags) const -> QTextCursor;
    auto multiTextCursor() const -> Utils::MultiTextCursor;
    auto textCursor() const -> QTextCursor;
    auto setTextCursor(const QTextCursor &) const -> void;
    auto document() const -> QTextDocument*;
    auto isReadOnly() const -> bool;
    auto findOne(const QRegularExpression &expr, QTextCursor from, QTextDocument::FindFlags options) const -> QTextCursor;

    BaseTextFindPrivate *d;
};

} // namespace Core
