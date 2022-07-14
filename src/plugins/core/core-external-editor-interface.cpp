// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-external-editor-interface.hpp"

#include "core-editor-factory-private-interface.hpp"

namespace Orca::Plugin::Core {

/*!
    \class Core::IExternalEditor
    \inheaderfile coreplugin/editormanager/iexternaleditor.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The IExternalEditor class enables registering an external
    editor in the \uicontrol{Open With} dialog.
*/

/*!
    \fn QString Core::IExternalEditor::displayName() const
    Returns a user-visible description of the editor type.
*/

/*!
    \fn Utils::Id Core::IExternalEditor::id() const
    Returns the ID of the factory or editor type.
*/

/*!
    \fn QStringList Core::IExternalEditor::mimeTypes() const
    Returns a list of MIME types that the editor supports
*/

/*!
    \fn bool Core::IExternalEditor::startEditor(const Utils::FilePath &fileName, QString *errorMessage)

    Opens the editor with \a fileName. Returns \c true on success or \c false
    on failure along with the error in \a errorMessage.
*/

static QList<IExternalEditor*> g_external_editors;

/*!
    \internal
*/
IExternalEditor::IExternalEditor()
{
  g_external_editors.append(this);
}

/*!
    \internal
*/
IExternalEditor::~IExternalEditor()
{
  g_external_editors.removeOne(this);
}

/*!
    Returns all available external editors.
*/
auto IExternalEditor::allExternalEditors() -> external_editor_list
{
  return g_external_editors;
}

/*!
    Returns all external editors available for this \a mimeType in the default
    order (editors ordered by MIME type hierarchy).
*/
auto IExternalEditor::externalEditors(const Utils::MimeType &mime_type) -> external_editor_list
{
  external_editor_list rc;
  const auto all_editors = allExternalEditors();
  mimeTypeFactoryLookup(mime_type, all_editors, &rc);
  return rc;
}

} // namespace Orca::Plugin::Core
