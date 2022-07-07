// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iexternaleditor.hpp"

namespace Core {
namespace Internal {

class SystemEditor final : public IExternalEditor {
  Q_OBJECT

public:
  explicit SystemEditor();

  auto startEditor(const Utils::FilePath &file_path, QString *error_message) -> bool override;
};

} // namespace Internal
} // namespace Core
