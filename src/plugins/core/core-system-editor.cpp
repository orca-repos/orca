// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-system-editor.hpp"

#include <utils/fileutils.hpp>

#include <QDesktopServices>
#include <QStringList>
#include <QUrl>

namespace Orca::Plugin::Core {

SystemEditor::SystemEditor()
{
  setId("CorePlugin.OpenWithSystemEditor");
  setDisplayName(tr("System Editor"));
  setMimeTypes({"application/octet-stream"});
}

auto SystemEditor::startEditor(const Utils::FilePath &file_path, QString *error_message) -> bool
{
  QUrl url;
  url.setPath(file_path.toString());
  url.setScheme(QLatin1String("file"));

  if (!QDesktopServices::openUrl(url)) {
    if (error_message)
      *error_message = tr("Could not open URL %1.").arg(url.toString());
    return false;
  }

  return true;
}

} // namespace Orca::Plugin::Core
