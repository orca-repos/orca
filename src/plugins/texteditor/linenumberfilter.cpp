// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "linenumberfilter.hpp"

#include "texteditor.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/icore.hpp>
#include <core/modemanager.hpp>

#include <QMetaType>
#include <QPair>
#include <QVariant>

using LineColumn = QPair<int, int>;
Q_DECLARE_METATYPE(LineColumn)

using namespace Core;

namespace TextEditor {
namespace Internal {

LineNumberFilter::LineNumberFilter(QObject *parent) : ILocatorFilter(parent)
{
  setId("Line in current document");
  setDisplayName(tr("Line in Current Document"));
  setDescription(tr("Jumps to the given line in the current document."));
  setPriority(High);
  setDefaultShortcutString("l");
  setDefaultIncludedByDefault(true);
}

auto LineNumberFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  m_hasCurrentEditor = EditorManager::currentEditor() != nullptr;
}

auto LineNumberFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> value;
  const auto lineAndColumn = entry.split(':');
  const int sectionCount = lineAndColumn.size();
  auto line = 0;
  auto column = 0;
  auto ok = false;
  if (sectionCount > 0)
    line = lineAndColumn.at(0).toInt(&ok);
  if (ok && sectionCount > 1)
    column = lineAndColumn.at(1).toInt(&ok);
  if (!ok)
    return value;
  if (m_hasCurrentEditor && (line > 0 || column > 0)) {
    LineColumn data;
    data.first = line;
    data.second = column - 1; // column API is 0-based
    QString text;
    if (line > 0 && column > 0)
      text = tr("Line %1, Column %2").arg(line).arg(column);
    else if (line > 0)
      text = tr("Line %1").arg(line);
    else
      text = tr("Column %1").arg(column);
    value.append(LocatorFilterEntry(this, text, QVariant::fromValue(data)));
  }
  return value;
}

auto LineNumberFilter::accept(const LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void
{
  Q_UNUSED(newText)
  Q_UNUSED(selectionStart)
  Q_UNUSED(selectionLength)
  IEditor *editor = EditorManager::currentEditor();
  if (editor) {
    EditorManager::addCurrentPositionToNavigationHistory();
    LineColumn data = selection.internal_data.value<LineColumn>();
    if (data.first < 1) // jump to column in same line
      data.first = editor->currentLine();
    editor->gotoLine(data.first, data.second);
    EditorManager::activateEditor(editor);
  }
}

} // namespace Internal
} // namespace TextEditor
