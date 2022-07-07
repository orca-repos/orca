// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsontreeitem.hpp"

#include <QJsonArray>
#include <QJsonObject>

Utils::JsonTreeItem::JsonTreeItem(const QString &displayName, const QJsonValue &value)
    : m_name(displayName)
    , m_value(value)
{ }

static auto typeName(QJsonValue::Type type) -> QString
{
    switch (type) {
    case QJsonValue::Null:
        return Utils::JsonTreeItem::tr("Null");
    case QJsonValue::Bool:
        return Utils::JsonTreeItem::tr("Bool");
    case QJsonValue::Double:
        return Utils::JsonTreeItem::tr("Double");
    case QJsonValue::String:
        return Utils::JsonTreeItem::tr("String");
    case QJsonValue::Array:
        return Utils::JsonTreeItem::tr("Array");
    case QJsonValue::Object:
        return Utils::JsonTreeItem::tr("Object");
    case QJsonValue::Undefined:
        return Utils::JsonTreeItem::tr("Undefined");
    }
    return {};
}

auto Utils::JsonTreeItem::data(int column, int role) const -> QVariant
{
    if (role != Qt::DisplayRole)
        return {};
    if (column == 0)
        return m_name;
    if (column == 2)
        return typeName(m_value.type());
    if (m_value.isObject())
        return QString('[' + tr("%n Items", nullptr, m_value.toObject().size()) + ']');
    if (m_value.isArray())
        return QString('[' + tr("%n Items", nullptr, m_value.toArray().size()) + ']');
    return m_value.toVariant();
}

auto Utils::JsonTreeItem::canFetchMore() const -> bool
{
    return canFetchObjectChildren() || canFetchArrayChildren();
}

auto Utils::JsonTreeItem::fetchMore() -> void
{
    if (canFetchObjectChildren()) {
        const QJsonObject &object = m_value.toObject();
        for (const QString &key : object.keys())
            appendChild(new JsonTreeItem(key, object.value(key)));
    } else if (canFetchArrayChildren()) {
        int index = 0;
        const QJsonArray &array = m_value.toArray();
        for (const QJsonValue &val : array)
            appendChild(new JsonTreeItem(QString::number(index++), val));
    }
}

auto Utils::JsonTreeItem::canFetchObjectChildren() const -> bool
{
    return m_value.isObject() && m_value.toObject().size() > childCount();
}

auto Utils::JsonTreeItem::canFetchArrayChildren() const -> bool
{
    return m_value.isArray() && m_value.toArray().size() > childCount();
}
