// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <cplusplus/LookupContext.h>
#include <cplusplus/Symbol.h>
#include <cplusplus/TypeOfExpression.h>

QT_FORWARD_DECLARE_CLASS(QTextCursor)

namespace CppEditor::Internal {

class CanonicalSymbol {
public:
  CanonicalSymbol(const CPlusPlus::Document::Ptr &document, const CPlusPlus::Snapshot &snapshot);

  auto context() const -> const CPlusPlus::LookupContext&;
  auto getScopeAndExpression(const QTextCursor &cursor, QString *code) -> CPlusPlus::Scope*;
  auto operator()(const QTextCursor &cursor) -> CPlusPlus::Symbol*;
  auto operator()(CPlusPlus::Scope *scope, const QString &code) -> CPlusPlus::Symbol*;

public:
  static auto canonicalSymbol(CPlusPlus::Scope *scope, const QString &code, CPlusPlus::TypeOfExpression &typeOfExpression) -> CPlusPlus::Symbol*;

private:
  CPlusPlus::Document::Ptr m_document;
  CPlusPlus::Snapshot m_snapshot;
  CPlusPlus::TypeOfExpression m_typeOfExpression;
};

} // namespace CppEditor::Internal
