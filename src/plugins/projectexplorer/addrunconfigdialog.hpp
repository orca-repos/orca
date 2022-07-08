// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "runconfiguration.hpp"

#include <QDialog>

namespace Utils { class TreeView; }

namespace ProjectExplorer {
class Target;

namespace Internal {

class AddRunConfigDialog : public QDialog {
  Q_OBJECT

public:
  AddRunConfigDialog(Target *target, QWidget *parent);

  auto creationInfo() const -> RunConfigurationCreationInfo { return m_creationInfo; }

private:
  auto accept() -> void override;

  Utils::TreeView *const m_view;
  RunConfigurationCreationInfo m_creationInfo;
};

} // namespace Internal
} // namespace ProjectExplorer
