// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "colorscheme.hpp"
#include "textstyles.hpp"

#include <QHash>
#include <QList>
#include <QString>
#include <QTextCharFormat>

QT_BEGIN_NAMESPACE
class QSettings;
class QFont;
QT_END_NAMESPACE

namespace TextEditor {

class FormatDescription;

/**
 * Font settings (default font and enumerated list of formats).
 */
class TEXTEDITOR_EXPORT FontSettings {
public:
  using FormatDescriptions = std::vector<FormatDescription>;

  FontSettings();

  auto clear() -> void;
  auto isEmpty() const -> bool { return m_scheme.isEmpty(); }
  auto toSettings(QSettings *s) const -> void;
  auto fromSettings(const FormatDescriptions &descriptions, const QSettings *s) -> bool;
  auto toTextCharFormats(const QVector<TextStyle> &categories) const -> QVector<QTextCharFormat>;
  auto toTextCharFormat(TextStyle category) const -> QTextCharFormat;
  auto toTextCharFormat(TextStyles textStyles) const -> QTextCharFormat;
  auto family() const -> QString;
  auto setFamily(const QString &family) -> void;
  auto fontSize() const -> int;
  auto setFontSize(int size) -> void;
  auto fontZoom() const -> int;
  auto setFontZoom(int zoom) -> void;
  auto font() const -> QFont;
  auto antialias() const -> bool;
  auto setAntialias(bool antialias) -> void;
  auto formatFor(TextStyle category) -> Format&;
  auto formatFor(TextStyle category) const -> Format;
  auto colorSchemeFileName() const -> QString;
  auto setColorSchemeFileName(const QString &fileName) -> void;
  auto loadColorScheme(const QString &fileName, const FormatDescriptions &descriptions) -> bool;
  auto saveColorScheme(const QString &fileName) -> bool;
  auto colorScheme() const -> const ColorScheme&;
  auto setColorScheme(const ColorScheme &scheme) -> void;
  auto equals(const FontSettings &f) const -> bool;

  static auto defaultFixedFontFamily() -> QString;
  static auto defaultFontSize() -> int;
  static auto defaultSchemeFileName(const QString &fileName = QString()) -> QString;

  friend auto operator==(const FontSettings &f1, const FontSettings &f2) -> bool { return f1.equals(f2); }
  friend auto operator!=(const FontSettings &f1, const FontSettings &f2) -> bool { return !f1.equals(f2); }

private:
  auto addMixinStyle(QTextCharFormat &textCharFormat, const MixinTextStyles &mixinStyles) const -> void;

  QString m_family;
  QString m_schemeFileName;
  int m_fontSize;
  int m_fontZoom;
  bool m_antialias;
  ColorScheme m_scheme;
  mutable QHash<TextStyle, QTextCharFormat> m_formatCache;
  mutable QHash<TextStyles, QTextCharFormat> m_textCharFormatCache;
};

} // namespace TextEditor
