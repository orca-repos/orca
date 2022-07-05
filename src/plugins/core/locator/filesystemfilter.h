// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ilocatorfilter.h"
#include "ui_filesystemfilter.h"

#include <QFutureInterface>
#include <QList>
#include <QString>

namespace Core {
namespace Internal {

class FileSystemFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  explicit FileSystemFilter();
  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  auto restoreState(const QByteArray &state) -> void override;
  auto openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool override;

protected:
  auto saveState(QJsonObject &object) const -> void final;
  auto restoreState(const QJsonObject &object) -> void final;

private:
  static auto matchLevelFor(const QRegularExpressionMatch &match, const QString &match_text) -> MatchLevel;

  static constexpr bool k_include_hidden_default = true;
  bool m_include_hidden = k_include_hidden_default;
  bool m_current_include_hidden = k_include_hidden_default;
  QString m_current_document_directory;
};

} // namespace Internal
} // namespace Core
