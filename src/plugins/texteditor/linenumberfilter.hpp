// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-locator-filter-interface.hpp>

#include <QString>
#include <QList>
#include <QFutureInterface>

namespace Orca::Plugin::Core {
class IEditor;
}

namespace TextEditor {
namespace Internal {

class LineNumberFilter : public Orca::Plugin::Core::ILocatorFilter {
  Q_OBJECT public:
  explicit LineNumberFilter(QObject *parent = nullptr);

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Orca::Plugin::Core::LocatorFilterEntry> override;
  auto accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

private:
  bool m_hasCurrentEditor = false;
};

} // namespace Internal
} // namespace TextEditor
