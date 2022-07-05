// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "treemodel.h"

#include "utils_global.h"

#include <QJsonValue>
#include <QCoreApplication>

namespace Utils {

class ORCA_UTILS_EXPORT JsonTreeItem : public TypedTreeItem<JsonTreeItem>
{
    Q_DECLARE_TR_FUNCTIONS(JsonTreeModelItem)

public:
    JsonTreeItem() = default;
    JsonTreeItem(const QString &displayName, const QJsonValue &value);

    auto data(int column, int role) const -> QVariant override;
    auto canFetchMore() const -> bool override;
    auto fetchMore() -> void override;

private:
    auto canFetchObjectChildren() const -> bool;
    auto canFetchArrayChildren() const -> bool;

    QString m_name;
    QJsonValue m_value;
};

} // namespace Utils
