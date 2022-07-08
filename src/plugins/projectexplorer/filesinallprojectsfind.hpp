// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "allprojectsfind.hpp"

namespace ProjectExplorer {
namespace Internal {

class FilesInAllProjectsFind : public AllProjectsFind {
  Q_OBJECT

public:
  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;

protected:
  auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* override;
  auto label() const -> QString override;
};

} // namespace Internal
} // namespace ProjectExplorer

