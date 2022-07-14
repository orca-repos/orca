// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-base-file-filter.hpp"
#include "core-global.hpp"

#include <QFutureInterface>
#include <QMutex>
#include <QString>

namespace Orca::Plugin::Core {
namespace Ui {
class DirectoryFilterOptions;
} // namespace Ui

class CORE_EXPORT DirectoryFilter : public BaseFileFilter {
  Q_OBJECT

public:
  explicit DirectoryFilter(Utils::Id id);

  auto restoreState(const QByteArray &state) -> void override;
  auto openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool override;
  auto refresh(QFutureInterface<void> &future) -> void override;
  auto setIsCustomFilter(bool value) -> void;
  auto setDirectories(const QStringList &directories) -> void;
  auto addDirectory(const QString &directory) -> void;
  auto removeDirectory(const QString &directory) -> void;
  auto directories() const -> QStringList;
  auto setFilters(const QStringList &filters) -> void;
  auto setExclusionFilters(const QStringList &exclusion_filters) -> void;

protected:
  auto saveState(QJsonObject &object) const -> void override;
  auto restoreState(const QJsonObject &object) -> void override;

private:
  auto handleAddDirectory() const -> void;
  auto handleEditDirectory() const -> void;
  auto handleRemoveDirectory() const -> void;
  auto updateOptionButtons() const -> void;
  auto updateFileIterator() const -> void;

  QStringList m_directories;
  QStringList m_filters;
  QStringList m_exclusion_filters;
  // Our config dialog, uses in addDirectory and editDirectory
  // to give their dialogs the right parent
  QDialog *m_dialog = nullptr;
  Ui::DirectoryFilterOptions *m_ui = nullptr;
  mutable QMutex m_lock;
  Utils::FilePaths m_files;
  bool m_is_custom_filter = true;
};

} // namespace Orca::Plugin::Core
