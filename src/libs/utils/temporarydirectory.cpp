// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "temporarydirectory.hpp"

#include "fileutils.hpp"

#include <QtCore/QCoreApplication>

#include "qtcassert.hpp"

namespace Utils {

static QTemporaryDir* m_masterTemporaryDir = nullptr;

static auto cleanupMasterTemporaryDir() -> void
{
  delete m_masterTemporaryDir;
  m_masterTemporaryDir = nullptr;
}

TemporaryDirectory::TemporaryDirectory(const QString &pattern) : QTemporaryDir(m_masterTemporaryDir->path() + '/' + pattern)
{
  QTC_CHECK(!QFileInfo(pattern).isAbsolute());
}

auto TemporaryDirectory::masterTemporaryDirectory() -> QTemporaryDir*
{
  return m_masterTemporaryDir;
}

auto TemporaryDirectory::setMasterTemporaryDirectory(const QString &pattern) -> void
{
  if (m_masterTemporaryDir)
    cleanupMasterTemporaryDir();
  else
    qAddPostRoutine(cleanupMasterTemporaryDir);
  m_masterTemporaryDir = new QTemporaryDir(pattern);
}

auto TemporaryDirectory::masterDirectoryPath() -> QString
{
  return m_masterTemporaryDir->path();
}

auto TemporaryDirectory::masterDirectoryFilePath() -> FilePath
{
  return FilePath::fromString(TemporaryDirectory::masterDirectoryPath());
}

auto TemporaryDirectory::path() const -> FilePath
{
  return FilePath::fromString(QTemporaryDir::path());
}

auto TemporaryDirectory::filePath(const QString &fileName) const -> FilePath
{
  return FilePath::fromString(QTemporaryDir::filePath(fileName));
}

} // namespace Utils
