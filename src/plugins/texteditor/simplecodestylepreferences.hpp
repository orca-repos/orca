// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "icodestylepreferences.hpp"

namespace TextEditor {

class TEXTEDITOR_EXPORT SimpleCodeStylePreferences : public ICodeStylePreferences {
public:
  explicit SimpleCodeStylePreferences(QObject *parentObject = nullptr);

  auto value() const -> QVariant override;
  auto setValue(const QVariant &) -> void override;
};

} // namespace TextEditor
