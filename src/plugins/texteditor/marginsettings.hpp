// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT MarginSettings {
public:
  MarginSettings();

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, const QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto equals(const MarginSettings &other) const -> bool;

  friend auto operator==(const MarginSettings &one, const MarginSettings &two) -> bool { return one.equals(two); }
  friend auto operator!=(const MarginSettings &one, const MarginSettings &two) -> bool { return !one.equals(two); }

  bool m_showMargin;
  bool m_useIndenter;
  int m_marginColumn;
};

} // namespace TextEditor
