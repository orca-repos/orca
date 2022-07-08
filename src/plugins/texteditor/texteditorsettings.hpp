// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <utils/id.hpp>

#include <QObject>

QT_BEGIN_NAMESPACE
template <typename Key, typename T>
class QMap;
QT_END_NAMESPACE

namespace TextEditor {

class FontSettings;
class TypingSettings;
class StorageSettings;
class BehaviorSettings;
class MarginSettings;
class DisplaySettings;
class CompletionSettings;
class HighlighterSettings;
class ExtraEncodingSettings;
class ICodeStylePreferences;
class ICodeStylePreferencesFactory;
class CodeStylePool;
class CommentsSettings;

/**
 * This class provides a central place for basic text editor settings. These
 * settings include font settings, tab settings, storage settings, behavior
 * settings, display settings and completion settings.
 */
class TEXTEDITOR_EXPORT TextEditorSettings : public QObject {
  Q_OBJECT

public:
  TextEditorSettings();
  ~TextEditorSettings() override;

  static auto instance() -> TextEditorSettings*;
  static auto fontSettings() -> const FontSettings&;
  static auto typingSettings() -> const TypingSettings&;
  static auto storageSettings() -> const StorageSettings&;
  static auto behaviorSettings() -> const BehaviorSettings&;
  static auto marginSettings() -> const MarginSettings&;
  static auto displaySettings() -> const DisplaySettings&;
  static auto completionSettings() -> const CompletionSettings&;
  static auto highlighterSettings() -> const HighlighterSettings&;
  static auto extraEncodingSettings() -> const ExtraEncodingSettings&;
  static auto commentsSettings() -> const CommentsSettings&;
  static auto codeStyleFactory(Utils::Id languageId) -> ICodeStylePreferencesFactory*;
  static auto codeStyleFactories() -> const QMap<Utils::Id, ICodeStylePreferencesFactory*>&;
  static auto registerCodeStyleFactory(ICodeStylePreferencesFactory *codeStyleFactory) -> void;
  static auto unregisterCodeStyleFactory(Utils::Id languageId) -> void;
  static auto codeStylePool() -> CodeStylePool*;
  static auto codeStylePool(Utils::Id languageId) -> CodeStylePool*;
  static auto registerCodeStylePool(Utils::Id languageId, CodeStylePool *pool) -> void;
  static auto unregisterCodeStylePool(Utils::Id languageId) -> void;
  static auto codeStyle() -> ICodeStylePreferences*;
  static auto codeStyle(Utils::Id languageId) -> ICodeStylePreferences*;
  static auto codeStyles() -> QMap<Utils::Id, ICodeStylePreferences*>;
  static auto registerCodeStyle(Utils::Id languageId, ICodeStylePreferences *prefs) -> void;
  static auto unregisterCodeStyle(Utils::Id languageId) -> void;
  static auto registerMimeTypeForLanguageId(const char *mimeType, Utils::Id languageId) -> void;
  static auto languageId(const QString &mimeType) -> Utils::Id;
  static auto increaseFontZoom(int step) -> int;
  static auto resetFontZoom() -> void;

signals:
  auto fontSettingsChanged(const FontSettings &) -> void;
  auto typingSettingsChanged(const TypingSettings &) -> void;
  auto storageSettingsChanged(const StorageSettings &) -> void;
  auto behaviorSettingsChanged(const BehaviorSettings &) -> void;
  auto marginSettingsChanged(const MarginSettings &) -> void;
  auto displaySettingsChanged(const DisplaySettings &) -> void;
  auto completionSettingsChanged(const CompletionSettings &) -> void;
  auto extraEncodingSettingsChanged(const ExtraEncodingSettings &) -> void;
  auto commentsSettingsChanged(const CommentsSettings &) -> void;
};

} // namespace TextEditor
