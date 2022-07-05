// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filecrumblabel.h"

#include <utils/hostosinfo.h>

#include <QDir>
#include <QUrl>

namespace Utils {

FileCrumbLabel::FileCrumbLabel(QWidget *parent) : QLabel(parent)
{
  setTextFormat(Qt::RichText);
  setWordWrap(true);
  connect(this, &QLabel::linkActivated, this, [this](const QString &url) {
    emit pathClicked(FilePath::fromString(QUrl(url).toLocalFile()));
  });
  setPath(FilePath());
}

static auto linkForPath(const FilePath &path, const QString &display) -> QString
{
  return "<a href=\"" + QUrl::fromLocalFile(path.toString()).toString(QUrl::FullyEncoded) + "\">" + display + "</a>";
}

auto FileCrumbLabel::setPath(const FilePath &path) -> void
{
  QStringList links;
  FilePath current = path;
  while (!current.isEmpty()) {
    const QString fileName = current.fileName();
    if (!fileName.isEmpty()) {
      links.prepend(linkForPath(current, fileName));
    } else if (HostOsInfo::isWindowsHost() && QDir(current.toString()).isRoot()) {
      // Only on Windows add the drive letter, without the '/' at the end
      QString display = current.toString();
      if (display.endsWith('/'))
        display.chop(1);
      links.prepend(linkForPath(current, display));
    }
    current = current.parentDir();
  }
  const auto pathSeparator = HostOsInfo::isWindowsHost() ? QLatin1String("&nbsp;\\ ") : QLatin1String("&nbsp;/ ");
  const QString prefix = HostOsInfo::isWindowsHost() ? QString("\\ ") : QString("/ ");
  setText(prefix + links.join(pathSeparator));
}

} // Utils
