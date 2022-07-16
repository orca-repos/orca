// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-base-file-filter.hpp"

#include "core-editor-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/linecolumn.hpp>
#include <utils/link.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QRegularExpression>

using namespace Utils;

namespace Orca::Plugin::Core {

class Data {
public:
  auto clear() -> void
  {
    iterator.clear();
    previous_result_paths.clear();
    previous_entry.clear();
  }

  QSharedPointer<BaseFileFilter::Iterator> iterator;
  FilePaths previous_result_paths;
  bool force_new_search_list{};
  QString previous_entry;
};

class BaseFileFilterPrivate {
public:
  Data m_data;
  Data m_current;
};

/*!
    \class Orca::Plugin::Core::BaseFileFilter
    \inheaderfile coreplugin/locator/basefilefilter.h
    \inmodule Orca

    \brief The BaseFileFilter class is a base class for locator filter classes.
*/

/*!
    \class Orca::Plugin::Core::BaseFileFilter::Iterator
    \inmodule Orca
    \internal
*/

/*!
    \class Orca::Plugin::Core::BaseFileFilter::ListIterator
    \inmodule Orca
    \internal
*/

BaseFileFilter::Iterator::~Iterator() = default;

/*!
    \internal
*/
BaseFileFilter::BaseFileFilter() : d(new BaseFileFilterPrivate)
{
  d->m_data.force_new_search_list = true;
  setFileIterator(new ListIterator({}));
}

/*!
    \internal
*/
BaseFileFilter::~BaseFileFilter()
{
  delete d;
}

/*!
    \reimp
*/
auto BaseFileFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  d->m_current = d->m_data;
  d->m_data.force_new_search_list = false;
}

auto BaseFileFilter::matchLevelFor(const QRegularExpressionMatch &match, const QString &match_text) -> MatchLevel
{
  const auto consecutive_pos = match.capturedStart(1);

  if (consecutive_pos == 0)
    return MatchLevel::Best;

  if (consecutive_pos > 0) {
    if (const auto prev_char = match_text.at(consecutive_pos - 1); prev_char == '_' || prev_char == '.')
      return MatchLevel::Better;
  }

  if (match.capturedStart() == 0)
    return MatchLevel::Good;

  return MatchLevel::Normal;
}

/*!
    \reimp
*/
auto BaseFileFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &orig_entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> entries[int(MatchLevel::Count)];

  // If search string contains spaces, treat them as wildcard '*' and search in full path
  const auto entry = QDir::fromNativeSeparators(orig_entry).replace(' ', '*');
  QString postfix;
  auto link = Link::fromString(entry, true, &postfix);
  const auto regexp = createRegExp(link.targetFilePath.toString());

  if (!regexp.isValid()) {
    d->m_current.clear(); // free memory
    return {};
  }

  auto contains_path_separator = [](const QString &candidate) {
    return candidate.contains('/') || candidate.contains('*');
  };

  const auto has_path_separator = contains_path_separator(link.targetFilePath.toString());
  const auto contains_previous_entry = !d->m_current.previous_entry.isEmpty() && link.targetFilePath.toString().contains(d->m_current.previous_entry);

  if (const auto path_separator_added = !contains_path_separator(d->m_current.previous_entry) && has_path_separator; !d->m_current.force_new_search_list && contains_previous_entry && !path_separator_added)
    d->m_current.iterator.reset(new ListIterator(d->m_current.previous_result_paths));

  QTC_ASSERT(d->m_current.iterator.data(), return QList<LocatorFilterEntry>());
  d->m_current.previous_result_paths.clear();
  d->m_current.previous_entry = link.targetFilePath.toString();
  d->m_current.iterator->toFront();

  auto canceled = false;
  while (d->m_current.iterator->hasNext()) {
    if (future.isCanceled()) {
      canceled = true;
      break;
    }

    d->m_current.iterator->next();

    auto path = d->m_current.iterator->filePath();
    auto match_text = has_path_separator ? path.toString() : path.fileName();

    if (auto match = regexp.match(match_text); match.hasMatch()) {
      LocatorFilterEntry filter_entry(this, path.fileName(), QString(path.toString() + postfix));
      filter_entry.file_path = path;
      filter_entry.extra_info = path.shortNativePath();

      const auto match_level = matchLevelFor(match, match_text);
      if (has_path_separator) {
        match = regexp.match(filter_entry.extra_info);
        filter_entry.highlight_info = highlightInfo(match, LocatorFilterEntry::HighlightInfo::ExtraInfo);
      } else {
        filter_entry.highlight_info = highlightInfo(match);
      }

      entries[static_cast<int>(match_level)].append(filter_entry);
      d->m_current.previous_result_paths.append(path);
    }
  }

  if (canceled) {
    // we keep the old list of previous search results if this search was canceled
    // so a later search without forceNewSearchList will use that previous list instead of an
    // incomplete list of a canceled search
    d->m_current.clear(); // free memory
  } else {
    d->m_current.iterator.clear();
    QMetaObject::invokeMethod(this, &BaseFileFilter::updatePreviousResultData, Qt::QueuedConnection);
  }

  for (auto &entry : entries) {
    if (entry.size() < 1000)
      sort(entry, LocatorFilterEntry::compareLexigraphically);
  }

  return std::accumulate(std::begin(entries), std::end(entries), QList<LocatorFilterEntry>());
}

/*!
    \reimp
*/
auto BaseFileFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)
  openEditorAt(selection);
}

auto BaseFileFilter::openEditorAt(const LocatorFilterEntry &selection) -> void
{
  const auto locator_text = FilePath::fromVariant(selection.internal_data);

  if (const int postfix_length = locator_text.fileName().length() - selection.file_path.fileName().length(); postfix_length > 0) {
    const auto postfix = selection.internal_data.toString().right(postfix_length);
    auto postfix_pos = -1;
    const auto line_column = LineColumn::extractFromFileName(postfix, postfix_pos);
    if (postfix_pos >= 0) {
      const Link link(selection.file_path, line_column.line, line_column.column);
      EditorManager::openEditorAt(link, {}, EditorManager::AllowExternalEditor);
      return;
    }
  }

  EditorManager::openEditor(selection.file_path, {}, EditorManager::AllowExternalEditor);
}

/*!
   Takes ownership of the \a iterator. The previously set iterator might not be deleted until
   a currently running search is finished.
*/

auto BaseFileFilter::setFileIterator(Iterator *iterator) const -> void
{
  d->m_data.clear();
  d->m_data.force_new_search_list = true;
  d->m_data.iterator.reset(iterator);
}

/*!
    Returns the file iterator.
*/
auto BaseFileFilter::fileIterator() const -> QSharedPointer<Iterator>
{
  return d->m_data.iterator;
}

auto BaseFileFilter::updatePreviousResultData() const -> void
{
  if (d->m_data.force_new_search_list) // in the meantime the iterator was reset / cache invalidated
    return;                         // do not update with the new result list etc

  d->m_data.previous_entry = d->m_current.previous_entry;
  d->m_data.previous_result_paths = d->m_current.previous_result_paths;
  // forceNewSearchList was already reset in prepareSearch
}

BaseFileFilter::ListIterator::ListIterator(const FilePaths &file_paths)
{
  m_file_paths = file_paths;
  toFront();
}

auto BaseFileFilter::ListIterator::toFront() -> void
{
  m_path_position = m_file_paths.constBegin() - 1;
}

auto BaseFileFilter::ListIterator::hasNext() const -> bool
{
  QTC_ASSERT(m_path_position != m_file_paths.constEnd(), return false);
  return m_path_position + 1 != m_file_paths.constEnd();
}

auto BaseFileFilter::ListIterator::next() -> FilePath
{
  QTC_ASSERT(m_path_position != m_file_paths.constEnd(), return {});
  ++m_path_position;
  QTC_ASSERT(m_path_position != m_file_paths.constEnd(), return {});
  return *m_path_position;
}

auto BaseFileFilter::ListIterator::filePath() const -> FilePath
{
  QTC_ASSERT(m_path_position != m_file_paths.constEnd(), return {});
  return *m_path_position;
}

} // namespace Orca::Plugin::Core
