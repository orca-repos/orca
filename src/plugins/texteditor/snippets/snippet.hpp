// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "snippetparser.hpp"

#include <texteditor/texteditor_global.hpp>

#include <QCoreApplication>
#include <QString>

namespace TextEditor {

class TEXTEDITOR_EXPORT Snippet {
  Q_DECLARE_TR_FUNCTIONS(Snippet)

public:
  explicit Snippet(const QString &groupId = QString(), const QString &id = QString());
  ~Snippet();

  auto id() const -> const QString&;
  auto groupId() const -> const QString&;
  auto isBuiltIn() const -> bool;
  auto setTrigger(const QString &trigger) -> void;
  auto trigger() const -> const QString&;
  static auto isValidTrigger(const QString &trigger) -> bool;
  auto setContent(const QString &content) -> void;
  auto content() const -> const QString&;
  auto setComplement(const QString &complement) -> void;
  auto complement() const -> const QString&;
  auto setIsRemoved(bool removed) -> void;
  auto isRemoved() const -> bool;
  auto setIsModified(bool modified) -> void;
  auto isModified() const -> bool;
  auto generateTip() const -> QString;

  static const QChar kVariableDelimiter;
  static const QChar kEscapeChar;

  static auto parse(const QString &snippet) -> SnippetParseResult;

private:
  bool m_isRemoved = false;
  bool m_isModified = false;
  QString m_groupId;
  QString m_id; // Only built-in snippets have an id.
  QString m_trigger;
  QString m_content;
  QString m_complement;
};

} // TextEditor
