// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentfwd.h"
#include "utils_global.h"

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QTimer>
#include <QValidator>

namespace Utils {

class ORCA_UTILS_EXPORT NameValueValidator : public QValidator {
  Q_OBJECT public:
  NameValueValidator(QWidget *parent, Utils::NameValueModel *model, QTreeView *view, const QModelIndex &index, const QString &toolTipText);

  auto validate(QString &in, int &pos) const -> QValidator::State override;

  auto fixup(QString &input) const -> void override;

private:
  const QString m_toolTipText;
  Utils::NameValueModel *m_model;
  QTreeView *m_view;
  QPersistentModelIndex m_index;
  mutable QTimer m_hideTipTimer;
};

} // namespace Utils
