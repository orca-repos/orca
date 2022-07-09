/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "basemessage.h"
#include "lsputils.h"

#include <utils/mimetypes/mimetype.hpp>
#include <utils/qtcassert.hpp>
#include <utils/variant.hpp>

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QJsonValue>

#include <functional>

namespace LanguageServerProtocol {

class IContent;

class LANGUAGESERVERPROTOCOL_EXPORT MessageId : public Utils::variant<int, QString>
{
public:
    MessageId() = default;
    explicit MessageId(int id) : variant(id) {}
    explicit MessageId(const QString &id) : variant(id) {}
    explicit MessageId(const QJsonValue &value)
    {
        if (value.isDouble())
            *this = MessageId(value.toInt());
        else if (value.isString())
            *this = MessageId(value.toString());
        else
            m_valid = false;
    }

    operator QJsonValue() const
    {
        if (auto id = Utils::get_if<int>(this))
            return *id;
        if (auto id = Utils::get_if<QString>(this))
            return *id;
        return QJsonValue();
    }

    bool isValid() const { return m_valid; }

    QString toString() const
    {
        if (auto id = Utils::get_if<QString>(this))
            return *id;
        if (auto id = Utils::get_if<int>(this))
            return QString::number(*id);
        return {};
    }

    friend auto qHash(const MessageId &id)
    {
        if (Utils::holds_alternative<int>(id))
            return QT_PREPEND_NAMESPACE(qHash(Utils::get<int>(id)));
        if (Utils::holds_alternative<QString>(id))
            return QT_PREPEND_NAMESPACE(qHash(Utils::get<QString>(id)));
        return QT_PREPEND_NAMESPACE(qHash(0));
    }

private:
    bool m_valid = true;
};

struct ResponseHandler
{
    MessageId id;
    using Callback = std::function<void(const QByteArray &, QTextCodec *)>;
    Callback callback;
};

using ResponseHandlers = std::function<void(const MessageId &, const QByteArray &, QTextCodec *)>;
using MethodHandler = std::function<void(const QString &, const MessageId &, const IContent *)>;

template <typename Error>
inline QDebug operator<<(QDebug stream, const LanguageServerProtocol::MessageId &id)
{
    if (Utils::holds_alternative<int>(id))
        stream << Utils::get<int>(id);
    else
        stream << Utils::get<QString>(id);
    return stream;
}

class LANGUAGESERVERPROTOCOL_EXPORT IContent
{
public:
    virtual ~IContent() = default;

    virtual QByteArray toRawData() const = 0;
    virtual QByteArray mimeType() const = 0;
    virtual bool isValid(QString *errorMessage) const = 0;

    virtual Utils::optional<ResponseHandler> responseHandler() const
    { return Utils::nullopt; }

    BaseMessage toBaseMessage() const
    { return BaseMessage(mimeType(), toRawData()); }
};

} // namespace LanguageClient
