// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

namespace ProjectExplorer {

class DeployConfiguration;

namespace Internal {

class DeploymentDataView : public QWidget {
  Q_OBJECT

public:
  explicit DeploymentDataView(DeployConfiguration *dc);
};

} // Internal
} // ProjectExplorer
