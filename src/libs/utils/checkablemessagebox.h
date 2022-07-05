// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QDialogButtonBox>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {

class CheckableMessageBoxPrivate;

class ORCA_UTILS_EXPORT CheckableMessageBox : public QDialog {
  Q_OBJECT
  Q_PROPERTY(QString text READ text WRITE setText)
  Q_PROPERTY(QMessageBox::Icon icon READ icon WRITE setIcon)
  Q_PROPERTY(bool isChecked READ isChecked WRITE setChecked)
  Q_PROPERTY(QString checkBoxText READ checkBoxText WRITE setCheckBoxText)
  Q_PROPERTY(QDialogButtonBox::StandardButtons buttons READ standardButtons WRITE setStandardButtons)
  Q_PROPERTY(QDialogButtonBox::StandardButton defaultButton READ defaultButton WRITE setDefaultButton)

public:
  explicit CheckableMessageBox(QWidget *parent);
  ~CheckableMessageBox() override;

  static auto question(QWidget *parent, const QString &title, const QString &question, const QString &checkBoxText, bool *checkBoxSetting, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No) -> QDialogButtonBox::StandardButton;
  static auto information(QWidget *parent, const QString &title, const QString &text, const QString &checkBoxText, bool *checkBoxSetting, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton) -> QDialogButtonBox::StandardButton;
  static auto doNotAskAgainQuestion(QWidget *parent, const QString &title, const QString &text, QSettings *settings, const QString &settingsSubKey, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No, QDialogButtonBox::StandardButton acceptButton = QDialogButtonBox::Yes) -> QDialogButtonBox::StandardButton;
  static auto doNotShowAgainInformation(QWidget *parent, const QString &title, const QString &text, QSettings *settings, const QString &settingsSubKey, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok, QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton) -> QDialogButtonBox::StandardButton;
  auto text() const -> QString;
  auto setText(const QString &) -> void;
  auto isChecked() const -> bool;
  auto setChecked(bool s) -> void;
  auto checkBoxText() const -> QString;
  auto setCheckBoxText(const QString &) -> void;
  auto isCheckBoxVisible() const -> bool;
  auto setCheckBoxVisible(bool) -> void;
  auto detailedText() const -> QString;
  auto setDetailedText(const QString &text) -> void;
  auto standardButtons() const -> QDialogButtonBox::StandardButtons;
  auto setStandardButtons(QDialogButtonBox::StandardButtons s) -> void;
  auto button(QDialogButtonBox::StandardButton b) const -> QPushButton*;
  auto addButton(const QString &text, QDialogButtonBox::ButtonRole role) -> QPushButton*;
  auto defaultButton() const -> QDialogButtonBox::StandardButton;
  auto setDefaultButton(QDialogButtonBox::StandardButton s) -> void;
  auto icon() const -> QMessageBox::Icon;
  auto setIcon(QMessageBox::Icon icon) -> void;

  // Query the result
  auto clickedButton() const -> QAbstractButton*;
  auto clickedStandardButton() const -> QDialogButtonBox::StandardButton;

  // check and set "ask again" status
  static auto shouldAskAgain(QSettings *settings, const QString &settingsSubKey) -> bool;
  static auto doNotAskAgain(QSettings *settings, const QString &settingsSubKey) -> void;

  // Conversion convenience
  static auto dialogButtonBoxToMessageBoxButton(QDialogButtonBox::StandardButton) -> QMessageBox::StandardButton;
  static auto resetAllDoNotAskAgainQuestions(QSettings *settings) -> void;
  static auto hasSuppressedQuestions(QSettings *settings) -> bool;
  static auto msgDoNotAskAgain() -> QString;
  static auto msgDoNotShowAgain() -> QString;

private:
  CheckableMessageBoxPrivate *d;
};

} // namespace Utils
