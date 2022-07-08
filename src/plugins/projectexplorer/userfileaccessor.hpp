// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/fileutils.hpp>
#include <utils/settingsaccessor.hpp>

#include <QHash>
#include <QVariantMap>
#include <QMessageBox>

namespace ProjectExplorer {

class Project;

namespace Internal {

class UserFileAccessor : public Utils::MergingSettingsAccessor {
public:
  UserFileAccessor(Project *project);

  auto project() const -> Project*;
  virtual auto retrieveSharedSettings() const -> QVariant;
  auto projectUserFile() const -> Utils::FilePath;
  auto externalUserFile() const -> Utils::FilePath;
  auto sharedFile() const -> Utils::FilePath;

protected:
  auto postprocessMerge(const QVariantMap &main, const QVariantMap &secondary, const QVariantMap &result) const -> QVariantMap final;
  auto preprocessReadSettings(const QVariantMap &data) const -> QVariantMap final;
  auto prepareToWriteSettings(const QVariantMap &data) const -> QVariantMap final;
  auto merge(const SettingsMergeData &global, const SettingsMergeData &local) const -> Utils::SettingsMergeResult final;

private:
  auto userStickyTrackerFunction(QStringList &stickyKeys) const -> Utils::SettingsMergeFunction;

  Project *m_project;
};

} // namespace Internal
} // namespace ProjectExplorer
