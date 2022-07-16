// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "assistinterface.hpp"

#include <utils/textutils.hpp>
#include <utils/qtcassert.hpp>

#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>

/*!
    \class TextEditor::AssistInterface
    \brief The AssistInterface class acts as an interface for providing access
    to the document from which a proposal is computed.
    \ingroup CodeAssist

    This interface existis in order to avoid a direct dependency on the text editor. This is
    particularly important and safer for asynchronous providers, since in such cases computation
    of the proposal is not done in the GUI thread.

    In general this API tries to be as decoupled as possible from the base text editor.
    This is in order to make the design a bit more generic and allow code assist to be
    pluggable into different types of documents (there are still issues to be treated).

    This class is part of the CodeAssist API.

    \sa IAssistProposal, IAssistProvider, IAssistProcessor
*/

/*!
    \fn int TextEditor::AssistInterface::position() const

    Returns the cursor position.
*/

/*!
    \fn QChar TextEditor::AssistInterface::characterAt(int position) const

    Returns the character at \a position.
*/

/*!
    \fn QString TextEditor::AssistInterface::textAt(int position, int length) const

    Returns the text at \a position with the given \a length.
*/

/*!
    \fn QString TextEditor::AssistInterface::fileName() const

    Returns the file associated.
*/

/*!
    \fn QTextDocument *TextEditor::AssistInterface::textDocument() const
    Returns the document.
*/

/*!
    \fn void TextEditor::AssistInterface::detach(QThread *destination)

    Detaches the interface. If it is necessary to take any special care in order to allow
    this interface to be run in a separate thread \a destination this needs to be done
    in this function.
*/

/*!
    \fn AssistReason TextEditor::AssistInterface::reason() const

    The reason which triggered the assist.
*/

namespace TextEditor {

AssistInterface::AssistInterface(QTextDocument *textDocument, int position, const Utils::FilePath &filePath, AssistReason reason) : m_textDocument(textDocument), m_isAsync(false), m_position(position), m_filePath(filePath), m_reason(reason) {}

AssistInterface::~AssistInterface()
{
  if (m_isAsync)
    delete m_textDocument;
}

auto AssistInterface::characterAt(int position) const -> QChar
{
  return m_textDocument->characterAt(position);
}

auto AssistInterface::textAt(int pos, int length) const -> QString
{
  return Utils::Text::textAt(QTextCursor(m_textDocument), pos, length);
}

auto AssistInterface::prepareForAsyncUse() -> void
{
  m_text = m_textDocument->toPlainText();
  m_userStates.reserve(m_textDocument->blockCount());
  for (auto block = m_textDocument->firstBlock(); block.isValid(); block = block.next())
    m_userStates.append(block.userState());
  m_textDocument = nullptr;
  m_isAsync = true;
}

auto AssistInterface::recreateTextDocument() -> void
{
  m_textDocument = new QTextDocument(m_text);
  m_text.clear();

  QTC_CHECK(m_textDocument->blockCount() == m_userStates.count());
  auto block = m_textDocument->firstBlock();
  for (auto i = 0; i < m_userStates.count() && block.isValid(); ++i, block = block.next())
    block.setUserState(m_userStates[i]);
}

auto AssistInterface::reason() const -> AssistReason
{
  return m_reason;
}

} // namespace TextEditor
