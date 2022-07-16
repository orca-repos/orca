// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodestylesettings.hpp"

#include "cppcodestylepreferences.hpp"
#include "cppeditorconstants.hpp"
#include "cpptoolssettings.hpp"

#include <projectexplorer/editorconfiguration.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projecttree.hpp>

#include <texteditor/tabsettings.hpp>

#include <cplusplus/Overview.h>

#include <utils/qtcassert.hpp>
#include <utils/settingsutils.hpp>

static constexpr char indentBlockBracesKey[] = "IndentBlockBraces";
static constexpr char indentBlockBodyKey[] = "IndentBlockBody";
static constexpr char indentClassBracesKey[] = "IndentClassBraces";
static constexpr char indentEnumBracesKey[] = "IndentEnumBraces";
static constexpr char indentNamespaceBracesKey[] = "IndentNamespaceBraces";
static constexpr char indentNamespaceBodyKey[] = "IndentNamespaceBody";
static constexpr char indentAccessSpecifiersKey[] = "IndentAccessSpecifiers";
static constexpr char indentDeclarationsRelativeToAccessSpecifiersKey[] = "IndentDeclarationsRelativeToAccessSpecifiers";
static constexpr char indentFunctionBodyKey[] = "IndentFunctionBody";
static constexpr char indentFunctionBracesKey[] = "IndentFunctionBraces";
static constexpr char indentSwitchLabelsKey[] = "IndentSwitchLabels";
static constexpr char indentStatementsRelativeToSwitchLabelsKey[] = "IndentStatementsRelativeToSwitchLabels";
static constexpr char indentBlocksRelativeToSwitchLabelsKey[] = "IndentBlocksRelativeToSwitchLabels";
static constexpr char indentControlFlowRelativeToSwitchLabelsKey[] = "IndentControlFlowRelativeToSwitchLabels";
static constexpr char bindStarToIdentifierKey[] = "BindStarToIdentifier";
static constexpr char bindStarToTypeNameKey[] = "BindStarToTypeName";
static constexpr char bindStarToLeftSpecifierKey[] = "BindStarToLeftSpecifier";
static constexpr char bindStarToRightSpecifierKey[] = "BindStarToRightSpecifier";
static constexpr char extraPaddingForConditionsIfConfusingAlignKey[] = "ExtraPaddingForConditionsIfConfusingAlign";
static constexpr char alignAssignmentsKey[] = "AlignAssignments";
static constexpr char shortGetterNameKey[] = "ShortGetterName";

namespace CppEditor {

CppCodeStyleSettings::CppCodeStyleSettings() = default;

auto CppCodeStyleSettings::toMap() const -> QVariantMap
{
  return {{indentBlockBracesKey, indentBlockBraces}, {indentBlockBodyKey, indentBlockBody}, {indentClassBracesKey, indentClassBraces}, {indentEnumBracesKey, indentEnumBraces}, {indentNamespaceBracesKey, indentNamespaceBraces}, {indentNamespaceBodyKey, indentNamespaceBody}, {indentAccessSpecifiersKey, indentAccessSpecifiers}, {indentDeclarationsRelativeToAccessSpecifiersKey, indentDeclarationsRelativeToAccessSpecifiers}, {indentFunctionBodyKey, indentFunctionBody}, {indentFunctionBracesKey, indentFunctionBraces}, {indentSwitchLabelsKey, indentSwitchLabels}, {indentStatementsRelativeToSwitchLabelsKey, indentStatementsRelativeToSwitchLabels}, {indentBlocksRelativeToSwitchLabelsKey, indentBlocksRelativeToSwitchLabels}, {indentControlFlowRelativeToSwitchLabelsKey, indentControlFlowRelativeToSwitchLabels}, {bindStarToIdentifierKey, bindStarToIdentifier}, {bindStarToTypeNameKey, bindStarToTypeName}, {bindStarToLeftSpecifierKey, bindStarToLeftSpecifier}, {bindStarToRightSpecifierKey, bindStarToRightSpecifier}, {extraPaddingForConditionsIfConfusingAlignKey, extraPaddingForConditionsIfConfusingAlign}, {alignAssignmentsKey, alignAssignments}, {shortGetterNameKey, preferGetterNameWithoutGetPrefix}};
}

auto CppCodeStyleSettings::fromMap(const QVariantMap &map) -> void
{
  indentBlockBraces = map.value(indentBlockBracesKey, indentBlockBraces).toBool();
  indentBlockBody = map.value(indentBlockBodyKey, indentBlockBody).toBool();
  indentClassBraces = map.value(indentClassBracesKey, indentClassBraces).toBool();
  indentEnumBraces = map.value(indentEnumBracesKey, indentEnumBraces).toBool();
  indentNamespaceBraces = map.value(indentNamespaceBracesKey, indentNamespaceBraces).toBool();
  indentNamespaceBody = map.value(indentNamespaceBodyKey, indentNamespaceBody).toBool();
  indentAccessSpecifiers = map.value(indentAccessSpecifiersKey, indentAccessSpecifiers).toBool();
  indentDeclarationsRelativeToAccessSpecifiers = map.value(indentDeclarationsRelativeToAccessSpecifiersKey, indentDeclarationsRelativeToAccessSpecifiers).toBool();
  indentFunctionBody = map.value(indentFunctionBodyKey, indentFunctionBody).toBool();
  indentFunctionBraces = map.value(indentFunctionBracesKey, indentFunctionBraces).toBool();
  indentSwitchLabels = map.value(indentSwitchLabelsKey, indentSwitchLabels).toBool();
  indentStatementsRelativeToSwitchLabels = map.value(indentStatementsRelativeToSwitchLabelsKey, indentStatementsRelativeToSwitchLabels).toBool();
  indentBlocksRelativeToSwitchLabels = map.value(indentBlocksRelativeToSwitchLabelsKey, indentBlocksRelativeToSwitchLabels).toBool();
  indentControlFlowRelativeToSwitchLabels = map.value(indentControlFlowRelativeToSwitchLabelsKey, indentControlFlowRelativeToSwitchLabels).toBool();
  bindStarToIdentifier = map.value(bindStarToIdentifierKey, bindStarToIdentifier).toBool();
  bindStarToTypeName = map.value(bindStarToTypeNameKey, bindStarToTypeName).toBool();
  bindStarToLeftSpecifier = map.value(bindStarToLeftSpecifierKey, bindStarToLeftSpecifier).toBool();
  bindStarToRightSpecifier = map.value(bindStarToRightSpecifierKey, bindStarToRightSpecifier).toBool();
  extraPaddingForConditionsIfConfusingAlign = map.value(extraPaddingForConditionsIfConfusingAlignKey, extraPaddingForConditionsIfConfusingAlign).toBool();
  alignAssignments = map.value(alignAssignmentsKey, alignAssignments).toBool();
  preferGetterNameWithoutGetPrefix = map.value(shortGetterNameKey, preferGetterNameWithoutGetPrefix).toBool();
}

auto CppCodeStyleSettings::equals(const CppCodeStyleSettings &rhs) const -> bool
{
  return indentBlockBraces == rhs.indentBlockBraces && indentBlockBody == rhs.indentBlockBody && indentClassBraces == rhs.indentClassBraces && indentEnumBraces == rhs.indentEnumBraces && indentNamespaceBraces == rhs.indentNamespaceBraces && indentNamespaceBody == rhs.indentNamespaceBody && indentAccessSpecifiers == rhs.indentAccessSpecifiers && indentDeclarationsRelativeToAccessSpecifiers == rhs.indentDeclarationsRelativeToAccessSpecifiers && indentFunctionBody == rhs.indentFunctionBody && indentFunctionBraces == rhs.indentFunctionBraces && indentSwitchLabels == rhs.indentSwitchLabels && indentStatementsRelativeToSwitchLabels == rhs.indentStatementsRelativeToSwitchLabels && indentBlocksRelativeToSwitchLabels == rhs.indentBlocksRelativeToSwitchLabels && indentControlFlowRelativeToSwitchLabels == rhs.indentControlFlowRelativeToSwitchLabels && bindStarToIdentifier == rhs.bindStarToIdentifier && bindStarToTypeName == rhs.bindStarToTypeName && bindStarToLeftSpecifier == rhs.bindStarToLeftSpecifier && bindStarToRightSpecifier == rhs.bindStarToRightSpecifier && extraPaddingForConditionsIfConfusingAlign == rhs.extraPaddingForConditionsIfConfusingAlign && alignAssignments == rhs.alignAssignments && preferGetterNameWithoutGetPrefix == rhs.preferGetterNameWithoutGetPrefix;
}

auto CppCodeStyleSettings::getProjectCodeStyle(ProjectExplorer::Project *project) -> CppCodeStyleSettings
{
  if (!project)
    return currentGlobalCodeStyle();

  auto editorConfiguration = project->editorConfiguration();
  QTC_ASSERT(editorConfiguration, return currentGlobalCodeStyle());

  auto codeStylePreferences = editorConfiguration->codeStyle(Constants::CPP_SETTINGS_ID);
  QTC_ASSERT(codeStylePreferences, return currentGlobalCodeStyle());

  auto cppCodeStylePreferences = dynamic_cast<const CppCodeStylePreferences*>(codeStylePreferences);
  if (!cppCodeStylePreferences)
    return currentGlobalCodeStyle();

  return cppCodeStylePreferences->currentCodeStyleSettings();
}

auto CppCodeStyleSettings::currentProjectCodeStyle() -> CppCodeStyleSettings
{
  return getProjectCodeStyle(ProjectExplorer::ProjectTree::currentProject());
}

auto CppCodeStyleSettings::currentGlobalCodeStyle() -> CppCodeStyleSettings
{
  auto cppCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
  QTC_ASSERT(cppCodeStylePreferences, return CppCodeStyleSettings());

  return cppCodeStylePreferences->currentCodeStyleSettings();
}

auto CppCodeStyleSettings::getProjectTabSettings(ProjectExplorer::Project *project) -> TextEditor::TabSettings
{
  if (!project)
    return currentGlobalTabSettings();

  auto editorConfiguration = project->editorConfiguration();
  QTC_ASSERT(editorConfiguration, return currentGlobalTabSettings());

  auto codeStylePreferences = editorConfiguration->codeStyle(Constants::CPP_SETTINGS_ID);
  QTC_ASSERT(codeStylePreferences, return currentGlobalTabSettings());
  return codeStylePreferences->currentTabSettings();
}

auto CppCodeStyleSettings::currentProjectTabSettings() -> TextEditor::TabSettings
{
  return getProjectTabSettings(ProjectExplorer::ProjectTree::currentProject());
}

auto CppCodeStyleSettings::currentGlobalTabSettings() -> TextEditor::TabSettings
{
  auto cppCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
  QTC_ASSERT(cppCodeStylePreferences, return TextEditor::TabSettings());

  return cppCodeStylePreferences->currentTabSettings();
}

static auto configureOverviewWithCodeStyleSettings(CPlusPlus::Overview &overview, const CppCodeStyleSettings &settings) -> void
{
  overview.starBindFlags = {};
  if (settings.bindStarToIdentifier)
    overview.starBindFlags |= CPlusPlus::Overview::BindToIdentifier;
  if (settings.bindStarToTypeName)
    overview.starBindFlags |= CPlusPlus::Overview::BindToTypeName;
  if (settings.bindStarToLeftSpecifier)
    overview.starBindFlags |= CPlusPlus::Overview::BindToLeftSpecifier;
  if (settings.bindStarToRightSpecifier)
    overview.starBindFlags |= CPlusPlus::Overview::BindToRightSpecifier;
}

auto CppCodeStyleSettings::currentProjectCodeStyleOverview() -> CPlusPlus::Overview
{
  CPlusPlus::Overview overview;
  const Utils::optional<CppCodeStyleSettings> codeStyleSettings = currentProjectCodeStyle();
  configureOverviewWithCodeStyleSettings(overview, codeStyleSettings.value_or(currentGlobalCodeStyle()));
  return overview;
}

auto CppCodeStyleSettings::currentGlobalCodeStyleOverview() -> CPlusPlus::Overview
{
  CPlusPlus::Overview overview;
  configureOverviewWithCodeStyleSettings(overview, currentGlobalCodeStyle());
  return overview;
}

} // namespace CppEditor
