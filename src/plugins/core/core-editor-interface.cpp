// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-editor-interface.hpp"

/*!
    \class Core::IEditor
    \inheaderfile coreplugin/editormanager/ieditor.h
    \inmodule Orca

    \brief The IEditor class provides an interface for editing an open document
    in \QC.

    IEditor instances are usually created by a corresponding IEditorFactory.

    An IEditor instance provides an editor widget for a single IDocument via
    the IContext::widget() method. If the the editor type supports it, multiple
    editors can be opened for the same document. Multiple IEditor instances
    share ownership of the same IDocument instance in that case.

    The IEditor::toolBar() is integrated into the toolbar above the editor
    widget, next to the document drop down.

    \sa Core::IEditorFactory, Core::EditorManager
*/

namespace Orca::Plugin::Core {

/*!
    \fn IDocument *IEditor::document() const

    Returns the document that is edited by this editor. The editor owns the
    document. If the editor supports splitting, all editors created with
    duplicate() share ownership of the document.

    This must never return \c nullptr.
*/

/*!
    \fn IEditor *IEditor::duplicate()

    Returns a duplicate of the editor, for example when the user splits the
    editor view. The default implementation returns \c nullptr.

    \sa duplicateSupported()
*/

/*!
    \fn QByteArray IEditor::saveState() const

    Returns the state of the editor, like scroll and cursor position, as a
    QByteArray. This is used for restoring the state for example after the
    document was closed (manually or automatically) and re-opened later. The
    default implementation returns an empty QByteArray.

    \sa restoreState()
*/

/*!
    \fn void IEditor::restoreState(const QByteArray &state)

    Restores the \a state of the editor. The default implementation does
    nothing.

    \sa saveState()
*/

/*!
    \fn int IEditor::currentLine() const

    Returns the current line in the document, if appropriate. The default
    implementation returns \c 0. Line numbers start at \c 1 for the first line.

    \sa currentColumn()
    \sa gotoLine()
*/

/*!
    \fn int IEditor::currentColumn() const

    Returns the current column in the document, if appropriate. The default
    implementation returns \c 0. Column numbers start at \c 0 for the first
    column.

    \sa currentLine()
    \sa gotoLine()
*/

/*!
    \fn void IEditor::gotoLine(int line, int column, bool centerLine)

    Goes to \a line and \a column in the document. If \a centerLine is
    \c true, centers the line in the editor.

    Line numbers start at \c 1 for the first line, column numbers start at \c 0
    for the first column.

    The default implementation does nothing.

    \sa currentLine()
    \sa currentColumn()
*/

/*!
    \fn QWidget IEditor::toolBar()

    Returns the toolbar for the editor.

    The editor toolbar is located at the top of the editor view. The editor
    toolbar is context sensitive and shows items relevant to the document
    currently open in the editor.
*/

/*!
    \fn bool IEditor::isDesignModePreferred() const

    Returns whether the document should be opened in the Design mode by
    default. This requires design mode to support that document type. The
    default implementation returns \c false.
*/

/*!
    Creates an IEditor.

    Implementations must create a corresponding document, or share an existing
    document with another IEditor.
*/
IEditor::IEditor() : m_duplicate_supported(false) {}

/*!
    Returns whether duplication is supported, for example when the user splits
    the editor view.

    \sa duplicate()
    \sa setDuplicateSupported()
*/
auto IEditor::duplicateSupported() const -> bool
{
  return m_duplicate_supported;
}

/*!
    Sets whether duplication is supported to \a duplicatesSupported.

    The default is \c false.

    \sa duplicate()
    \sa duplicateSupported()
*/
auto IEditor::setDuplicateSupported(const bool duplicate_supported) -> void
{
  m_duplicate_supported = duplicate_supported;
}

} // namespace Orca::Plugin::Core
