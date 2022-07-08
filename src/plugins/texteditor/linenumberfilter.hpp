// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/ilocatorfilter.hpp>

#include <QString>
#include <QList>
#include <QFutureInterface>

namespace Core {
class IEditor;
}

namespace TextEditor {
namespace Internal {

class LineNumberFilter : public Core::ILocatorFilter {
  Q_OBJECT public:
  explicit LineNumberFilter(QObject *parent = nullptr);

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry> override;
  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

private:
  bool m_hasCurrentEditor = false;
};

} // namespace Internal
} // namespace TextEditor
