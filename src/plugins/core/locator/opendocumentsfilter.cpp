// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "opendocumentsfilter.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/locator/basefilefilter.hpp>

#include <utils/fileutils.hpp>
#include <utils/link.hpp>

#include <QAbstractItemModel>
#include <QMutexLocker>
#include <QRegularExpression>

namespace Core {
namespace Internal {

OpenDocumentsFilter::OpenDocumentsFilter()
{
  setId("Open documents");
  setDisplayName(tr("Open Documents"));
  setDefaultShortcutString("o");
  setPriority(High);
  setDefaultIncludedByDefault(true);

  connect(DocumentModel::model(), &QAbstractItemModel::dataChanged, this, &OpenDocumentsFilter::refreshInternally);
  connect(DocumentModel::model(), &QAbstractItemModel::rowsInserted, this, &OpenDocumentsFilter::refreshInternally);
  connect(DocumentModel::model(), &QAbstractItemModel::rowsRemoved, this, &OpenDocumentsFilter::refreshInternally);
}

auto OpenDocumentsFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> good_entries;
  QList<LocatorFilterEntry> better_entries;
  QString postfix;

  auto link = Utils::Link::fromString(entry, true, &postfix);
  const auto regexp = createRegExp(link.targetFilePath.toString());

  if (!regexp.isValid())
    return good_entries;

  for (const auto editor_entries = editors(); const auto &editor_entry : editor_entries) {
    if (future.isCanceled())
      break;

    auto file_name = editor_entry.file_name.toString();

    if (file_name.isEmpty())
      continue;

    auto display_name = editor_entry.display_name;

    if (const auto match = regexp.match(display_name); match.hasMatch()) {
      LocatorFilterEntry filter_entry(this, display_name, QString(file_name + postfix));
      filter_entry.file_path = Utils::FilePath::fromString(file_name);
      filter_entry.extra_info = filter_entry.file_path.shortNativePath();
      filter_entry.highlight_info = highlightInfo(match);

      if (match.capturedStart() == 0)
        better_entries.append(filter_entry);
      else
        good_entries.append(filter_entry);
    }
  }

  better_entries.append(good_entries);
  return better_entries;
}

auto OpenDocumentsFilter::refreshInternally() -> void
{
  QMutexLocker lock(&m_mutex);
  m_editors.clear();

  for (const auto document_entries = DocumentModel::entries(); const auto e : document_entries) {
    Entry entry;
    // create copy with only the information relevant to use
    // to avoid model deleting entries behind our back
    entry.display_name = e->displayName();
    entry.file_name = e->fileName();
    m_editors.append(entry);
  }
}

auto OpenDocumentsFilter::editors() const -> QList<Entry>
{
  QMutexLocker lock(&m_mutex);
  return m_editors;
}

auto OpenDocumentsFilter::refresh(QFutureInterface<void> &future) -> void
{
  Q_UNUSED(future)
  QMetaObject::invokeMethod(this, &OpenDocumentsFilter::refreshInternally, Qt::QueuedConnection);
}

auto OpenDocumentsFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)
  BaseFileFilter::openEditorAt(selection);
}

} // namespace Internal
} // namespace Core
