// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwizardpage.hpp"
#include "customwizardparameters.hpp"

#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/textfieldcheckbox.hpp>
#include <utils/textfieldcombobox.hpp>

#include <QDebug>
#include <QDir>
#include <QDate>
#include <QTime>

#include <QWizardPage>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QRegularExpressionValidator>
#include <QComboBox>
#include <QTextEdit>
#include <QSpacerItem>

enum {
  debug = 0
};

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

/*!
    \class ProjectExplorer::Internal::CustomWizardFieldPage
    \brief The CustomWizardFieldPage class is a simple custom wizard page
    presenting the fields to be used
    as page 2 of a BaseProjectWizardDialog if there are any fields.

    Uses the 'field' functionality of QWizard.
    Implements validatePage() as the field logic cannot be tied up
    with additional validation. Performs checking of the Javascript-based
    validation rules of the parameters and displays error messages in a red
    warning label.

    \sa ProjectExplorer::CustomWizard
*/

CustomWizardFieldPage::LineEditData::LineEditData(QLineEdit *le, const QString &defText, const QString &pText) : lineEdit(le), defaultText(defText), placeholderText(pText) {}

CustomWizardFieldPage::TextEditData::TextEditData(QTextEdit *le, const QString &defText) : textEdit(le), defaultText(defText) {}

CustomWizardFieldPage::PathChooserData::PathChooserData(PathChooser *pe, const QString &defText) : pathChooser(pe), defaultText(defText) {}

CustomWizardFieldPage::CustomWizardFieldPage(const QSharedPointer<CustomWizardContext> &ctx, const QSharedPointer<CustomWizardParameters> &parameters, QWidget *parent) : QWizardPage(parent), m_parameters(parameters), m_context(ctx), m_formLayout(new QFormLayout), m_errorLabel(new QLabel)
{
  const auto vLayout = new QVBoxLayout;
  m_formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  if (debug)
    qDebug() << Q_FUNC_INFO << parameters->fields.size();
  foreach(const CustomWizardField &f, parameters->fields)
    addField(f);
  vLayout->addLayout(m_formLayout);
  m_errorLabel->setVisible(false);
  m_errorLabel->setStyleSheet(QLatin1String("background: red"));
  vLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
  vLayout->addWidget(m_errorLabel);
  setLayout(vLayout);
  if (!parameters->fieldPageTitle.isEmpty())
    setTitle(parameters->fieldPageTitle);
}

auto CustomWizardFieldPage::addRow(const QString &name, QWidget *w) -> void
{
  m_formLayout->addRow(name, w);
}

auto CustomWizardFieldPage::showError(const QString &m) -> void
{
  m_errorLabel->setText(m);
  m_errorLabel->setVisible(true);
}

auto CustomWizardFieldPage::clearError() -> void
{
  m_errorLabel->clear();
  m_errorLabel->setVisible(false);
}

/*!
    Creates a widget based on the control attributes map and registers it with
    the QWizard.
*/

auto CustomWizardFieldPage::addField(const CustomWizardField &field) -> void
{
  //  Register field, indicate mandatory by '*' (only when registering)
  auto fieldName = field.name;
  if (field.mandatory)
    fieldName += QLatin1Char('*');
  auto spansRow = false;
  // Check known classes: QComboBox
  const auto className = field.controlAttributes.value(QLatin1String("class"));
  QWidget *fieldWidget = nullptr;
  if (className == QLatin1String("QComboBox")) {
    fieldWidget = registerComboBox(fieldName, field);
  } else if (className == QLatin1String("QTextEdit")) {
    fieldWidget = registerTextEdit(fieldName, field);
  } else if (className == QLatin1String("Utils::PathChooser")) {
    fieldWidget = registerPathChooser(fieldName, field);
  } else if (className == QLatin1String("QCheckBox")) {
    fieldWidget = registerCheckBox(fieldName, field.description, field);
    spansRow = true; // Do not create a label for the checkbox.
  } else {
    fieldWidget = registerLineEdit(fieldName, field);
  }
  if (spansRow)
    m_formLayout->addRow(fieldWidget);
  else
    addRow(field.description, fieldWidget);
}

// Return the list of values and display texts for combo
static auto comboChoices(const CustomWizardField::ControlAttributeMap &controlAttributes, QStringList *values, QStringList *displayTexts) -> void
{
  using AttribMapConstIt = CustomWizardField::ControlAttributeMap::ConstIterator;

  values->clear();
  displayTexts->clear();
  // Pre 2.2 Legacy: "combochoices" attribute with a comma-separated list, for
  // display == value.
  const auto attribConstEnd = controlAttributes.constEnd();
  const auto choicesIt = controlAttributes.constFind(QLatin1String("combochoices"));
  if (choicesIt != attribConstEnd) {
    const auto &choices = choicesIt.value();
    if (!choices.isEmpty())
      *values = *displayTexts = choices.split(QLatin1Char(','));
    return;
  }
  // From 2.2 on: Separate lists of value and text. Add all values found.
  for (auto i = 0; ; i++) {
    const auto valueKey = CustomWizardField::comboEntryValueKey(i);
    const auto valueIt = controlAttributes.constFind(valueKey);
    if (valueIt == attribConstEnd)
      break;
    values->push_back(valueIt.value());
    const auto textKey = CustomWizardField::comboEntryTextKey(i);
    displayTexts->push_back(controlAttributes.value(textKey));
  }
}

auto CustomWizardFieldPage::registerComboBox(const QString &fieldName, const CustomWizardField &field) -> QWidget*
{
  const auto combo = new TextFieldComboBox;
  do {
    // Set up items and current index
    QStringList values;
    QStringList displayTexts;
    comboChoices(field.controlAttributes, &values, &displayTexts);
    combo->setItems(displayTexts, values);
    bool ok;
    const auto currentIndexS = field.controlAttributes.value(QLatin1String("defaultindex"));
    if (currentIndexS.isEmpty())
      break;
    const auto currentIndex = currentIndexS.toInt(&ok);
    if (!ok || currentIndex < 0 || currentIndex >= combo->count())
      break;
    combo->setCurrentIndex(currentIndex);
  } while (false);
  registerField(fieldName, combo, "indexText", SIGNAL(text4Changed(QString)));
  // Connect to completeChanged() for derived classes that reimplement isComplete()
  connect(combo, &TextFieldComboBox::text4Changed, this, &QWizardPage::completeChanged);
  return combo;
} // QComboBox

auto CustomWizardFieldPage::registerTextEdit(const QString &fieldName, const CustomWizardField &field) -> QWidget*
{
  const auto textEdit = new QTextEdit;
  // Suppress formatting by default (inverting QTextEdit's default value) when
  // pasting from Bug tracker, etc.
  const auto acceptRichText = field.controlAttributes.value(QLatin1String("acceptRichText")) == QLatin1String("true");
  textEdit->setAcceptRichText(acceptRichText);
  // Connect to completeChanged() for derived classes that reimplement isComplete()
  registerField(fieldName, textEdit, "plainText", SIGNAL(textChanged()));
  connect(textEdit, &QTextEdit::textChanged, this, &QWizardPage::completeChanged);
  const auto defaultText = field.controlAttributes.value(QLatin1String("defaulttext"));
  m_textEdits.push_back(TextEditData(textEdit, defaultText));
  return textEdit;
} // QTextEdit

auto CustomWizardFieldPage::registerPathChooser(const QString &fieldName, const CustomWizardField &field) -> QWidget*
{
  const auto pathChooser = new PathChooser;
  const auto expectedKind = field.controlAttributes.value(QLatin1String("expectedkind")).toLower();
  if (expectedKind == QLatin1String("existingdirectory"))
    pathChooser->setExpectedKind(PathChooser::ExistingDirectory);
  else if (expectedKind == QLatin1String("directory"))
    pathChooser->setExpectedKind(PathChooser::Directory);
  else if (expectedKind == QLatin1String("file"))
    pathChooser->setExpectedKind(PathChooser::File);
  else if (expectedKind == QLatin1String("existingcommand"))
    pathChooser->setExpectedKind(PathChooser::ExistingCommand);
  else if (expectedKind == QLatin1String("command"))
    pathChooser->setExpectedKind(PathChooser::Command);
  else if (expectedKind == QLatin1String("any"))
    pathChooser->setExpectedKind(PathChooser::Any);
  pathChooser->setHistoryCompleter(QString::fromLatin1("PE.Custom.") + m_parameters->id.toString() + QLatin1Char('.') + field.name);

  registerField(fieldName, pathChooser, "path", SIGNAL(rawPathChanged(QString)));
  // Connect to completeChanged() for derived classes that reimplement isComplete()
  connect(pathChooser, &PathChooser::rawPathChanged, this, &QWizardPage::completeChanged);
  const auto defaultText = field.controlAttributes.value(QLatin1String("defaulttext"));
  m_pathChoosers.push_back(PathChooserData(pathChooser, defaultText));
  return pathChooser;
} // Utils::PathChooser

auto CustomWizardFieldPage::registerCheckBox(const QString &fieldName, const QString &fieldDescription, const CustomWizardField &field) -> QWidget*
{
  using AttributeMapConstIt = CustomWizardField::ControlAttributeMap::const_iterator;

  const auto checkBox = new TextFieldCheckBox(fieldDescription);
  const auto defaultValue = field.controlAttributes.value(QLatin1String("defaultvalue")) == QLatin1String("true");
  checkBox->setChecked(defaultValue);
  const auto trueTextIt = field.controlAttributes.constFind(QLatin1String("truevalue"));
  if (trueTextIt != field.controlAttributes.constEnd()) // Also set empty texts
    checkBox->setTrueText(trueTextIt.value());
  const auto falseTextIt = field.controlAttributes.constFind(QLatin1String("falsevalue"));
  if (falseTextIt != field.controlAttributes.constEnd()) // Also set empty texts
    checkBox->setFalseText(falseTextIt.value());
  registerField(fieldName, checkBox, "compareText", SIGNAL(textChanged(QString)));
  // Connect to completeChanged() for derived classes that reimplement isComplete()
  connect(checkBox, &TextFieldCheckBox::textChanged, this, &QWizardPage::completeChanged);
  return checkBox;
}

auto CustomWizardFieldPage::registerLineEdit(const QString &fieldName, const CustomWizardField &field) -> QWidget*
{
  const auto lineEdit = new QLineEdit;

  const auto validationRegExp = field.controlAttributes.value(QLatin1String("validator"));
  if (!validationRegExp.isEmpty()) {
    const QRegularExpression re(validationRegExp);
    if (re.isValid())
      lineEdit->setValidator(new QRegularExpressionValidator(re, lineEdit));
    else
      qWarning("Invalid custom wizard field validator regular expression %s.", qPrintable(validationRegExp));
  }
  registerField(fieldName, lineEdit, "text", SIGNAL(textEdited(QString)));
  // Connect to completeChanged() for derived classes that reimplement isComplete()
  connect(lineEdit, &QLineEdit::textEdited, this, &QWizardPage::completeChanged);

  const auto defaultText = field.controlAttributes.value(QLatin1String("defaulttext"));
  const auto placeholderText = field.controlAttributes.value(QLatin1String("placeholdertext"));
  m_lineEdits.push_back(LineEditData(lineEdit, defaultText, placeholderText));
  return lineEdit;
}

auto CustomWizardFieldPage::initializePage() -> void
{
  QWizardPage::initializePage();
  clearError();
  for (const auto &led : qAsConst(m_lineEdits)) {
    if (!led.userChange.isNull()) {
      led.lineEdit->setText(led.userChange);
    } else if (!led.defaultText.isEmpty()) {
      auto defaultText = led.defaultText;
      CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
      led.lineEdit->setText(defaultText);
    }
    if (!led.placeholderText.isEmpty())
      led.lineEdit->setPlaceholderText(led.placeholderText);
  }
  for (const auto &ted : qAsConst(m_textEdits)) {
    if (!ted.userChange.isNull()) {
      ted.textEdit->setText(ted.userChange);
    } else if (!ted.defaultText.isEmpty()) {
      auto defaultText = ted.defaultText;
      CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
      ted.textEdit->setText(defaultText);
    }
  }
  for (const auto &ped : qAsConst(m_pathChoosers)) {
    if (!ped.userChange.isNull()) {
      ped.pathChooser->setFilePath(FilePath::fromUserInput(ped.userChange));
    } else if (!ped.defaultText.isEmpty()) {
      auto defaultText = ped.defaultText;
      CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
      ped.pathChooser->setFilePath(FilePath::fromUserInput(defaultText));
    }
  }
}

auto CustomWizardFieldPage::cleanupPage() -> void
{
  for (auto i = 0; i < m_lineEdits.count(); ++i) {
    auto &led = m_lineEdits[i];
    auto defaultText = led.defaultText;
    CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
    if (led.lineEdit->text() != defaultText)
      led.userChange = led.lineEdit->text();
    else
      led.userChange.clear();

  }
  for (auto i = 0; i < m_textEdits.count(); ++i) {
    auto &ted = m_textEdits[i];
    auto defaultText = ted.defaultText;
    CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
    if (ted.textEdit->toHtml() != ted.defaultText && ted.textEdit->toPlainText() != ted.defaultText)
      ted.userChange = ted.textEdit->toHtml();
    else
      ted.userChange.clear();
  }
  for (auto i = 0; i < m_pathChoosers.count(); ++i) {
    auto &ped = m_pathChoosers[i];
    auto defaultText = ped.defaultText;
    CustomWizardContext::replaceFields(m_context->baseReplacements, &defaultText);
    if (ped.pathChooser->filePath().toString() != ped.defaultText)
      ped.userChange = ped.pathChooser->filePath().toString();
    else
      ped.userChange.clear();
  }
  QWizardPage::cleanupPage();
}

auto CustomWizardFieldPage::validatePage() -> bool
{
  clearError();
  // Check line edits with validators
  foreach(const LineEditData &led, m_lineEdits) {
    if (const auto val = led.lineEdit->validator()) {
      auto pos = 0;
      auto text = led.lineEdit->text();
      if (val->validate(text, pos) != QValidator::Acceptable) {
        led.lineEdit->setFocus();
        return false;
      }
    }
  }
  // Any user validation rules -> Check all and display messages with
  // place holders applied.
  if (!m_parameters->rules.isEmpty()) {
    const auto values = replacementMap(wizard(), m_context, m_parameters->fields);
    QString message;
    if (!CustomWizardValidationRule::validateRules(m_parameters->rules, values, &message)) {
      showError(message);
      return false;
    }
  }
  return QWizardPage::validatePage();
}

auto CustomWizardFieldPage::replacementMap(const QWizard *w, const QSharedPointer<CustomWizardContext> &ctx, const FieldList &f) -> QMap<QString, QString>
{
  auto fieldReplacementMap = ctx->baseReplacements;
  foreach(const Internal::CustomWizardField &field, f) {
    const auto value = w->field(field.name).toString();
    fieldReplacementMap.insert(field.name, value);
  }
  // Insert paths for generator scripts.
  fieldReplacementMap.insert(QLatin1String("Path"), ctx->path.toUserOutput());
  fieldReplacementMap.insert(QLatin1String("TargetPath"), ctx->targetPath.toUserOutput());

  return fieldReplacementMap;
}

/*!
    \class ProjectExplorer::Internal::CustomWizardPage
    \brief The CustomWizardPage class provides a custom wizard page presenting
    the fields to be used and a path chooser
    at the bottom (for use by "class"/"file" wizards).

    Does validation on the Path chooser only (as the other fields can by validated by regexps).

    \sa ProjectExplorer::CustomWizard
*/

CustomWizardPage::CustomWizardPage(const QSharedPointer<CustomWizardContext> &ctx, const QSharedPointer<CustomWizardParameters> &parameters, QWidget *parent) : CustomWizardFieldPage(ctx, parameters, parent), m_pathChooser(new PathChooser)
{
  m_pathChooser->setHistoryCompleter(QLatin1String("PE.ProjectDir.History"));
  addRow(tr("Path:"), m_pathChooser);
  connect(m_pathChooser, &PathChooser::validChanged, this, &QWizardPage::completeChanged);
}

auto CustomWizardPage::filePath() const -> FilePath
{
  return m_pathChooser->filePath();
}

auto CustomWizardPage::setFilePath(const FilePath &path) -> void
{
  m_pathChooser->setFilePath(path);
}

auto CustomWizardPage::isComplete() const -> bool
{
  return m_pathChooser->isValid() && CustomWizardFieldPage::isComplete();
}

} // namespace Internal
} // namespace ProjectExplorer
