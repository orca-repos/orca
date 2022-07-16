// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <QObject>
#include <QStringList>

namespace QtSupport {

class QTSUPPORT_EXPORT CodeGenerator : public QObject {
  Q_OBJECT

public:
  CodeGenerator(QObject *parent = nullptr) : QObject(parent) { }

  // Ui file related:
  // Change the class name in a UI XML form
  static auto changeUiClassName(const QString &uiXml, const QString &newUiClassName) -> Q_INVOKABLE QString;

  // Low level method to get everything at the same time:
  static auto uiData(const QString &uiXml, QString *formBaseClass, QString *uiClassName) -> bool;
  static auto uiClassName(const QString &uiXml) -> Q_INVOKABLE QString;

  // Generic Qt:
  static auto qtIncludes(const QStringList &qt4, const QStringList &qt5) -> Q_INVOKABLE QString;

  // UI file integration
  static auto uiAsPointer() -> Q_INVOKABLE bool;
  static auto uiAsMember() -> Q_INVOKABLE bool;
  static auto uiAsInheritance() -> Q_INVOKABLE bool;
};

} // namespace QtSupport
