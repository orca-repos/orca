// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filewizardpage.h"
#include "ui_filewizardpage.h"

#include "wizard.h"

/*!
  \class Utils::FileWizardPage

  \brief The FileWizardPage class is a standard wizard page for a single file
  letting the user choose name
  and path.

  The name and path labels can be changed. By default they are simply "Name:"
  and "Path:".
*/

namespace Utils {

class FileWizardPagePrivate {
public:
  FileWizardPagePrivate() = default;
  Ui::WizardPage m_ui;
  bool m_complete = false;
};

FileWizardPage::FileWizardPage(QWidget *parent) : WizardPage(parent), d(new FileWizardPagePrivate)
{
  d->m_ui.setupUi(this);
  connect(d->m_ui.pathChooser, &PathChooser::validChanged, this, &FileWizardPage::slotValidChanged);
  connect(d->m_ui.nameLineEdit, &FancyLineEdit::validChanged, this, &FileWizardPage::slotValidChanged);
  connect(d->m_ui.pathChooser, &PathChooser::returnPressed, this, &FileWizardPage::slotActivated);
  connect(d->m_ui.nameLineEdit, &FancyLineEdit::validReturnPressed, this, &FileWizardPage::slotActivated);
  setProperty(SHORT_TITLE_PROPERTY, tr("Location"));
  registerFieldWithName(QLatin1String("Path"), d->m_ui.pathChooser, "path", SIGNAL(pathChanged(QString)));
  registerFieldWithName(QLatin1String("FileName"), d->m_ui.nameLineEdit);
}

FileWizardPage::~FileWizardPage()
{
  delete d;
}

auto FileWizardPage::fileName() const -> QString
{
  return d->m_ui.nameLineEdit->text();
}

auto FileWizardPage::filePath() const -> FilePath
{
  return d->m_ui.pathChooser->filePath();
}

auto FileWizardPage::setFilePath(const FilePath &filePath) -> void
{
  d->m_ui.pathChooser->setFilePath(filePath);
}

auto FileWizardPage::path() const -> QString
{
  return d->m_ui.pathChooser->filePath().toString();
}

auto FileWizardPage::setPath(const QString &path) -> void
{
  d->m_ui.pathChooser->setFilePath(FilePath::fromString(path));
}

auto FileWizardPage::setFileName(const QString &name) -> void
{
  d->m_ui.nameLineEdit->setText(name);
}

auto FileWizardPage::setAllowDirectoriesInFileSelector(bool allow) -> void
{
  d->m_ui.nameLineEdit->setAllowDirectories(allow);
}

auto FileWizardPage::isComplete() const -> bool
{
  return d->m_complete;
}

auto FileWizardPage::setFileNameLabel(const QString &label) -> void
{
  d->m_ui.nameLabel->setText(label);
}

auto FileWizardPage::setPathLabel(const QString &label) -> void
{
  d->m_ui.pathLabel->setText(label);
}

auto FileWizardPage::setDefaultSuffix(const QString &suffix) -> void
{
  if (suffix.isEmpty()) {
    const auto layout = qobject_cast<QFormLayout*>(this->layout());
    if (layout->rowCount() == 3)
      layout->removeRow(0);
  } else {
    d->m_ui.defaultSuffixLabel->setText(tr("The default suffix if you do not explicitly specify a file extension is \".%1\".").arg(suffix));
  }
}

auto FileWizardPage::forceFirstCapitalLetterForFileName() const -> bool
{
  return d->m_ui.nameLineEdit->forceFirstCapitalLetter();
}

auto FileWizardPage::setForceFirstCapitalLetterForFileName(bool b) -> void
{
  d->m_ui.nameLineEdit->setForceFirstCapitalLetter(b);
}

auto FileWizardPage::slotValidChanged() -> void
{
  const bool newComplete = d->m_ui.pathChooser->isValid() && d->m_ui.nameLineEdit->isValid();
  if (newComplete != d->m_complete) {
    d->m_complete = newComplete;
    emit completeChanged();
  }
}

auto FileWizardPage::slotActivated() -> void
{
  if (d->m_complete) emit activated();
}

auto FileWizardPage::validateBaseName(const QString &name, QString *errorMessage /* = 0*/) -> bool
{
  return FileNameValidatingLineEdit::validateFileName(name, false, errorMessage);
}

} // namespace Utils
