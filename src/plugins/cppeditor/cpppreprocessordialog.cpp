// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpppreprocessordialog.hpp"
#include "ui_cpppreprocessordialog.h"

#include "cppeditorwidget.hpp"
#include "cppeditorconstants.hpp"
#include "cpptoolsreuse.hpp"

#include <projectexplorer/session.hpp>

using namespace CppEditor::Internal;

CppPreProcessorDialog::CppPreProcessorDialog(const QString &filePath, QWidget *parent) : QDialog(parent), m_ui(new Ui::CppPreProcessorDialog()), m_filePath(filePath)
{
  m_ui->setupUi(this);
  m_ui->editorLabel->setText(m_ui->editorLabel->text().arg(Utils::FilePath::fromString(m_filePath).fileName()));
  m_ui->editWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  decorateCppEditor(m_ui->editWidget);

  const QString key = Constants::EXTRA_PREPROCESSOR_DIRECTIVES + m_filePath;
  const auto directives = ProjectExplorer::SessionManager::value(key).toString();
  m_ui->editWidget->setPlainText(directives);
}

CppPreProcessorDialog::~CppPreProcessorDialog()
{
  delete m_ui;
}

auto CppPreProcessorDialog::exec() -> int
{
  if (QDialog::exec() == Rejected)
    return Rejected;

  const QString key = Constants::EXTRA_PREPROCESSOR_DIRECTIVES + m_filePath;
  ProjectExplorer::SessionManager::setValue(key, extraPreprocessorDirectives());

  return Accepted;
}

auto CppPreProcessorDialog::extraPreprocessorDirectives() const -> QString
{
  return m_ui->editWidget->toPlainText();
}
