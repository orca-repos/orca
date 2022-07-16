// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace TextEditor {

class ICodeStylePreferences;
class TabSettingsWidget;
class TypingSettings;
class StorageSettings;
class BehaviorSettings;
class ExtraEncodingSettings;

struct BehaviorSettingsWidgetPrivate;

class TEXTEDITOR_EXPORT BehaviorSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit BehaviorSettingsWidget(QWidget *parent = nullptr);
  ~BehaviorSettingsWidget() override;

  auto setActive(bool active) -> void;
  auto setAssignedCodec(QTextCodec *codec) -> void;
  auto assignedCodecName() const -> QByteArray;
  auto setCodeStyle(ICodeStylePreferences *preferences) -> void;
  auto setAssignedTypingSettings(const TypingSettings &typingSettings) -> void;
  auto assignedTypingSettings(TypingSettings *typingSettings) const -> void;
  auto setAssignedStorageSettings(const StorageSettings &storageSettings) -> void;
  auto assignedStorageSettings(StorageSettings *storageSettings) const -> void;
  auto setAssignedBehaviorSettings(const BehaviorSettings &behaviorSettings) -> void;
  auto assignedBehaviorSettings(BehaviorSettings *behaviorSettings) const -> void;
  auto setAssignedExtraEncodingSettings(const ExtraEncodingSettings &encodingSettings) -> void;
  auto assignedExtraEncodingSettings(ExtraEncodingSettings *encodingSettings) const -> void;
  auto setAssignedLineEnding(int lineEnding) -> void;
  auto assignedLineEnding() const -> int;
  auto tabSettingsWidget() const -> TabSettingsWidget*;

signals:
  auto typingSettingsChanged(const TypingSettings &settings) -> void;
  auto storageSettingsChanged(const StorageSettings &settings) -> void;
  auto behaviorSettingsChanged(const BehaviorSettings &settings) -> void;
  auto extraEncodingSettingsChanged(const ExtraEncodingSettings &settings) -> void;
  auto textCodecChanged(QTextCodec *codec) -> void;

private:
  auto slotTypingSettingsChanged() -> void;
  auto slotStorageSettingsChanged() -> void;
  auto slotBehaviorSettingsChanged() -> void;
  auto slotExtraEncodingChanged() -> void;
  auto slotEncodingBoxChanged(int index) -> void;
  auto updateConstrainTooltipsBoxTooltip() const -> void;

  BehaviorSettingsWidgetPrivate *d;
};

} // TextEditor
