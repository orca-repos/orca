// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT ExtraEncodingSettings {
public:
  ExtraEncodingSettings();
  ~ExtraEncodingSettings();

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto equals(const ExtraEncodingSettings &s) const -> bool;

  friend auto operator==(const ExtraEncodingSettings &a, const ExtraEncodingSettings &b) -> bool { return a.equals(b); }
  friend auto operator!=(const ExtraEncodingSettings &a, const ExtraEncodingSettings &b) -> bool { return !a.equals(b); }

  static auto lineTerminationModeNames() -> QStringList;

  enum Utf8BomSetting {
    AlwaysAdd = 0,
    OnlyKeep = 1,
    AlwaysDelete = 2
  };

  Utf8BomSetting m_utf8BomSetting;
};

} // TextEditor
