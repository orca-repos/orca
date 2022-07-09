// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppvirtualfunctionassistprovider.hpp"

#include "cppvirtualfunctionproposalitem.hpp"

#include "cpptoolsreuse.hpp"
#include "functionutils.hpp"
#include "symbolfinder.hpp"

#include <cplusplus/Icons.h>
#include <cplusplus/Overview.h>

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>

#include <texteditor/codeassist/genericproposalmodel.hpp>
#include <texteditor/codeassist/genericproposalwidget.hpp>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/codeassist/iassistproposal.hpp>
#include <texteditor/texteditorconstants.hpp>

#include <utils/qtcassert.hpp>

using namespace CPlusPlus;
using namespace TextEditor;

namespace CppEditor {

/// Activate current item with the same shortcut that is configured for Follow Symbol Under Cursor.
/// This is limited to single-key shortcuts without modifiers.
class VirtualFunctionProposalWidget : public GenericProposalWidget {
public:
  VirtualFunctionProposalWidget(bool openInSplit)
  {
    auto id = openInSplit ? TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR_IN_NEXT_SPLIT : TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR;
    if (auto command = Core::ActionManager::command(id))
      m_sequence = command->keySequence();
  }

protected:
  auto eventFilter(QObject *o, QEvent *e) -> bool override
  {
    if (e->type() == QEvent::ShortcutOverride && m_sequence.count() == 1) {
      auto ke = static_cast<const QKeyEvent*>(e);
      const QKeySequence seq(ke->key());
      if (seq == m_sequence) {
        activateCurrentProposalItem();
        e->accept();
        return true;
      }
    }
    return GenericProposalWidget::eventFilter(o, e);
  }

  auto showProposal(const QString &prefix) -> void override
  {
    auto proposalModel = model();
    if (proposalModel && proposalModel->size() == 1) {
      const auto item = dynamic_cast<VirtualFunctionProposalItem*>(proposalModel->proposalItem(0));
      if (item && item->link().hasValidTarget()) {
        emit proposalItemActivated(proposalModel->proposalItem(0));
        deleteLater();
        return;
      }
    }
    GenericProposalWidget::showProposal(prefix);
  }

private:
  QKeySequence m_sequence;
};

class VirtualFunctionAssistProcessor : public IAssistProcessor {
public:
  VirtualFunctionAssistProcessor(const VirtualFunctionAssistProvider::Parameters &params) : m_params(params) {}

  auto immediateProposal(const AssistInterface *) -> IAssistProposal* override
  {
    QTC_ASSERT(m_params.function, return nullptr);

    auto *hintItem = new VirtualFunctionProposalItem(Utils::Link());
    hintItem->setText(QCoreApplication::translate("VirtualFunctionsAssistProcessor", "collecting overrides ..."));
    hintItem->setOrder(-1000);

    QList<AssistProposalItemInterface*> items;
    items << itemFromFunction(m_params.function);
    items << hintItem;
    return new VirtualFunctionProposal(m_params.cursorPosition, items, m_params.openInNextSplit);
  }

  auto perform(const AssistInterface *assistInterface) -> IAssistProposal* override
  {
    delete assistInterface;

    QTC_ASSERT(m_params.function, return nullptr);
    QTC_ASSERT(m_params.staticClass, return nullptr);
    QTC_ASSERT(!m_params.snapshot.isEmpty(), return nullptr);

    Class *functionsClass = m_finder.findMatchingClassDeclaration(m_params.function, m_params.snapshot);
    if (!functionsClass)
      return nullptr;

    const QList<Function*> overrides = Internal::FunctionUtils::overrides(m_params.function, functionsClass, m_params.staticClass, m_params.snapshot);
    if (overrides.isEmpty())
      return nullptr;

    QList<AssistProposalItemInterface*> items;
    foreach(Function *func, overrides)
      items << itemFromFunction(func);
    items.first()->setOrder(1000); // Ensure top position for function of static type

    return new VirtualFunctionProposal(m_params.cursorPosition, items, m_params.openInNextSplit);
  }

private:
  auto maybeDefinitionFor(Function *func) const -> Function*
  {
    if (Function *definition = m_finder.findMatchingDefinition(func, m_params.snapshot))
      return definition;
    return func;
  }

  auto itemFromFunction(Function *func) const -> VirtualFunctionProposalItem*
  {
    const Utils::Link link = maybeDefinitionFor(func)->toLink();
    QString text = m_overview.prettyName(LookupContext::fullyQualifiedName(func));
    if (func->isPureVirtual())
      text += QLatin1String(" = 0");

    auto *item = new VirtualFunctionProposalItem(link, m_params.openInNextSplit);
    item->setText(text);
    item->setIcon(Icons::iconForSymbol(func));

    return item;
  }

  VirtualFunctionAssistProvider::Parameters m_params;
  Overview m_overview;
  mutable SymbolFinder m_finder;
};

VirtualFunctionAssistProvider::VirtualFunctionAssistProvider() = default;

auto VirtualFunctionAssistProvider::configure(const Parameters &parameters) -> bool
{
  m_params = parameters;
  return true;
}

auto VirtualFunctionAssistProvider::runType() const -> IAssistProvider::RunType
{
  return AsynchronousWithThread;
}

auto VirtualFunctionAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new VirtualFunctionAssistProcessor(m_params);
}

VirtualFunctionProposal::VirtualFunctionProposal(int cursorPos, const QList<AssistProposalItemInterface*> &items, bool openInSplit) : GenericProposal(cursorPos, items), m_openInSplit(openInSplit)
{
  setFragile(true);
}

auto VirtualFunctionProposal::createWidget() const -> IAssistProposalWidget*
{
  return new VirtualFunctionProposalWidget(m_openInSplit);
}

} // namespace CppEditor
