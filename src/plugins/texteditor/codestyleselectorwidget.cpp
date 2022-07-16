// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codestyleselectorwidget.hpp"
#include "ui_codestyleselectorwidget.h"
#include "icodestylepreferences.hpp"
#include "icodestylepreferencesfactory.hpp"
#include "codestylepool.hpp"
#include "tabsettings.hpp"

#include <utils/fileutils.hpp>

#include <QPushButton>
#include <QDialogButtonBox>
#include <QDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>

using namespace TextEditor;
using namespace Utils;

namespace TextEditor {
namespace Internal {

class CodeStyleDialog : public QDialog {
  Q_OBJECT

public:
  explicit CodeStyleDialog(ICodeStylePreferencesFactory *factory, ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project = nullptr, QWidget *parent = nullptr);
  ~CodeStyleDialog() override;

  auto codeStyle() const -> ICodeStylePreferences*;

private:
  auto slotCopyClicked() -> void;
  auto slotDisplayNameChanged() -> void;

  ICodeStylePreferences *m_codeStyle;
  QLineEdit *m_lineEdit;
  QDialogButtonBox *m_buttons;
  QLabel *m_warningLabel = nullptr;
  QPushButton *m_copyButton = nullptr;
  QString m_originalDisplayName;
};

CodeStyleDialog::CodeStyleDialog(ICodeStylePreferencesFactory *factory, ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project, QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("Edit Code Style"));
  const auto layout = new QVBoxLayout(this);
  const auto label = new QLabel(tr("Code style name:"));
  m_lineEdit = new QLineEdit(codeStyle->displayName(), this);
  const auto nameLayout = new QHBoxLayout;
  nameLayout->addWidget(label);
  nameLayout->addWidget(m_lineEdit);
  layout->addLayout(nameLayout);

  if (codeStyle->isReadOnly()) {
    const auto warningLayout = new QHBoxLayout;
    m_warningLabel = new QLabel(tr("You cannot save changes to a built-in code style. " "Copy it first to create your own version."), this);
    auto font = m_warningLabel->font();
    font.setItalic(true);
    m_warningLabel->setFont(font);
    m_warningLabel->setWordWrap(true);
    m_copyButton = new QPushButton(tr("Copy Built-in Code Style"), this);
    m_copyButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_copyButton, &QAbstractButton::clicked, this, &CodeStyleDialog::slotCopyClicked);
    warningLayout->addWidget(m_warningLabel);
    warningLayout->addWidget(m_copyButton);
    layout->addLayout(warningLayout);
  }

  m_originalDisplayName = codeStyle->displayName();
  m_codeStyle = factory->createCodeStyle();
  m_codeStyle->setTabSettings(codeStyle->tabSettings());
  m_codeStyle->setValue(codeStyle->value());
  m_codeStyle->setId(codeStyle->id());
  m_codeStyle->setDisplayName(m_originalDisplayName);
  const auto editor = factory->createEditor(m_codeStyle, project, this);

  m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  if (codeStyle->isReadOnly()) {
    const auto okButton = m_buttons->button(QDialogButtonBox::Ok);
    okButton->setEnabled(false);
  }

  if (editor)
    layout->addWidget(editor);
  layout->addWidget(m_buttons);
  resize(850, 600);

  connect(m_lineEdit, &QLineEdit::textChanged, this, &CodeStyleDialog::slotDisplayNameChanged);
  connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

auto CodeStyleDialog::codeStyle() const -> ICodeStylePreferences*
{
  return m_codeStyle;
}

auto CodeStyleDialog::slotCopyClicked() -> void
{
  if (m_warningLabel)
    m_warningLabel->hide();
  if (m_copyButton)
    m_copyButton->hide();
  const auto okButton = m_buttons->button(QDialogButtonBox::Ok);
  okButton->setEnabled(true);
  if (m_lineEdit->text() == m_originalDisplayName)
    m_lineEdit->setText(tr("%1 (Copy)").arg(m_lineEdit->text()));
  m_lineEdit->selectAll();
}

auto CodeStyleDialog::slotDisplayNameChanged() -> void
{
  m_codeStyle->setDisplayName(m_lineEdit->text());
}

CodeStyleDialog::~CodeStyleDialog()
{
  delete m_codeStyle;
}

} // Internal

CodeStyleSelectorWidget::CodeStyleSelectorWidget(ICodeStylePreferencesFactory *factory, ProjectExplorer::Project *project, QWidget *parent) : QWidget(parent), m_factory(factory), m_project(project), m_ui(new Internal::Ui::CodeStyleSelectorWidget)
{
  m_ui->setupUi(this);
  m_ui->importButton->setEnabled(false);
  m_ui->exportButton->setEnabled(false);

  connect(m_ui->delegateComboBox, QOverload<int>::of(&QComboBox::activated), this, &CodeStyleSelectorWidget::slotComboBoxActivated);
  connect(m_ui->copyButton, &QAbstractButton::clicked, this, &CodeStyleSelectorWidget::slotCopyClicked);
  connect(m_ui->editButton, &QAbstractButton::clicked, this, &CodeStyleSelectorWidget::slotEditClicked);
  connect(m_ui->removeButton, &QAbstractButton::clicked, this, &CodeStyleSelectorWidget::slotRemoveClicked);
  connect(m_ui->importButton, &QAbstractButton::clicked, this, &CodeStyleSelectorWidget::slotImportClicked);
  connect(m_ui->exportButton, &QAbstractButton::clicked, this, &CodeStyleSelectorWidget::slotExportClicked);
}

CodeStyleSelectorWidget::~CodeStyleSelectorWidget()
{
  delete m_ui;
}

auto CodeStyleSelectorWidget::setCodeStyle(ICodeStylePreferences *codeStyle) -> void
{
  if (m_codeStyle == codeStyle)
    return; // nothing changes

  // cleanup old
  if (m_codeStyle) {
    const auto codeStylePool = m_codeStyle->delegatingPool();
    if (codeStylePool) {
      disconnect(codeStylePool, &CodeStylePool::codeStyleAdded, this, &CodeStyleSelectorWidget::slotCodeStyleAdded);
      disconnect(codeStylePool, &CodeStylePool::codeStyleRemoved, this, &CodeStyleSelectorWidget::slotCodeStyleRemoved);
    }
    disconnect(m_codeStyle, &ICodeStylePreferences::currentDelegateChanged, this, &CodeStyleSelectorWidget::slotCurrentDelegateChanged);

    m_ui->exportButton->setEnabled(false);
    m_ui->importButton->setEnabled(false);
    m_ui->delegateComboBox->clear();
  }
  m_codeStyle = codeStyle;
  // fillup new
  if (m_codeStyle) {
    QList<ICodeStylePreferences*> delegates;
    const auto codeStylePool = m_codeStyle->delegatingPool();
    if (codeStylePool) {
      delegates = codeStylePool->codeStyles();

      connect(codeStylePool, &CodeStylePool::codeStyleAdded, this, &CodeStyleSelectorWidget::slotCodeStyleAdded);
      connect(codeStylePool, &CodeStylePool::codeStyleRemoved, this, &CodeStyleSelectorWidget::slotCodeStyleRemoved);
      m_ui->exportButton->setEnabled(true);
      m_ui->importButton->setEnabled(true);
    }

    for (auto i = 0; i < delegates.count(); i++)
      slotCodeStyleAdded(delegates.at(i));

    slotCurrentDelegateChanged(m_codeStyle->currentDelegate());

    connect(m_codeStyle, &ICodeStylePreferences::currentDelegateChanged, this, &CodeStyleSelectorWidget::slotCurrentDelegateChanged);
  }
}

auto CodeStyleSelectorWidget::slotComboBoxActivated(int index) -> void
{
  if (m_ignoreGuiSignals)
    return;

  if (index < 0 || index >= m_ui->delegateComboBox->count())
    return;
  auto delegate = m_ui->delegateComboBox->itemData(index).value<ICodeStylePreferences*>();

  QSignalBlocker blocker(this);
  m_codeStyle->setCurrentDelegate(delegate);
}

auto CodeStyleSelectorWidget::slotCurrentDelegateChanged(ICodeStylePreferences *delegate) -> void
{
  m_ignoreGuiSignals = true;
  m_ui->delegateComboBox->setCurrentIndex(m_ui->delegateComboBox->findData(QVariant::fromValue(delegate)));
  m_ui->delegateComboBox->setToolTip(m_ui->delegateComboBox->currentText());
  m_ignoreGuiSignals = false;

  const auto removeEnabled = delegate && !delegate->isReadOnly() && !delegate->currentDelegate();
  m_ui->removeButton->setEnabled(removeEnabled);
}

auto CodeStyleSelectorWidget::slotCopyClicked() -> void
{
  if (!m_codeStyle)
    return;

  const auto codeStylePool = m_codeStyle->delegatingPool();
  const auto currentPreferences = m_codeStyle->currentPreferences();
  auto ok = false;
  const auto newName = QInputDialog::getText(this, tr("Copy Code Style"), tr("Code style name:"), QLineEdit::Normal, tr("%1 (Copy)").arg(currentPreferences->displayName()), &ok);
  if (!ok || newName.trimmed().isEmpty())
    return;
  const auto copy = codeStylePool->cloneCodeStyle(currentPreferences);
  if (copy) {
    copy->setDisplayName(newName);
    m_codeStyle->setCurrentDelegate(copy);
  }
}

auto CodeStyleSelectorWidget::slotEditClicked() -> void
{
  if (!m_codeStyle)
    return;

  auto codeStyle = m_codeStyle->currentPreferences();
  // check if it's read-only

  Internal::CodeStyleDialog dialog(m_factory, codeStyle, m_project, this);
  if (dialog.exec() == QDialog::Accepted) {
    const auto dialogCodeStyle = dialog.codeStyle();
    if (codeStyle->isReadOnly()) {
      const auto codeStylePool = m_codeStyle->delegatingPool();
      codeStyle = codeStylePool->cloneCodeStyle(dialogCodeStyle);
      if (codeStyle)
        m_codeStyle->setCurrentDelegate(codeStyle);
      return;
    }
    codeStyle->setTabSettings(dialogCodeStyle->tabSettings());
    codeStyle->setValue(dialogCodeStyle->value());
    codeStyle->setDisplayName(dialogCodeStyle->displayName());
  }
}

auto CodeStyleSelectorWidget::slotRemoveClicked() -> void
{
  if (!m_codeStyle)
    return;

  const auto codeStylePool = m_codeStyle->delegatingPool();
  const auto currentPreferences = m_codeStyle->currentPreferences();

  QMessageBox messageBox(QMessageBox::Warning, tr("Delete Code Style"), tr("Are you sure you want to delete this code style permanently?"), QMessageBox::Discard | QMessageBox::Cancel, this);

  // Change the text and role of the discard button
  const auto deleteButton = static_cast<QPushButton*>(messageBox.button(QMessageBox::Discard));
  deleteButton->setText(tr("Delete"));
  messageBox.addButton(deleteButton, QMessageBox::AcceptRole);
  messageBox.setDefaultButton(deleteButton);

  connect(deleteButton, &QAbstractButton::clicked, &messageBox, &QDialog::accept);
  if (messageBox.exec() == QDialog::Accepted)
    codeStylePool->removeCodeStyle(currentPreferences);
}

auto CodeStyleSelectorWidget::slotImportClicked() -> void
{
  const auto fileName = FileUtils::getOpenFilePath(this, tr("Import Code Style"), {}, tr("Code styles (*.xml);;All files (*)"));
  if (!fileName.isEmpty()) {
    const auto codeStylePool = m_codeStyle->delegatingPool();
    const auto importedStyle = codeStylePool->importCodeStyle(fileName);
    if (importedStyle)
      m_codeStyle->setCurrentDelegate(importedStyle);
    else
      QMessageBox::warning(this, tr("Import Code Style"), tr("Cannot import code style from %1"), fileName.toUserOutput());
  }
}

auto CodeStyleSelectorWidget::slotExportClicked() -> void
{
  const auto currentPreferences = m_codeStyle->currentPreferences();
  const auto filePath = FileUtils::getSaveFilePath(this, tr("Export Code Style"), FilePath::fromString(QString::fromUtf8(currentPreferences->id() + ".xml")), tr("Code styles (*.xml);;All files (*)"));
  if (!filePath.isEmpty()) {
    const auto codeStylePool = m_codeStyle->delegatingPool();
    codeStylePool->exportCodeStyle(filePath, currentPreferences);
  }
}

auto CodeStyleSelectorWidget::slotCodeStyleAdded(ICodeStylePreferences *codeStylePreferences) -> void
{
  if (codeStylePreferences == m_codeStyle || codeStylePreferences->id() == m_codeStyle->id())
    return;

  const auto data = QVariant::fromValue(codeStylePreferences);
  const auto name = displayName(codeStylePreferences);
  m_ui->delegateComboBox->addItem(name, data);
  m_ui->delegateComboBox->setItemData(m_ui->delegateComboBox->count() - 1, name, Qt::ToolTipRole);
  connect(codeStylePreferences, &ICodeStylePreferences::displayNameChanged, this, &CodeStyleSelectorWidget::slotUpdateName);
  if (codeStylePreferences->delegatingPool()) {
    connect(codeStylePreferences, &ICodeStylePreferences::currentPreferencesChanged, this, &CodeStyleSelectorWidget::slotUpdateName);
  }
}

auto CodeStyleSelectorWidget::slotCodeStyleRemoved(ICodeStylePreferences *codeStylePreferences) -> void
{
  m_ignoreGuiSignals = true;
  m_ui->delegateComboBox->removeItem(m_ui->delegateComboBox->findData(QVariant::fromValue(codeStylePreferences)));
  disconnect(codeStylePreferences, &ICodeStylePreferences::displayNameChanged, this, &CodeStyleSelectorWidget::slotUpdateName);
  if (codeStylePreferences->delegatingPool()) {
    disconnect(codeStylePreferences, &ICodeStylePreferences::currentPreferencesChanged, this, &CodeStyleSelectorWidget::slotUpdateName);
  }
  m_ignoreGuiSignals = false;
}

auto CodeStyleSelectorWidget::slotUpdateName() -> void
{
  const auto changedCodeStyle = qobject_cast<ICodeStylePreferences*>(sender());
  if (!changedCodeStyle)
    return;

  updateName(changedCodeStyle);

  const auto codeStyles = m_codeStyle->delegatingPool()->codeStyles();
  for (auto i = 0; i < codeStyles.count(); i++) {
    const auto codeStyle = codeStyles.at(i);
    if (codeStyle->currentDelegate() == changedCodeStyle)
      updateName(codeStyle);
  }

  m_ui->delegateComboBox->setToolTip(m_ui->delegateComboBox->currentText());
}

auto CodeStyleSelectorWidget::updateName(ICodeStylePreferences *codeStyle) -> void
{
  const int idx = m_ui->delegateComboBox->findData(QVariant::fromValue(codeStyle));
  if (idx < 0)
    return;

  const auto name = displayName(codeStyle);
  m_ui->delegateComboBox->setItemText(idx, name);
  m_ui->delegateComboBox->setItemData(idx, name, Qt::ToolTipRole);
}

auto CodeStyleSelectorWidget::displayName(ICodeStylePreferences *codeStyle) const -> QString
{
  auto name = codeStyle->displayName();
  if (codeStyle->currentDelegate())
    name = tr("%1 [proxy: %2]").arg(name).arg(codeStyle->currentDelegate()->displayName());
  if (codeStyle->isReadOnly())
    name = tr("%1 [built-in]").arg(name);
  return name;
}

} // TextEditor

#include "codestyleselectorwidget.moc"
