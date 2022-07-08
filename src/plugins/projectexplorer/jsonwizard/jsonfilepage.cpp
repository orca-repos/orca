// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonfilepage.hpp"

#include "jsonwizard.hpp"

#include <utils/filepath.hpp>

using namespace Utils;

namespace ProjectExplorer {

JsonFilePage::JsonFilePage(QWidget *parent) : FileWizardPage(parent)
{
  setAllowDirectoriesInFileSelector(true);
}

auto JsonFilePage::initializePage() -> void
{
  const auto wiz = qobject_cast<JsonWizard*>(wizard());
  if (!wiz)
    return;

  if (fileName().isEmpty())
    setFileName(wiz->stringValue(QLatin1String("InitialFileName")));
  if (filePath().isEmpty())
    setPath(wiz->stringValue(QLatin1String("InitialPath")));
  setDefaultSuffix(wiz->stringValue("DefaultSuffix"));
}

auto JsonFilePage::validatePage() -> bool
{
  if (filePath().isEmpty() || fileName().isEmpty())
    return false;

  const auto dir = filePath();
  if (!dir.isDir())
    return false;

  const auto target = dir.resolvePath(fileName());

  wizard()->setProperty("TargetPath", target.toString());
  return true;
}

} // namespace ProjectExplorer
