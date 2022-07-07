// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <QMetaType>
#include <QFlags>
#include <QTextDocument>

namespace Core {
namespace Constants {

constexpr char c_findtoolbar[]          = "Find.ToolBar";
constexpr char m_find[]                 = "Find.FindMenu";
constexpr char m_find_advanced[]        = "Find.FindAdvancedMenu";
constexpr char g_find_currentdocument[] = "Find.FindMenu.CurrentDocument";
constexpr char g_find_filters[]         = "Find.FindMenu.Filters";
constexpr char g_find_flags[]           = "Find.FindMenu.Flags";
constexpr char g_find_actions[]         = "Find.FindMenu.Actions";
constexpr char advanced_find[]          = "Find.Dialog";
constexpr char find_in_document[]       = "Find.FindInCurrentDocument";
constexpr char find_next_selected[]     = "Find.FindNextSelected";
constexpr char find_prev_selected[]     = "Find.FindPreviousSelected";
constexpr char find_select_all[]        = "Find.SelectAll";
constexpr char find_next[]              = "Find.FindNext";
constexpr char find_previous[]          = "Find.FindPrevious";
constexpr char replace[]                = "Find.Replace";
constexpr char replace_next[]           = "Find.ReplaceNext";
constexpr char replace_previous[]       = "Find.ReplacePrevious";
constexpr char replace_all[]            = "Find.ReplaceAll";
constexpr char case_sensitive[]         = "Find.CaseSensitive";
constexpr char whole_words[]            = "Find.WholeWords";
constexpr char regular_expressions[]    = "Find.RegularExpressions";
constexpr char preserve_case[]          = "Find.PreserveCase";
constexpr char task_search[]            = "Find.Task.Search";

} // namespace Constants

enum FindFlag {
    FindBackward = 0x01,
    FindCaseSensitively = 0x02,
    FindWholeWords = 0x04,
    FindRegularExpression = 0x08,
    FindPreserveCase = 0x10
};

Q_DECLARE_FLAGS(FindFlags, FindFlag)

// defined in findplugin.cpp
CORE_EXPORT auto textDocumentFlagsForFindFlags(FindFlags flags) -> QTextDocument::FindFlags;

} // namespace Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Core::FindFlags)
Q_DECLARE_METATYPE(Core::FindFlags)
