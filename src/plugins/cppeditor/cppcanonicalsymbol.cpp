// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcanonicalsymbol.hpp"

#include "cpptoolsreuse.hpp"

#include <cplusplus/ExpressionUnderCursor.h>

#include <utils/textutils.hpp>

#include <QTextCursor>
#include <QTextDocument>

using namespace CPlusPlus;

namespace CppEditor::Internal {

CanonicalSymbol::CanonicalSymbol(const Document::Ptr &document, const Snapshot &snapshot) : m_document(document), m_snapshot(snapshot)
{
  m_typeOfExpression.init(document, snapshot);
  m_typeOfExpression.setExpandTemplates(true);
}

auto CanonicalSymbol::context() const -> const LookupContext&
{
  return m_typeOfExpression.context();
}

auto CanonicalSymbol::getScopeAndExpression(const QTextCursor &cursor, QString *code) -> Scope*
{
  if (!m_document)
    return nullptr;

  auto tc = cursor;
  int line, column;
  Utils::Text::convertPosition(cursor.document(), tc.position(), &line, &column);

  auto pos = tc.position();
  auto textDocument = cursor.document();
  if (!isValidIdentifierChar(textDocument->characterAt(pos)))
    if (!(pos > 0 && isValidIdentifierChar(textDocument->characterAt(pos - 1))))
      return nullptr;

  while (isValidIdentifierChar(textDocument->characterAt(pos)))
    ++pos;
  tc.setPosition(pos);

  ExpressionUnderCursor expressionUnderCursor(m_document->languageFeatures());
  *code = expressionUnderCursor(tc);
  return m_document->scopeAt(line, column - 1);
}

auto CanonicalSymbol::operator()(const QTextCursor &cursor) -> Symbol*
{
  QString code;

  if (Scope *scope = getScopeAndExpression(cursor, &code))
    return operator()(scope, code);

  return nullptr;
}

auto CanonicalSymbol::operator()(Scope *scope, const QString &code) -> Symbol*
{
  return canonicalSymbol(scope, code, m_typeOfExpression);
}

auto CanonicalSymbol::canonicalSymbol(Scope *scope, const QString &code, TypeOfExpression &typeOfExpression) -> Symbol*
{
  const QList<LookupItem> results = typeOfExpression(code.toUtf8(), scope, TypeOfExpression::Preprocess);

  for (int i = results.size() - 1; i != -1; --i) {
    const LookupItem &r = results.at(i);
    Symbol *decl = r.declaration();

    if (!(decl && decl->enclosingScope()))
      break;

    if (Class *classScope = r.declaration()->enclosingScope()->asClass()) {
      const Identifier *declId = decl->identifier();
      const Identifier *classId = classScope->identifier();

      if (classId && classId->match(declId))
        continue; // skip it, it's a ctor or a dtor.

      if (Function *funTy = r.declaration()->type()->asFunctionType()) {
        if (funTy->isVirtual())
          return r.declaration();
      }
    }
  }

  for (const auto &r : results) {
    if (r.declaration())
      return r.declaration();
  }

  return nullptr;
}

} // namespace CppEditor::Internal
