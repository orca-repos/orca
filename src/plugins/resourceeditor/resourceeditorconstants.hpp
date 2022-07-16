// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace ResourceEditor {
namespace Constants {

constexpr char C_RESOURCEEDITOR[] = "Qt4.ResourceEditor";
constexpr char RESOURCEEDITOR_ID[] = "Qt4.ResourceEditor";
constexpr char C_RESOURCEEDITOR_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("OpenWith::Editors", "Resource Editor");
constexpr char REFRESH[] = "ResourceEditor.Refresh";
constexpr char C_RESOURCE_MIMETYPE[] = "application/vnd.qt.xml.resource";
constexpr char C_ADD_PREFIX[] = "ResourceEditor.AddPrefix";
constexpr char C_REMOVE_PREFIX[] = "ResourceEditor.RemovePrefix";
constexpr char C_RENAME_PREFIX[] = "ResourceEditor.RenamePrefix";
constexpr char C_REMOVE_NON_EXISTING[] = "ResourceEditor.RemoveNonExisting";
constexpr char C_REMOVE_FILE[] = "ResourceEditor.RemoveFile";
constexpr char C_RENAME_FILE[] = "ResourceEditor.RenameFile";
constexpr char C_OPEN_EDITOR[] = "ResourceEditor.OpenEditor";
constexpr char C_COPY_PATH[] = "ResourceEditor.CopyPath";
constexpr char C_COPY_URL[] = "ResourceEditor.CopyUrl";

} // namespace Constants
} // namespace ResourceEditor
