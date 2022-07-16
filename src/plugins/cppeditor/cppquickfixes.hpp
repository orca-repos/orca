// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppquickfix.hpp"

///
/// Adding New Quick Fixes
///
/// When adding new Quick Fixes, make sure that the match() function is "cheap".
/// Otherwise, since the match() functions are also called to generate context menu
/// entries, the user might experience a delay opening the context menu.
///

namespace CppEditor {
namespace Internal {

auto createCppQuickFixes() -> void;
auto destroyCppQuickFixes() -> void;

class ExtraRefactoringOperations : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Adds an include for an undefined identifier or only forward declared identifier.

  Activates on: the undefined identifier
*/
class AddIncludeForUndefinedIdentifier : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

// Exposed for tests
class AddIncludeForUndefinedIdentifierOp : public CppQuickFixOperation {
public:
  AddIncludeForUndefinedIdentifierOp(const CppQuickFixInterface &interface, int priority, const QString &include);

  auto perform() -> void override;
  auto include() const -> QString { return m_include; }

private:
  QString m_include;
};

class AddForwardDeclForUndefinedIdentifierOp : public CppQuickFixOperation {
public:
  AddForwardDeclForUndefinedIdentifierOp(const CppQuickFixInterface &interface, int priority, const QString &fqClassName, int symbolPos);

private:
  auto perform() -> void override;

  const QString m_className;
  const int m_symbolPos;
};

/*!
  Rewrite
    a op b

  As
    b flipop a

  Activates on: <= < > >= == != && ||
*/
class FlipLogicalOperands : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Rewrite
    a op b -> !(a invop b)
    (a op b) -> !(a invop b)
    !(a op b) -> (a invob b)

  Activates on: <= < > >= == !=
*/
class InverseLogicalComparison : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Rewrite
    !a && !b

  As
    !(a || b)

  Activates on: &&
*/
class RewriteLogicalAnd : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Replace
     "abcd"
     QLatin1String("abcd")
     QLatin1Literal("abcd")

  With
     @"abcd"

  Activates on: the string literal, if the file type is a Objective-C(++) file.
*/
class ConvertCStringToNSString : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Base class for converting numeric literals between decimal, octal and hex.
  Does the base check for the specific ones and parses the number.

  Test cases:
    0xFA0Bu;
    0X856A;
    298.3;
    199;
    074;
    199L;
    074L;
    -199;
    -017;
    0783; // invalid octal
    0; // border case, allow only hex<->decimal

  Activates on: numeric literals
*/
class ConvertNumericLiteral : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Replace
    "abcd"

  With
    tr("abcd") or
    QCoreApplication::translate("CONTEXT", "abcd") or
    QT_TRANSLATE_NOOP("GLOBAL", "abcd")

  depending on what is available.

  Activates on: the string literal
*/
class TranslateStringLiteral : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Replace
    "abcd"  -> QLatin1String("abcd")
    @"abcd" -> QLatin1String("abcd") (Objective C)
    'a'     -> QLatin1Char('a')
    'a'     -> "a"
    "a"     -> 'a' or QLatin1Char('a') (Single character string constants)
    "\n"    -> '\n', QLatin1Char('\n')

  Except if they are already enclosed in
    QLatin1Char, QT_TRANSLATE_NOOP, tr,
    trUtf8, QLatin1Literal, QLatin1String

  Activates on: the string or character literal
*/

class WrapStringLiteral : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Turns "an_example_symbol" into "anExampleSymbol" and
  "AN_EXAMPLE_SYMBOL" into "AnExampleSymbol".

  Activates on: identifiers
*/
class ConvertToCamelCase : public CppQuickFixFactory {
public:
  ConvertToCamelCase(bool test = false) : CppQuickFixFactory(), m_test(test) {}

  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;

private:
  const bool m_test;
};

/*!
  Replace
    if (Type name = foo()) {...}

  With
    Type name = foo();
    if (name) {...}

  Activates on: the name of the introduced variable
*/
class MoveDeclarationOutOfIf : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Replace
    while (Type name = foo()) {...}

  With
    Type name;
    while ((name = foo()) != 0) {...}

  Activates on: the name of the introduced variable
*/
class MoveDeclarationOutOfWhile : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Replace
     if (something && something_else) {
     }

  with
     if (something)
        if (something_else) {
        }
     }

  and
    if (something || something_else)
      x;

  with
    if (something)
      x;
    else if (something_else)
      x;

    Activates on: && or ||
*/
class SplitIfStatement : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Rewrite
    int *a, b;

  As
    int *a;
    int b;

  Activates on: the type or the variable names.
*/
class SplitSimpleDeclaration : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Rewrites
    a = foo();

  As
    Type a = foo();

  Where Type is the return type of foo()

  Activates on: the assignee, if the type of the right-hand side of the assignment is known.
*/
class AddLocalDeclaration : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Add curly braces to a if statement that doesn't already contain a
  compound statement. I.e.

  if (a)
      b;
  becomes
  if (a) {
      b;
  }

  Activates on: the if
*/
class AddBracesToIf : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Switches places of the parameter declaration under cursor
  with the next or the previous one in the parameter declaration list

  Activates on: parameter declarations
*/
class RearrangeParamDeclarationList : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Reformats a pointer, reference or rvalue reference type/declaration.

  Works also with selections (except when the cursor is not on any AST).

  Activates on: simple declarations, parameters and return types of function
                declarations and definitions, control flow statements (if,
                while, for, foreach) with declarations.
*/
class ReformatPointerDeclaration : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Adds missing case statements for "switch (enumVariable)"
 */
class CompleteSwitchCaseStatement : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void override;
};

/*!
  Adds a declarations to a definition
 */
class InsertDeclFromDef : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Adds a definition for a declaration.
 */
class InsertDefFromDecl : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Adds a class member from an initialization in the constructor.
 */
class InsertMemberFromInitialization : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;

private:
  auto getType(const CppQuickFixInterface &interface, const CPlusPlus::MemInitializerAST *memInitializer, const CPlusPlus::FunctionDefinitionAST *ctor) const -> QString;
};

/*!
  Adds a definition for any number of member function declarations.
 */
class InsertDefsFromDecls : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;

  enum class Mode {
    Off,
    // Testing: simulates user canceling the dialog
    Alternating,
    // Testing: simulates user choosing a different DefPos for every function
    User // Normal interactive mode
  };

  auto setMode(Mode mode) -> void { m_mode = mode; }

private:
  Mode m_mode = Mode::User;
};

/*!
  Extracts the selected code and puts it to a function
 */
class ExtractFunction : public CppQuickFixFactory {
public:
  using FunctionNameGetter = std::function<QString()>;

  ExtractFunction(FunctionNameGetter functionNameGetter = FunctionNameGetter());
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;

private:
  FunctionNameGetter m_functionNameGetter; // For tests to avoid GUI pop-up.
};

/*!
  Extracts the selected constant and converts it to a parameter of the current function.

  Activates on numeric, bool, character, or string literal in the function body.
 */
class ExtractLiteralAsParameter : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Converts the selected variable to a pointer if it is a stack variable or reference, or vice versa.

  Activates on variable declarations.
 */
class ConvertFromAndToPointer : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Adds getter and setter functions for a member variable
 */
class GenerateGetterSetter : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Adds getter and setter functions for several member variables
 */
class GenerateGettersSettersForClass : public CppQuickFixFactory {
protected:
  auto setTest() -> void { m_test = true; }

private:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;

  bool m_test = false;
};

/*!
  Adds missing members for a Q_PROPERTY
 */
class InsertQtPropertyMembers : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Converts a Qt 4 QObject::connect() to Qt 5 style.
 */
class ConvertQt4Connect : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
 Applies function signature changes
 */
class ApplyDeclDefLinkChanges : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
 Moves the definition of a member function outside the class or moves the definition of a member
 function or a normal function to the implementation file.
 */
class MoveFuncDefOutside : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
 Moves all member function definitions outside the class or to the implementation file.
 */
class MoveAllFuncDefOutside : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
 Moves the definition of a function to its declaration.
 */
class MoveFuncDefToDecl : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Assigns the return value of a function call or a new expression to a local variable
 */
class AssignToLocalVariable : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Optimizes a for loop to avoid permanent condition check and forces to use preincrement
  or predecrement operators in the expression of the for loop.
 */
class OptimizeForLoop : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Escapes or unescapes a string literal as UTF-8.

  Escapes non-ASCII characters in a string literal to hexadecimal escape sequences.
  Unescapes octal or hexadecimal escape sequences in a string literal.
  String literals are handled as UTF-8 even if file's encoding is not UTF-8.
 */
class EscapeStringLiteral : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Removes a using directive (using namespace xyz).

*/
class RemoveUsingNamespace : public CppQuickFixFactory {
public:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;
};

/*!
  Generate constructor
 */
class GenerateConstructor : public CppQuickFixFactory {
protected:
  auto setTest() -> void { m_test = true; }

private:
  auto match(const CppQuickFixInterface &interface, TextEditor::QuickFixOperations &result) -> void override;

  bool m_test = false;
};

} // namespace Internal
} // namespace CppEditor
