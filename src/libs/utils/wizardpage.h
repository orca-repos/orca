// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QSet>
#include <QString>
#include <QVariant>
#include <QWizardPage>

#include <functional>

namespace Utils {

class Wizard;
namespace Internal {

class ORCA_UTILS_EXPORT ObjectToFieldWidgetConverter : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)

public:
  template <class T, typename... Arguments>
  static auto create(T *sender, void (T::*member)(Arguments ...), const std::function<QVariant()> &toVariantFunction) -> ObjectToFieldWidgetConverter*
  {
    auto widget = new ObjectToFieldWidgetConverter();
    widget->toVariantFunction = toVariantFunction;
    connect(sender, &QObject::destroyed, widget, &QObject::deleteLater);
    connect(sender, member, widget, [widget]() { emit widget->valueChanged(widget->value()); });
    return widget;
  }

signals:
  auto valueChanged(const QVariant &) -> void;

private:
  ObjectToFieldWidgetConverter() = default;

  // is used by the property value
  auto value() -> QVariant { return toVariantFunction(); }
  std::function<QVariant()> toVariantFunction;
};

} // Internal

class ORCA_UTILS_EXPORT WizardPage : public QWizardPage
{
    Q_OBJECT public:
    explicit WizardPage(QWidget *parent = nullptr);

    virtual auto pageWasAdded() -> void; // called when this page was added to a Utils::Wizard

    template <class T, typename... Arguments>
    auto registerObjectAsFieldWithName(const QString &name, T *sender, void (T::*changeSignal)(Arguments ...), const std::function<QVariant()> &senderToVariant) -> void {
        registerFieldWithName(name, Internal::ObjectToFieldWidgetConverter::create(sender, changeSignal, senderToVariant), "value", SIGNAL(valueChanged(QValue)));
    }

    auto registerFieldWithName(const QString &name, QWidget *widget, const char *property = nullptr, const char *changedSignal = nullptr) -> void;

    virtual auto handleReject() -> bool;
    virtual auto handleAccept() -> bool;

signals:
    // Emitted when there is something that the developer using this page should be aware of.
    auto reportError(const QString &errorMessage) -> void;

private:
    auto registerFieldName(const QString &name) -> void;

    QSet<QString> m_toRegister;
};

} // namespace Utils
