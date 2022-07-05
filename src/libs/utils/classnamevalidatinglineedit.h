// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fancylineedit.h"

namespace Utils {

struct ClassNameValidatingLineEditPrivate;

class ORCA_UTILS_EXPORT ClassNameValidatingLineEdit : public FancyLineEdit {
  Q_OBJECT
  Q_PROPERTY(bool namespacesEnabled READ namespacesEnabled WRITE setNamespacesEnabled DESIGNABLE true)
  Q_PROPERTY(bool lowerCaseFileName READ lowerCaseFileName WRITE setLowerCaseFileName)

public:
  explicit ClassNameValidatingLineEdit(QWidget *parent = nullptr);
  ~ClassNameValidatingLineEdit() override;

  auto namespacesEnabled() const -> bool;
  auto setNamespacesEnabled(bool b) -> void;
  auto namespaceDelimiter() -> QString;
  auto setNamespaceDelimiter(const QString &delimiter) -> void;
  auto lowerCaseFileName() const -> bool;
  auto setLowerCaseFileName(bool v) -> void;
  auto forceFirstCapitalLetter() const -> bool;
  auto setForceFirstCapitalLetter(bool b) -> void;

  // Clean an input string to get a valid class name.
  static auto createClassName(const QString &name) -> QString;

signals:
  // Will be emitted with a suggestion for a base name of the
  // source/header file of the class.
  auto updateFileName(const QString &t) -> void;

protected:
  auto validateClassName(FancyLineEdit *edit, QString *errorMessage) const -> bool;
  auto handleChanged(const QString &t) -> void override;
  auto fixInputString(const QString &string) -> QString override;

private:
  auto updateRegExp() const -> void;

  ClassNameValidatingLineEditPrivate *d;
};

} // namespace Utils
