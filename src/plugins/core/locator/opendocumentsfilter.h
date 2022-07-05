// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ilocatorfilter.h"

#include <core/editormanager/documentmodel.h>

#include <QFutureInterface>
#include <QList>
#include <QMutex>
#include <QString>

namespace Core {
namespace Internal {

class OpenDocumentsFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  OpenDocumentsFilter();

  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  auto refresh(QFutureInterface<void> &future) -> void override;

public slots:
  auto refreshInternally() -> void;

private:
  class Entry {
  public:
    Utils::FilePath file_name;
    QString display_name;
  };

  auto editors() const -> QList<Entry>;

  mutable QMutex m_mutex;
  QList<Entry> m_editors;
};

} // namespace Internal
} // namespace Core
