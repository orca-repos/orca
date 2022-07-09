// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>

#include <QStringList>

namespace CppEditor {

class CppLocatorData; // FIXME: Belongs in namespace Internal

namespace Internal {

/**
 * This class extends the JS features in our macro expander.
 */
class CppToolsJsExtension : public QObject
{
    Q_OBJECT

public:
    explicit CppToolsJsExtension(CppLocatorData *locatorData, QObject *parent = nullptr) : QObject(parent), m_locatorData(locatorData) { }

    // Generate header guard:
    auto headerGuard(const QString &in) const -> Q_INVOKABLE QString;

    // Work with classes:
    auto namespaces(const QString &klass) const -> Q_INVOKABLE QStringList;
    auto hasNamespaces(const QString &klass) const -> Q_INVOKABLE bool;
    auto className(const QString &klass) const -> Q_INVOKABLE QString;
    // Fix the filename casing as configured in C++/File Naming:
    auto classToFileName(const QString &klass, const QString &extension) const -> Q_INVOKABLE QString;
    auto classToHeaderGuard(const QString &klass, const QString &extension) const -> Q_INVOKABLE QString;
    auto openNamespaces(const QString &klass) const -> Q_INVOKABLE QString;
    auto closeNamespaces(const QString &klass) const -> Q_INVOKABLE QString;
    auto hasQObjectParent(const QString &klassName) const -> Q_INVOKABLE bool;

    // Find header file for class.
    auto includeStatement(const QString &fullyQualifiedClassName, const QString &suffix, const QStringList &specialClasses, const QString &pathOfIncludingFile) -> Q_INVOKABLE QString;

private:
    CppLocatorData * const m_locatorData;
};

} // namespace Internal
} // namespace CppEditor
