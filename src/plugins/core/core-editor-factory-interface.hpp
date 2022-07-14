// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/id.hpp>
#include <utils/mimetypes/mimetype.hpp>

#include <QObject>

#include <functional>

namespace Utils {
class FilePath;
}

namespace Orca::Plugin::Core {

class IExternalEditor;
class IEditor;
class IEditorFactory;
class EditorType;

using editor_factory_list = QList<IEditorFactory *>;
using editor_type_list = QList<EditorType *>;

class CORE_EXPORT EditorType : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(EditorType)

public:
  ~EditorType() override;

  static auto allEditorTypes() -> editor_type_list;
  static auto editorTypeForId(const Utils::Id &id) -> EditorType*;
  static auto defaultEditorTypes(const Utils::MimeType &mime_type) -> editor_type_list;
  static auto preferredEditorTypes(const Utils::FilePath &file_path) -> editor_type_list;
  auto id() const -> Utils::Id { return m_id; }
  auto displayName() const -> QString { return m_display_name; }
  auto mimeTypes() const -> QStringList { return m_mime_types; }
  virtual auto asEditorFactory() -> IEditorFactory* { return nullptr; }
  virtual auto asExternalEditor() -> IExternalEditor* { return nullptr; }

protected:
  EditorType();
  auto setId(const Utils::Id id) -> void { m_id = id; }
  auto setDisplayName(const QString &display_name) -> void { m_display_name = display_name; }
  auto setMimeTypes(const QStringList &mime_types) -> void { m_mime_types = mime_types; }
  auto addMimeType(const QString &mime_type) -> void { m_mime_types.append(mime_type); }

private:
  Utils::Id m_id;
  QString m_display_name;
  QStringList m_mime_types;
};

class CORE_EXPORT IEditorFactory : public EditorType {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IEditorFactory)

public:
  IEditorFactory();
  ~IEditorFactory() override;

  static auto allEditorFactories() -> editor_factory_list;
  static auto preferredEditorFactories(const Utils::FilePath &file_path) -> editor_factory_list;
  auto createEditor() const -> IEditor*;
  auto asEditorFactory() -> IEditorFactory* override { return this; }

protected:
  auto setEditorCreator(const std::function<IEditor *()> &creator) -> void;

private:
  std::function<IEditor *()> m_creator;
};

} // namespace Orca::Plugin::Core
