// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QDialog>

namespace ProjectExplorer {
namespace Internal {

class ParseIssuesDialog : public QDialog {
  Q_OBJECT

public:
  ParseIssuesDialog(QWidget *parent = nullptr);
  ~ParseIssuesDialog() override;

private:
  auto accept() -> void override;

  class Private;
  Private *const d;
};

} // namespace Internal
} // namespace ProjectExplorer
