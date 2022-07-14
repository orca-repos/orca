// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/itemviews.hpp>

namespace Orca::Plugin::Core {

class OpenDocumentsDelegate;

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
  OpenDocumentsDelegate *m_delegate;
};

} // namespace Orca::Plugin::Core
