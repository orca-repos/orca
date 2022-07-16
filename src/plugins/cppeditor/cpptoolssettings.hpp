// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QObject>

namespace TextEditor {
class CommentsSettings;
}

namespace CppEditor
{
class CppCodeStylePreferences;

namespace Internal {
class CppToolsSettingsPrivate;
}

/**
 * This class provides a central place for cpp tools settings.
 */
class CPPEDITOR_EXPORT CppToolsSettings : public QObject {
  Q_OBJECT

public:
  CppToolsSettings();
  ~CppToolsSettings() override;

  static auto instance() -> CppToolsSettings*;
  auto cppCodeStyle() const -> CppCodeStylePreferences*;
  auto commentsSettings() const -> const TextEditor::CommentsSettings&;
  auto setCommentsSettings(const TextEditor::CommentsSettings &commentsSettings) -> void;
  auto sortedEditorDocumentOutline() const -> bool;
  auto setSortedEditorDocumentOutline(bool sorted) -> void;
  auto showHeaderErrorInfoBar() const -> bool;
  auto setShowHeaderErrorInfoBar(bool show) -> void;
  auto showNoProjectInfoBar() const -> bool;
  auto setShowNoProjectInfoBar(bool show) -> void;

signals:
  auto editorDocumentOutlineSortingChanged(bool isSorted) -> void;
  auto showHeaderErrorInfoBarChanged(bool isShown) -> void;
  auto showNoProjectInfoBarChanged(bool isShown) -> void;

private:
  Internal::CppToolsSettingsPrivate *d;
  static CppToolsSettings *m_instance;
};

} // namespace CppEditor
