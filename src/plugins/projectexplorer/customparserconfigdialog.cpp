// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customparserconfigdialog.hpp"
#include "ui_customparserconfigdialog.h"

#include <utils/theme/theme.hpp>

#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>

namespace ProjectExplorer {
namespace Internal {

CustomParserConfigDialog::CustomParserConfigDialog(QWidget *parent) : QDialog(parent), ui(new Ui::CustomParserConfigDialog)
{
  ui->setupUi(this);

  connect(ui->errorPattern, &QLineEdit::textChanged, this, &CustomParserConfigDialog::changed);
  connect(ui->errorOutputMessage, &QLineEdit::textChanged, this, &CustomParserConfigDialog::changed);
  connect(ui->errorFileNameCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);
  connect(ui->errorLineNumberCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);
  connect(ui->errorMessageCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);
  connect(ui->warningPattern, &QLineEdit::textChanged, this, &CustomParserConfigDialog::changed);
  connect(ui->warningOutputMessage, &QLineEdit::textChanged, this, &CustomParserConfigDialog::changed);
  connect(ui->warningFileNameCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);
  connect(ui->warningLineNumberCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);
  connect(ui->warningMessageCap, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomParserConfigDialog::changed);

  changed();
  m_dirty = false;
}

CustomParserConfigDialog::~CustomParserConfigDialog()
{
  delete ui;
}

auto CustomParserConfigDialog::setExampleSettings() -> void
{
  setErrorPattern(QLatin1String("#error (.*):(\\d+): (.*)"));
  setErrorFileNameCap(1);
  setErrorLineNumberCap(2);
  setErrorMessageCap(3);
  setErrorChannel(CustomParserExpression::ParseBothChannels);
  setWarningPattern(QLatin1String("#warning (.*):(\\d+): (.*)"));
  setWarningFileNameCap(1);
  setWarningLineNumberCap(2);
  setWarningMessageCap(3);
  setWarningChannel(CustomParserExpression::ParseBothChannels);
  ui->errorOutputMessage->setText(QLatin1String("#error /home/user/src/test.c:891: Unknown identifier `test`"));
  ui->warningOutputMessage->setText(QLatin1String("#warning /home/user/src/test.c:49: Unreferenced variable `test`"));
}

auto CustomParserConfigDialog::setSettings(const CustomParserSettings &settings) -> void
{
  if (settings.error.pattern().isEmpty() && settings.warning.pattern().isEmpty()) {
    setExampleSettings();
    return;
  }

  setErrorPattern(settings.error.pattern());
  setErrorFileNameCap(settings.error.fileNameCap());
  setErrorLineNumberCap(settings.error.lineNumberCap());
  setErrorMessageCap(settings.error.messageCap());
  setErrorChannel(settings.error.channel());
  setErrorExample(settings.error.example());
  setWarningPattern(settings.warning.pattern());
  setWarningFileNameCap(settings.warning.fileNameCap());
  setWarningLineNumberCap(settings.warning.lineNumberCap());
  setWarningMessageCap(settings.warning.messageCap());
  setWarningChannel(settings.warning.channel());
  setWarningExample(settings.warning.example());
}

auto CustomParserConfigDialog::settings() const -> CustomParserSettings
{
  CustomParserSettings result;
  result.error.setPattern(errorPattern());
  result.error.setFileNameCap(errorFileNameCap());
  result.error.setLineNumberCap(errorLineNumberCap());
  result.error.setMessageCap(errorMessageCap());
  result.error.setChannel(errorChannel());
  result.error.setExample(errorExample());
  result.warning.setPattern(warningPattern());
  result.warning.setFileNameCap(warningFileNameCap());
  result.warning.setLineNumberCap(warningLineNumberCap());
  result.warning.setMessageCap(warningMessageCap());
  result.warning.setChannel(warningChannel());
  result.warning.setExample(warningExample());
  return result;
}

auto CustomParserConfigDialog::setErrorPattern(const QString &errorPattern) -> void
{
  ui->errorPattern->setText(errorPattern);
}

auto CustomParserConfigDialog::errorPattern() const -> QString
{
  return ui->errorPattern->text();
}

auto CustomParserConfigDialog::setErrorFileNameCap(int fileNameCap) -> void
{
  ui->errorFileNameCap->setValue(fileNameCap);
}

auto CustomParserConfigDialog::errorFileNameCap() const -> int
{
  return ui->errorFileNameCap->value();
}

auto CustomParserConfigDialog::setErrorLineNumberCap(int lineNumberCap) -> void
{
  ui->errorLineNumberCap->setValue(lineNumberCap);
}

auto CustomParserConfigDialog::errorLineNumberCap() const -> int
{
  return ui->errorLineNumberCap->value();
}

auto CustomParserConfigDialog::setErrorMessageCap(int messageCap) -> void
{
  ui->errorMessageCap->setValue(messageCap);
}

auto CustomParserConfigDialog::errorMessageCap() const -> int
{
  return ui->errorMessageCap->value();
}

auto CustomParserConfigDialog::setErrorChannel(CustomParserExpression::CustomParserChannel errorChannel) -> void
{
  ui->errorStdErrChannel->setChecked(errorChannel & static_cast<int>(CustomParserExpression::ParseStdErrChannel));
  ui->errorStdOutChannel->setChecked(errorChannel & static_cast<int>(CustomParserExpression::ParseStdOutChannel));
}

auto CustomParserConfigDialog::errorChannel() const -> CustomParserExpression::CustomParserChannel
{
  if (ui->errorStdErrChannel->isChecked() && !ui->errorStdOutChannel->isChecked())
    return CustomParserExpression::ParseStdErrChannel;
  if (ui->errorStdOutChannel->isChecked() && !ui->errorStdErrChannel->isChecked())
    return CustomParserExpression::ParseStdOutChannel;
  return CustomParserExpression::ParseBothChannels;
}

auto CustomParserConfigDialog::setErrorExample(const QString &errorExample) -> void
{
  ui->errorOutputMessage->setText(errorExample);
}

auto CustomParserConfigDialog::errorExample() const -> QString
{
  return ui->errorOutputMessage->text();
}

auto CustomParserConfigDialog::setWarningPattern(const QString &warningPattern) -> void
{
  ui->warningPattern->setText(warningPattern);
}

auto CustomParserConfigDialog::warningPattern() const -> QString
{
  return ui->warningPattern->text();
}

auto CustomParserConfigDialog::setWarningFileNameCap(int warningFileNameCap) -> void
{
  ui->warningFileNameCap->setValue(warningFileNameCap);
}

auto CustomParserConfigDialog::warningFileNameCap() const -> int
{
  return ui->warningFileNameCap->value();
}

auto CustomParserConfigDialog::setWarningLineNumberCap(int warningLineNumberCap) -> void
{
  ui->warningLineNumberCap->setValue(warningLineNumberCap);
}

auto CustomParserConfigDialog::warningLineNumberCap() const -> int
{
  return ui->warningLineNumberCap->value();
}

auto CustomParserConfigDialog::setWarningMessageCap(int warningMessageCap) -> void
{
  ui->warningMessageCap->setValue(warningMessageCap);
}

auto CustomParserConfigDialog::warningMessageCap() const -> int
{
  return ui->warningMessageCap->value();
}

auto CustomParserConfigDialog::setWarningChannel(CustomParserExpression::CustomParserChannel warningChannel) -> void
{
  ui->warningStdErrChannel->setChecked(warningChannel & static_cast<int>(CustomParserExpression::ParseStdErrChannel));
  ui->warningStdOutChannel->setChecked(warningChannel & static_cast<int>(CustomParserExpression::ParseStdOutChannel));
}

auto CustomParserConfigDialog::warningChannel() const -> CustomParserExpression::CustomParserChannel
{
  if (ui->warningStdErrChannel->isChecked() && !ui->warningStdOutChannel->isChecked())
    return CustomParserExpression::ParseStdErrChannel;
  if (ui->warningStdOutChannel->isChecked() && !ui->warningStdErrChannel->isChecked())
    return CustomParserExpression::ParseStdOutChannel;
  return CustomParserExpression::ParseBothChannels;
}

auto CustomParserConfigDialog::setWarningExample(const QString &warningExample) -> void
{
  ui->warningOutputMessage->setText(warningExample);
}

auto CustomParserConfigDialog::warningExample() const -> QString
{
  return ui->warningOutputMessage->text();
}

auto CustomParserConfigDialog::isDirty() const -> bool
{
  return m_dirty;
}

auto CustomParserConfigDialog::checkPattern(QLineEdit *pattern, const QString &outputText, QString *errorMessage, QRegularExpressionMatch *match) -> bool
{
  QRegularExpression rx;
  rx.setPattern(pattern->text());

  QPalette palette;
  palette.setColor(QPalette::Text, Utils::orcaTheme()->color(rx.isValid() ? Utils::Theme::TextColorNormal : Utils::Theme::TextColorError));
  pattern->setPalette(palette);
  pattern->setToolTip(rx.isValid() ? QString() : rx.errorString());

  if (rx.isValid())
    *match = rx.match(outputText);
  if (rx.pattern().isEmpty() || !rx.isValid() || !match->hasMatch()) {
    *errorMessage = QString::fromLatin1("<font color=\"%1\">%2 ").arg(Utils::orcaTheme()->color(Utils::Theme::TextColorError).name(), tr("Not applicable:"));
    if (rx.pattern().isEmpty())
      *errorMessage += tr("Pattern is empty.");
    else if (!rx.isValid())
      *errorMessage += rx.errorString();
    else if (outputText.isEmpty())
      *errorMessage += tr("No message given.");
    else
      *errorMessage += tr("Pattern does not match the message.");

    return false;
  }

  errorMessage->clear();
  return true;
}

auto CustomParserConfigDialog::changed() -> void
{
  QRegularExpressionMatch match;
  QString errorMessage;

  if (checkPattern(ui->errorPattern, ui->errorOutputMessage->text(), &errorMessage, &match)) {
    ui->errorFileNameTest->setText(match.captured(ui->errorFileNameCap->value()));
    ui->errorLineNumberTest->setText(match.captured(ui->errorLineNumberCap->value()));
    ui->errorMessageTest->setText(match.captured(ui->errorMessageCap->value()));
  } else {
    ui->errorFileNameTest->setText(errorMessage);
    ui->errorLineNumberTest->setText(errorMessage);
    ui->errorMessageTest->setText(errorMessage);
  }

  if (checkPattern(ui->warningPattern, ui->warningOutputMessage->text(), &errorMessage, &match)) {
    ui->warningFileNameTest->setText(match.captured(ui->warningFileNameCap->value()));
    ui->warningLineNumberTest->setText(match.captured(ui->warningLineNumberCap->value()));
    ui->warningMessageTest->setText(match.captured(ui->warningMessageCap->value()));
  } else {
    ui->warningFileNameTest->setText(errorMessage);
    ui->warningLineNumberTest->setText(errorMessage);
    ui->warningMessageTest->setText(errorMessage);
  }
  m_dirty = true;
}

} // namespace Internal
} // namespace ProjectExplorer
