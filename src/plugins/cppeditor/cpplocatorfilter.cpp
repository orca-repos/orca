// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpplocatorfilter.hpp"

#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"

#include <core/editormanager/editormanager.hpp>
#include <utils/algorithm.hpp>

#include <QRegularExpression>

#include <algorithm>
#include <numeric>

namespace CppEditor {

CppLocatorFilter::CppLocatorFilter(CppLocatorData *locatorData) : m_data(locatorData)
{
  setId(Constants::LOCATOR_FILTER_ID);
  setDisplayName(Constants::LOCATOR_FILTER_DISPLAY_NAME);
  setDefaultShortcutString(":");
  setDefaultIncludedByDefault(false);
}

CppLocatorFilter::~CppLocatorFilter() = default;

auto CppLocatorFilter::filterEntryFromIndexItem(IndexItem::Ptr info) -> Core::LocatorFilterEntry
{
  const auto id = QVariant::fromValue(info);
  Core::LocatorFilterEntry filterEntry(this, info->scopedSymbolName(), id, info->icon());
  if (info->type() == IndexItem::Class || info->type() == IndexItem::Enum)
    filterEntry.extra_info = info->shortNativeFilePath();
  else
    filterEntry.extra_info = info->symbolType();

  return filterEntry;
}

auto CppLocatorFilter::matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry>
{
  QList<Core::LocatorFilterEntry> entries[int(MatchLevel::Count)];
  const auto caseSensitivityForPrefix = caseSensitivity(entry);
  const auto wanted = matchTypes();

  const auto regexp = createRegExp(entry);
  if (!regexp.isValid())
    return {};
  const auto hasColonColon = entry.contains("::");
  const auto shortRegexp = hasColonColon ? createRegExp(entry.mid(entry.lastIndexOf("::") + 2)) : regexp;

  m_data->filterAllFiles([&](const IndexItem::Ptr &info) -> IndexItem::VisitorResult {
    if (future.isCanceled())
      return IndexItem::Break;
    const auto type = info->type();
    if (type & wanted) {
      const auto symbolName = info->symbolName();
      auto matchString = hasColonColon ? info->scopedSymbolName() : symbolName;
      int matchOffset = hasColonColon ? matchString.size() - symbolName.size() : 0;
      auto match = regexp.match(matchString);
      auto matchInParameterList = false;
      if (!match.hasMatch() && (type == IndexItem::Function)) {
        matchString += info->symbolType();
        match = regexp.match(matchString);
        matchInParameterList = true;
      }

      if (match.hasMatch()) {
        auto filterEntry = filterEntryFromIndexItem(info);

        // Highlight the matched characters, therefore it may be necessary
        // to update the match if the displayName is different from matchString
        if (QStringView(matchString).mid(matchOffset) != filterEntry.display_name) {
          match = shortRegexp.match(filterEntry.display_name);
          matchOffset = 0;
        }
        filterEntry.highlight_info = highlightInfo(match);
        if (matchInParameterList && filterEntry.highlight_info.starts.isEmpty()) {
          match = regexp.match(filterEntry.extra_info);
          filterEntry.highlight_info = highlightInfo(match);
          filterEntry.highlight_info.dataType = Core::LocatorFilterEntry::HighlightInfo::ExtraInfo;
        } else if (matchOffset > 0) {
          for (int &start : filterEntry.highlight_info.starts)
            start -= matchOffset;
        }

        if (matchInParameterList)
          entries[int(MatchLevel::Normal)].append(filterEntry);
        else if (filterEntry.display_name.startsWith(entry, caseSensitivityForPrefix))
          entries[int(MatchLevel::Best)].append(filterEntry);
        else if (filterEntry.display_name.contains(entry, caseSensitivityForPrefix))
          entries[int(MatchLevel::Better)].append(filterEntry);
        else
          entries[int(MatchLevel::Good)].append(filterEntry);
      }
    }

    if (info->type() & IndexItem::Enum)
      return IndexItem::Continue;
    else
      return IndexItem::Recurse;
  });

  for (auto &entry : entries) {
    if (entry.size() < 1000)
      Utils::sort(entry, Core::LocatorFilterEntry::compareLexigraphically);
  }

  return std::accumulate(std::begin(entries), std::end(entries), QList<Core::LocatorFilterEntry>());
}

auto CppLocatorFilter::accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void
{
  Q_UNUSED(newText)
  Q_UNUSED(selectionStart)
  Q_UNUSED(selectionLength)
  auto info = qvariant_cast<IndexItem::Ptr>(selection.internal_data);
  Core::EditorManager::openEditorAt({Utils::FilePath::fromString(info->fileName()), info->line(), info->column()}, {}, Core::EditorManager::AllowExternalEditor);
}

CppClassesFilter::CppClassesFilter(CppLocatorData *locatorData) : CppLocatorFilter(locatorData)
{
  setId(Constants::CLASSES_FILTER_ID);
  setDisplayName(Constants::CLASSES_FILTER_DISPLAY_NAME);
  setDefaultShortcutString("c");
  setDefaultIncludedByDefault(false);
}

CppClassesFilter::~CppClassesFilter() = default;

auto CppClassesFilter::filterEntryFromIndexItem(IndexItem::Ptr info) -> Core::LocatorFilterEntry
{
  const auto id = QVariant::fromValue(info);
  Core::LocatorFilterEntry filterEntry(this, info->symbolName(), id, info->icon());
  filterEntry.extra_info = info->symbolScope().isEmpty() ? info->shortNativeFilePath() : info->symbolScope();
  filterEntry.file_path = Utils::FilePath::fromString(info->fileName());
  return filterEntry;
}

CppFunctionsFilter::CppFunctionsFilter(CppLocatorData *locatorData) : CppLocatorFilter(locatorData)
{
  setId(Constants::FUNCTIONS_FILTER_ID);
  setDisplayName(Constants::FUNCTIONS_FILTER_DISPLAY_NAME);
  setDefaultShortcutString("m");
  setDefaultIncludedByDefault(false);
}

CppFunctionsFilter::~CppFunctionsFilter() = default;

auto CppFunctionsFilter::filterEntryFromIndexItem(IndexItem::Ptr info) -> Core::LocatorFilterEntry
{
  const auto id = QVariant::fromValue(info);

  auto name = info->symbolName();
  auto extraInfo = info->symbolScope();
  info->unqualifiedNameAndScope(name, &name, &extraInfo);
  if (extraInfo.isEmpty()) {
    extraInfo = info->shortNativeFilePath();
  } else {
    extraInfo.append(" (" + Utils::FilePath::fromString(info->fileName()).fileName() + ')');
  }

  Core::LocatorFilterEntry filterEntry(this, name + info->symbolType(), id, info->icon());
  filterEntry.extra_info = extraInfo;

  return filterEntry;
}

} // namespace CppEditor
