// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"
#include <utils/projectintropage.hpp>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT JsonProjectPage : public Utils::ProjectIntroPage {
  Q_OBJECT

public:
  JsonProjectPage(QWidget *parent = nullptr);

  auto initializePage() -> void override;
  auto validatePage() -> bool override;
  static auto uniqueProjectName(const QString &path) -> QString;
};

} // namespace ProjectExplorer
