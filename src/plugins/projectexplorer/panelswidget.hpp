// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT PanelsWidget : public QWidget {
  Q_OBJECT

public:
  explicit PanelsWidget(QWidget *parent = nullptr);
  PanelsWidget(const QString &displayName, QWidget *widget);
  ~PanelsWidget() override;

  auto addPropertiesPanel(const QString &displayName, QWidget *widget) -> void;
  static int constexpr PanelVMargin = 14;

private:
  QVBoxLayout *m_layout;
  QWidget *m_root;
};

} // namespace ProjectExplorer
