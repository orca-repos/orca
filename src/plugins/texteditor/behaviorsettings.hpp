// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

/**
 * Settings that describe how the text editor behaves. This does not include
 * the TabSettings and StorageSettings.
 */
class TEXTEDITOR_EXPORT BehaviorSettings {
public:
  BehaviorSettings();

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto equals(const BehaviorSettings &bs) const -> bool;

  friend auto operator==(const BehaviorSettings &t1, const BehaviorSettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const BehaviorSettings &t1, const BehaviorSettings &t2) -> bool { return !t1.equals(t2); }

  bool m_mouseHiding;
  bool m_mouseNavigation;
  bool m_scrollWheelZooming;
  bool m_constrainHoverTooltips;
  bool m_camelCaseNavigation;
  bool m_keyboardTooltips;
  bool m_smartSelectionChanging;
};

} // namespace TextEditor
