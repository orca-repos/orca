// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-help-item.hpp>
#include <texteditor/texteditor.hpp>

#include <cplusplus/CppDocument.h>

#include <QFuture>
#include <QIcon>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTextCursor>

#include <functional>

namespace CPlusPlus {
class LookupItem;
class LookupContext;
}

namespace CppEditor {
class CppModelManager;

namespace Internal {
class CppElement;

class CppElementEvaluator final {
public:
  explicit CppElementEvaluator(TextEditor::TextEditorWidget *editor);
  ~CppElementEvaluator();

  auto setTextCursor(const QTextCursor &tc) -> void;
  auto execute() -> void;
  static auto asyncExecute(TextEditor::TextEditorWidget *editor) -> QFuture<QSharedPointer<CppElement>>;
  static auto asyncExecute(const QString &expression, const QString &fileName) -> QFuture<QSharedPointer<CppElement>>;
  auto identifiedCppElement() const -> bool;
  auto cppElement() const -> const QSharedPointer<CppElement>&;
  auto hasDiagnosis() const -> bool;
  auto diagnosis() const -> const QString&;

  static auto linkFromExpression(const QString &expression, const QString &fileName) -> Utils::Link;

private:
  class CppElementEvaluatorPrivate *d;
};

class CppClass;

class CppElement {
protected:
  CppElement();

public:
  virtual ~CppElement();
  virtual auto toCppClass() -> CppClass*;

  Orca::Plugin::Core::HelpItem::Category helpCategory = Orca::Plugin::Core::HelpItem::Unknown;
  QStringList helpIdCandidates;
  QString helpMark;
  Utils::Link link;
  QString tooltip;
};

class CppDeclarableElement : public CppElement {
public:
  explicit CppDeclarableElement(CPlusPlus::Symbol *declaration);
  
  CPlusPlus::Symbol *declaration;
  QString name;
  QString qualifiedName;
  QString type;
  QIcon icon;
};

class CppClass : public CppDeclarableElement {
public:
  CppClass();
  explicit CppClass(CPlusPlus::Symbol *declaration);

  auto operator==(const CppClass &other) -> bool;

  auto toCppClass() -> CppClass* final;
  auto lookupBases(QFutureInterfaceBase &futureInterface, CPlusPlus::Symbol *declaration, const CPlusPlus::LookupContext &context) -> void;
  auto lookupDerived(QFutureInterfaceBase &futureInterface, CPlusPlus::Symbol *declaration, const CPlusPlus::Snapshot &snapshot) -> void;

  QList<CppClass> bases;
  QList<CppClass> derived;
};

} // namespace Internal
} // namespace CppEditor
