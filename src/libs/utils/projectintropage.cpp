// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectintropage.hpp"
#include "ui_projectintropage.h"

#include "filenamevalidatinglineedit.hpp"
#include "wizard.hpp"

#include <QDir>

/*!
    \class Utils::ProjectIntroPage

    \brief The ProjectIntroPage class is the standard wizard page for a project,
    letting the user choose its name
    and path.

    Looks similar to FileWizardPage, but provides additional
    functionality:
    \list
    \li Contains a description label at the top for displaying introductory text.
    \li Does on the fly validation (connected to changed()) and displays
       warnings and errors in a status label at the bottom (the page is complete
       when fully validated, validatePage() is thus not implemented).
    \endlist

    \note Careful when changing projectintropage.ui. It must have main
    geometry cleared and QLayout::SetMinimumSize constraint on the main
    layout, otherwise, QWizard will squeeze it due to its strange expanding
    hacks.
*/

namespace Utils {

class ProjectIntroPagePrivate {
public:
  Ui::ProjectIntroPage m_ui;
  bool m_complete = false;
  QRegularExpressionValidator m_projectNameValidator;
  QString m_projectNameValidatorUserMessage;
  bool m_forceSubProject = false;
  FilePaths m_projectDirectories;
};

ProjectIntroPage::ProjectIntroPage(QWidget *parent) : WizardPage(parent), d(new ProjectIntroPagePrivate)
{
  d->m_ui.setupUi(this);
  d->m_ui.stateLabel->setFilled(true);
  hideStatusLabel();
  d->m_ui.nameLineEdit->setPlaceholderText(tr("Enter project name"));
  d->m_ui.nameLineEdit->setFocus();
  d->m_ui.nameLineEdit->setValidationFunction([this](FancyLineEdit *edit, QString *errorString) {
    return validateProjectName(edit->text(), errorString);
  });
  d->m_ui.projectLabel->setVisible(d->m_forceSubProject);
  d->m_ui.projectComboBox->setVisible(d->m_forceSubProject);
  d->m_ui.pathChooser->setDisabled(d->m_forceSubProject);
  d->m_ui.projectsDirectoryCheckBox->setDisabled(d->m_forceSubProject);
  connect(d->m_ui.pathChooser, &PathChooser::pathChanged, this, &ProjectIntroPage::slotChanged);
  connect(d->m_ui.nameLineEdit, &QLineEdit::textChanged, this, &ProjectIntroPage::slotChanged);
  connect(d->m_ui.pathChooser, &PathChooser::validChanged, this, &ProjectIntroPage::slotChanged);
  connect(d->m_ui.pathChooser, &PathChooser::returnPressed, this, &ProjectIntroPage::slotActivated);
  connect(d->m_ui.nameLineEdit, &FancyLineEdit::validReturnPressed, this, &ProjectIntroPage::slotActivated);
  connect(d->m_ui.projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectIntroPage::slotChanged);

  setProperty(SHORT_TITLE_PROPERTY, tr("Location"));
  registerFieldWithName(QLatin1String("Path"), d->m_ui.pathChooser, "path", SIGNAL(pathChanged(QString)));
  registerFieldWithName(QLatin1String("ProjectName"), d->m_ui.nameLineEdit);
}

auto ProjectIntroPage::insertControl(int row, QWidget *label, QWidget *control) -> void
{
  d->m_ui.formLayout->insertRow(row, label, control);
}

ProjectIntroPage::~ProjectIntroPage()
{
  delete d;
}

auto ProjectIntroPage::projectName() const -> QString
{
  return d->m_ui.nameLineEdit->text();
}

auto ProjectIntroPage::filePath() const -> FilePath
{
  return d->m_ui.pathChooser->filePath();
}

auto ProjectIntroPage::setFilePath(const FilePath &path) -> void
{
  d->m_ui.pathChooser->setFilePath(path);
}

auto ProjectIntroPage::setProjectNameRegularExpression(const QRegularExpression &regEx, const QString &userErrorMessage) -> void
{
  Q_ASSERT_X(regEx.isValid(), Q_FUNC_INFO, qPrintable(regEx.errorString()));
  d->m_projectNameValidator.setRegularExpression(regEx);
  d->m_projectNameValidatorUserMessage = userErrorMessage;
}

auto ProjectIntroPage::setProjectName(const QString &name) -> void
{
  d->m_ui.nameLineEdit->setText(name);
  d->m_ui.nameLineEdit->selectAll();
}

auto ProjectIntroPage::description() const -> QString
{
  return d->m_ui.descriptionLabel->text();
}

auto ProjectIntroPage::setDescription(const QString &description) -> void
{
  d->m_ui.descriptionLabel->setText(description);
}

auto ProjectIntroPage::isComplete() const -> bool
{
  return d->m_complete;
}

auto ProjectIntroPage::validate() -> bool
{
  if (d->m_forceSubProject) {
    int index = d->m_ui.projectComboBox->currentIndex();
    if (index == 0)
      return false;
    d->m_ui.pathChooser->setFilePath(d->m_projectDirectories.at(index));
  }
  // Validate and display status
  if (!d->m_ui.pathChooser->isValid()) {
    displayStatusMessage(InfoLabel::Error, d->m_ui.pathChooser->errorMessage());
    return false;
  }

  // Name valid?
  switch (d->m_ui.nameLineEdit->state()) {
  case FancyLineEdit::Invalid:
    displayStatusMessage(InfoLabel::Error, d->m_ui.nameLineEdit->errorMessage());
    return false;
  case FancyLineEdit::DisplayingPlaceholderText:
    displayStatusMessage(InfoLabel::Error, tr("Name is empty."));
    return false;
  case FancyLineEdit::Valid:
    break;
  }

  // Check existence of the directory
  const FilePath projectDir = filePath().pathAppended(QDir::fromNativeSeparators(d->m_ui.nameLineEdit->text()));

  if (!projectDir.exists()) {
    // All happy
    hideStatusLabel();
    return true;
  }

  if (projectDir.isDir()) {
    displayStatusMessage(InfoLabel::Warning, tr("The project already exists."));
    return true;
  }
  // Not a directory, but something else, likely causing directory creation to fail
  displayStatusMessage(InfoLabel::Error, tr("A file with that name already exists."));
  return false;
}

auto ProjectIntroPage::fieldsUpdated() -> void
{
  slotChanged();
}

auto ProjectIntroPage::slotChanged() -> void
{
  const bool newComplete = validate();
  if (newComplete != d->m_complete) {
    d->m_complete = newComplete;
    emit completeChanged();
  }
}

auto ProjectIntroPage::slotActivated() -> void
{
  if (d->m_complete) emit activated();
}

auto ProjectIntroPage::forceSubProject() const -> bool
{
  return d->m_forceSubProject;
}

auto ProjectIntroPage::setForceSubProject(bool force) -> void
{
  d->m_forceSubProject = force;
  d->m_ui.projectLabel->setVisible(d->m_forceSubProject);
  d->m_ui.projectComboBox->setVisible(d->m_forceSubProject);
  d->m_ui.pathChooser->setDisabled(d->m_forceSubProject);
  d->m_ui.projectsDirectoryCheckBox->setDisabled(d->m_forceSubProject);
}

auto ProjectIntroPage::setProjectList(const QStringList &projectList) -> void
{
  d->m_ui.projectComboBox->clear();
  d->m_ui.projectComboBox->addItems(projectList);
}

auto ProjectIntroPage::setProjectDirectories(const FilePaths &directoryList) -> void
{
  d->m_projectDirectories = directoryList;
}

auto ProjectIntroPage::projectIndex() const -> int
{
  return d->m_ui.projectComboBox->currentIndex();
}

auto ProjectIntroPage::validateProjectName(const QString &name, QString *errorMessage) -> bool
{
  int pos = -1;
  // if we have a pattern it was set
  if (!d->m_projectNameValidator.regularExpression().pattern().isEmpty()) {
    if (name.isEmpty()) {
      if (errorMessage)
        *errorMessage = tr("Name is empty.");
      return false;
    }
    // pos is set by reference
    QString tmp = name;
    QValidator::State validatorState = d->m_projectNameValidator.validate(tmp, pos);

    // if pos is set by validate it is cought at the bottom where it shows
    // a more detailed error message
    if (validatorState != QValidator::Acceptable && (pos == -1 || pos >= name.count())) {
      if (errorMessage) {
        if (d->m_projectNameValidatorUserMessage.isEmpty())
          *errorMessage = tr("Project name is invalid.");
        else
          *errorMessage = d->m_projectNameValidatorUserMessage;
      }
      return false;
    }
  } else {
    // no validator means usually a qmake project
    // Validation is file name + checking for dots
    if (!FileNameValidatingLineEdit::validateFileName(name, false, errorMessage))
      return false;
    if (name.contains(QLatin1Char('.'))) {
      if (errorMessage)
        *errorMessage = tr("Invalid character \".\".");
      return false;
    }
    pos = FileUtils::indexOfQmakeUnfriendly(name);
  }
  if (pos >= 0) {
    if (errorMessage)
      *errorMessage = tr("Invalid character \"%1\" found.").arg(name.at(pos));
    return false;
  }
  return true;
}

auto ProjectIntroPage::displayStatusMessage(InfoLabel::InfoType t, const QString &s) -> void
{
  d->m_ui.stateLabel->setType(t);
  d->m_ui.stateLabel->setText(s);

  emit statusMessageChanged(t, s);
}

auto ProjectIntroPage::hideStatusLabel() -> void
{
  displayStatusMessage(InfoLabel::None, {});
}

auto ProjectIntroPage::useAsDefaultPath() const -> bool
{
  return d->m_ui.projectsDirectoryCheckBox->isChecked();
}

auto ProjectIntroPage::setUseAsDefaultPath(bool u) -> void
{
  d->m_ui.projectsDirectoryCheckBox->setChecked(u);
}

} // namespace Utils
