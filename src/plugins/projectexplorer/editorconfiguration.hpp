// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/id.hpp>

#include <QObject>
#include <QVariantMap>

#include <memory>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace TextEditor {
class BaseTextEditor;
class TextEditorWidget;
class TextDocument;
class TabSettings;
class ICodeStylePreferences;
class TypingSettings;
class StorageSettings;
class BehaviorSettings;
class ExtraEncodingSettings;
class MarginSettings;
}

namespace ProjectExplorer {

class Project;
struct EditorConfigurationPrivate;

class PROJECTEXPLORER_EXPORT EditorConfiguration : public QObject {
  Q_OBJECT

public:
  EditorConfiguration();
  ~EditorConfiguration() override;

  auto setUseGlobalSettings(bool use) -> void;
  auto useGlobalSettings() const -> bool;
  auto cloneGlobalSettings() -> void;

  // The default codec is returned in the case the project doesn't override it.
  auto textCodec() const -> QTextCodec*;
  auto typingSettings() const -> const TextEditor::TypingSettings&;
  auto storageSettings() const -> const TextEditor::StorageSettings&;
  auto behaviorSettings() const -> const TextEditor::BehaviorSettings&;
  auto extraEncodingSettings() const -> const TextEditor::ExtraEncodingSettings&;
  auto marginSettings() const -> const TextEditor::MarginSettings&;
  auto codeStyle() const -> TextEditor::ICodeStylePreferences*;
  auto codeStyle(Utils::Id languageId) const -> TextEditor::ICodeStylePreferences*;
  auto codeStyles() const -> QMap<Utils::Id, TextEditor::ICodeStylePreferences*>;
  auto configureEditor(TextEditor::BaseTextEditor *textEditor) const -> void;
  auto deconfigureEditor(TextEditor::BaseTextEditor *textEditor) const -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto setTypingSettings(const TextEditor::TypingSettings &settings) -> void;
  auto setStorageSettings(const TextEditor::StorageSettings &settings) -> void;
  auto setBehaviorSettings(const TextEditor::BehaviorSettings &settings) -> void;
  auto setExtraEncodingSettings(const TextEditor::ExtraEncodingSettings &settings) -> void;
  auto setMarginSettings(const TextEditor::MarginSettings &settings) -> void;
  auto setShowWrapColumn(bool onoff) -> void;
  auto setUseIndenter(bool onoff) -> void;
  auto setWrapColumn(int column) -> void;
  auto setTextCodec(QTextCodec *textCodec) -> void;
  auto slotAboutToRemoveProject(Project *project) -> void;

signals:
  auto typingSettingsChanged(const TextEditor::TypingSettings &) -> void;
  auto storageSettingsChanged(const TextEditor::StorageSettings &) -> void;
  auto behaviorSettingsChanged(const TextEditor::BehaviorSettings &) -> void;
  auto extraEncodingSettingsChanged(const TextEditor::ExtraEncodingSettings &) -> void;
  auto marginSettingsChanged(const TextEditor::MarginSettings &) -> void;

private:
  auto switchSettings(TextEditor::TextEditorWidget *baseTextEditor) const -> void;

  const std::unique_ptr<EditorConfigurationPrivate> d;
};

// Return the editor settings in the case it's not null. Otherwise, try to find the project
// the file belongs to and return the project settings. If the file doesn't belong to any
// project return the global settings.
PROJECTEXPLORER_EXPORT auto actualTabSettings(const QString &fileName, const TextEditor::TextDocument *baseTextDocument) -> TextEditor::TabSettings;

} // namespace ProjectExplorer
