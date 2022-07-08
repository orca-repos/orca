// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <QMetaType>
#include <QFlags>
#include <QTextDocument>

namespace Core {
namespace Constants {

constexpr char C_FINDTOOLBAR[]          = "Find.ToolBar";
constexpr char M_FIND[]                 = "Find.FindMenu";
constexpr char M_FIND_ADVANCED[]        = "Find.FindAdvancedMenu";
constexpr char G_FIND_CURRENTDOCUMENT[] = "Find.FindMenu.CurrentDocument";
constexpr char G_FIND_FILTERS[]         = "Find.FindMenu.Filters";
constexpr char G_FIND_FLAGS[]           = "Find.FindMenu.Flags";
constexpr char G_FIND_ACTIONS[]         = "Find.FindMenu.Actions";
constexpr char ADVANCED_FIND[]          = "Find.Dialog";
constexpr char FIND_IN_DOCUMENT[]       = "Find.FindInCurrentDocument";
constexpr char FIND_NEXT_SELECTED[]     = "Find.FindNextSelected";
constexpr char FIND_PREV_SELECTED[]     = "Find.FindPreviousSelected";
constexpr char FIND_SELECT_ALL[]        = "Find.SelectAll";
constexpr char FIND_NEXT[]              = "Find.FindNext";
constexpr char FIND_PREVIOUS[]          = "Find.FindPrevious";
constexpr char REPLACE[]                = "Find.Replace";
constexpr char REPLACE_NEXT[]           = "Find.ReplaceNext";
constexpr char REPLACE_PREVIOUS[]       = "Find.ReplacePrevious";
constexpr char REPLACE_ALL[]            = "Find.ReplaceAll";
constexpr char CASE_SENSITIVE[]         = "Find.CaseSensitive";
constexpr char WHOLE_WORDS[]            = "Find.WholeWords";
constexpr char REGULAR_EXPRESSION[]     = "Find.RegularExpressions";
constexpr char PRESERVE_CASE[]          = "Find.PreserveCase";
constexpr char TASK_SEARCH[]            = "Find.Task.Search";

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
