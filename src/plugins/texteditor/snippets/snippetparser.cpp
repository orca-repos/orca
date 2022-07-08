// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippetparser.hpp"

namespace TextEditor {

auto SnippetParseError::htmlMessage() const -> QString
{
  auto message = errorMessage;
  if (pos < 0 || pos > 50)
    return message;
  auto detail = text.left(50);
  if (detail != text)
    detail.append("...");
  detail.replace(QChar::Space, "&nbsp;");
  message.append("<br><code>" + detail + "<br>");
  for (auto i = 0; i < pos; ++i)
    message.append("&nbsp;");
  message.append("^</code>");
  return message;
}

} // namespace TextEditor
