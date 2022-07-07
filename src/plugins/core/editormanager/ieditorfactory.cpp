// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ieditorfactory.hpp"
#include "ieditorfactory_p.hpp"
#include "editormanager.hpp"

#include <utils/algorithm.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>

using namespace Utils;

namespace Core {

/*!
    \class Core::IEditorFactory
    \inheaderfile coreplugin/editormanager/ieditorfactory.h
    \inmodule Orca

    \brief The IEditorFactory class creates suitable editors for documents
    according to their MIME type.

    Whenever a user wants to edit or create a document, the EditorManager
    scans all IEditorFactory instances for suitable editors. The selected
    IEditorFactory is then asked to create an editor.

    Implementations should set the properties of the IEditorFactory subclass in
    their constructor with EditorType::setId(), EditorType::setDisplayName(),
    EditorType::setMimeTypes(), and setEditorCreator()

    IEditorFactory instances automatically register themselves in \QC in their
    constructor.

    \sa Core::EditorType
    \sa Core::IEditor
    \sa Core::IDocument
    \sa Core::EditorManager
*/

/*!
    \class Core::EditorType
    \inheaderfile coreplugin/editormanager/ieditorfactory.h
    \inmodule Orca

    \brief The EditorType class is the base class for Core::IEditorFactory and
    Core::IExternalEditor.
*/

/*!
    \fn void Core::EditorType::addMimeType(const QString &mimeType)

    Adds \a mimeType to the list of MIME types supported by this editor type.

    \sa mimeTypes()
    \sa setMimeTypes()
*/

/*!
    \fn QString Core::EditorType::displayName() const

    Returns a user-visible description of the editor type.

    \sa setDisplayName()
*/

/*!
    \fn Utils::Id Core::EditorType::id() const

    Returns the ID of the editors' document type.

    \sa setId()
*/

/*!
    \fn QString Core::EditorType::mimeTypes() const

    Returns the list of supported MIME types of this editor type.

    \sa addMimeType()
    \sa setMimeTypes()
*/

/*!
    \fn void Core::EditorType::setDisplayName(const QString &displayName)

    Sets the \a displayName of the editor type. This is for example shown in
    the \uicontrol {Open With} menu and the MIME type preferences.

    \sa displayName()
*/

/*!
    \fn void Core::EditorType::setId(Utils::Id id)

    Sets the \a id of the editors' document type. This must be the same as the
    IDocument::id() of the documents returned by created editors.

    \sa id()
*/

/*!
    \fn void Core::EditorType::setMimeTypes(const QStringList &mimeTypes)

    Sets the MIME types supported by the editor type to \a mimeTypes.

    \sa addMimeType()
    \sa mimeTypes()
*/

static QList<EditorType*> g_editor_types;
static QHash<MimeType, EditorType*> g_user_preferred_editor_types;
static QList<IEditorFactory*> g_editor_factories;

/*!
    \internal
*/
EditorType::EditorType()
{
  g_editor_types.append(this);
}

/*!
    \internal
*/
EditorType::~EditorType()
{
  g_editor_types.removeOne(this);
}

/*!
    Returns all registered internal and external editors.
*/
auto EditorType::allEditorTypes() -> editor_type_list
{
  return g_editor_types;
}

auto EditorType::editorTypeForId(const Id &id) -> EditorType*
{
  return findOrDefault(allEditorTypes(), equal(&EditorType::id, id));
}

/*!
    Returns all available internal and external editors for the \a mimeType in the
    default order: Editor types ordered by MIME type hierarchy, internal editors
    first.
*/
auto EditorType::defaultEditorTypes(const MimeType &mime_type) -> editor_type_list
{
  editor_type_list result;
  const auto all_types = allEditorTypes();

  const auto all_editor_factories = filtered(all_types, [](EditorType *e) {
    return e->asEditorFactory() != nullptr;
  });

  const auto all_external_editors = filtered(all_types, [](EditorType *e) {
    return e->asExternalEditor() != nullptr;
  });

  Internal::mimeTypeFactoryLookup(mime_type, all_editor_factories, &result);
  Internal::mimeTypeFactoryLookup(mime_type, all_external_editors, &result);
  return result;
}

auto EditorType::preferredEditorTypes(const FilePath &file_path) -> editor_type_list
{
  // default factories by mime type
  const auto mime_type = mimeTypeForFile(file_path);
  auto factories = defaultEditorTypes(mime_type);

  // user preferred factory to front
  if (const auto user_preferred = Internal::userPreferredEditorTypes().value(mime_type)) {
    factories.removeAll(user_preferred);
    factories.prepend(user_preferred);
  }

  // make binary editor first internal editor for text files > 48 MB
  if (file_path.fileSize() > EditorManager::maxTextFileSize() && mime_type.inherits("text/plain")) {
    const auto binary = mimeTypeForName("application/octet-stream");
    if (const auto binary_editors = defaultEditorTypes(binary); !binary_editors.isEmpty()) {
      const auto binary_editor = binary_editors.first();
      factories.removeAll(binary_editor);
      auto insertion_index = 0;
      while (factories.size() > insertion_index && factories.at(insertion_index)->asExternalEditor() != nullptr) {
        ++insertion_index;
      }
      factories.insert(insertion_index, binary_editor);
    }
  }

  return factories;
}

/*!
    Creates an IEditorFactory.

    Registers the IEditorFactory in \QC.
*/
IEditorFactory::IEditorFactory()
{
  g_editor_factories.append(this);
}

/*!
    \internal
*/
IEditorFactory::~IEditorFactory()
{
  g_editor_factories.removeOne(this);
}

/*!
    \internal
*/
auto IEditorFactory::allEditorFactories() -> editor_factory_list
{
  return g_editor_factories;
}

/*!
    Returns the available editor factories for \a filePath in order of
    preference. That is the default order for the document's MIME type but with
    a user overridden default editor first, and the binary editor as the very
    first item if a text document is too large to be opened as a text file.
*/
auto IEditorFactory::preferredEditorFactories(const FilePath &file_path) -> editor_factory_list
{
  const auto default_editor_factories = [](const MimeType &mime_type) {
    const auto types = defaultEditorTypes(mime_type);
    const auto ieditor_types = filtered(types, [](EditorType *type) {
      return type->asEditorFactory() != nullptr;
    });
    return Utils::qobject_container_cast<IEditorFactory*>(ieditor_types);
  };

  // default factories by mime type
  const auto mime_type = mimeTypeForFile(file_path);
  auto factories = default_editor_factories(mime_type);
  const auto factories_move_to_front = [&factories](IEditorFactory *f) {
    factories.removeAll(f);
    factories.prepend(f);
  };

  // user preferred factory to front
  if (const auto user_preferred = Internal::userPreferredEditorTypes().value(mime_type); user_preferred && user_preferred->asEditorFactory())
    factories_move_to_front(user_preferred->asEditorFactory());

  // open text files > 48 MB in binary editor
  if (file_path.fileSize() > EditorManager::maxTextFileSize() && mime_type.inherits("text/plain")) {
    const auto binary = mimeTypeForName("application/octet-stream");
    if (const auto binary_editors = default_editor_factories(binary); !binary_editors.isEmpty())
      factories_move_to_front(binary_editors.first());
  }

  return factories;
}

/*!
    Creates an editor.

    Uses the function set with setEditorCreator() to create the editor.

    \sa setEditorCreator()
*/
auto IEditorFactory::createEditor() const -> IEditor*
{
  QTC_ASSERT(m_creator, return nullptr);
  return m_creator();
}

/*!
    Sets the function that is used to create an editor instance in
    createEditor() to \a creator.

    \sa createEditor()
*/
auto IEditorFactory::setEditorCreator(const std::function<IEditor *()> &creator) -> void
{
  m_creator = creator;
}

/*!
    \internal
*/
auto Internal::userPreferredEditorTypes() -> QHash<MimeType, EditorType*>
{
  return g_user_preferred_editor_types;
}

/*!
    \internal
*/
auto Internal::setUserPreferredEditorTypes(const QHash<MimeType, EditorType*> &factories) -> void
{
  g_user_preferred_editor_types = factories;
}

} // Core
