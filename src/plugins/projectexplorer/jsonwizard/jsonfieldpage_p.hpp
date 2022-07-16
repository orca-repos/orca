// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "jsonfieldpage.hpp"

#include <utils/pathchooser.hpp>

#include <QWidget>
#include <QString>
#include <QVariant>
#include <QStandardItem>
#include <QDir>
#include <QTextStream>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QItemSelectionModel;
QT_END_NAMESPACE

namespace ProjectExplorer {

// --------------------------------------------------------------------
// JsonFieldPage::Field::FieldPrivate:
// --------------------------------------------------------------------

class JsonFieldPage::Field::FieldPrivate {
public:
  QString m_name;
  QString m_displayName;
  QString m_toolTip;
  bool m_isMandatory = false;
  bool m_hasSpan = false;
  bool m_hasUserChanges = false;
  QVariant m_visibleExpression;
  QVariant m_enabledExpression;
  QVariant m_isCompleteExpando;
  QString m_isCompleteExpandoMessage;
  QString m_persistenceKey;
  QLabel *m_label = nullptr;
  QWidget *m_widget = nullptr;
  QString m_type;
};

// --------------------------------------------------------------------
// Field Implementations:
// --------------------------------------------------------------------

class LabelField : public JsonFieldPage::Field {
  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "LabelField{text:" << m_text << "}";
    return result;
  }

  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;

  bool m_wordWrap = false;
  QString m_text;
};

class SpacerField : public JsonFieldPage::Field {
public:
  auto suppressName() const -> bool override { return true; }

private:
  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "SpacerField{factor:" << m_factor << "}";
    return result;
  }

  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;

  int m_factor = 1;
};

class PROJECTEXPLORER_EXPORT LineEditField : public JsonFieldPage::Field {
private:
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;

  auto setup(JsonFieldPage *page, const QString &name) -> void override;

  auto validate(Utils::MacroExpander *expander, QString *message) -> bool override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;

  auto fromSettings(const QVariant &value) -> void override;
  auto toSettings() const -> QVariant override;

  auto setupCompletion(Utils::FancyLineEdit *lineEdit) -> void;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "LineEditField{currentText:" << m_currentText << "; default:" << m_defaultText << "; placeholder:" << m_placeholderText << "; history id:" << m_historyId << "; validator: " << m_validatorRegExp.pattern() << "; fixupExpando: " << m_fixupExpando << "; completion: " << QString::number((int)m_completion) << "}";
    return result;
  }

  bool m_isModified = false;
  bool m_isValidating = false;
  bool m_restoreLastHistoryItem = false;
  bool m_isPassword = false;
  QString m_placeholderText;
  QString m_defaultText;
  QString m_disabledText;
  QString m_historyId;
  QRegularExpression m_validatorRegExp;
  QString m_fixupExpando;
  mutable QString m_currentText;

  enum class Completion {
    Classes,
    Namespaces,
    None
  };

  Completion m_completion = Completion::None;

public:
  auto setText(const QString &text) -> void;
};

class TextEditField : public JsonFieldPage::Field {
private:
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;

  auto setup(JsonFieldPage *page, const QString &name) -> void override;

  auto validate(Utils::MacroExpander *expander, QString *message) -> bool override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;

  auto fromSettings(const QVariant &value) -> void override;
  auto toSettings() const -> QVariant override;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "TextEditField{default:" << m_defaultText << "; rich:" << m_acceptRichText << "; disabled: " << m_disabledText << "}";
    return result;
  }

  QString m_defaultText;
  bool m_acceptRichText = false;
  QString m_disabledText;

  mutable QString m_currentText;
};

class PathChooserField : public JsonFieldPage::Field {
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;
  auto setEnabled(bool e) -> void override;
  auto setup(JsonFieldPage *page, const QString &name) -> void override;
  auto validate(Utils::MacroExpander *expander, QString *message) -> bool override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;
  auto fromSettings(const QVariant &value) -> void override;
  auto toSettings() const -> QVariant override;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "PathChooser{path:" << m_path.toString() << "; base:" << m_basePath << "; historyId:" << m_historyId << "; kind:" << (int)Utils::PathChooser::ExistingDirectory << "}";
    return result;
  }

  Utils::FilePath m_path;
  Utils::FilePath m_basePath;
  QString m_historyId;
  Utils::PathChooser::Kind m_kind = Utils::PathChooser::ExistingDirectory;
};

class PROJECTEXPLORER_EXPORT CheckBoxField : public JsonFieldPage::Field {
public:
  auto suppressName() const -> bool override { return true; }
  auto setChecked(bool) -> void;

private:
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;
  auto setup(JsonFieldPage *page, const QString &name) -> void override;
  auto validate(Utils::MacroExpander *expander, QString *message) -> bool override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;
  auto fromSettings(const QVariant &value) -> void override;
  auto toSettings() const -> QVariant override;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "CheckBoxField{checked:" << m_checkedValue << "; unchecked: " + m_uncheckedValue << "; checkedExpression: QVariant(" << m_checkedExpression.typeName() << ":" << m_checkedExpression.toString() << ")" << "; isModified:" << m_isModified << "}";
    return result;
  }

  QString m_checkedValue;
  QString m_uncheckedValue;
  QVariant m_checkedExpression;

  bool m_isModified = false;
};

class ListField : public JsonFieldPage::Field {
public:
  enum SpecialRoles {
    ValueRole = Qt::UserRole,
    ConditionRole = Qt::UserRole + 1,
    IconStringRole = Qt::UserRole + 2
  };

  ListField();
  ~ListField() override;

  auto model() -> QStandardItemModel* { return m_itemModel; }
  virtual auto selectRow(int row) -> bool;

protected:
  auto parseData(const QVariant &data, QString *errorMessage) -> bool override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override = 0;
  auto setup(JsonFieldPage *page, const QString &name) -> void override = 0;
  auto validate(Utils::MacroExpander *expander, QString *message) -> bool override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;
  auto itemModel() -> QStandardItemModel*;
  auto selectionModel() const -> QItemSelectionModel*;
  auto setSelectionModel(QItemSelectionModel *selectionModel) -> void;
  auto maxIconSize() const -> QSize;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "ListField{index:" << m_index << "; disabledIndex:" << m_disabledIndex << "; savedIndex: " << m_savedIndex << "; items Count: " << m_itemList.size() << "; items:";

    if (m_itemList.empty())
      out << "(empty)";
    else
      out << m_itemList.front()->text() << ", ...";

    out << "}";

    return result;
  }

private:
  auto addPossibleIconSize(const QIcon &icon) -> void;
  auto updateIndex() -> void;
  auto fromSettings(const QVariant &value) -> void override;
  auto toSettings() const -> QVariant override;

  std::vector<std::unique_ptr<QStandardItem>> m_itemList;
  QStandardItemModel *m_itemModel = nullptr;
  QItemSelectionModel *m_selectionModel = nullptr;
  int m_index = -1;
  int m_disabledIndex = -1;
  QSize m_maxIconSize;
  mutable int m_savedIndex = -1;
};

class PROJECTEXPLORER_EXPORT ComboBoxField : public ListField {
private:
  auto setup(JsonFieldPage *page, const QString &name) -> void override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;
  auto toSettings() const -> QVariant override;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "ComboBox{" << ListField::toString() << "}";
    return result;
  }

public:
  auto selectRow(int row) -> bool override;
  auto selectedRow() const -> int;
};

class IconListField : public ListField {
public:
  auto setup(JsonFieldPage *page, const QString &name) -> void override;
  auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* override;
  auto initializeData(Utils::MacroExpander *expander) -> void override;

  auto toString() const -> QString override
  {
    QString result;
    QTextStream out(&result);
    out << "IconList{" << ListField::toString() << "}";
    return result;
  }
};

} // namespace ProjectExplorer
