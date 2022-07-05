// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "temporaryfile.h"

#include "temporarydirectory.h"
#include "qtcassert.h"

namespace Utils {

TemporaryFile::TemporaryFile(const QString &pattern) : QTemporaryFile(TemporaryDirectory::masterTemporaryDirectory()->path() + '/' + pattern)
{
  QTC_CHECK(!QFileInfo(pattern).isAbsolute());
}

} // namespace Utils
