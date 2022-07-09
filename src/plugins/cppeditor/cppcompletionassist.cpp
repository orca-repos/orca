// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcompletionassist.hpp"

#include "builtineditordocumentparser.hpp"
#include "cppdoxygen.hpp"
#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"
#include "editordocumenthandle.hpp"

#include <core/icore.hpp>
#include <cppeditor/cppeditorconstants.hpp>
#include <texteditor/codeassist/assistproposalitem.hpp>
#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/ifunctionhintproposalmodel.hpp>
#include <texteditor/codeassist/functionhintproposal.hpp>
#include <texteditor/snippets/snippet.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/completionsettings.hpp>

#include <utils/textutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>

#include <cplusplus/BackwardsScanner.h>
#include <cplusplus/CppRewriter.h>
#include <cplusplus/ExpressionUnderCursor.h>
#include <cplusplus/MatchingText.h>
#include <cplusplus/Overview.h>
#include <cplusplus/ResolveExpression.h>

#include <QDirIterator>
#include <QLatin1String>
#include <QTextCursor>
#include <QTextDocument>
#include <QIcon>

using namespace CPlusPlus;
using namespace CppEditor;
using namespace TextEditor;

namespace CppEditor::Internal {

struct CompleteFunctionDeclaration {
  explicit CompleteFunctionDeclaration(Function *f = nullptr) : function(f) {}

  Function *function;
};

// ---------------------
// CppAssistProposalItem
// ---------------------
class CppAssistProposalItem final : public AssistProposalItem {
public:
  ~CppAssistProposalItem() noexcept override = default;

  auto prematurelyApplies(const QChar &c) const -> bool override;
  auto applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void override;
  auto isOverloaded() const -> bool { return m_isOverloaded; }
  auto markAsOverloaded() -> void { m_isOverloaded = true; }
  auto keepCompletionOperator(unsigned compOp) -> void { m_completionOperator = compOp; }
  auto keepTypeOfExpression(const QSharedPointer<TypeOfExpression> &typeOfExp) -> void { m_typeOfExpression = typeOfExp; }

  auto isKeyword() const -> bool final
  {
    return m_isKeyword;
  }

  auto setIsKeyword(bool isKeyword) -> void { m_isKeyword = isKeyword; }
  auto hash() const -> quint64 override;

private:
  QSharedPointer<TypeOfExpression> m_typeOfExpression;
  unsigned m_completionOperator = T_EOF_SYMBOL;
  mutable QChar m_typedChar;
  bool m_isOverloaded = false;
  bool m_isKeyword = false;
};

} // CppEditor::Internal

Q_DECLARE_METATYPE(CppEditor::Internal::CompleteFunctionDeclaration)

namespace CppEditor::Internal {

auto CppAssistProposalModel::isSortable(const QString &prefix) const -> bool
{
  if (m_completionOperator != T_EOF_SYMBOL)
    return true;

  return !prefix.isEmpty();
}

auto CppAssistProposalModel::proposalItem(int index) const -> AssistProposalItemInterface*
{
  auto item = GenericProposalModel::proposalItem(index);
  if (!item->isSnippet()) {
    auto cppItem = static_cast<CppAssistProposalItem*>(item);
    cppItem->keepCompletionOperator(m_completionOperator);
    cppItem->keepTypeOfExpression(m_typeOfExpression);
  }
  return item;
}

auto CppAssistProposalItem::prematurelyApplies(const QChar &typedChar) const -> bool
{
  if (m_completionOperator == T_SIGNAL || m_completionOperator == T_SLOT) {
    if (typedChar == QLatin1Char('(') || typedChar == QLatin1Char(',')) {
      m_typedChar = typedChar;
      return true;
    }
  } else if (m_completionOperator == T_STRING_LITERAL || m_completionOperator == T_ANGLE_STRING_LITERAL) {
    if (typedChar == QLatin1Char('/') && text().endsWith(QLatin1Char('/'))) {
      m_typedChar = typedChar;
      return true;
    }
  } else if (data().value<Symbol*>()) {
    if (typedChar == QLatin1Char(':') || typedChar == QLatin1Char(';') || typedChar == QLatin1Char('.') || typedChar == QLatin1Char(',') || typedChar == QLatin1Char('(')) {
      m_typedChar = typedChar;
      return true;
    }
  } else if (data().canConvert<CompleteFunctionDeclaration>()) {
    if (typedChar == QLatin1Char('(')) {
      m_typedChar = typedChar;
      return true;
    }
  }

  return false;
}

static auto isDereferenced(TextDocumentManipulatorInterface &manipulator, int basePosition) -> bool
{
  auto cursor = manipulator.textCursorAt(basePosition);
  cursor.setPosition(basePosition);

  BackwardsScanner scanner(cursor, LanguageFeatures());
  for (int pos = scanner.startToken() - 1; pos >= 0; pos--) {
    switch (scanner[pos].kind()) {
    case T_COLON_COLON:
    case T_IDENTIFIER:
      //Ignore scope specifiers
      break;

    case T_AMPER:
      return true;
    default:
      return false;
    }
  }
  return false;
}

auto CppAssistProposalItem::hash() const -> quint64
{
  if (data().canConvert<Symbol*>())
    return quint64(data().value<Symbol*>()->index());
  else if (data().canConvert<CompleteFunctionDeclaration>())
    return quint64(data().value<CompleteFunctionDeclaration>().function->index());

  return 0;
}

auto CppAssistProposalItem::applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  Symbol *symbol = nullptr;

  if (data().isValid())
    symbol = data().value<Symbol*>();

  QString toInsert;
  QString extraChars;
  auto extraLength = 0;
  auto cursorOffset = 0;
  auto setAutoCompleteSkipPos = false;

  auto autoParenthesesEnabled = true;

  if (m_completionOperator == T_SIGNAL || m_completionOperator == T_SLOT) {
    toInsert = text();
    extraChars += QLatin1Char(')');

    if (m_typedChar == QLatin1Char('(')) // Eat the opening parenthesis
      m_typedChar = QChar();
  } else if (m_completionOperator == T_STRING_LITERAL || m_completionOperator == T_ANGLE_STRING_LITERAL) {
    toInsert = text();
    if (!toInsert.endsWith(QLatin1Char('/'))) {
      extraChars += QLatin1Char((m_completionOperator == T_ANGLE_STRING_LITERAL) ? '>' : '"');
    } else {
      if (m_typedChar == QLatin1Char('/')) // Eat the slash
        m_typedChar = QChar();
    }
  } else {
    toInsert = text();

    const auto &completionSettings = TextEditorSettings::completionSettings();
    const auto autoInsertBrackets = completionSettings.m_autoInsertBrackets;

    if (autoInsertBrackets && symbol && symbol->type()) {
      if (Function *function = symbol->type()->asFunctionType()) {
        // If the member is a function, automatically place the opening parenthesis,
        // except when it might take template parameters.
        if (!function->hasReturnType() && (function->unqualifiedName() && !function->unqualifiedName()->isDestructorNameId())) {
          // Don't insert any magic, since the user might have just wanted to select the class

          /// ### port me
          #if 0
                } else if (function->templateParameterCount() != 0 && typedChar != QLatin1Char('(')) {
                    // If there are no arguments, then we need the template specification
                    if (function->argumentCount() == 0)
                        extraChars += QLatin1Char('<');
          #endif
        } else if (!isDereferenced(manipulator, basePosition) && !function->isAmbiguous()) {
          // When the user typed the opening parenthesis, he'll likely also type the closing one,
          // in which case it would be annoying if we put the cursor after the already automatically
          // inserted closing parenthesis.
          const auto skipClosingParenthesis = m_typedChar != QLatin1Char('(');

          if (completionSettings.m_spaceAfterFunctionName)
            extraChars += QLatin1Char(' ');
          extraChars += QLatin1Char('(');
          if (m_typedChar == QLatin1Char('('))
            m_typedChar = QChar();

          // If the function doesn't return anything, automatically place the semicolon,
          // unless we're doing a scope completion (then it might be function definition).
          const auto characterAtCursor = manipulator.characterAt(manipulator.currentPosition());
          bool endWithSemicolon = m_typedChar == QLatin1Char(';') || (function->returnType()->isVoidType() && m_completionOperator != T_COLON_COLON);
          const auto semicolon = m_typedChar.isNull() ? QLatin1Char(';') : m_typedChar;

          if (endWithSemicolon && characterAtCursor == semicolon) {
            endWithSemicolon = false;
            m_typedChar = QChar();
          }

          // If the function takes no arguments, automatically place the closing parenthesis
          if (!isOverloaded() && !function->hasArguments() && skipClosingParenthesis) {
            extraChars += QLatin1Char(')');
            if (endWithSemicolon) {
              extraChars += semicolon;
              m_typedChar = QChar();
            }
          } else if (autoParenthesesEnabled) {
            const auto lookAhead = manipulator.characterAt(manipulator.currentPosition() + 1);
            if (MatchingText::shouldInsertMatchingText(lookAhead)) {
              extraChars += QLatin1Char(')');
              --cursorOffset;
              setAutoCompleteSkipPos = true;
              if (endWithSemicolon) {
                extraChars += semicolon;
                --cursorOffset;
                m_typedChar = QChar();
              }
            }
            // TODO: When an opening parenthesis exists, the "semicolon" should really be
            // inserted after the matching closing parenthesis.
          }
        }
      }
    }

    if (autoInsertBrackets && data().canConvert<CompleteFunctionDeclaration>()) {
      if (m_typedChar == QLatin1Char('('))
        m_typedChar = QChar();

      // everything from the closing parenthesis on are extra chars, to
      // make sure an auto-inserted ")" gets replaced by ") const" if necessary
      int closingParen = toInsert.lastIndexOf(QLatin1Char(')'));
      extraChars = toInsert.mid(closingParen);
      toInsert.truncate(closingParen);
    }
  }

  // Append an unhandled typed character, adjusting cursor offset when it had been adjusted before
  if (!m_typedChar.isNull()) {
    extraChars += m_typedChar;
    if (cursorOffset != 0)
      --cursorOffset;
  }

  // Avoid inserting characters that are already there
  auto currentPosition = manipulator.currentPosition();
  auto cursor = manipulator.textCursorAt(basePosition);
  cursor.movePosition(QTextCursor::EndOfWord);
  const auto textAfterCursor = manipulator.textAt(currentPosition, cursor.position() - currentPosition);
  if (toInsert != textAfterCursor && toInsert.indexOf(textAfterCursor, currentPosition - basePosition) >= 0) {
    currentPosition = cursor.position();
  }

  for (auto i = 0; i < extraChars.length(); ++i) {
    const auto a = extraChars.at(i);
    const auto b = manipulator.characterAt(currentPosition + i);
    if (a == b)
      ++extraLength;
    else
      break;
  }

  toInsert += extraChars;

  // Insert the remainder of the name
  const auto length = currentPosition - basePosition + extraLength;
  manipulator.replace(basePosition, length, toInsert);
  manipulator.setCursorPosition(basePosition + toInsert.length());
  if (cursorOffset)
    manipulator.setCursorPosition(manipulator.currentPosition() + cursorOffset);
  if (setAutoCompleteSkipPos)
    manipulator.setAutoCompleteSkipPosition(manipulator.currentPosition());
}

// --------------------
// CppFunctionHintModel
// --------------------
class CppFunctionHintModel : public IFunctionHintProposalModel {
public:
  CppFunctionHintModel(const QList<Function*> &functionSymbols, const QSharedPointer<TypeOfExpression> &typeOfExp) : m_functionSymbols(functionSymbols), m_currentArg(-1), m_typeOfExpression(typeOfExp) {}

  auto reset() -> void override {}
  auto size() const -> int override { return m_functionSymbols.size(); }
  auto text(int index) const -> QString override;
  auto activeArgument(const QString &prefix) const -> int override;

private:
  QList<Function*> m_functionSymbols;
  mutable int m_currentArg;
  QSharedPointer<TypeOfExpression> m_typeOfExpression;
};

auto CppFunctionHintModel::text(int index) const -> QString
{
  Overview overview;
  overview.showReturnTypes = true;
  overview.showArgumentNames = true;
  overview.markedArgument = m_currentArg + 1;
  auto f = m_functionSymbols.at(index);

  const QString prettyMethod = overview.prettyType(f->type(), f->name());
  const int begin = overview.markedArgumentBegin;
  const int end = overview.markedArgumentEnd;

  QString hintText;
  hintText += prettyMethod.left(begin).toHtmlEscaped();
  hintText += QLatin1String("<b>");
  hintText += prettyMethod.mid(begin, end - begin).toHtmlEscaped();
  hintText += QLatin1String("</b>");
  hintText += prettyMethod.mid(end).toHtmlEscaped();
  return hintText;
}

auto CppFunctionHintModel::activeArgument(const QString &prefix) const -> int
{
  auto argnr = 0;
  auto parcount = 0;
  SimpleLexer tokenize;
  Tokens tokens = tokenize(prefix);
  for (auto i = 0; i < tokens.count(); ++i) {
    const Token &tk = tokens.at(i);
    if (tk.is(T_LPAREN))
      ++parcount;
    else if (tk.is(T_RPAREN))
      --parcount;
    else if (!parcount && tk.is(T_COMMA))
      ++argnr;
  }

  if (parcount < 0)
    return -1;

  if (argnr != m_currentArg)
    m_currentArg = argnr;

  return argnr;
}

// ---------------------------
// InternalCompletionAssistProvider
// ---------------------------

auto InternalCompletionAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new InternalCppCompletionAssistProcessor;
}

auto InternalCompletionAssistProvider::createAssistInterface(const Utils::FilePath &filePath, const TextEditorWidget *textEditorWidget, const LanguageFeatures &languageFeatures, int position, AssistReason reason) const -> AssistInterface*
{
  QTC_ASSERT(textEditorWidget, return nullptr);

  return new CppCompletionAssistInterface(filePath, textEditorWidget, BuiltinEditorDocumentParser::get(filePath.toString()), languageFeatures, position, reason, CppModelManager::instance()->workingCopy());
}

// -----------------
// CppAssistProposal
// -----------------
class CppAssistProposal : public GenericProposal {
public:
  CppAssistProposal(int cursorPos, GenericProposalModelPtr model) : GenericProposal(cursorPos, model), m_replaceDotForArrow(model.staticCast<CppAssistProposalModel>()->m_replaceDotForArrow) {}

  auto isCorrective(TextEditorWidget *) const -> bool override { return m_replaceDotForArrow; }
  auto makeCorrection(TextEditorWidget *editorWidget) -> void override;

private:
  bool m_replaceDotForArrow;
};

auto CppAssistProposal::makeCorrection(TextEditorWidget *editorWidget) -> void
{
  const auto oldPosition = editorWidget->position();
  editorWidget->setCursorPosition(basePosition() - 1);
  editorWidget->replace(1, QLatin1String("->"));
  editorWidget->setCursorPosition(oldPosition + 1);
  moveBasePosition(1);
}

namespace {

class ConvertToCompletionItem : protected NameVisitor {
  // The completion item.
  AssistProposalItem *_item = nullptr;

  // The current symbol.
  Symbol *_symbol = nullptr;

  // The pretty printer.
  Overview overview;

public:
  ConvertToCompletionItem()
  {
    overview.showReturnTypes = true;
    overview.showArgumentNames = true;
  }

  auto operator()(Symbol *symbol) -> AssistProposalItem*
  {
    //using declaration can be qualified
    if (!symbol || !symbol->name() || (symbol->name()->isQualifiedNameId() && !symbol->asUsingDeclaration()))
      return nullptr;

    auto previousItem = switchCompletionItem(nullptr);
    auto previousSymbol = switchSymbol(symbol);
    accept(symbol->unqualifiedName());
    if (_item)
      _item->setData(QVariant::fromValue(symbol));
    (void)switchSymbol(previousSymbol);
    return switchCompletionItem(previousItem);
  }

protected:
  auto switchSymbol(Symbol *symbol) -> Symbol*
  {
    auto previousSymbol = _symbol;
    _symbol = symbol;
    return previousSymbol;
  }

  auto switchCompletionItem(AssistProposalItem *item) -> AssistProposalItem*
  {
    auto previousItem = _item;
    _item = item;
    return previousItem;
  }

  auto newCompletionItem(const Name *name) -> AssistProposalItem*
  {
    AssistProposalItem *item = new CppAssistProposalItem;
    item->setText(overview.prettyName(name));
    return item;
  }

  auto visit(const Identifier *name) -> void override
  {
    _item = newCompletionItem(name);
    if (!_symbol->isScope() || _symbol->isFunction())
      _item->setDetail(overview.prettyType(_symbol->type(), name));
  }

  auto visit(const TemplateNameId *name) -> void override
  {
    _item = newCompletionItem(name);
    _item->setText(QString::fromUtf8(name->identifier()->chars(), name->identifier()->size()));
  }

  auto visit(const DestructorNameId *name) -> void override
  {
    _item = newCompletionItem(name);
  }

  auto visit(const OperatorNameId *name) -> void override
  {
    _item = newCompletionItem(name);
    _item->setDetail(overview.prettyType(_symbol->type(), name));
  }

  auto visit(const ConversionNameId *name) -> void override
  {
    _item = newCompletionItem(name);
  }

  auto visit(const QualifiedNameId *name) -> void override
  {
    _item = newCompletionItem(name->name());
  }
};

auto asClassOrTemplateClassType(FullySpecifiedType ty) -> Class*
{
  if (Class *classTy = ty->asClassType())
    return classTy;
  if (Template *templ = ty->asTemplateType()) {
    if (Symbol *decl = templ->declaration())
      return decl->asClass();
  }
  return nullptr;
}

auto enclosingNonTemplateScope(Symbol *symbol) -> Scope*
{
  if (symbol) {
    if (Scope *scope = symbol->enclosingScope()) {
      if (Template *templ = scope->asTemplate())
        return templ->enclosingScope();
      return scope;
    }
  }
  return nullptr;
}

auto asFunctionOrTemplateFunctionType(FullySpecifiedType ty) -> Function*
{
  if (Function *funTy = ty->asFunctionType())
    return funTy;
  if (Template *templ = ty->asTemplateType()) {
    if (Symbol *decl = templ->declaration())
      return decl->asFunction();
  }
  return nullptr;
}

auto isQPrivateSignal(const Symbol *symbol) -> bool
{
  if (!symbol)
    return false;

  static Identifier qPrivateSignalIdentifier("QPrivateSignal", 14);

  if (FullySpecifiedType type = symbol->type()) {
    if (NamedType *namedType = type->asNamedType()) {
      if (const Name *name = namedType->name()) {
        if (name->match(&qPrivateSignalIdentifier))
          return true;
      }
    }
  }
  return false;
}

auto createQt4SignalOrSlot(CPlusPlus::Function *function, const Overview &overview) -> QString
{
  QString signature;
  signature += Overview().prettyName(function->name());
  signature += QLatin1Char('(');
  for (unsigned i = 0, to = function->argumentCount(); i < to; ++i) {
    Symbol *arg = function->argumentAt(i);
    if (isQPrivateSignal(arg))
      continue;
    if (i != 0)
      signature += QLatin1Char(',');
    signature += overview.prettyType(arg->type());
  }
  signature += QLatin1Char(')');

  const auto normalized = QMetaObject::normalizedSignature(signature.toUtf8());
  return QString::fromUtf8(normalized, normalized.size());
}

auto createQt5SignalOrSlot(CPlusPlus::Function *function, const Overview &overview) -> QString
{
  QString text;
  text += overview.prettyName(function->name());
  return text;
}

/*!
    \class BackwardsEater
    \brief Checks strings and expressions before given position.

    Similar to BackwardsScanner, but also can handle expressions. Ignores whitespace.
*/
class BackwardsEater {
public:
  explicit BackwardsEater(const CppCompletionAssistInterface *assistInterface, int position) : m_position(position), m_assistInterface(assistInterface) { }

  auto isPositionValid() const -> bool
  {
    return m_position >= 0;
  }

  auto eatConnectOpenParenthesis() -> bool
  {
    return eatString(QLatin1String("(")) && eatString(QLatin1String("connect"));
  }

  auto eatExpressionCommaAmpersand() -> bool
  {
    return eatString(QLatin1String("&")) && eatString(QLatin1String(",")) && eatExpression();
  }

  auto eatConnectOpenParenthesisExpressionCommaAmpersandExpressionComma() -> bool
  {
    return eatString(QLatin1String(",")) && eatExpression() && eatExpressionCommaAmpersand() && eatConnectOpenParenthesis();
  }

private:
  auto eatExpression() -> bool
  {
    if (!isPositionValid())
      return false;

    maybeEatWhitespace();

    QTextCursor cursor(m_assistInterface->textDocument());
    cursor.setPosition(m_position + 1);
    ExpressionUnderCursor expressionUnderCursor(m_assistInterface->languageFeatures());
    const QString expression = expressionUnderCursor(cursor);
    if (expression.isEmpty())
      return false;
    m_position = m_position - expression.length();
    return true;
  }

  auto eatString(const QString &string) -> bool
  {
    if (!isPositionValid())
      return false;

    if (string.isEmpty())
      return true;

    maybeEatWhitespace();

    const int stringLength = string.length();
    const auto stringStart = m_position - (stringLength - 1);

    if (stringStart < 0)
      return false;

    if (m_assistInterface->textAt(stringStart, stringLength) == string) {
      m_position = stringStart - 1;
      return true;
    }

    return false;
  }

  auto maybeEatWhitespace() -> void
  {
    while (isPositionValid() && m_assistInterface->characterAt(m_position).isSpace())
      --m_position;
  }

private:
  int m_position;
  const CppCompletionAssistInterface *const m_assistInterface;
};

auto canCompleteConnectSignalAt2ndArgument(const CppCompletionAssistInterface *assistInterface, int startOfExpression) -> bool
{
  BackwardsEater eater(assistInterface, startOfExpression);

  return eater.isPositionValid() && eater.eatExpressionCommaAmpersand() && eater.eatConnectOpenParenthesis();
}

auto canCompleteConnectSignalAt4thArgument(const CppCompletionAssistInterface *assistInterface, int startPosition) -> bool
{
  BackwardsEater eater(assistInterface, startPosition);

  return eater.isPositionValid() && eater.eatExpressionCommaAmpersand() && eater.eatConnectOpenParenthesisExpressionCommaAmpersandExpressionComma();
}

auto canCompleteClassNameAt2ndOr4thConnectArgument(const CppCompletionAssistInterface *assistInterface, int startPosition) -> bool
{
  BackwardsEater eater(assistInterface, startPosition);

  if (!eater.isPositionValid())
    return false;

  return eater.eatConnectOpenParenthesis() || eater.eatConnectOpenParenthesisExpressionCommaAmpersandExpressionComma();
}

auto classOrNamespaceFromLookupItem(const LookupItem &lookupItem, const LookupContext &context) -> ClassOrNamespace*
{
  const Name *name = nullptr;

  if (Symbol *d = lookupItem.declaration()) {
    if (Class *k = d->asClass())
      name = k->name();
  }

  if (!name) {
    FullySpecifiedType type = lookupItem.type().simplified();

    if (PointerType *pointerType = type->asPointerType())
      type = pointerType->elementType().simplified();
    else
      return nullptr; // not a pointer or a reference to a pointer.

    NamedType *namedType = type->asNamedType();
    if (!namedType) // not a class name.
      return nullptr;

    name = namedType->name();
  }

  return name ? context.lookupType(name, lookupItem.scope()) : nullptr;
}

auto classFromLookupItem(const LookupItem &lookupItem, const LookupContext &context) -> Class*
{
  auto b = classOrNamespaceFromLookupItem(lookupItem, context);
  if (!b)
    return nullptr;

  foreach(Symbol *s, b->symbols()) {
    if (Class *klass = s->asClass())
      return klass;
  }
  return nullptr;
}

auto minimalName(Symbol *symbol, Scope *targetScope, const LookupContext &context) -> const Name*
{
  ClassOrNamespace *target = context.lookupType(targetScope);
  if (!target)
    target = context.globalNamespace();
  return LookupContext::minimalName(symbol, target, context.bindings()->control().data());
}

} // Anonymous

// ------------------------------------
// InternalCppCompletionAssistProcessor
// ------------------------------------
InternalCppCompletionAssistProcessor::InternalCppCompletionAssistProcessor() : m_model(new CppAssistProposalModel) {}

InternalCppCompletionAssistProcessor::~InternalCppCompletionAssistProcessor() = default;

auto InternalCppCompletionAssistProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  m_interface.reset(static_cast<const CppCompletionAssistInterface*>(interface));

  if (interface->reason() != ExplicitlyInvoked && !accepts())
    return nullptr;

  auto index = startCompletionHelper();
  if (index != -1) {
    if (m_hintProposal)
      return m_hintProposal;

    return createContentProposal();
  }

  return nullptr;
}

auto InternalCppCompletionAssistProcessor::accepts() const -> bool
{
  const auto pos = m_interface->position();
  unsigned token = T_EOF_SYMBOL;

  const int start = startOfOperator(pos, &token, /*want function call=*/ true);
  if (start != pos) {
    if (token == T_POUND) {
      const int column = pos - m_interface->textDocument()->findBlock(start).position();
      if (column != 1)
        return false;
    }

    return true;
  } else {
    // Trigger completion after n characters of a name have been typed, when not editing an existing name
    auto characterUnderCursor = m_interface->characterAt(pos);

    if (!isValidIdentifierChar(characterUnderCursor)) {
      const auto startOfName = findStartOfName(pos);
      if (pos - startOfName >= TextEditorSettings::completionSettings().m_characterThreshold) {
        const auto firstCharacter = m_interface->characterAt(startOfName);
        if (isValidFirstIdentifierChar(firstCharacter)) {
          return !isInCommentOrString(m_interface.data(), m_interface->languageFeatures());
        }
      }
    }
  }

  return false;
}

auto InternalCppCompletionAssistProcessor::createContentProposal() -> IAssistProposal*
{
  // Duplicates are kept only if they are snippets.
  QSet<QString> processed;
  auto it = m_completions.begin();
  while (it != m_completions.end()) {
    auto item = static_cast<CppAssistProposalItem*>(*it);
    if (!processed.contains(item->text()) || item->isSnippet()) {
      ++it;
      if (!item->isSnippet()) {
        processed.insert(item->text());
        if (!item->isOverloaded()) {
          if (auto symbol = qvariant_cast<Symbol*>(item->data())) {
            if (Function *funTy = symbol->type()->asFunctionType()) {
              if (funTy->hasArguments())
                item->markAsOverloaded();
            }
          }
        }
      }
    } else {
      delete *it;
      it = m_completions.erase(it);
    }
  }

  m_model->loadContent(m_completions);
  return new CppAssistProposal(m_positionForProposal, m_model);
}

auto InternalCppCompletionAssistProcessor::createHintProposal(QList<Function*> functionSymbols) const -> IAssistProposal*
{
  FunctionHintProposalModelPtr model(new CppFunctionHintModel(functionSymbols, m_model->m_typeOfExpression));
  return new FunctionHintProposal(m_positionForProposal, model);
}

auto InternalCppCompletionAssistProcessor::startOfOperator(int positionInDocument, unsigned *kind, bool wantFunctionCall) const -> int
{
  const auto ch = m_interface->characterAt(positionInDocument - 1);
  const auto ch2 = m_interface->characterAt(positionInDocument - 2);
  const auto ch3 = m_interface->characterAt(positionInDocument - 3);

  auto start = positionInDocument - CppCompletionAssistProvider::activationSequenceChar(ch, ch2, ch3, kind, wantFunctionCall,
                                                                                        /*wantQt5SignalSlots*/ true);

  const auto dotAtIncludeCompletionHandler = [this](int &start, unsigned *kind) {
    start = findStartOfName(start);
    const auto ch4 = m_interface->characterAt(start - 1);
    const auto ch5 = m_interface->characterAt(start - 2);
    const auto ch6 = m_interface->characterAt(start - 3);
    start = start - CppCompletionAssistProvider::activationSequenceChar(ch4, ch5, ch6, kind, false, false);
  };

  CppCompletionAssistProcessor::startOfOperator(m_interface->textDocument(), positionInDocument, kind, start, m_interface->languageFeatures(),
                                                /*adjustForQt5SignalSlotCompletion=*/ true, dotAtIncludeCompletionHandler);
  return start;
}

auto InternalCppCompletionAssistProcessor::findStartOfName(int pos) const -> int
{
  if (pos == -1)
    pos = m_interface->position();
  QChar chr;

  // Skip to the start of a name
  do {
    chr = m_interface->characterAt(--pos);
  } while (isValidIdentifierChar(chr));

  return pos + 1;
}

auto InternalCppCompletionAssistProcessor::startCompletionHelper() -> int
{
  if (m_interface->languageFeatures().objCEnabled) {
    if (tryObjCCompletion())
      return m_positionForProposal;
  }

  const auto startOfName = findStartOfName();
  m_positionForProposal = startOfName;
  m_model->m_completionOperator = T_EOF_SYMBOL;

  auto endOfOperator = m_positionForProposal;

  // Skip whitespace preceding this position
  while (m_interface->characterAt(endOfOperator - 1).isSpace())
    --endOfOperator;

  auto endOfExpression = startOfOperator(endOfOperator, &m_model->m_completionOperator,
                                         /*want function call =*/ true);

  if (m_model->m_completionOperator == T_DOXY_COMMENT) {
    for (auto i = 1; i < T_DOXY_LAST_TAG; ++i)
      addCompletionItem(QString::fromLatin1(doxygenTagSpell(i)), Icons::keywordIcon());
    return m_positionForProposal;
  }

  // Pre-processor completion
  if (m_model->m_completionOperator == T_POUND) {
    completePreprocessor();
    m_positionForProposal = startOfName;
    return m_positionForProposal;
  }

  // Include completion
  if (m_model->m_completionOperator == T_STRING_LITERAL || m_model->m_completionOperator == T_ANGLE_STRING_LITERAL || m_model->m_completionOperator == T_SLASH) {

    QTextCursor c(m_interface->textDocument());
    c.setPosition(endOfExpression);
    if (completeInclude(c))
      m_positionForProposal = endOfExpression + 1;
    return m_positionForProposal;
  }

  ExpressionUnderCursor expressionUnderCursor(m_interface->languageFeatures());
  QTextCursor tc(m_interface->textDocument());

  if (m_model->m_completionOperator == T_COMMA) {
    tc.setPosition(endOfExpression);
    const int start = expressionUnderCursor.startOfFunctionCall(tc);
    if (start == -1) {
      m_model->m_completionOperator = T_EOF_SYMBOL;
      return -1;
    }

    endOfExpression = start;
    m_positionForProposal = start + 1;
    m_model->m_completionOperator = T_LPAREN;
  }

  QString expression;
  auto startOfExpression = m_interface->position();
  tc.setPosition(endOfExpression);

  if (m_model->m_completionOperator) {
    expression = expressionUnderCursor(tc);
    startOfExpression = endOfExpression - expression.length();

    if (m_model->m_completionOperator == T_AMPER) {
      // We expect 'expression' to be either "sender" or "receiver" in
      //  "connect(sender, &" or
      //  "connect(otherSender, &Foo::signal1, receiver, &"
      const auto beforeExpression = startOfExpression - 1;
      if (canCompleteClassNameAt2ndOr4thConnectArgument(m_interface.data(), beforeExpression)) {
        m_model->m_completionOperator = CompleteQt5SignalOrSlotClassNameTrigger;
      } else {
        // Ensure global completion
        startOfExpression = endOfExpression = m_positionForProposal;
        expression.clear();
        m_model->m_completionOperator = T_EOF_SYMBOL;
      }
    } else if (m_model->m_completionOperator == T_COLON_COLON) {
      // We expect 'expression' to be "Foo" in
      //  "connect(sender, &Foo::" or
      //  "connect(sender, &Bar::signal1, receiver, &Foo::"
      const auto beforeExpression = startOfExpression - 1;
      if (canCompleteConnectSignalAt2ndArgument(m_interface.data(), beforeExpression))
        m_model->m_completionOperator = CompleteQt5SignalTrigger;
      else if (canCompleteConnectSignalAt4thArgument(m_interface.data(), beforeExpression))
        m_model->m_completionOperator = CompleteQt5SlotTrigger;
    } else if (m_model->m_completionOperator == T_LPAREN) {
      if (expression.endsWith(QLatin1String("SIGNAL"))) {
        m_model->m_completionOperator = T_SIGNAL;
      } else if (expression.endsWith(QLatin1String("SLOT"))) {
        m_model->m_completionOperator = T_SLOT;
      } else if (m_interface->position() != endOfOperator) {
        // We don't want a function completion when the cursor isn't at the opening brace
        expression.clear();
        m_model->m_completionOperator = T_EOF_SYMBOL;
        m_positionForProposal = startOfName;
        startOfExpression = m_interface->position();
      }
    }
  } else if (expression.isEmpty()) {
    while (startOfExpression > 0 && m_interface->characterAt(startOfExpression).isSpace())
      --startOfExpression;
  }

  auto line = 0, column = 0;
  Utils::Text::convertPosition(m_interface->textDocument(), startOfExpression, &line, &column);
  const auto fileName = m_interface->filePath().toString();
  return startCompletionInternal(fileName, line, column - 1, expression, endOfExpression);
}

auto InternalCppCompletionAssistProcessor::tryObjCCompletion() -> bool
{
  auto end = m_interface->position();
  while (m_interface->characterAt(end).isSpace())
    ++end;
  if (m_interface->characterAt(end) != QLatin1Char(']'))
    return false;

  QTextCursor tc(m_interface->textDocument());
  tc.setPosition(end);
  BackwardsScanner tokens(tc, m_interface->languageFeatures());
  if (tokens[tokens.startToken() - 1].isNot(T_RBRACKET))
    return false;

  const int start = tokens.startOfMatchingBrace(tokens.startToken());
  if (start == tokens.startToken())
    return false;

  const int startPos = tokens[start].bytesBegin() + tokens.startPosition();
  const QString expr = m_interface->textAt(startPos, m_interface->position() - startPos);

  Document::Ptr thisDocument = m_interface->snapshot().document(m_interface->filePath());
  if (!thisDocument)
    return false;

  m_model->m_typeOfExpression->init(thisDocument, m_interface->snapshot());

  auto line = 0, column = 0;
  Utils::Text::convertPosition(m_interface->textDocument(), m_interface->position(), &line, &column);
  Scope *scope = thisDocument->scopeAt(line, column - 1);
  if (!scope)
    return false;

  const QList<LookupItem> items = (*m_model->m_typeOfExpression)(expr.toUtf8(), scope);
  LookupContext lookupContext(thisDocument, m_interface->snapshot());

  foreach(const LookupItem &item, items) {
    FullySpecifiedType ty = item.type().simplified();
    if (ty->isPointerType()) {
      ty = ty->asPointerType()->elementType().simplified();

      if (NamedType *namedTy = ty->asNamedType()) {
        ClassOrNamespace *binding = lookupContext.lookupType(namedTy->name(), item.scope());
        completeObjCMsgSend(binding, false);
      }
    } else {
      if (ObjCClass *clazz = ty->asObjCClassType()) {
        ClassOrNamespace *binding = lookupContext.lookupType(clazz->name(), item.scope());
        completeObjCMsgSend(binding, true);
      }
    }
  }

  if (m_completions.isEmpty())
    return false;

  m_positionForProposal = m_interface->position();
  return true;
}

namespace {
enum CompletionOrder {
  // default order is 0
  FunctionArgumentsOrder = 2,
  FunctionLocalsOrder = 2,
  // includes local types
  PublicClassMemberOrder = 1,
  InjectedClassNameOrder = -1,
  MacrosOrder = -2,
  KeywordsOrder = -2
};
}

auto InternalCppCompletionAssistProcessor::addCompletionItem(const QString &text, const QIcon &icon, int order, const QVariant &data) -> void
{
  AssistProposalItem *item = new CppAssistProposalItem;
  item->setText(text);
  item->setIcon(icon);
  item->setOrder(order);
  item->setData(data);
  m_completions.append(item);
}

auto InternalCppCompletionAssistProcessor::addCompletionItem(Symbol *symbol, int order) -> void
{
  ConvertToCompletionItem toCompletionItem;
  auto item = toCompletionItem(symbol);
  if (item) {
    item->setIcon(Icons::iconForSymbol(symbol));
    item->setOrder(order);
    m_completions.append(item);
  }
}

auto InternalCppCompletionAssistProcessor::completeObjCMsgSend(ClassOrNamespace *binding, bool staticClassAccess) -> void
{
  QList<Scope*> memberScopes;
  foreach(Symbol *s, binding->symbols()) {
    if (ObjCClass *c = s->asObjCClass())
      memberScopes.append(c);
  }

  foreach(Scope *scope, memberScopes) {
    for (auto i = 0; i < scope->memberCount(); ++i) {
      Symbol *symbol = scope->memberAt(i);

      if (ObjCMethod *method = symbol->type()->asObjCMethodType()) {
        if (method->isStatic() == staticClassAccess) {
          Overview oo;
          const SelectorNameId *selectorName = method->name()->asSelectorNameId();
          QString text;
          QString data;
          if (selectorName->hasArguments()) {
            for (auto i = 0; i < selectorName->nameCount(); ++i) {
              if (i > 0)
                text += QLatin1Char(' ');
              Symbol *arg = method->argumentAt(i);
              text += QString::fromUtf8(selectorName->nameAt(i)->identifier()->chars());
              text += QLatin1Char(':');
              text += Snippet::kVariableDelimiter;
              text += QLatin1Char('(');
              text += oo.prettyType(arg->type());
              text += QLatin1Char(')');
              text += oo.prettyName(arg->name());
              text += Snippet::kVariableDelimiter;
            }
          } else {
            text = QString::fromUtf8(selectorName->identifier()->chars());
          }
          data = text;

          if (!text.isEmpty())
            addCompletionItem(text, QIcon(), 0, QVariant::fromValue(data));
        }
      }
    }
  }
}

auto InternalCppCompletionAssistProcessor::completeInclude(const QTextCursor &cursor) -> bool
{
  QString directoryPrefix;
  if (m_model->m_completionOperator == T_SLASH) {
    auto c = cursor;
    c.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    auto sel = c.selectedText();
    int startCharPos = sel.indexOf(QLatin1Char('"'));
    if (startCharPos == -1) {
      startCharPos = sel.indexOf(QLatin1Char('<'));
      m_model->m_completionOperator = T_ANGLE_STRING_LITERAL;
    } else {
      m_model->m_completionOperator = T_STRING_LITERAL;
    }
    if (startCharPos != -1)
      directoryPrefix = sel.mid(startCharPos + 1, sel.length() - 1);
  }

  // Make completion for all relevant includes
  auto headerPaths = m_interface->headerPaths();
  const auto currentFilePath = ProjectExplorer::HeaderPath::makeUser(m_interface->filePath().toFileInfo().path());
  if (!headerPaths.contains(currentFilePath))
    headerPaths.append(currentFilePath);

  const auto suffixes = Utils::mimeTypeForName(QLatin1String("text/x-c++hdr")).suffixes();

  foreach(const ProjectExplorer::HeaderPath &headerPath, headerPaths) {
    auto realPath = headerPath.path;
    if (!directoryPrefix.isEmpty()) {
      realPath += QLatin1Char('/');
      realPath += directoryPrefix;
      if (headerPath.type == ProjectExplorer::HeaderPathType::Framework)
        realPath += QLatin1String(".framework/Headers");
    }
    completeInclude(realPath, suffixes);
  }

  return !m_completions.isEmpty();
}

auto InternalCppCompletionAssistProcessor::completeInclude(const QString &realPath, const QStringList &suffixes) -> void
{
  QDirIterator i(realPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  while (i.hasNext()) {
    const auto fileName = i.next();
    const auto fileInfo = i.fileInfo();
    const auto suffix = fileInfo.suffix();
    if (suffix.isEmpty() || suffixes.contains(suffix)) {
      auto text = fileName.mid(realPath.length() + 1);
      if (fileInfo.isDir())
        text += QLatin1Char('/');
      addCompletionItem(text, Icons::keywordIcon());
    }
  }
}

auto InternalCppCompletionAssistProcessor::completePreprocessor() -> void
{
  foreach(const QString &preprocessorCompletion, preprocessorCompletions())
    addCompletionItem(preprocessorCompletion);

  if (objcKeywordsWanted())
    addCompletionItem(QLatin1String("import"));
}

auto InternalCppCompletionAssistProcessor::objcKeywordsWanted() const -> bool
{
  if (!m_interface->languageFeatures().objCEnabled)
    return false;

  const auto mt = Utils::mimeTypeForFile(m_interface->filePath());
  return mt.matchesName(QLatin1String(CppEditor::Constants::OBJECTIVE_C_SOURCE_MIMETYPE)) || mt.matchesName(QLatin1String(CppEditor::Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE));
}

auto InternalCppCompletionAssistProcessor::startCompletionInternal(const QString &fileName, int line, int positionInBlock, const QString &expr, int endOfExpression) -> int
{
  auto expression = expr.trimmed();

  Document::Ptr thisDocument = m_interface->snapshot().document(fileName);
  if (!thisDocument)
    return -1;

  m_model->m_typeOfExpression->init(thisDocument, m_interface->snapshot());

  Scope *scope = thisDocument->scopeAt(line, positionInBlock);
  QTC_ASSERT(scope, return -1);

  if (expression.isEmpty()) {
    if (m_model->m_completionOperator == T_EOF_SYMBOL || m_model->m_completionOperator == T_COLON_COLON) {
      (void)(*m_model->m_typeOfExpression)(expression.toUtf8(), scope);
      return globalCompletion(scope) ? m_positionForProposal : -1;
    }

    if (m_model->m_completionOperator == T_SIGNAL || m_model->m_completionOperator == T_SLOT) {
      // Apply signal/slot completion on 'this'
      expression = QLatin1String("this");
    }
  }

  auto utf8Exp = expression.toUtf8();
  QList<LookupItem> results = (*m_model->m_typeOfExpression)(utf8Exp, scope, TypeOfExpression::Preprocess);

  if (results.isEmpty()) {
    if (m_model->m_completionOperator == T_SIGNAL || m_model->m_completionOperator == T_SLOT) {
      if (!(expression.isEmpty() || expression == QLatin1String("this"))) {
        expression = QLatin1String("this");
        results = (*m_model->m_typeOfExpression)(utf8Exp, scope);
      }

      if (results.isEmpty())
        return -1;

    } else if (m_model->m_completionOperator == T_LPAREN) {
      // Find the expression that precedes the current name
      auto index = endOfExpression;
      while (m_interface->characterAt(index - 1).isSpace())
        --index;
      index = findStartOfName(index);

      QTextCursor tc(m_interface->textDocument());
      tc.setPosition(index);

      ExpressionUnderCursor expressionUnderCursor(m_interface->languageFeatures());
      const QString baseExpression = expressionUnderCursor(tc);

      // Resolve the type of this expression
      const QList<LookupItem> results = (*m_model->m_typeOfExpression)(baseExpression.toUtf8(), scope, TypeOfExpression::Preprocess);

      // If it's a class, add completions for the constructors
      foreach(const LookupItem &result, results) {
        if (result.type()->isClassType()) {
          if (completeConstructorOrFunction(results, endOfExpression, true))
            return m_positionForProposal;

          break;
        }
      }
      return -1;

    } else if (m_model->m_completionOperator == CompleteQt5SignalOrSlotClassNameTrigger) {
      // Fallback to global completion if we could not lookup sender/receiver object.
      return globalCompletion(scope) ? m_positionForProposal : -1;

    } else {
      return -1; // nothing to do.
    }
  }

  switch (m_model->m_completionOperator) {
  case T_LPAREN:
    if (completeConstructorOrFunction(results, endOfExpression, false))
      return m_positionForProposal;
    break;

  case T_DOT:
  case T_ARROW:
    if (completeMember(results))
      return m_positionForProposal;
    break;

  case T_COLON_COLON:
    if (completeScope(results))
      return m_positionForProposal;
    break;

  case T_SIGNAL:
    if (completeQtMethod(results, CompleteQt4Signals))
      return m_positionForProposal;
    break;

  case T_SLOT:
    if (completeQtMethod(results, CompleteQt4Slots))
      return m_positionForProposal;
    break;

  case CompleteQt5SignalOrSlotClassNameTrigger:
    if (completeQtMethodClassName(results, scope) || globalCompletion(scope))
      return m_positionForProposal;
    break;

  case CompleteQt5SignalTrigger:
    // Fallback to scope completion if "X::" is a namespace and not a class.
    if (completeQtMethod(results, CompleteQt5Signals) || completeScope(results))
      return m_positionForProposal;
    break;

  case CompleteQt5SlotTrigger:
    // Fallback to scope completion if "X::" is a namespace and not a class.
    if (completeQtMethod(results, CompleteQt5Slots) || completeScope(results))
      return m_positionForProposal;
    break;

  default:
    break;
  } // end of switch

  // nothing to do.
  return -1;
}

auto InternalCppCompletionAssistProcessor::globalCompletion(Scope *currentScope) -> bool
{
  const LookupContext &context = m_model->m_typeOfExpression->context();

  if (m_model->m_completionOperator == T_COLON_COLON) {
    completeNamespace(context.globalNamespace());
    return !m_completions.isEmpty();
  }

  QList<ClassOrNamespace*> usingBindings;
  ClassOrNamespace *currentBinding = nullptr;

  for (Scope *scope = currentScope; scope; scope = scope->enclosingScope()) {
    if (Block *block = scope->asBlock()) {
      if (ClassOrNamespace *binding = context.lookupType(scope)) {
        for (int i = 0; i < scope->memberCount(); ++i) {
          Symbol *member = scope->memberAt(i);
          if (member->isEnum()) {
            if (ClassOrNamespace *b = binding->findBlock(block))
              completeNamespace(b);
          }
          if (!member->name())
            continue;
          if (UsingNamespaceDirective *u = member->asUsingNamespaceDirective()) {
            if (ClassOrNamespace *b = binding->lookupType(u->name()))
              usingBindings.append(b);
          } else if (Class *c = member->asClass()) {
            if (c->name()->isAnonymousNameId()) {
              if (ClassOrNamespace *b = binding->findBlock(block))
                completeClass(b);
            }
          }
        }
      }
    } else if (scope->isFunction() || scope->isClass() || scope->isNamespace()) {
      currentBinding = context.lookupType(scope);
      break;
    }
  }

  for (Scope *scope = currentScope; scope; scope = scope->enclosingScope()) {
    if (scope->isBlock()) {
      for (int i = 0; i < scope->memberCount(); ++i)
        addCompletionItem(scope->memberAt(i), FunctionLocalsOrder);
    } else if (Function *fun = scope->asFunction()) {
      for (int i = 0, argc = fun->argumentCount(); i < argc; ++i)
        addCompletionItem(fun->argumentAt(i), FunctionArgumentsOrder);
    } else if (Template *templ = scope->asTemplate()) {
      for (int i = 0, argc = templ->templateParameterCount(); i < argc; ++i)
        addCompletionItem(templ->templateParameterAt(i), FunctionArgumentsOrder);
      break;
    }
  }

  QSet<ClassOrNamespace*> processed;
  for (; currentBinding; currentBinding = currentBinding->parent()) {
    if (processed.contains(currentBinding))
      break;
    processed.insert(currentBinding);

    foreach(ClassOrNamespace* u, currentBinding->usings())
      usingBindings.append(u);

    const QList<Symbol*> symbols = currentBinding->symbols();

    if (!symbols.isEmpty()) {
      if (symbols.first()->isClass())
        completeClass(currentBinding);
      else
        completeNamespace(currentBinding);
    }
  }

  foreach(ClassOrNamespace *b, usingBindings)
    completeNamespace(b);

  addKeywords();
  addMacros(CppModelManager::configurationFileName(), context.snapshot());
  addMacros(context.thisDocument()->fileName(), context.snapshot());
  addSnippets();
  return !m_completions.isEmpty();
}

auto InternalCppCompletionAssistProcessor::addKeywordCompletionItem(const QString &text) -> void
{
  auto item = new CppAssistProposalItem;
  item->setText(text);
  item->setIcon(Icons::keywordIcon());
  item->setOrder(KeywordsOrder);
  item->setIsKeyword(true);
  m_completions.append(item);
}

auto InternalCppCompletionAssistProcessor::completeMember(const QList<LookupItem> &baseResults) -> bool
{
  const LookupContext &context = m_model->m_typeOfExpression->context();

  if (baseResults.isEmpty())
    return false;

  ResolveExpression resolveExpression(context);

  bool *replaceDotForArrow = nullptr;
  if (!m_interface->languageFeatures().objCEnabled)
    replaceDotForArrow = &m_model->m_replaceDotForArrow;

  if (ClassOrNamespace *binding = resolveExpression.baseExpression(baseResults, m_model->m_completionOperator, replaceDotForArrow)) {
    if (binding)
      completeClass(binding, /*static lookup = */ true);

    return !m_completions.isEmpty();
  }

  return false;
}

auto InternalCppCompletionAssistProcessor::completeScope(const QList<LookupItem> &results) -> bool
{
  const LookupContext &context = m_model->m_typeOfExpression->context();
  if (results.isEmpty())
    return false;

  foreach(const LookupItem &result, results) {
    FullySpecifiedType ty = result.type();
    Scope *scope = result.scope();

    if (NamedType *namedTy = ty->asNamedType()) {
      if (ClassOrNamespace *b = context.lookupType(namedTy->name(), scope)) {
        completeClass(b);
        break;
      }

    } else if (Class *classTy = ty->asClassType()) {
      if (ClassOrNamespace *b = context.lookupType(classTy)) {
        completeClass(b);
        break;
      }

      // it can be class defined inside a block
      if (classTy->enclosingScope()->isBlock()) {
        if (ClassOrNamespace *b = context.lookupType(classTy->name(), classTy->enclosingScope())) {
          completeClass(b);
          break;
        }
      }

    } else if (Namespace *nsTy = ty->asNamespaceType()) {
      if (ClassOrNamespace *b = context.lookupType(nsTy)) {
        completeNamespace(b);
        break;
      }

    } else if (Template *templ = ty->asTemplateType()) {
      if (!result.binding())
        continue;
      if (ClassOrNamespace *b = result.binding()->lookupType(templ->name())) {
        completeClass(b);
        break;
      }

    } else if (Enum *e = ty->asEnumType()) {
      // it can be class defined inside a block
      if (e->enclosingScope()->isBlock()) {
        if (ClassOrNamespace *b = context.lookupType(e)) {
          Block *block = e->enclosingScope()->asBlock();
          if (ClassOrNamespace *bb = b->findBlock(block)) {
            completeNamespace(bb);
            break;
          }
        }
      }

      if (ClassOrNamespace *b = context.lookupType(e)) {
        completeNamespace(b);
        break;
      }

    }
  }

  return !m_completions.isEmpty();
}

auto InternalCppCompletionAssistProcessor::completeNamespace(ClassOrNamespace *b) -> void
{
  QSet<ClassOrNamespace*> bindingsVisited;
  QList<ClassOrNamespace*> bindingsToVisit;
  bindingsToVisit.append(b);

  while (!bindingsToVisit.isEmpty()) {
    auto binding = bindingsToVisit.takeFirst();
    if (!binding || bindingsVisited.contains(binding))
      continue;

    bindingsVisited.insert(binding);
    bindingsToVisit += binding->usings();

    QList<Scope*> scopesToVisit;
    QSet<Scope*> scopesVisited;

    foreach(Symbol *bb, binding->symbols()) {
      if (Scope *scope = bb->asScope())
        scopesToVisit.append(scope);
    }

    foreach(Enum *e, binding->unscopedEnums())
      scopesToVisit.append(e);

    while (!scopesToVisit.isEmpty()) {
      Scope *scope = scopesToVisit.takeFirst();
      if (!scope || scopesVisited.contains(scope))
        continue;

      scopesVisited.insert(scope);

      for (Scope::iterator it = scope->memberBegin(); it != scope->memberEnd(); ++it) {
        Symbol *member = *it;
        addCompletionItem(member);
      }
    }
  }
}

auto InternalCppCompletionAssistProcessor::completeClass(ClassOrNamespace *b, bool staticLookup) -> void
{
  QSet<ClassOrNamespace*> bindingsVisited;
  QList<ClassOrNamespace*> bindingsToVisit;
  bindingsToVisit.append(b);

  while (!bindingsToVisit.isEmpty()) {
    auto binding = bindingsToVisit.takeFirst();
    if (!binding || bindingsVisited.contains(binding))
      continue;

    bindingsVisited.insert(binding);
    bindingsToVisit += binding->usings();

    QList<Scope*> scopesToVisit;
    QSet<Scope*> scopesVisited;

    foreach(Symbol *bb, binding->symbols()) {
      if (Class *k = bb->asClass())
        scopesToVisit.append(k);
      else if (Block *b = bb->asBlock())
        scopesToVisit.append(b);
    }

    foreach(Enum *e, binding->unscopedEnums())
      scopesToVisit.append(e);

    while (!scopesToVisit.isEmpty()) {
      Scope *scope = scopesToVisit.takeFirst();
      if (!scope || scopesVisited.contains(scope))
        continue;

      scopesVisited.insert(scope);

      if (staticLookup)
        addCompletionItem(scope, InjectedClassNameOrder); // add a completion item for the injected class name.

      addClassMembersToCompletion(scope, staticLookup);
    }
  }
}

auto InternalCppCompletionAssistProcessor::addClassMembersToCompletion(Scope *scope, bool staticLookup) -> void
{
  if (!scope)
    return;

  std::set<Class*> nestedAnonymouses;

  for (Scope::iterator it = scope->memberBegin(); it != scope->memberEnd(); ++it) {
    Symbol *member = *it;
    if (member->isFriend() || member->isQtPropertyDeclaration() || member->isQtEnum()) {
      continue;
    } else if (!staticLookup && (member->isTypedef() || member->isEnum() || member->isClass())) {
      continue;
    } else if (member->isClass() && member->name()->isAnonymousNameId()) {
      nestedAnonymouses.insert(member->asClass());
    } else if (member->isDeclaration()) {
      Class *declTypeAsClass = member->asDeclaration()->type()->asClassType();
      if (declTypeAsClass && declTypeAsClass->name()->isAnonymousNameId())
        nestedAnonymouses.erase(declTypeAsClass);
    }

    if (member->isPublic())
      addCompletionItem(member, PublicClassMemberOrder);
    else
      addCompletionItem(member);
  }
  for (Class *klass : nestedAnonymouses)
    addClassMembersToCompletion(klass, staticLookup);
}

auto InternalCppCompletionAssistProcessor::completeQtMethod(const QList<LookupItem> &results, CompleteQtMethodMode type) -> bool
{
  if (results.isEmpty())
    return false;

  const LookupContext &context = m_model->m_typeOfExpression->context();

  ConvertToCompletionItem toCompletionItem;
  Overview o;
  o.showReturnTypes = false;
  o.showArgumentNames = false;
  o.showFunctionSignatures = true;

  QSet<QString> signatures;
  foreach(const LookupItem &lookupItem, results) {
    ClassOrNamespace *b = classOrNamespaceFromLookupItem(lookupItem, context);
    if (!b)
      continue;

    QList<ClassOrNamespace*> todo;
    QSet<ClassOrNamespace*> processed;
    QList<Scope*> scopes;
    todo.append(b);
    while (!todo.isEmpty()) {
      auto binding = todo.takeLast();
      if (!processed.contains(binding)) {
        processed.insert(binding);

        foreach(Symbol *s, binding->symbols()) if (Class *clazz = s->asClass())
          scopes.append(clazz);

        todo.append(binding->usings());
      }
    }

    const auto wantSignals = type == CompleteQt4Signals || type == CompleteQt5Signals;
    const auto wantQt5SignalOrSlot = type == CompleteQt5Signals || type == CompleteQt5Slots;
    foreach(Scope *scope, scopes) {
      Class *klass = scope->asClass();
      if (!klass)
        continue;

      for (auto i = 0; i < scope->memberCount(); ++i) {
        Symbol *member = scope->memberAt(i);
        Function *fun = member->type()->asFunctionType();
        if (!fun || fun->isGenerated())
          continue;
        if (wantSignals && !fun->isSignal())
          continue;
        else if (!wantSignals && type == CompleteQt4Slots && !fun->isSlot())
          continue;

        int count = fun->argumentCount();
        while (true) {
          const QString completionText = wantQt5SignalOrSlot ? createQt5SignalOrSlot(fun, o) : createQt4SignalOrSlot(fun, o);

          if (!signatures.contains(completionText)) {
            AssistProposalItem *ci = toCompletionItem(fun);
            if (!ci)
              break;
            signatures.insert(completionText);
            ci->setText(completionText); // fix the completion item.
            ci->setIcon(Icons::iconForSymbol(fun));
            if (wantQt5SignalOrSlot && fun->isSlot())
              ci->setOrder(1);
            m_completions.append(ci);
          }

          if (count && fun->argumentAt(count - 1)->asArgument()->hasInitializer())
            --count;
          else
            break;
        }
      }
    }
  }

  return !m_completions.isEmpty();
}

auto InternalCppCompletionAssistProcessor::completeQtMethodClassName(const QList<LookupItem> &results, Scope *cursorScope) -> bool
{
  QTC_ASSERT(cursorScope, return false);

  if (results.isEmpty())
    return false;

  const LookupContext &context = m_model->m_typeOfExpression->context();
  const QIcon classIcon = Utils::CodeModelIcon::iconForType(Utils::CodeModelIcon::Class);
  Overview overview;

  foreach(const LookupItem &lookupItem, results) {
    Class *klass = classFromLookupItem(lookupItem, context);
    if (!klass)
      continue;
    const Name *name = minimalName(klass, cursorScope, context);
    QTC_ASSERT(name, continue);

    addCompletionItem(overview.prettyName(name), classIcon);
    break;
  }

  return !m_completions.isEmpty();
}

auto InternalCppCompletionAssistProcessor::addKeywords() -> void
{
  int keywordLimit = T_FIRST_OBJC_AT_KEYWORD;
  if (objcKeywordsWanted())
    keywordLimit = T_LAST_OBJC_AT_KEYWORD + 1;

  // keyword completion items.
  for (int i = T_FIRST_KEYWORD; i < keywordLimit; ++i)
    addKeywordCompletionItem(QLatin1String(Token::name(i)));

  // primitive type completion items.
  for (int i = T_FIRST_PRIMITIVE; i <= T_LAST_PRIMITIVE; ++i)
    addKeywordCompletionItem(QLatin1String(Token::name(i)));

  // "Identifiers with special meaning"
  if (m_interface->languageFeatures().cxx11Enabled) {
    addKeywordCompletionItem(QLatin1String("override"));
    addKeywordCompletionItem(QLatin1String("final"));
  }
}

auto InternalCppCompletionAssistProcessor::addMacros(const QString &fileName, const Snapshot &snapshot) -> void
{
  QSet<QString> processed;
  QSet<QString> definedMacros;

  addMacros_helper(snapshot, fileName, &processed, &definedMacros);

  foreach(const QString &macroName, definedMacros)
    addCompletionItem(macroName, Icons::macroIcon(), MacrosOrder);
}

auto InternalCppCompletionAssistProcessor::addMacros_helper(const Snapshot &snapshot, const QString &fileName, QSet<QString> *processed, QSet<QString> *definedMacros) -> void
{
  Document::Ptr doc = snapshot.document(fileName);

  if (!doc || processed->contains(doc->fileName()))
    return;

  processed->insert(doc->fileName());

  foreach(const Document::Include &i, doc->resolvedIncludes())
    addMacros_helper(snapshot, i.resolvedFileName(), processed, definedMacros);

  foreach(const CPlusPlus::Macro &macro, doc->definedMacros()) {
    const QString macroName = macro.nameToQString();
    if (!macro.isHidden())
      definedMacros->insert(macroName);
    else
      definedMacros->remove(macroName);
  }
}

auto InternalCppCompletionAssistProcessor::completeConstructorOrFunction(const QList<LookupItem> &results, int endOfExpression, bool toolTipOnly) -> bool
{
  const LookupContext &context = m_model->m_typeOfExpression->context();
  QList<Function*> functions;

  foreach(const LookupItem &result, results) {
    FullySpecifiedType exprTy = result.type().simplified();

    if (Class *klass = asClassOrTemplateClassType(exprTy)) {
      const Name *className = klass->name();
      if (!className)
        continue; // nothing to do for anonymous classes.

      for (auto i = 0; i < klass->memberCount(); ++i) {
        Symbol *member = klass->memberAt(i);
        const Name *memberName = member->name();

        if (!memberName)
          continue; // skip anonymous member.

        else if (memberName->isQualifiedNameId())
          continue; // skip

        if (Function *funTy = member->type()->asFunctionType()) {
          if (memberName->match(className)) {
            // it's a ctor.
            functions.append(funTy);
          }
        }
      }

      break;
    }
  }

  if (functions.isEmpty()) {
    foreach(const LookupItem &result, results) {
      FullySpecifiedType ty = result.type().simplified();

      if (Function *fun = asFunctionOrTemplateFunctionType(ty)) {

        if (!fun->name()) {
          continue;
        } else if (!functions.isEmpty() && enclosingNonTemplateScope(functions.first()) != enclosingNonTemplateScope(fun)) {
          continue; // skip fun, it's an hidden declaration.
        }

        auto newOverload = true;

        foreach(Function *f, functions) {
          if (fun->match(f)) {
            newOverload = false;
            break;
          }
        }

        if (newOverload)
          functions.append(fun);
      }
    }
  }

  if (functions.isEmpty()) {
    const Name *functionCallOp = context.bindings()->control()->operatorNameId(OperatorNameId::FunctionCallOp);

    foreach(const LookupItem &result, results) {
      FullySpecifiedType ty = result.type().simplified();
      Scope *scope = result.scope();

      if (NamedType *namedTy = ty->asNamedType()) {
        if (ClassOrNamespace *b = context.lookupType(namedTy->name(), scope)) {
          foreach(const LookupItem &r, b->lookup(functionCallOp)) {
            Symbol *overload = r.declaration();
            FullySpecifiedType overloadTy = overload->type().simplified();

            if (Function *funTy = overloadTy->asFunctionType())
              functions.append(funTy);
          }
        }
      }
    }
  }

  // There are two different kinds of completion we want to provide:
  // 1. If this is a function call, we want to pop up a tooltip that shows the user
  // the possible overloads with their argument types and names.
  // 2. If this is a function definition, we want to offer autocompletion of
  // the function signature.

  // check if function signature autocompletion is appropriate
  // Also check if the function name is a destructor name.
  auto isDestructor = false;
  if (!functions.isEmpty() && !toolTipOnly) {

    // function definitions will only happen in class or namespace scope,
    // so get the current location's enclosing scope.

    // get current line and column
    auto lineSigned = 0, columnSigned = 0;
    Utils::Text::convertPosition(m_interface->textDocument(), m_interface->position(), &lineSigned, &columnSigned);
    unsigned line = lineSigned, column = columnSigned - 1;

    // find a scope that encloses the current location, starting from the lastVisibileSymbol
    // and moving outwards

    Scope *sc = context.thisDocument()->scopeAt(line, column);

    if (sc && (sc->isClass() || sc->isNamespace())) {
      // It may still be a function call. If the whole line parses as a function
      // declaration, we should be certain that it isn't.
      auto autocompleteSignature = false;

      QTextCursor tc(m_interface->textDocument());
      tc.setPosition(endOfExpression);
      BackwardsScanner bs(tc, m_interface->languageFeatures());
      const int startToken = bs.startToken();
      int lineStartToken = bs.startOfLine(startToken);
      // make sure the required tokens are actually available
      bs.LA(startToken - lineStartToken);
      QString possibleDecl = bs.mid(lineStartToken).trimmed().append(QLatin1String("();"));

      Document::Ptr doc = Document::create(QLatin1String("<completion>"));
      doc->setUtf8Source(possibleDecl.toUtf8());
      if (doc->parse(Document::ParseDeclaration)) {
        doc->check();
        if (SimpleDeclarationAST *sd = doc->translationUnit()->ast()->asSimpleDeclaration()) {
          if (sd->declarator_list && sd->declarator_list->value->postfix_declarator_list && sd->declarator_list->value->postfix_declarator_list->value->asFunctionDeclarator()) {

            autocompleteSignature = true;

            CoreDeclaratorAST *coreDecl = sd->declarator_list->value->core_declarator;
            if (coreDecl && coreDecl->asDeclaratorId() && coreDecl->asDeclaratorId()->name) {
              NameAST *declName = coreDecl->asDeclaratorId()->name;
              if (declName->asDestructorName()) {
                isDestructor = true;
              } else if (QualifiedNameAST *qName = declName->asQualifiedName()) {
                if (qName->unqualified_name && qName->unqualified_name->asDestructorName())
                  isDestructor = true;
              }
            }
          }
        }
      }

      if (autocompleteSignature && !isDestructor) {
        // set up for rewriting function types with minimally qualified names
        // to do it correctly we'd need the declaration's context and scope, but
        // that'd be too expensive to get here. instead, we just minimize locally
        SubstitutionEnvironment env;
        env.setContext(context);
        env.switchScope(sc);
        ClassOrNamespace *targetCoN = context.lookupType(sc);
        if (!targetCoN)
          targetCoN = context.globalNamespace();
        UseMinimalNames q(targetCoN);
        env.enter(&q);
        Control *control = context.bindings()->control().data();

        // set up signature autocompletion
        foreach(Function *f, functions) {
          Overview overview;
          overview.showArgumentNames = true;
          overview.showDefaultArguments = false;

          const FullySpecifiedType localTy = rewriteType(f->type(), &env, control);

          // gets: "parameter list) cv-spec",
          const QString completion = overview.prettyType(localTy).mid(1);
          if (completion == QLatin1String(")"))
            continue;

          addCompletionItem(completion, QIcon(), 0, QVariant::fromValue(CompleteFunctionDeclaration(f)));
        }
        return true;
      }
    }
  }

  if (!functions.empty() && !isDestructor) {
    m_hintProposal = createHintProposal(functions);
    return true;
  }

  return false;
}

auto CppCompletionAssistInterface::getCppSpecifics() const -> void
{
  if (m_gotCppSpecifics)
    return;
  m_gotCppSpecifics = true;

  if (m_parser) {
    m_parser->update({CppModelManager::instance()->workingCopy(), nullptr, Utils::Language::Cxx, false});
    m_snapshot = m_parser->snapshot();
    m_headerPaths = m_parser->headerPaths();
  }
}

} // CppEditor::Internal
