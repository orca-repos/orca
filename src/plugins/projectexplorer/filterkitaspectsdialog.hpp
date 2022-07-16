// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.hpp>

#include <QDialog>

namespace Utils {
class BaseTreeModel;
}

namespace ProjectExplorer {
class Kit;
namespace Internal {

class FilterKitAspectsDialog : public QDialog {
  Q_OBJECT

public:
  FilterKitAspectsDialog(const Kit *kit, QWidget *parent);

  auto irrelevantAspects() const -> QSet<Utils::Id>;

private:
  Utils::BaseTreeModel *const m_model;
};

} // namespace Internal
} // namespace ProjectExplorer
