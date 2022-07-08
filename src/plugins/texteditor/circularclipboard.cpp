// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "circularclipboard.hpp"

using namespace TextEditor::Internal;

static constexpr int kMaxSize = 10;

CircularClipboard::CircularClipboard() = default;
CircularClipboard::~CircularClipboard() = default;

auto CircularClipboard::instance() -> CircularClipboard*
{
  static CircularClipboard clipboard;
  return &clipboard;
}

auto CircularClipboard::collect(const QMimeData *mimeData) -> void
{
  collect(QSharedPointer<const QMimeData>(mimeData));
}

auto CircularClipboard::collect(const QSharedPointer<const QMimeData> &mimeData) -> void
{
  //Avoid duplicates
  const auto text = mimeData->text();
  for (auto i = m_items.begin(); i != m_items.end(); ++i) {
    if (mimeData == *i || text == (*i)->text()) {
      m_items.erase(i);
      break;
    }
  }
  if (m_items.size() >= kMaxSize)
    m_items.removeLast();
  m_items.prepend(mimeData);
}

auto CircularClipboard::next() const -> QSharedPointer<const QMimeData>
{
  if (m_items.isEmpty())
    return QSharedPointer<const QMimeData>();

  if (m_current == m_items.length() - 1)
    m_current = 0;
  else
    ++m_current;

  return m_items.at(m_current);
}

auto CircularClipboard::toLastCollect() -> void
{
  m_current = -1;
}

auto CircularClipboard::size() const -> int
{
  return m_items.size();
}
