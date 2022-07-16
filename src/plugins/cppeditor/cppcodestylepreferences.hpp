// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppcodestylesettings.hpp"

#include <texteditor/icodestylepreferences.hpp>

namespace CppEditor {

class CPPEDITOR_EXPORT CppCodeStylePreferences : public TextEditor::ICodeStylePreferences {
  Q_OBJECT

public:
  explicit CppCodeStylePreferences(QObject *parent = nullptr);
  auto value() const -> QVariant override;
  auto setValue(const QVariant &) -> void override;
  auto codeStyleSettings() const -> CppCodeStyleSettings;
  // tracks parent hierarchy until currentParentSettings is null
  auto currentCodeStyleSettings() const -> CppCodeStyleSettings;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &map) -> void override;

public slots:
  auto setCodeStyleSettings(const CppCodeStyleSettings &data) -> void;

signals:
  auto codeStyleSettingsChanged(const CppCodeStyleSettings &) -> void;
  auto currentCodeStyleSettingsChanged(const CppCodeStyleSettings &) -> void;

private:
  auto slotCurrentValueChanged(const QVariant &) -> void;

  CppCodeStyleSettings m_data;
};

} // namespace CppEditor
