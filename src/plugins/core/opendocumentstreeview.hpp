// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/itemviews.hpp>

namespace Core {

namespace Internal {
class OpenDocumentsDelegate;
}

class CORE_EXPORT OpenDocumentsTreeView : public Utils::TreeView {
  Q_OBJECT

public:
  explicit OpenDocumentsTreeView(QWidget *parent = nullptr);

  auto setModel(QAbstractItemModel *model) -> void override;
  auto setCloseButtonVisible(bool visible) const -> void;

signals:
  auto closeActivated(const QModelIndex &index) -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  Internal::OpenDocumentsDelegate *m_delegate;
};

} // namespace Core
