// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "assistproposalitem.hpp"

#include <texteditor/quickfix.hpp>
#include <texteditor/snippets/snippet.hpp>

namespace TextEditor {

/*!
    \class TextEditor::AssistProposalItem
    \brief The AssistProposalItem class acts as an interface for representing an assist
    proposal item.
    \ingroup CodeAssist

    This is class is part of the CodeAssist API.
*/

/*!
    \fn bool TextEditor::AssistProposalItem::implicitlyApplies() const

    Returns whether this item should implicitly apply in the case it is the only proposal
    item available.
*/

/*!
    \fn bool TextEditor::AssistProposalItem::prematurelyApplies(const QChar &c) const

    Returns whether the character \a c causes this item to be applied.
*/

/*!
    \fn void TextEditor::AssistProposalItem::apply(BaseTextEditor *editor, int basePosition) const

    This is the place to implement the actual application of the item.
*/

auto AssistProposalItem::setIcon(const QIcon &icon) -> void
{
  m_icon = icon;
}

auto AssistProposalItem::icon() const -> QIcon
{
  return m_icon;
}

auto AssistProposalItem::setText(const QString &text) -> void
{
  m_text = text;
}

auto AssistProposalItem::text() const -> QString
{
  return m_text;
}

auto AssistProposalItem::setDetail(const QString &detail) -> void
{
  m_detail = detail;
}

auto AssistProposalItem::detail() const -> QString
{
  return m_detail;
}

auto AssistProposalItem::setData(const QVariant &var) -> void
{
  m_data = var;
}

auto AssistProposalItem::data() const -> const QVariant&
{
  return m_data;
}

auto AssistProposalItem::isSnippet() const -> bool
{
  return data().canConvert<QString>();
}

auto AssistProposalItem::isValid() const -> bool
{
  return m_data.isValid();
}

auto AssistProposalItem::hash() const -> quint64
{
  return 0;
}

auto AssistProposalItem::implicitlyApplies() const -> bool
{
  return !data().canConvert<QString>() && !data().canConvert<QuickFixOperation::Ptr>();
}

auto AssistProposalItem::prematurelyApplies(const QChar &c) const -> bool
{
  Q_UNUSED(c)
  return false;
}

auto AssistProposalItem::apply(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  if (data().canConvert<QString>()) {
    applySnippet(manipulator, basePosition);
  } else if (data().canConvert<QuickFixOperation::Ptr>()) {
    applyQuickFix(manipulator, basePosition);
  } else {
    applyContextualContent(manipulator, basePosition);
    manipulator.encourageApply();
  }
}

auto AssistProposalItem::applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  const auto currentPosition = manipulator.currentPosition();
  manipulator.replace(basePosition, currentPosition - basePosition, text());
}

auto AssistProposalItem::applySnippet(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  manipulator.insertCodeSnippet(basePosition, data().toString(), &Snippet::parse);
}

auto AssistProposalItem::applyQuickFix(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  Q_UNUSED(manipulator)
  Q_UNUSED(basePosition)

  const auto op = data().value<QuickFixOperation::Ptr>();
  op->perform();
}

} // namespace TextEditor
