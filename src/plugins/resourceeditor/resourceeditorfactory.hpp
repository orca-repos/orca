// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/editormanager/ieditorfactory.hpp>

namespace ResourceEditor {
namespace Internal {

class ResourceEditorPlugin;

class ResourceEditorFactory final : public Core::IEditorFactory {
public:
  explicit ResourceEditorFactory(ResourceEditorPlugin *plugin);
};

} // namespace Internal
} // namespace ResourceEditor
