// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fontsettings.hpp"
#include "fontsettingspage.hpp"

#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/stringutils.hpp>
#include <utils/theme/theme.hpp>

#include <core/core-interface.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QSettings>
#include <QTextCharFormat>

static constexpr char fontFamilyKey[] = "FontFamily";
static constexpr char fontSizeKey[] = "FontSize";
static constexpr char fontZoomKey[] = "FontZoom";
static constexpr char antialiasKey[] = "FontAntialias";
static constexpr char schemeFileNamesKey[] = "ColorSchemes";

namespace {
constexpr bool DEFAULT_ANTIALIAS = true;
} // anonymous namespace

namespace TextEditor {

FontSettings::FontSettings() : m_family(defaultFixedFontFamily()), m_fontSize(defaultFontSize()), m_fontZoom(100), m_antialias(DEFAULT_ANTIALIAS) {}

auto FontSettings::clear() -> void
{
  m_family = defaultFixedFontFamily();
  m_fontSize = defaultFontSize();
  m_fontZoom = 100;
  m_antialias = DEFAULT_ANTIALIAS;
  m_scheme.clear();
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

static auto settingsGroup() -> QString
{
  return Utils::settingsKey(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
}

auto FontSettings::toSettings(QSettings *s) const -> void
{
  s->beginGroup(settingsGroup());
  if (m_family != defaultFixedFontFamily() || s->contains(QLatin1String(fontFamilyKey)))
    s->setValue(QLatin1String(fontFamilyKey), m_family);

  if (m_fontSize != defaultFontSize() || s->contains(QLatin1String(fontSizeKey)))
    s->setValue(QLatin1String(fontSizeKey), m_fontSize);

  if (m_fontZoom != 100 || s->contains(QLatin1String(fontZoomKey)))
    s->setValue(QLatin1String(fontZoomKey), m_fontZoom);

  if (m_antialias != DEFAULT_ANTIALIAS || s->contains(QLatin1String(antialiasKey)))
    s->setValue(QLatin1String(antialiasKey), m_antialias);

  auto schemeFileNames = s->value(QLatin1String(schemeFileNamesKey)).toMap();
  if (m_schemeFileName != defaultSchemeFileName() || schemeFileNames.contains(Utils::orcaTheme()->id())) {
    schemeFileNames.insert(Utils::orcaTheme()->id(), m_schemeFileName);
    s->setValue(QLatin1String(schemeFileNamesKey), schemeFileNames);
  }

  s->endGroup();
}

auto FontSettings::fromSettings(const FormatDescriptions &descriptions, const QSettings *s) -> bool
{
  clear();

  auto group = settingsGroup();
  if (!s->childGroups().contains(group))
    return false;

  group += QLatin1Char('/');

  m_family = s->value(group + QLatin1String(fontFamilyKey), defaultFixedFontFamily()).toString();
  m_fontSize = s->value(group + QLatin1String(fontSizeKey), m_fontSize).toInt();
  m_fontZoom = s->value(group + QLatin1String(fontZoomKey), m_fontZoom).toInt();
  m_antialias = s->value(group + QLatin1String(antialiasKey), DEFAULT_ANTIALIAS).toBool();

  if (s->contains(group + QLatin1String(schemeFileNamesKey))) {
    // Load the selected color scheme for the current theme
    const auto schemeFileNames = s->value(group + QLatin1String(schemeFileNamesKey)).toMap();
    if (schemeFileNames.contains(Utils::orcaTheme()->id())) {
      const QString scheme = schemeFileNames.value(Utils::orcaTheme()->id()).toString();
      loadColorScheme(scheme, descriptions);
    }
  }

  return true;
}

auto FontSettings::equals(const FontSettings &f) const -> bool
{
  return m_family == f.m_family && m_schemeFileName == f.m_schemeFileName && m_fontSize == f.m_fontSize && m_fontZoom == f.m_fontZoom && m_antialias == f.m_antialias && m_scheme == f.m_scheme;
}

auto qHash(const TextStyle &textStyle)
{
  return ::qHash(quint8(textStyle));
}

static auto isOverlayCategory(TextStyle category) -> bool
{
  return category == C_OCCURRENCES || category == C_OCCURRENCES_RENAME || category == C_SEARCH_RESULT || category == C_SEARCH_RESULT_ALT1 || category == C_SEARCH_RESULT_ALT2 || category == C_PARENTHESES_MISMATCH;
}

/**
 * Returns the QTextCharFormat of the given format category.
 */
auto FontSettings::toTextCharFormat(TextStyle category) const -> QTextCharFormat
{
  const auto textCharFormatIterator = m_formatCache.find(category);
  if (textCharFormatIterator != m_formatCache.end())
    return *textCharFormatIterator;

  const auto &f = m_scheme.formatFor(category);
  QTextCharFormat tf;

  if (category == C_TEXT) {
    tf.setFontFamily(m_family);
    tf.setFontPointSize(m_fontSize * m_fontZoom / 100.);
    tf.setFontStyleStrategy(m_antialias ? QFont::PreferAntialias : QFont::NoAntialias);
  }

  if (category == C_OCCURRENCES_UNUSED) {
    tf.setToolTip(QCoreApplication::translate("FontSettings_C_OCCURRENCES_UNUSED", "Unused variable"));
  }

  if (f.foreground().isValid() && !isOverlayCategory(category))
    tf.setForeground(f.foreground());
  if (f.background().isValid()) {
    if (category == C_TEXT || f.background() != m_scheme.formatFor(C_TEXT).background())
      tf.setBackground(f.background());
  } else if (isOverlayCategory(category)) {
    // overlays without a background schouldn't get painted
    tf.setBackground(QColor());
  } else if (f.underlineStyle() != QTextCharFormat::NoUnderline) {
    // underline does not need to fill without having background color
    tf.setBackground(Qt::BrushStyle::NoBrush);
  }

  tf.setFontWeight(f.bold() ? QFont::Bold : QFont::Normal);
  tf.setFontItalic(f.italic());

  tf.setUnderlineColor(f.underlineColor());
  tf.setUnderlineStyle(f.underlineStyle());

  m_formatCache.insert(category, tf);
  return tf;
}

auto qHash(TextStyles textStyles)
{
  return ::qHash(reinterpret_cast<quint64&>(textStyles));
}

auto operator==(const TextStyles &first, const TextStyles &second) -> bool
{
  return first.mainStyle == second.mainStyle && first.mixinStyles == second.mixinStyles;
}

namespace {

auto clamp(double value) -> double
{
  return std::max(0.0, std::min(1.0, value));
}

auto mixBrush(const QBrush &original, double relativeSaturation, double relativeLightness) -> QBrush
{
  const auto originalColor = original.color().toHsl();
  QColor mixedColor(QColor::Hsl);

  const auto mixedSaturation = clamp(originalColor.hslSaturationF() + relativeSaturation);

  const auto mixedLightness = clamp(originalColor.lightnessF() + relativeLightness);

  mixedColor.setHslF(originalColor.hslHueF(), mixedSaturation, mixedLightness);

  return mixedColor;
}
}

auto FontSettings::addMixinStyle(QTextCharFormat &textCharFormat, const MixinTextStyles &mixinStyles) const -> void
{
  for (const auto mixinStyle : mixinStyles) {
    const auto &format = m_scheme.formatFor(mixinStyle);

    if (format.foreground().isValid()) {
      textCharFormat.setForeground(format.foreground());
    } else {
      if (textCharFormat.hasProperty(QTextFormat::ForegroundBrush)) {
        textCharFormat.setForeground(mixBrush(textCharFormat.foreground(), format.relativeForegroundSaturation(), format.relativeForegroundLightness()));
      }
    }
    if (format.background().isValid()) {
      textCharFormat.setBackground(format.background());
    } else {
      if (textCharFormat.hasProperty(QTextFormat::BackgroundBrush)) {
        textCharFormat.setBackground(mixBrush(textCharFormat.background(), format.relativeBackgroundSaturation(), format.relativeBackgroundLightness()));
      }
    }
    if (!textCharFormat.fontItalic())
      textCharFormat.setFontItalic(format.italic());

    if (textCharFormat.fontWeight() == QFont::Normal)
      textCharFormat.setFontWeight(format.bold() ? QFont::Bold : QFont::Normal);

    if (textCharFormat.underlineStyle() == QTextCharFormat::NoUnderline) {
      textCharFormat.setUnderlineStyle(format.underlineStyle());
      textCharFormat.setUnderlineColor(format.underlineColor());
    }
  };
}

auto FontSettings::toTextCharFormat(TextStyles textStyles) const -> QTextCharFormat
{
  const auto textCharFormatIterator = m_textCharFormatCache.find(textStyles);
  if (textCharFormatIterator != m_textCharFormatCache.end())
    return *textCharFormatIterator;

  auto textCharFormat = toTextCharFormat(textStyles.mainStyle);

  addMixinStyle(textCharFormat, textStyles.mixinStyles);

  m_textCharFormatCache.insert(textStyles, textCharFormat);

  return textCharFormat;
}

/**
 * Returns the list of QTextCharFormats that corresponds to the list of
 * requested format categories.
 */
auto FontSettings::toTextCharFormats(const QVector<TextStyle> &categories) const -> QVector<QTextCharFormat>
{
  QVector<QTextCharFormat> rc;
  const int size = categories.size();
  rc.reserve(size);
  for (auto i = 0; i < size; i++)
    rc.append(toTextCharFormat(categories.at(i)));
  return rc;
}

/**
 * Returns the configured font family.
 */
auto FontSettings::family() const -> QString
{
  return m_family;
}

auto FontSettings::setFamily(const QString &family) -> void
{
  m_family = family;
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

/**
 * Returns the configured font size.
 */
auto FontSettings::fontSize() const -> int
{
  return m_fontSize;
}

auto FontSettings::setFontSize(int size) -> void
{
  m_fontSize = size;
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

/**
 * Returns the configured font zoom factor in percent.
 */
auto FontSettings::fontZoom() const -> int
{
  return m_fontZoom;
}

auto FontSettings::setFontZoom(int zoom) -> void
{
  m_fontZoom = zoom;
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

auto FontSettings::font() const -> QFont
{
  QFont f(family(), fontSize());
  f.setStyleStrategy(m_antialias ? QFont::PreferAntialias : QFont::NoAntialias);
  return f;
}

/**
 * Returns the configured antialiasing behavior.
 */
auto FontSettings::antialias() const -> bool
{
  return m_antialias;
}

auto FontSettings::setAntialias(bool antialias) -> void
{
  m_antialias = antialias;
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

/**
 * Returns the format for the given font category.
 */
auto FontSettings::formatFor(TextStyle category) -> Format&
{
  return m_scheme.formatFor(category);
}

auto FontSettings::formatFor(TextStyle category) const -> Format
{
  return m_scheme.formatFor(category);
}

/**
 * Returns the file name of the currently selected color scheme.
 */
auto FontSettings::colorSchemeFileName() const -> QString
{
  return m_schemeFileName;
}

/**
 * Sets the file name of the color scheme. Does not load the scheme from the
 * given file. If you want to load a scheme, use loadColorScheme() instead.
 */
auto FontSettings::setColorSchemeFileName(const QString &fileName) -> void
{
  m_schemeFileName = fileName;
}

auto FontSettings::loadColorScheme(const QString &fileName, const FormatDescriptions &descriptions) -> bool
{
  m_formatCache.clear();
  m_textCharFormatCache.clear();
  auto loaded = true;
  m_schemeFileName = fileName;

  if (!m_scheme.load(m_schemeFileName)) {
    loaded = false;
    m_schemeFileName.clear();
    qWarning() << "Failed to load color scheme:" << fileName;
  }

  // Apply default formats to undefined categories
  foreach(const FormatDescription &desc, descriptions) {
    const auto id = desc.id();
    if (!m_scheme.contains(id)) {
      if (id == C_NAMESPACE && m_scheme.contains(C_TYPE)) {
        m_scheme.setFormatFor(C_NAMESPACE, m_scheme.formatFor(C_TYPE));
        continue;
      }
      Format format;
      const auto &descFormat = desc.format();
      // Default fallback for background and foreground is C_TEXT, which is set through
      // the editor's palette, i.e. we leave these as invalid colors in that case
      if (descFormat != format || !m_scheme.contains(C_TEXT)) {
        format.setForeground(descFormat.foreground());
        format.setBackground(descFormat.background());
      }
      format.setRelativeForegroundSaturation(descFormat.relativeForegroundSaturation());
      format.setRelativeForegroundLightness(descFormat.relativeForegroundLightness());
      format.setRelativeBackgroundSaturation(descFormat.relativeBackgroundSaturation());
      format.setRelativeBackgroundLightness(descFormat.relativeBackgroundLightness());
      format.setBold(descFormat.bold());
      format.setItalic(descFormat.italic());
      format.setUnderlineColor(descFormat.underlineColor());
      format.setUnderlineStyle(descFormat.underlineStyle());
      m_scheme.setFormatFor(id, format);
    }
  }

  return loaded;
}

auto FontSettings::saveColorScheme(const QString &fileName) -> bool
{
  const bool saved = m_scheme.save(fileName, Orca::Plugin::Core::ICore::dialogParent());
  if (saved)
    m_schemeFileName = fileName;
  return saved;
}

/**
 * Returns the currently active color scheme.
 */
auto FontSettings::colorScheme() const -> const ColorScheme&
{
  return m_scheme;
}

auto FontSettings::setColorScheme(const ColorScheme &scheme) -> void
{
  m_scheme = scheme;
  m_formatCache.clear();
  m_textCharFormatCache.clear();
}

static auto defaultFontFamily() -> QString
{
  if (Utils::HostOsInfo::isMacHost())
    return QLatin1String("Monaco");

  const QString sourceCodePro("Source Code Pro");
  const QFontDatabase dataBase;
  if (dataBase.hasFamily(sourceCodePro))
    return sourceCodePro;

  if (Utils::HostOsInfo::isAnyUnixHost())
    return QLatin1String("Monospace");
  return QLatin1String("Courier");
}

auto FontSettings::defaultFixedFontFamily() -> QString
{
  static QString rc;
  if (rc.isEmpty()) {
    auto f = QFont(defaultFontFamily());
    f.setStyleHint(QFont::TypeWriter);
    rc = f.family();
  }
  return rc;
}

auto FontSettings::defaultFontSize() -> int
{
  if (Utils::HostOsInfo::isMacHost())
    return 12;
  if (Utils::HostOsInfo::isAnyUnixHost())
    return 9;
  return 10;
}

/**
 * Returns the default scheme file name, or the path to a shipped scheme when
 * one exists with the given \a fileName.
 */
auto FontSettings::defaultSchemeFileName(const QString &fileName) -> QString
{
  Utils::FilePath defaultScheme = Orca::Plugin::Core::ICore::resourcePath("styles");

  if (!fileName.isEmpty() && (defaultScheme / fileName).exists()) {
    defaultScheme = defaultScheme / fileName;
  } else {
    const QString themeScheme = Utils::orcaTheme()->defaultTextEditorColorScheme();
    if (!themeScheme.isEmpty() && (defaultScheme / themeScheme).exists())
      defaultScheme = defaultScheme / themeScheme;
    else
      defaultScheme = defaultScheme / "default.xml";
  }

  return defaultScheme.toString();
}

} // namespace TextEditor
