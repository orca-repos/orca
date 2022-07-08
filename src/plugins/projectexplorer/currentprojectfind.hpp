// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "allprojectsfind.hpp"

namespace ProjectExplorer {

class Project;

namespace Internal {

class CurrentProjectFind : public AllProjectsFind {
  Q_OBJECT

public:
  CurrentProjectFind();

  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto isEnabled() const -> bool override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;

protected:
  auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* override;
  auto additionalParameters() const -> QVariant override;
  auto label() const -> QString override;

private:
  auto handleProjectChanged() -> void;
  auto recheckEnabled() -> void;
};

} // namespace Internal
} // namespace ProjectExplorer
