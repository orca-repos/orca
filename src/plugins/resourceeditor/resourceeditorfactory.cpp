// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourceeditorfactory.hpp"
#include "resourceeditorw.hpp"
#include "resourceeditorplugin.hpp"
#include "resourceeditorconstants.hpp"

#include <core/fileiconprovider.hpp>
#include <core/editormanager/editormanager.hpp>
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

  Core::FileIconProvider::registerIconOverlayForSuffix(ProjectExplorer::Constants::FILEOVERLAY_QRC, "qrc");

  setEditorCreator([plugin] {
    return new ResourceEditorW(Core::Context(C_RESOURCEEDITOR), plugin);
  });
}
