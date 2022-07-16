// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "profilehighlighter.hpp"
#include "profilecompletionassist.hpp"

#include <extensionsystem/pluginmanager.hpp>
#include <utils/qtcassert.hpp>

#include <QTextDocument>

using namespace TextEditor;

namespace QmakeProjectManager {
namespace Internal {

static auto styleForFormat(int format) -> TextStyle
{
  const auto f = ProFileHighlighter::ProfileFormats(format);
  switch (f) {
  case ProFileHighlighter::ProfileVariableFormat:
    return C_TYPE;
  case ProFileHighlighter::ProfileFunctionFormat:
    return C_KEYWORD;
  case ProFileHighlighter::ProfileCommentFormat:
    return C_COMMENT;
  case ProFileHighlighter::ProfileVisualWhitespaceFormat:
    return C_VISUAL_WHITESPACE;
  case ProFileHighlighter::NumProfileFormats: QTC_CHECK(false); // should never get here
    return C_TEXT;
  }
  QTC_CHECK(false); // should never get here
  return C_TEXT;
}

ProFileHighlighter::ProFileHighlighter() : m_keywords(qmakeKeywords())
{
  setTextFormatCategories(NumProfileFormats, styleForFormat);
}

auto ProFileHighlighter::highlightBlock(const QString &text) -> void
{
  if (text.isEmpty())
    return;

  QString buf;
  auto inCommentMode = false;

  QTextCharFormat emptyFormat;
  auto i = 0;
  for (;;) {
    const auto c = text.at(i);
    if (inCommentMode) {
      setFormat(i, 1, formatForCategory(ProfileCommentFormat));
    } else {
      if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('.') || c.isDigit()) {
        buf += c;
        setFormat(i - buf.length() + 1, buf.length(), emptyFormat);
        if (!buf.isEmpty() && m_keywords.isFunction(buf))
          setFormat(i - buf.length() + 1, buf.length(), formatForCategory(ProfileFunctionFormat));
        else if (!buf.isEmpty() && m_keywords.isVariable(buf))
          setFormat(i - buf.length() + 1, buf.length(), formatForCategory(ProfileVariableFormat));
      } else if (c == QLatin1Char('(')) {
        if (!buf.isEmpty() && m_keywords.isFunction(buf))
          setFormat(i - buf.length(), buf.length(), formatForCategory(ProfileFunctionFormat));
        buf.clear();
      } else if (c == QLatin1Char('#')) {
        inCommentMode = true;
        setFormat(i, 1, formatForCategory(ProfileCommentFormat));
        buf.clear();
      } else {
        if (!buf.isEmpty() && m_keywords.isVariable(buf))
          setFormat(i - buf.length(), buf.length(), formatForCategory(ProfileVariableFormat));
        buf.clear();
      }
    }
    i++;
    if (i >= text.length())
      break;
  }

  formatSpaces(text);
}

} // namespace Internal
} // namespace QmakeProjectManager
