// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourceeditorfactory.hpp"
#include "resourceeditorw.hpp"
#include "resourceeditorplugin.hpp"
#include "resourceeditorconstants.hpp"

#include <core/core-file-icon-provider.hpp>
#include <core/core-editor-manager.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <qdebug.h>

using namespace ResourceEditor::Internal;
using namespace ResourceEditor::Constants;

ResourceEditorFactory::ResourceEditorFactory(ResourceEditorPlugin *plugin)
{
  setId(RESOURCEEDITOR_ID);
  setMimeTypes(QStringList(QLatin1String(C_RESOURCE_MIMETYPE)));
  setDisplayName(QCoreApplication::translate("OpenWith::Editors", C_RESOURCEEDITOR_DISPLAY_NAME));

  Orca::Plugin::Core::registerIconOverlayForSuffix(ProjectExplorer::Constants::FILEOVERLAY_QRC, "qrc");

  setEditorCreator([plugin] {
    return new ResourceEditorW(Orca::Plugin::Core::Context(C_RESOURCEEDITOR), plugin);
  });
}
