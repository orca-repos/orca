// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "customparser.hpp"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

namespace ProjectExplorer {
namespace Internal {

namespace Ui {
class CustomParserConfigDialog;
}

class CustomParserConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit CustomParserConfigDialog(QWidget *parent = nullptr);
  ~CustomParserConfigDialog() override;

  auto setExampleSettings() -> void;
  auto setSettings(const CustomParserSettings &settings) -> void;
  auto settings() const -> CustomParserSettings;
  auto setErrorPattern(const QString &errorPattern) -> void;
  auto errorPattern() const -> QString;
  auto setErrorFileNameCap(int errorFileNameCap) -> void;
  auto errorFileNameCap() const -> int;
  auto setErrorLineNumberCap(int errorLineNumberCap) -> void;
  auto errorLineNumberCap() const -> int;
  auto setErrorMessageCap(int errorMessageCap) -> void;
  auto errorMessageCap() const -> int;
  auto setErrorChannel(CustomParserExpression::CustomParserChannel errorChannel) -> void;
  auto errorChannel() const -> CustomParserExpression::CustomParserChannel;
  auto setErrorExample(const QString &errorExample) -> void;
  auto errorExample() const -> QString;
  auto setWarningPattern(const QString &warningPattern) -> void;
  auto warningPattern() const -> QString;
  auto setWarningFileNameCap(int warningFileNameCap) -> void;
  auto warningFileNameCap() const -> int;
  auto setWarningLineNumberCap(int warningLineNumberCap) -> void;
  auto warningLineNumberCap() const -> int;
  auto setWarningMessageCap(int warningMessageCap) -> void;
  auto warningMessageCap() const -> int;
  auto setWarningChannel(CustomParserExpression::CustomParserChannel warningChannel) -> void;
  auto warningChannel() const -> CustomParserExpression::CustomParserChannel;
  auto setWarningExample(const QString &warningExample) -> void;
  auto warningExample() const -> QString;
  auto isDirty() const -> bool;

private:
  auto changed() -> void;
  auto checkPattern(QLineEdit *pattern, const QString &outputText, QString *errorMessage, QRegularExpressionMatch *match) -> bool;

  Ui::CustomParserConfigDialog *ui;
  bool m_dirty;
};

} // namespace Internal
} // namespace ProjectExplorer
