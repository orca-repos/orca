// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <utils/pathchooser.hpp>
#include <utils/wizardpage.hpp>
#include <utils/algorithm.hpp>

#include <QRegularExpression>
#include <QVariant>

#include <memory>

QT_BEGIN_NAMESPACE
class QFormLayout;
class QLabel;
class QLineEdit;
class QTextEdit;
QT_END_NAMESPACE

namespace Utils {
class MacroExpander;
} // namespace Utils

namespace ProjectExplorer {

// Documentation inside.
class PROJECTEXPLORER_EXPORT JsonFieldPage : public Utils::WizardPage {
    Q_OBJECT

public:
  class PROJECTEXPLORER_EXPORT Field {
  public:
    class FieldPrivate;

    Field();
    virtual ~Field();

    static auto parse(const QVariant &input, QString *errorMessage) -> Field*;
    auto createWidget(JsonFieldPage *page) -> void;
    auto adjustState(Utils::MacroExpander *expander) -> void;
    virtual auto setEnabled(bool e) -> void;
    auto setVisible(bool v) -> void;
    auto setType(const QString &type) -> void;
    virtual auto validate(Utils::MacroExpander *expander, QString *message) -> bool;
    auto initialize(Utils::MacroExpander *expander) -> void;
    virtual auto cleanup(Utils::MacroExpander *expander) -> void { Q_UNUSED(expander) }
    virtual auto suppressName() const -> bool { return false; }
    auto widget(const QString &displayName, JsonFieldPage *page) -> QWidget*;
    auto name() const -> QString;
    auto displayName() const -> QString;
    auto toolTip() const -> QString;
    auto persistenceKey() const -> QString;
    auto isMandatory() const -> bool;
    auto hasSpan() const -> bool;
    auto hasUserChanges() const -> bool;

  protected:
    auto widget() const -> QWidget*;
    virtual auto parseData(const QVariant &data, QString *errorMessage) -> bool = 0;
    virtual auto initializeData(Utils::MacroExpander *expander) -> void { Q_UNUSED(expander) }
    virtual auto createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget* = 0;
    virtual auto setup(JsonFieldPage *page, const QString &name) -> void { Q_UNUSED(page); Q_UNUSED(name) }
    auto type() const -> QString;
    auto setHasUserChanges() -> void;

  private:
    virtual auto fromSettings(const QVariant &value) -> void;
    virtual auto toSettings() const -> QVariant;
    auto setTexts(const QString &name, const QString &displayName, const QString &toolTip) -> void;
    auto setIsMandatory(bool b) -> void;
    auto setHasSpan(bool b) -> void;
    auto setVisibleExpression(const QVariant &v) -> void;
    auto setEnabledExpression(const QVariant &v) -> void;
    auto setIsCompleteExpando(const QVariant &v, const QString &m) -> void;
    auto setPersistenceKey(const QString &key) -> void;
    virtual auto toString() const -> QString = 0;

    friend class JsonFieldPage;
    friend PROJECTEXPLORER_EXPORT auto operator<<(QDebug &d, const Field &f) -> QDebug&;

    const std::unique_ptr<FieldPrivate> d;
  };

  JsonFieldPage(Utils::MacroExpander *expander, QWidget *parent = nullptr);
  ~JsonFieldPage() override;

  using FieldFactory = std::function<Field *()>;

  static auto registerFieldFactory(const QString &id, const FieldFactory &ff) -> void;
  auto setup(const QVariant &data) -> bool;
  auto isComplete() const -> bool override;
  auto initializePage() -> void override;
  auto cleanupPage() -> void override;
  auto validatePage() -> bool override;
  auto layout() const -> QFormLayout* { return m_formLayout; }
  auto showError(const QString &m) const -> void;
  auto clearError() const -> void;
  auto expander() -> Utils::MacroExpander*;
  auto value(const QString &key) -> QVariant;

  auto jsonField(const QString &name) -> Field*
  {
    return Utils::findOr(m_fields, nullptr, [&name](Field *f) {
      return f->name() == name;
    });
  }

private:
    static QHash<QString, FieldFactory> m_factories;

    static auto createFieldData(const QString &type) -> Field*;
    static auto fullSettingsKey(const QString &fieldKey) -> QString;

    QFormLayout *m_formLayout;
    QLabel *m_errorLabel;
    QList<Field *> m_fields;
    Utils::MacroExpander *m_expander;
};

PROJECTEXPLORER_EXPORT auto operator<<(QDebug &debug, const JsonFieldPage::Field &field) -> QDebug&;

} // namespace ProjectExplorer
