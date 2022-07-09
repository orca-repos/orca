// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppfileiterationorder.hpp"

#include <utils/qtcassert.hpp>

namespace CppEditor {

FileIterationOrder::Entry::Entry(const QString &filePath, const QString &projectPartId, int commonPrefixLength, int commonProjectPartPrefixLength) : filePath(filePath), projectPartId(projectPartId), commonFilePathPrefixLength(commonPrefixLength), commonProjectPartPrefixLength(commonProjectPartPrefixLength) {}

namespace {

auto cmpPrefixLengthAndText(int prefixLength1, int prefixLength2) -> bool
{
  return prefixLength1 > prefixLength2;
}

auto cmpLessFilePath(const FileIterationOrder::Entry &first, const FileIterationOrder::Entry &second) -> bool
{
  return cmpPrefixLengthAndText(first.commonFilePathPrefixLength, second.commonFilePathPrefixLength);
}

auto cmpLessProjectPartId(const FileIterationOrder::Entry &first, const FileIterationOrder::Entry &second) -> bool
{
  return cmpPrefixLengthAndText(first.commonProjectPartPrefixLength, second.commonProjectPartPrefixLength);
}

auto hasProjectPart(const FileIterationOrder::Entry &entry) -> bool
{
  return !entry.projectPartId.isEmpty();
}

} // anonymous namespace

auto operator<(const FileIterationOrder::Entry &first, const FileIterationOrder::Entry &second) -> bool
{
  if (hasProjectPart(first)) {
    if (hasProjectPart(second)) {
      if (first.projectPartId == second.projectPartId)
        return cmpLessFilePath(first, second);
      else
        return cmpLessProjectPartId(first, second);
    } else {
      return true;
    }
  } else {
    if (hasProjectPart(second))
      return false;
    else
      return cmpLessFilePath(first, second);
  }
}

FileIterationOrder::FileIterationOrder() = default;

FileIterationOrder::FileIterationOrder(const QString &referenceFilePath, const QString &referenceProjectPartId)
{
  setReference(referenceFilePath, referenceProjectPartId);
}

auto FileIterationOrder::setReference(const QString &filePath, const QString &projectPartId) -> void
{
  m_referenceFilePath = filePath;
  m_referenceProjectPartId = projectPartId;
}

auto FileIterationOrder::isValid() const -> bool
{
  return !m_referenceFilePath.isEmpty();
}

static auto commonPrefixLength(const QString &filePath1, const QString &filePath2) -> int
{
  const auto mismatches = std::mismatch(filePath1.begin(), filePath1.end(), filePath2.begin(), filePath2.end());
  return mismatches.first - filePath1.begin();
}

auto FileIterationOrder::createEntryFromFilePath(const QString &filePath, const QString &projectPartId) const -> FileIterationOrder::Entry
{
  const auto filePrefixLength = commonPrefixLength(m_referenceFilePath, filePath);
  const auto projectPartPrefixLength = commonPrefixLength(m_referenceProjectPartId, projectPartId);
  return Entry(filePath, projectPartId, filePrefixLength, projectPartPrefixLength);
}

auto FileIterationOrder::insert(const QString &filePath, const QString &projectPartId) -> void
{
  const auto entry = createEntryFromFilePath(filePath, projectPartId);
  m_set.insert(entry);
}

auto FileIterationOrder::remove(const QString &filePath, const QString &projectPartId) -> void
{
  const auto needleElement = createEntryFromFilePath(filePath, projectPartId);
  const auto range = m_set.equal_range(needleElement);

  const auto toRemove = std::find_if(range.first, range.second, [filePath](const Entry &entry) {
    return entry.filePath == filePath;
  });
  QTC_ASSERT(toRemove != range.second, return);
  m_set.erase(toRemove);
}

auto FileIterationOrder::toStringList() const -> QStringList
{
  QStringList result;

  for (const auto &entry : m_set)
    result.append(entry.filePath);

  return result;
}

} // namespace CppEditor
