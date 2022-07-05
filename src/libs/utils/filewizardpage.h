// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "filepath.h"
#include "wizardpage.h"

namespace Utils {

class FileWizardPagePrivate;

class ORCA_UTILS_EXPORT FileWizardPage : public WizardPage {
  Q_OBJECT Q_PROPERTY(QString path READ path WRITE setPath DESIGNABLE true)
  Q_PROPERTY(QString fileName READ fileName WRITE setFileName DESIGNABLE true)

public:
  explicit FileWizardPage(QWidget *parent = nullptr);
  ~FileWizardPage() override;

  auto fileName() const -> QString;
  auto path() const -> QString; // Deprecated: Use filePath()
  auto filePath() const -> Utils::FilePath;
  auto isComplete() const -> bool override;
  auto setFileNameLabel(const QString &label) -> void;
  auto setPathLabel(const QString &label) -> void;
  auto setDefaultSuffix(const QString &suffix) -> void;
  auto forceFirstCapitalLetterForFileName() const -> bool;
  auto setForceFirstCapitalLetterForFileName(bool b) -> void;
  auto setAllowDirectoriesInFileSelector(bool allow) -> void;

  // Validate a base name entry field (potentially containing extension)
  static auto validateBaseName(const QString &name, QString *errorMessage = nullptr) -> bool;

signals:
  auto activated() -> void;
  auto pathChanged() -> void;

public slots:
  auto setPath(const QString &path) -> void; // Deprecated: Use setFilePath
  auto setFileName(const QString &name) -> void;
  auto setFilePath(const Utils::FilePath &filePath) -> void;

private:
  auto slotValidChanged() -> void;
  auto slotActivated() -> void;

  FileWizardPagePrivate *d;
};

} // namespace Utils
