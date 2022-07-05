// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ieditorfactory.h"

#include <core/core_global.h>

#include <utils/id.h>
#include <utils/mimetypes/mimetype.h>

namespace Utils {
class FilePath;
}

namespace Core {

class IExternalEditor;

using external_editor_list = QList<IExternalEditor *>;

class CORE_EXPORT IExternalEditor : public EditorType {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IExternalEditor)

public:
  explicit IExternalEditor();
  ~IExternalEditor() override;

  static auto allExternalEditors() -> external_editor_list;
  static auto externalEditors(const Utils::MimeType &mime_type) -> external_editor_list;
  auto asExternalEditor() -> IExternalEditor* override { return this; }
  virtual auto startEditor(const Utils::FilePath &file_path, QString *error_message) -> bool = 0;
};

} // namespace Core
