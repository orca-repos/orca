// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <texteditor/snippets/snippetparser.hpp>

namespace LanguageClient {

LANGUAGECLIENT_EXPORT auto parseSnippet(const QString &snippet) -> TextEditor::SnippetParseResult;

} // namespace LanguageClient
