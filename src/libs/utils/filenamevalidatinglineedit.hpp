// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fancylineedit.hpp"

namespace Utils {

class ORCA_UTILS_EXPORT FileNameValidatingLineEdit : public FancyLineEdit {
  Q_OBJECT
  Q_PROPERTY(bool allowDirectories READ allowDirectories WRITE setAllowDirectories)
  Q_PROPERTY(QStringList requiredExtensions READ requiredExtensions WRITE setRequiredExtensions)
  Q_PROPERTY(bool forceFirstCapitalLetter READ forceFirstCapitalLetter WRITE setForceFirstCapitalLetter)

public:
  explicit FileNameValidatingLineEdit(QWidget *parent = nullptr);
  static auto validateFileName(const QString &name, bool allowDirectories = false, QString *errorMessage = nullptr) -> bool;
  static auto validateFileNameExtension(const QString &name, const QStringList &requiredExtensions = QStringList(), QString *errorMessage = nullptr) -> bool;

  /**
   * Sets whether entering directories is allowed. This will enable the user
   * to enter slashes in the filename. Default is off.
   */
  auto allowDirectories() const -> bool;
  auto setAllowDirectories(bool v) -> void;

  /**
   * Sets whether the first letter is forced to be a capital letter
   * Default is off.
   */
  auto forceFirstCapitalLetter() const -> bool;
  auto setForceFirstCapitalLetter(bool b) -> void;

  /**
   * Sets a requred extension. If the extension is empty no extension is required.
   * Default is empty.
   */
  auto requiredExtensions() const -> QStringList;
  auto setRequiredExtensions(const QStringList &extensionList) -> void;

protected:
  auto fixInputString(const QString &string) -> QString override;

private:
  bool m_allowDirectories;
  QStringList m_requiredExtensionList;
  bool m_forceFirstCapitalLetter;
};

} // namespace Utils