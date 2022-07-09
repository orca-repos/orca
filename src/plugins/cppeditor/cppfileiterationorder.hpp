// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QStringList>

#include <set>

namespace CppEditor {

class CPPEDITOR_EXPORT FileIterationOrder {
public:
  struct Entry {
    Entry(const QString &filePath, const QString &projectPartId = QString(), int commonFilePathPrefixLength = 0, int commonProjectPartPrefixLength = 0);

    friend CPPEDITOR_EXPORT auto operator<(const Entry &first, const Entry &second) -> bool;

    const QString filePath;
    const QString projectPartId;
    int commonFilePathPrefixLength = 0;
    int commonProjectPartPrefixLength = 0;
  };

  FileIterationOrder();
  FileIterationOrder(const QString &referenceFilePath, const QString &referenceProjectPartId);

  auto setReference(const QString &filePath, const QString &projectPartId) -> void;
  auto isValid() const -> bool;
  auto insert(const QString &filePath, const QString &projectPartId = QString()) -> void;
  auto remove(const QString &filePath, const QString &projectPartId) -> void;
  auto toStringList() const -> QStringList;

private:
  auto createEntryFromFilePath(const QString &filePath, const QString &projectPartId) const -> Entry;

private:
  QString m_referenceFilePath;
  QString m_referenceProjectPartId;
  std::multiset<Entry> m_set;
};

CPPEDITOR_EXPORT auto operator<(const FileIterationOrder::Entry &first, const FileIterationOrder::Entry &second) -> bool;

} // namespace CppEditor
