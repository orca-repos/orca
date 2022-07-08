// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <utils/id.hpp>
#include <utils/variant.hpp>

namespace TextEditor {

class TEXTEDITOR_EXPORT NameMangler {
public:
  virtual ~NameMangler();

  virtual auto id() const -> Utils::Id = 0;
  virtual auto mangle(const QString &unmangled) const -> QString = 0;
};

class TEXTEDITOR_EXPORT ParsedSnippet {
public:
  class Part {
  public:
    Part() = default;
    explicit Part(const QString &text) : text(text) {}
    QString text;
    int variableIndex = -1; // if variable index is >= 0 the text is interpreted as a variable
    NameMangler *mangler = nullptr;
    bool finalPart = false;
  };

  QList<Part> parts;
  QList<QList<int>> variables;
};

class TEXTEDITOR_EXPORT SnippetParseError {
public:
  QString errorMessage;
  QString text;
  int pos;

  auto htmlMessage() const -> QString;
};

using SnippetParseResult = Utils::variant<ParsedSnippet, SnippetParseError>;
using SnippetParser = std::function<SnippetParseResult (const QString &)>;

} // namespace TextEditor
