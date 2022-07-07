// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.hpp"
#include "porting.hpp"

#include <QString>
#include <qmetatype.h>

#include <functional>


namespace Utils {

class ORCA_UTILS_EXPORT Link {
public:
  Link(const Utils::FilePath &filePath = Utils::FilePath(), int line = 0, int column = 0) : targetFilePath(filePath), targetLine(line), targetColumn(column) {}

  static auto fromString(const QString &fileName, bool canContainLineNumber = false, QString *postfix = nullptr) -> Link;
  static auto fromFilePath(const FilePath &filePath, bool canContainLineNumber = false, QString *postfix = nullptr) -> Link;

  auto hasValidTarget() const -> bool { return !targetFilePath.isEmpty(); }
  auto hasValidLinkText() const -> bool { return linkTextStart != linkTextEnd; }

  auto operator==(const Link &other) const -> bool
  {
    return targetFilePath == other.targetFilePath && targetLine == other.targetLine && targetColumn == other.targetColumn && linkTextStart == other.linkTextStart && linkTextEnd == other.linkTextEnd;
  }

  auto operator!=(const Link &other) const -> bool { return !(*this == other); }

  int linkTextStart = -1;
  int linkTextEnd = -1;

  Utils::FilePath targetFilePath;
  int targetLine;
  int targetColumn;
};

ORCA_UTILS_EXPORT auto qHash(const Link &l) -> QHashValueType;

using ProcessLinkCallback = std::function<void(const Link &)>;

} // namespace Utils

Q_DECLARE_METATYPE(Utils::Link)
