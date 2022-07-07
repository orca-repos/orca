// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "temporaryfile.hpp"

#include "temporarydirectory.hpp"
#include "qtcassert.hpp"

namespace Utils {

TemporaryFile::TemporaryFile(const QString &pattern) : QTemporaryFile(TemporaryDirectory::masterTemporaryDirectory()->path() + '/' + pattern)
{
  QTC_CHECK(!QFileInfo(pattern).isAbsolute());
}

} // namespace Utils
