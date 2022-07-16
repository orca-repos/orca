// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>
#include <QMimeData>
#include <QSharedPointer>

namespace TextEditor {
namespace Internal {

class CircularClipboard {
public:
  static auto instance() -> CircularClipboard*;

  auto collect(const QMimeData *mimeData) -> void;
  auto collect(const QSharedPointer<const QMimeData> &mimeData) -> void;
  auto next() const -> QSharedPointer<const QMimeData>;
  auto toLastCollect() -> void;
  auto size() const -> int;

private:
  CircularClipboard();
  ~CircularClipboard();

  auto operator=(const CircularClipboard &) -> CircularClipboard&;

  mutable int m_current = -1;
  QList<QSharedPointer<const QMimeData>> m_items;
};

} // namespace Internal
} // namespace TextEditor
