// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonprojectpage.hpp"
#include "jsonwizard.hpp"

#include <core/documentmanager.hpp>

#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QVariant>

using namespace Utils;

namespace ProjectExplorer {

JsonProjectPage::JsonProjectPage(QWidget *parent) : ProjectIntroPage(parent) { }

auto JsonProjectPage::initializePage() -> void
{
  const auto wiz = qobject_cast<JsonWizard*>(wizard());
  QTC_ASSERT(wiz, return);
  setFilePath(FilePath::fromString(wiz->stringValue(QLatin1String("InitialPath"))));

  setProjectName(uniqueProjectName(filePath().toString()));
}

auto JsonProjectPage::validatePage() -> bool
{
  if (isComplete() && useAsDefaultPath()) {
    // Store the path as default path for new projects if desired.
    Core::DocumentManager::setProjectsDirectory(filePath());
    Core::DocumentManager::setUseProjectsDirectory(true);
  }

  const auto target = filePath().pathAppended(projectName());

  wizard()->setProperty("ProjectDirectory", target.toString());
  wizard()->setProperty("TargetPath", target.toString());

  return ProjectIntroPage::validatePage();
}

auto JsonProjectPage::uniqueProjectName(const QString &path) -> QString
{
  const QDir pathDir(path);
  //: File path suggestion for a new project. If you choose
  //: to translate it, make sure it is a valid path name without blanks
  //: and using only ascii chars.
  const auto prefix = tr("untitled");
  for (unsigned i = 0; ; ++i) {
    auto name = prefix;
    if (i)
      name += QString::number(i);
    if (!pathDir.exists(name))
      return name;
  }
}

} // namespace ProjectExplorer
