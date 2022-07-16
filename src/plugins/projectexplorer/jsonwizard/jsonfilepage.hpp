// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filewizardpage.hpp>

namespace ProjectExplorer {

class JsonFilePage : public Utils::FileWizardPage {
  Q_OBJECT

public:
  JsonFilePage(QWidget *parent = nullptr);

  auto initializePage() -> void override;
  auto validatePage() -> bool override;
};

} // namespace ProjectExplorer
