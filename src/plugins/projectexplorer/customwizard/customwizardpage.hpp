// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.hpp>

#include <QComboBox>
#include <QCheckBox>
#include <QWizardPage>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE
class QFormLayout;
class QLineEdit;
class QTextEdit;
class QLabel;
QT_END_NAMESPACE

namespace Utils {
class PathChooser;
}

namespace ProjectExplorer {
namespace Internal {

class CustomWizardField;
class CustomWizardParameters;
class CustomWizardContext;

class CustomWizardFieldPage : public QWizardPage {
  Q_OBJECT

public:
  using FieldList = QList<CustomWizardField>;

  explicit CustomWizardFieldPage(const QSharedPointer<CustomWizardContext> &ctx, const QSharedPointer<CustomWizardParameters> &parameters, QWidget *parent = nullptr);

  auto validatePage() -> bool override;
  auto initializePage() -> void override;
  auto cleanupPage() -> void override;
  static auto replacementMap(const QWizard *w, const QSharedPointer<CustomWizardContext> &ctx, const FieldList &f) -> QMap<QString, QString>;

protected:
  inline auto addRow(const QString &name, QWidget *w) -> void;
  auto showError(const QString &) -> void;
  auto clearError() -> void;

private:
  class LineEditData {
  public:
    explicit LineEditData(QLineEdit *le = nullptr, const QString &defText = QString(), const QString &pText = QString());

    QLineEdit *lineEdit;
    QString defaultText;
    QString placeholderText;
    QString userChange;
  };

  class TextEditData {
  public:
    explicit TextEditData(QTextEdit *le = nullptr, const QString &defText = QString());

    QTextEdit *textEdit;
    QString defaultText;
    QString userChange;
  };

  class PathChooserData {
  public:
    explicit PathChooserData(Utils::PathChooser *pe = nullptr, const QString &defText = QString());

    Utils::PathChooser *pathChooser;
    QString defaultText;
    QString userChange;
  };

  using LineEditDataList = QList<LineEditData>;
  using TextEditDataList = QList<TextEditData>;
  using PathChooserDataList = QList<PathChooserData>;

  auto registerLineEdit(const QString &fieldName, const CustomWizardField &field) -> QWidget*;
  auto registerComboBox(const QString &fieldName, const CustomWizardField &field) -> QWidget*;
  auto registerTextEdit(const QString &fieldName, const CustomWizardField &field) -> QWidget*;
  auto registerPathChooser(const QString &fieldName, const CustomWizardField &field) -> QWidget*;
  auto registerCheckBox(const QString &fieldName, const QString &fieldDescription, const CustomWizardField &field) -> QWidget*;
  auto addField(const CustomWizardField &f) -> void;

  const QSharedPointer<CustomWizardParameters> m_parameters;
  const QSharedPointer<CustomWizardContext> m_context;
  QFormLayout *m_formLayout;
  LineEditDataList m_lineEdits;
  TextEditDataList m_textEdits;
  PathChooserDataList m_pathChoosers;
  QLabel *m_errorLabel;
};

// Documentation inside.
class CustomWizardPage : public CustomWizardFieldPage {
  Q_OBJECT

public:
  explicit CustomWizardPage(const QSharedPointer<CustomWizardContext> &ctx, const QSharedPointer<CustomWizardParameters> &parameters, QWidget *parent = nullptr);

  auto filePath() const -> Utils::FilePath;
  auto setFilePath(const Utils::FilePath &path) -> void;
  auto isComplete() const -> bool override;

private:
  Utils::PathChooser *m_pathChooser;
};

} // namespace Internal
} // namespace ProjectExplorer
