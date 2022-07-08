// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QWidget>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT NamedWidget : public QWidget {
public:
  explicit NamedWidget(const QString &displayName, QWidget *parent = nullptr);

  auto displayName() const -> QString;

private:
  QString m_displayName;
};

} // namespace ProjectExplorer
