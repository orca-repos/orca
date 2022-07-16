// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/optional.hpp>

#include <QString>
#include <QStringList>

#include <vector>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace CppEditor {

class CppQuickFixSettings {
public:
  static auto instance() -> CppQuickFixSettings*
  {
    static CppQuickFixSettings settings(true);
    return &settings;
  }

  struct CustomTemplate {
    auto operator==(const CustomTemplate &b) const -> bool
    {
      return types == b.types && equalComparison == b.equalComparison && returnExpression == b.returnExpression && returnType == b.returnType && assignment == b.assignment;
    }

    QStringList types;
    QString equalComparison;
    QString returnExpression;
    QString returnType;
    QString assignment;
  };

  struct GetterSetterTemplate {
    QString equalComparison = "<cur> == <new>";
    QString returnExpression = "<cur>";
    QString assignment = "<cur> = <new>";
    const static inline QString TYPE_PATTERN = "<type>";
    const static inline QString TEMPLATE_PARAMETER_PATTERN = "<T>";
    Utils::optional<QString> returnTypeTemplate;
    auto replacePlaceholders(QString currentValueVariableName, QString newValueVariableName) -> void;
  };

  enum class FunctionLocation {
    InsideClass,
    OutsideClass,
    CppFile,
  };

  enum class MissingNamespaceHandling {
    CreateMissing,
    AddUsingDirective,
    RewriteType,
    // e.g. change classname to namespacename::classname in cpp file
  };

  explicit CppQuickFixSettings(bool loadGlobalSettings = false);

  auto loadGlobalSettings() -> void;
  auto loadSettingsFrom(QSettings *) -> void;
  auto saveSettingsTo(QSettings *) -> void;
  auto saveAsGlobalSettings() -> void;
  auto setDefaultSettings() -> void;
  static auto replaceNamePlaceholders(const QString &nameTemplate, const QString &name) -> QString;
  auto isValueType(QString type) const -> bool;
  auto findGetterSetterTemplate(QString fullyQualifiedType) const -> GetterSetterTemplate;

  auto getGetterName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(getterNameTemplate, variableName);
  }

  auto getSetterName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(setterNameTemplate, variableName);
  }

  auto getSignalName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(signalNameTemplate, variableName);
  }

  auto getResetName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(resetNameTemplate, variableName);
  }

  auto getSetterParameterName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(setterParameterNameTemplate, variableName);
  }

  auto getMemberVariableName(const QString &variableName) const -> QString
  {
    return replaceNamePlaceholders(memberVariableNameTemplate, variableName);
  }

  auto determineGetterLocation(int lineCount) const -> FunctionLocation;
  auto determineSetterLocation(int lineCount) const -> FunctionLocation;

  auto createMissingNamespacesinCppFile() const -> bool
  {
    return cppFileNamespaceHandling == MissingNamespaceHandling::CreateMissing;
  }

  auto addUsingNamespaceinCppFile() const -> bool
  {
    return cppFileNamespaceHandling == MissingNamespaceHandling::AddUsingDirective;
  }

  auto rewriteTypesinCppFile() const -> bool
  {
    return cppFileNamespaceHandling == MissingNamespaceHandling::RewriteType;
  }
  
  int getterOutsideClassFrom = 0;
  int getterInCppFileFrom = 1;
  int setterOutsideClassFrom = 0;
  int setterInCppFileFrom = 1;
  QString getterAttributes;                 // e.g. [[nodiscard]]
  QString getterNameTemplate = "<name>";    // or get<Name>
  QString setterNameTemplate = "set<Name>"; // or set_<name> or Set<Name>
  QString setterParameterNameTemplate = "new<Name>";
  QString signalNameTemplate = "<name>Changed";
  QString resetNameTemplate = "reset<Name>";
  bool signalWithNewValue = false;
  bool setterAsSlot = false;
  MissingNamespaceHandling cppFileNamespaceHandling = MissingNamespaceHandling::CreateMissing;
  QString memberVariableNameTemplate = "m_<name>";
  QStringList valueTypes; // if contains use value. Ignores namespaces and template parameters
  std::vector<CustomTemplate> customTemplates;
};

} // namespace CppEditor
