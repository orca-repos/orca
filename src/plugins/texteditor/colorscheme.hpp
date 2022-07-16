// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"
#include "texteditorconstants.hpp"

#include <QMap>
#include <QString>
#include <QColor>
#include <QTextCharFormat>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace TextEditor {

/*! Format for a particular piece of text (text/comment, etc). */
class TEXTEDITOR_EXPORT Format {
public:
  Format() = default;
  Format(const QColor &foreground, const QColor &background);

  auto foreground() const -> QColor { return m_foreground; }
  auto setForeground(const QColor &foreground) -> void;
  auto background() const -> QColor { return m_background; }
  auto setBackground(const QColor &background) -> void;
  auto relativeForegroundSaturation() const -> double { return m_relativeForegroundSaturation; }
  auto setRelativeForegroundSaturation(double relativeForegroundSaturation) -> void;
  auto relativeForegroundLightness() const -> double { return m_relativeForegroundLightness; }
  auto setRelativeForegroundLightness(double relativeForegroundLightness) -> void;
  auto relativeBackgroundSaturation() const -> double { return m_relativeBackgroundSaturation; }
  auto setRelativeBackgroundSaturation(double relativeBackgroundSaturation) -> void;
  auto relativeBackgroundLightness() const -> double { return m_relativeBackgroundLightness; }
  auto setRelativeBackgroundLightness(double relativeBackgroundLightness) -> void;
  auto bold() const -> bool { return m_bold; }
  auto setBold(bool bold) -> void;
  auto italic() const -> bool { return m_italic; }
  auto setItalic(bool italic) -> void;
  auto setUnderlineColor(const QColor &underlineColor) -> void;
  auto underlineColor() const -> QColor;
  auto setUnderlineStyle(QTextCharFormat::UnderlineStyle underlineStyle) -> void;
  auto underlineStyle() const -> QTextCharFormat::UnderlineStyle;
  auto equals(const Format &f) const -> bool;
  auto toString() const -> QString;
  auto fromString(const QString &str) -> bool;

  friend auto operator==(const Format &f1, const Format &f2) -> bool { return f1.equals(f2); }
  friend auto operator!=(const Format &f1, const Format &f2) -> bool { return !f1.equals(f2); }

private:
  QColor m_foreground;
  QColor m_background;
  QColor m_underlineColor;
  double m_relativeForegroundSaturation = 0.0;
  double m_relativeForegroundLightness = 0.0;
  double m_relativeBackgroundSaturation = 0.0;
  double m_relativeBackgroundLightness = 0.0;
  QTextCharFormat::UnderlineStyle m_underlineStyle = QTextCharFormat::NoUnderline;
  bool m_bold = false;
  bool m_italic = false;
};

/*! A color scheme combines a set of formats for different highlighting
    categories. It also provides saving and loading of the scheme to a file.
 */
class TEXTEDITOR_EXPORT ColorScheme {
public:
  auto setDisplayName(const QString &name) -> void { m_displayName = name; }
  auto displayName() const -> QString { return m_displayName; }
  auto isEmpty() const -> bool { return m_formats.isEmpty(); }
  auto contains(TextStyle category) const -> bool;
  auto formatFor(TextStyle category) -> Format&;
  auto formatFor(TextStyle category) const -> Format;
  auto setFormatFor(TextStyle category, const Format &format) -> void;
  auto clear() -> void;
  auto save(const QString &fileName, QWidget *parent) const -> bool;
  auto load(const QString &fileName) -> bool;

  auto equals(const ColorScheme &cs) const -> bool
  {
    return m_formats == cs.m_formats && m_displayName == cs.m_displayName;
  }

  static auto readNameOfScheme(const QString &fileName) -> QString;

  friend auto operator==(const ColorScheme &cs1, const ColorScheme &cs2) -> bool { return cs1.equals(cs2); }
  friend auto operator!=(const ColorScheme &cs1, const ColorScheme &cs2) -> bool { return !cs1.equals(cs2); }

private:
  QMap<TextStyle, Format> m_formats;
  QString m_displayName;
};

} // namespace TextEditor
