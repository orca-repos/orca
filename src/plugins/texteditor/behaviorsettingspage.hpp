// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "tabsettingswidget.hpp"

#include <core/dialogs/ioptionspage.hpp>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace TextEditor {

class TabSettings;
class TypingSettings;
class StorageSettings;
class BehaviorSettings;
class ExtraEncodingSettings;
class ICodeStylePreferences;
class CodeStylePool;

class BehaviorSettingsPage : public Core::IOptionsPage {
  Q_OBJECT

public:
  BehaviorSettingsPage();
  ~BehaviorSettingsPage() override;
  
  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;
  auto codeStyle() const -> ICodeStylePreferences*;
  auto codeStylePool() const -> CodeStylePool*;
  auto typingSettings() const -> const TypingSettings&;
  auto storageSettings() const -> const StorageSettings&;
  auto behaviorSettings() const -> const BehaviorSettings&;
  auto extraEncodingSettings() const -> const ExtraEncodingSettings&;

private:
  auto openCodingStylePreferences(TabSettingsWidget::CodingStyleLink link) -> void;
  auto settingsFromUI(TypingSettings *typingSettings, StorageSettings *storageSettings, BehaviorSettings *behaviorSettings, ExtraEncodingSettings *extraEncodingSettings) const -> void;
  auto settingsToUI() -> void;

  QList<QTextCodec*> m_codecs;
  struct BehaviorSettingsPagePrivate;
  BehaviorSettingsPagePrivate *d;
};

} // namespace TextEditor
