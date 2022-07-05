// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "filepath.h"
#include "infolabel.h"
#include "wizardpage.h"

namespace Utils {

class ProjectIntroPagePrivate;

class ORCA_UTILS_EXPORT ProjectIntroPage : public WizardPage {
  Q_OBJECT
  Q_PROPERTY(QString description READ description WRITE setDescription DESIGNABLE true)
  Q_PROPERTY(FilePath filePath READ filePath WRITE setFilePath DESIGNABLE true)
  Q_PROPERTY(QString projectName READ projectName WRITE setProjectName DESIGNABLE true)
  Q_PROPERTY(bool useAsDefaultPath READ useAsDefaultPath WRITE setUseAsDefaultPath DESIGNABLE true)
  Q_PROPERTY(bool forceSubProject READ forceSubProject WRITE setForceSubProject DESIGNABLE true)

public:
  explicit ProjectIntroPage(QWidget *parent = nullptr);
  ~ProjectIntroPage() override;

  auto projectName() const -> QString;
  auto filePath() const -> FilePath;
  auto description() const -> QString;
  auto useAsDefaultPath() const -> bool;

  // Insert an additional control into the form layout for the target.
  auto insertControl(int row, QWidget *label, QWidget *control) -> void;
  auto isComplete() const -> bool override;
  auto forceSubProject() const -> bool;
  auto setForceSubProject(bool force) -> void;
  auto setProjectList(const QStringList &projectList) -> void;
  auto setProjectDirectories(const Utils::FilePaths &directoryList) -> void;
  auto projectIndex() const -> int;
  auto validateProjectName(const QString &name, QString *errorMessage) -> bool;

  // Calls slotChanged() - i.e. tell the page that some of its fields have been updated.
  // This function is useful if you programmatically update the fields of the page (i.e. from
  // your client code).
  auto fieldsUpdated() -> void;

signals:
  auto activated() -> void;
  auto statusMessageChanged(InfoLabel::InfoType type, const QString &message) -> void;

public slots:
  auto setFilePath(const FilePath &path) -> void;
  auto setProjectName(const QString &name) -> void;
  auto setDescription(const QString &description) -> void;
  auto setUseAsDefaultPath(bool u) -> void;
  auto setProjectNameRegularExpression(const QRegularExpression &regEx, const QString &userErrorMessage) -> void;

private:
  auto slotChanged() -> void;
  auto slotActivated() -> void;
  auto validate() -> bool;
  auto displayStatusMessage(InfoLabel::InfoType t, const QString &) -> void;
  auto hideStatusLabel() -> void;

  ProjectIntroPagePrivate *d;
};

} // namespace Utils
