// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QDialog>
#include <QString>

namespace CppEditor {
namespace Internal {
namespace Ui {
class CppPreProcessorDialog;
}

class CppPreProcessorDialog : public QDialog {
  Q_OBJECT

public:
  explicit CppPreProcessorDialog(const QString &filePath, QWidget *parent);
  ~CppPreProcessorDialog() override;

  auto exec() -> int override;
  auto extraPreprocessorDirectives() const -> QString;

private:
  Ui::CppPreProcessorDialog *m_ui;
  const QString m_filePath;
  const QString m_projectPartId;
};

} // namespace Internal
} // namespace CppEditor
