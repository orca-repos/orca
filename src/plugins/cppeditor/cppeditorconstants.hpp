// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace CppEditor {
namespace Constants {

constexpr char M_CONTEXT[] = "CppEditor.ContextMenu";
constexpr char G_CONTEXT_FIRST[] = "CppEditor.GFirst";
constexpr char CPPEDITOR_ID[] = "CppEditor.C++Editor";
constexpr char CPPEDITOR_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("OpenWith::Editors", "C++ Editor");
constexpr char SWITCH_DECLARATION_DEFINITION[] = "CppEditor.SwitchDeclarationDefinition";
constexpr char OPEN_DECLARATION_DEFINITION_IN_NEXT_SPLIT[] = "CppEditor.OpenDeclarationDefinitionInNextSplit";
constexpr char OPEN_PREPROCESSOR_DIALOG[] = "CppEditor.OpenPreprocessorDialog";
constexpr char ERRORS_IN_HEADER_FILES[] = "CppEditor.ErrorsInHeaderFiles";
constexpr char MULTIPLE_PARSE_CONTEXTS_AVAILABLE[] = "CppEditor.MultipleParseContextsAvailable";
constexpr char NO_PROJECT_CONFIGURATION[] = "CppEditor.NoProjectConfiguration";
constexpr char M_REFACTORING_MENU_INSERTION_POINT[] = "CppEditor.RefactorGroup";
constexpr char UPDATE_CODEMODEL[] = "CppEditor.UpdateCodeModel";
constexpr char INSPECT_CPP_CODEMODEL[] = "CppEditor.InspectCppCodeModel";
constexpr char TYPE_HIERARCHY_ID[] = "CppEditor.TypeHierarchy";
constexpr char OPEN_TYPE_HIERARCHY[] = "CppEditor.OpenTypeHierarchy";
constexpr char INCLUDE_HIERARCHY_ID[] = "CppEditor.IncludeHierarchy";
constexpr char OPEN_INCLUDE_HIERARCHY[] = "CppEditor.OpenIncludeHierarchy";
constexpr char CPP_SNIPPETS_GROUP_ID[] = "C++";
constexpr char EXTRA_PREPROCESSOR_DIRECTIVES[] = "CppEditor.ExtraPreprocessorDirectives-";
constexpr char PREFERRED_PARSE_CONTEXT[] = "CppEditor.PreferredParseContext-";
constexpr char QUICK_FIX_PROJECT_PANEL_ID[] = "CppEditor.QuickFix";
constexpr char QUICK_FIX_SETTINGS_ID[] = "CppEditor.QuickFix";
constexpr char QUICK_FIX_SETTINGS_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "Quick Fixes");
constexpr char QUICK_FIX_SETTING_GETTER_OUTSIDE_CLASS_FROM[] = "GettersOutsideClassFrom";
constexpr char QUICK_FIX_SETTING_GETTER_IN_CPP_FILE_FROM[] = "GettersInCppFileFrom";
constexpr char QUICK_FIX_SETTING_SETTER_OUTSIDE_CLASS_FROM[] = "SettersOutsideClassFrom";
constexpr char QUICK_FIX_SETTING_SETTER_IN_CPP_FILE_FROM[] = "SettersInCppFileFrom";
constexpr char QUICK_FIX_SETTING_GETTER_ATTRIBUTES[] = "GetterAttributes";
constexpr char QUICK_FIX_SETTING_GETTER_NAME_TEMPLATE[] = "GetterNameTemplate";
constexpr char QUICK_FIX_SETTING_SETTER_NAME_TEMPLATE[] = "SetterNameTemplate";
constexpr char QUICK_FIX_SETTING_SIGNAL_NAME_TEMPLATE[] = "SignalNameTemplate";
constexpr char QUICK_FIX_SETTING_RESET_NAME_TEMPLATE[] = "ResetNameTemplate";
constexpr char QUICK_FIX_SETTING_SIGNAL_WITH_NEW_VALUE[] = "SignalWithNewValue";
constexpr char QUICK_FIX_SETTING_SETTER_AS_SLOT[] = "SetterAsSlot";
constexpr char QUICK_FIX_SETTING_SETTER_PARAMETER_NAME[] = "SetterParameterName";
constexpr char QUICK_FIX_SETTING_CPP_FILE_NAMESPACE_HANDLING[] = "CppFileNamespaceHandling";
constexpr char QUICK_FIX_SETTING_MEMBER_VARIABEL_NAME_TEMPLATE[] = "MemberVariableNameTemplate";
constexpr char QUICK_FIX_SETTING_VALUE_TYPES[] = "ValueTypes";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATES[] = "CustomTemplate";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATE_TYPES[] = "Types";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATE_COMPARISON[] = "Comparison";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATE_RETURN_TYPE[] = "ReturnType";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATE_RETURN_EXPRESSION[] = "ReturnExpression";
constexpr char QUICK_FIX_SETTING_CUSTOM_TEMPLATE_ASSIGNMENT[] = "Assignment";
constexpr char M_TOOLS_CPP[]              = "CppTools.Tools.Menu";
constexpr char SWITCH_HEADER_SOURCE[]     = "CppTools.SwitchHeaderSource";
constexpr char OPEN_HEADER_SOURCE_IN_NEXT_SPLIT[] = "CppTools.OpenHeaderSourceInNextSplit";
constexpr char TASK_INDEX[]               = "CppTools.Task.Index";
constexpr char TASK_SEARCH[]              = "CppTools.Task.Search";
constexpr char C_SOURCE_MIMETYPE[] = "text/x-csrc";
constexpr char CUDA_SOURCE_MIMETYPE[] = "text/vnd.nvidia.cuda.csrc";
constexpr char C_HEADER_MIMETYPE[] = "text/x-chdr";
constexpr char CPP_SOURCE_MIMETYPE[] = "text/x-c++src";
constexpr char OBJECTIVE_C_SOURCE_MIMETYPE[] = "text/x-objcsrc";
constexpr char OBJECTIVE_CPP_SOURCE_MIMETYPE[] = "text/x-objc++src";
constexpr char CPP_HEADER_MIMETYPE[] = "text/x-c++hdr";
constexpr char QDOC_MIMETYPE[] = "text/x-qdoc";
constexpr char MOC_MIMETYPE[] = "text/x-moc";
constexpr char AMBIGUOUS_HEADER_MIMETYPE[] = "application/vnd.qtc.ambiguousheader"; // not a real MIME type

// QSettings keys for use by the "New Class" wizards.
constexpr char CPPEDITOR_SETTINGSGROUP[] = "CppTools";
constexpr char LOWERCASE_CPPFILES_KEY[] = "LowerCaseFiles";
constexpr bool LOWERCASE_CPPFILES_DEFAULT = true;
constexpr char CPPEDITOR_SORT_EDITOR_DOCUMENT_OUTLINE[] = "SortedMethodOverview";
constexpr char CPPEDITOR_SHOW_INFO_BAR_FOR_HEADER_ERRORS[] = "ShowInfoBarForHeaderErrors";
constexpr char CPPEDITOR_SHOW_INFO_BAR_FOR_FOR_NO_PROJECT[] = "ShowInfoBarForNoProject";
constexpr char CPPEDITOR_MODEL_MANAGER_PCH_USAGE[] = "PCHUsage";
constexpr char CPPEDITOR_INTERPRET_AMBIGIUOUS_HEADERS_AS_C_HEADERS[] = "InterpretAmbiguousHeadersAsCHeaders";
constexpr char CPPEDITOR_SKIP_INDEXING_BIG_FILES[] = "SkipIndexingBigFiles";
constexpr char CPPEDITOR_INDEXER_FILE_SIZE_LIMIT[] = "IndexerFileSizeLimit";
constexpr char CPP_CLANG_DIAG_CONFIG_QUESTIONABLE[] = "Builtin.Questionable";
constexpr char CPP_CLANG_DIAG_CONFIG_BUILDSYSTEM[] = "Builtin.BuildSystem";
constexpr char CPP_CODE_STYLE_SETTINGS_ID[] = "A.Cpp.Code Style";
constexpr char CPP_CODE_STYLE_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "Code Style");
constexpr char CPP_FILE_SETTINGS_ID[] = "B.Cpp.File Naming";
constexpr char CPP_FILE_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "File Naming");
constexpr char CPP_CODE_MODEL_SETTINGS_ID[] = "C.Cpp.Code Model";
constexpr char CPP_DIAGNOSTIC_CONFIG_SETTINGS_ID[] = "C.Cpp.Diagnostic Config";
constexpr char CPP_DIAGNOSTIC_CONFIG_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "Diagnostic Configurations");
constexpr char CPP_CLANGD_SETTINGS_ID[] = "K.Cpp.Clangd";
constexpr char CPP_SETTINGS_CATEGORY[] = "I.C++";
constexpr char CPP_CLANG_FIXIT_AVAILABLE_MARKER_ID[] = "ClangFixItAvailableMarker";
constexpr char CPP_FUNCTION_DECL_DEF_LINK_MARKER_ID[] = "FunctionDeclDefLinkMarker";
constexpr char CPP_SETTINGS_ID[] = "Cpp";
constexpr char CPP_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++");
constexpr char CURRENT_DOCUMENT_FILTER_ID[] = "Methods in current Document";
constexpr char CURRENT_DOCUMENT_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++ Symbols in Current Document");
constexpr char CLASSES_FILTER_ID[] = "Classes";
constexpr char CLASSES_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++ Classes");
constexpr char FUNCTIONS_FILTER_ID[] = "Methods";
constexpr char FUNCTIONS_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++ Functions");
constexpr char INCLUDES_FILTER_ID[] = "All Included C/C++ Files";
constexpr char INCLUDES_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "All Included C/C++ Files");
constexpr char LOCATOR_FILTER_ID[] = "Classes and Methods";
constexpr char LOCATOR_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++ Classes, Enums, Functions and Type Aliases");
constexpr char SYMBOLS_FIND_FILTER_ID[] = "Symbols";
constexpr char SYMBOLS_FIND_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("CppEditor", "C++ Symbols");

constexpr const char CLANG_STATIC_ANALYZER_DOCUMENTATION_URL[]
    = "https://clang-analyzer.llvm.org/available_checks.html";

} // namespace Constants
} // namespace CppEditor
