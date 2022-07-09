// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <utils/fileutils.hpp>

#include <QHash>
#include <QString>
#include <QPair>

namespace CppEditor {

class CPPEDITOR_EXPORT WorkingCopy {
public:
  WorkingCopy();

  using Table = QHash<Utils::FilePath, QPair<QByteArray, unsigned>>;

  auto insert(const QString &fileName, const QByteArray &source, unsigned revision = 0) -> void { insert(Utils::FilePath::fromString(fileName), source, revision); }
  auto insert(const Utils::FilePath &fileName, const QByteArray &source, unsigned revision = 0) -> void { _elements.insert(fileName, qMakePair(source, revision)); }
  auto contains(const QString &fileName) const -> bool { return contains(Utils::FilePath::fromString(fileName)); }
  auto contains(const Utils::FilePath &fileName) const -> bool { return _elements.contains(fileName); }
  auto source(const QString &fileName) const -> QByteArray { return source(Utils::FilePath::fromString(fileName)); }
  auto source(const Utils::FilePath &fileName) const -> QByteArray { return _elements.value(fileName).first; }
  auto revision(const QString &fileName) const -> unsigned { return revision(Utils::FilePath::fromString(fileName)); }
  auto revision(const Utils::FilePath &fileName) const -> unsigned { return _elements.value(fileName).second; }
  auto get(const QString &fileName) const -> QPair<QByteArray, unsigned> { return get(Utils::FilePath::fromString(fileName)); }
  auto get(const Utils::FilePath &fileName) const -> QPair<QByteArray, unsigned> { return _elements.value(fileName); }
  auto elements() const -> const Table& { return _elements; }
  auto size() const -> int { return _elements.size(); }

private:
  Table _elements;
};

} // namespace CppEditor
