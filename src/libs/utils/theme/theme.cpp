// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "theme.h"
#include "theme_p.h"
#include "../algorithm.h"
#include "../hostosinfo.h"
#include "../qtcassert.h"
#ifdef Q_OS_MACOS
#import "theme_mac.h"
#endif

#include <QApplication>
#include <QFileInfo>
#include <QMetaEnum>
#include <QPalette>
#include <QSettings>

namespace Utils {

static Theme *m_orcaTheme = nullptr;

ThemePrivate::ThemePrivate()
{
  const QMetaObject &m = Theme::staticMetaObject;
  colors.resize(m.enumerator(m.indexOfEnumerator("Color")).keyCount());
  imageFiles.resize(m.enumerator(m.indexOfEnumerator("ImageFile")).keyCount());
  gradients.resize(m.enumerator(m.indexOfEnumerator("Gradient")).keyCount());
  flags.resize(m.enumerator(m.indexOfEnumerator("Flag")).keyCount());
}

auto orcaTheme() -> Theme*
{
  return m_orcaTheme;
}

auto proxyTheme() -> Theme*
{
  return new Theme(m_orcaTheme);
}

auto setThemeApplicationPalette() -> void
{
  if (m_orcaTheme && m_orcaTheme->flag(Theme::ApplyThemePaletteGlobally))
    QApplication::setPalette(m_orcaTheme->palette());
}

static auto setMacAppearance(Theme *theme) -> void
{
  #ifdef Q_OS_MACOS
    // Match the native UI theme and palette with the creator
    // theme by forcing light aqua for light themes
    // and dark aqua for dark themes.
    if (theme)
        Internal::forceMacAppearance(theme->flag(Theme::DarkUserInterface));
  #else
  Q_UNUSED(theme)
  #endif
}

static auto macOSSystemIsDark() -> bool
{
  #ifdef Q_OS_MACOS
    static bool systemIsDark = Internal::currentAppearanceIsDark();
    return systemIsDark;
  #else
  return false;
  #endif
}

auto setOrcaTheme(Theme *theme) -> void
{
  if (m_orcaTheme == theme)
    return;
  delete m_orcaTheme;
  m_orcaTheme = theme;

  setMacAppearance(theme);
  setThemeApplicationPalette();
}

Theme::Theme(const QString &id, QObject *parent) : QObject(parent), d(new ThemePrivate)
{
  d->id = id;
}

Theme::Theme(Theme *originTheme, QObject *parent) : QObject(parent), d(new ThemePrivate(*(originTheme->d))) {}

Theme::~Theme()
{
  delete d;
}

auto Theme::preferredStyles() const -> QStringList
{
  // Force Fusion style if we have a dark theme on Windows or Linux,
  // because the default QStyle might not be up for it
  if (!HostOsInfo::isMacHost() && d->preferredStyles.isEmpty() && flag(DarkUserInterface))
    return {"Fusion"};
  return d->preferredStyles;
}

auto Theme::defaultTextEditorColorScheme() const -> QString
{
  return d->defaultTextEditorColorScheme;
}

auto Theme::id() const -> QString
{
  return d->id;
}

auto Theme::flag(Theme::Flag f) const -> bool
{
  return d->flags[f];
}

auto Theme::color(Theme::Color role) const -> QColor
{
  return d->colors[role].first;
}

auto Theme::imageFile(Theme::ImageFile imageFile, const QString &fallBack) const -> QString
{
  const QString &file = d->imageFiles.at(imageFile);
  return file.isEmpty() ? fallBack : file;
}

auto Theme::gradient(Theme::Gradient role) const -> QGradientStops
{
  return d->gradients[role];
}

auto Theme::readNamedColor(const QString &color) const -> QPair<QColor, QString>
{
  if (d->palette.contains(color))
    return qMakePair(d->palette[color], color);
  if (color == QLatin1String("style"))
    return qMakePair(QColor(), QString());

  const QColor col('#' + color);
  if (!col.isValid()) {
    qWarning("Color \"%s\" is neither a named color nor a valid color", qPrintable(color));
    return qMakePair(Qt::black, QString());
  }
  return qMakePair(col, QString());
}

auto Theme::filePath() const -> QString
{
  return d->fileName;
}

auto Theme::displayName() const -> QString
{
  return d->displayName;
}

auto Theme::setDisplayName(const QString &name) -> void
{
  d->displayName = name;
}

auto Theme::readSettings(QSettings &settings) -> void
{
  d->fileName = settings.fileName();
  const QMetaObject &m = *metaObject();

  {
    d->displayName = settings.value(QLatin1String("ThemeName"), QLatin1String("unnamed")).toString();
    d->preferredStyles = settings.value(QLatin1String("PreferredStyles")).toStringList();
    d->preferredStyles.removeAll(QString());
    d->defaultTextEditorColorScheme = settings.value(QLatin1String("DefaultTextEditorColorScheme")).toString();
  }
  {
    settings.beginGroup(QLatin1String("Palette"));
    const QStringList allKeys = settings.allKeys();
    for (const QString &key : allKeys)
      d->palette[key] = readNamedColor(settings.value(key).toString()).first;
    settings.endGroup();
  }
  {
    settings.beginGroup(QLatin1String("Colors"));
    QMetaEnum e = m.enumerator(m.indexOfEnumerator("Color"));
    for (int i = 0, total = e.keyCount(); i < total; ++i) {
      const QString key = QLatin1String(e.key(i));
      if (!settings.contains(key)) {
        if (i < PaletteWindow || i > PalettePlaceholderTextDisabled)
          qWarning("Theme \"%s\" misses color setting for key \"%s\".", qPrintable(d->fileName), qPrintable(key));
        continue;
      }
      d->colors[i] = readNamedColor(settings.value(key).toString());
    }
    settings.endGroup();
  }
  {
    settings.beginGroup(QLatin1String("ImageFiles"));
    QMetaEnum e = m.enumerator(m.indexOfEnumerator("ImageFile"));
    for (int i = 0, total = e.keyCount(); i < total; ++i) {
      const QString key = QLatin1String(e.key(i));
      d->imageFiles[i] = settings.value(key).toString();
    }
    settings.endGroup();
  }
  {
    settings.beginGroup(QLatin1String("Gradients"));
    QMetaEnum e = m.enumerator(m.indexOfEnumerator("Gradient"));
    for (int i = 0, total = e.keyCount(); i < total; ++i) {
      const QString key = QLatin1String(e.key(i));
      QGradientStops stops;
      int size = settings.beginReadArray(key);
      for (int j = 0; j < size; ++j) {
        settings.setArrayIndex(j);
        QTC_ASSERT(settings.contains(QLatin1String("pos")), return);
        const double pos = settings.value(QLatin1String("pos")).toDouble();
        QTC_ASSERT(settings.contains(QLatin1String("color")), return);
        const QColor c('#' + settings.value(QLatin1String("color")).toString());
        stops.append(qMakePair(pos, c));
      }
      settings.endArray();
      d->gradients[i] = stops;
    }
    settings.endGroup();
  }
  {
    settings.beginGroup(QLatin1String("Flags"));
    QMetaEnum e = m.enumerator(m.indexOfEnumerator("Flag"));
    for (int i = 0, total = e.keyCount(); i < total; ++i) {
      const QString key = QLatin1String(e.key(i));
      QTC_ASSERT(settings.contains(key), return);;
      d->flags[i] = settings.value(key).toBool();
    }
    settings.endGroup();
  }
}

auto Theme::systemUsesDarkMode() -> bool
{
  if (HostOsInfo::isWindowsHost()) {
    constexpr char regkey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    bool ok;
    const auto setting = QSettings(regkey, QSettings::NativeFormat).value("AppsUseLightTheme").toInt(&ok);
    return ok && setting == 0;
  } else if (HostOsInfo::isMacHost()) {
    return macOSSystemIsDark();
  }
  return false;
}

// If you copy QPalette, default values stay at default, even if that default is different
// within the context of different widgets. Create deep copy.
static auto copyPalette(const QPalette &p) -> QPalette
{
  QPalette res;
  for (int group = 0; group < QPalette::NColorGroups; ++group) {
    for (int role = 0; role < QPalette::NColorRoles; ++role) {
      res.setBrush(QPalette::ColorGroup(group), QPalette::ColorRole(role), p.brush(QPalette::ColorGroup(group), QPalette::ColorRole(role)));
    }
  }
  return res;
}

auto Theme::setInitialPalette(Theme *initTheme) -> void
{
  macOSSystemIsDark(); // initialize value for system mode
  setMacAppearance(initTheme);
  initialPalette();
}

auto Theme::initialPalette() -> QPalette
{
  static QPalette palette = copyPalette(QApplication::palette());
  return palette;
}

auto Theme::palette() const -> QPalette
{
  QPalette pal = initialPalette();
  if (!flag(DerivePaletteFromTheme))
    return pal;

    const static struct {
        Color themeColor;
        QPalette::ColorRole paletteColorRole;
        QPalette::ColorGroup paletteColorGroup;
        bool setColorRoleAsBrush;
    } mapping[] = {
        {PaletteWindow,                    QPalette::Window,           QPalette::All,      false},
        {PaletteWindowDisabled,            QPalette::Window,           QPalette::Disabled, false},
        {PaletteWindowText,                QPalette::WindowText,       QPalette::All,      true},
        {PaletteWindowTextDisabled,        QPalette::WindowText,       QPalette::Disabled, true},
        {PaletteBase,                      QPalette::Base,             QPalette::All,      false},
        {PaletteBaseDisabled,              QPalette::Base,             QPalette::Disabled, false},
        {PaletteAlternateBase,             QPalette::AlternateBase,    QPalette::All,      false},
        {PaletteAlternateBaseDisabled,     QPalette::AlternateBase,    QPalette::Disabled, false},
        {PaletteToolTipBase,               QPalette::ToolTipBase,      QPalette::All,      true},
        {PaletteToolTipBaseDisabled,       QPalette::ToolTipBase,      QPalette::Disabled, true},
        {PaletteToolTipText,               QPalette::ToolTipText,      QPalette::All,      false},
        {PaletteToolTipTextDisabled,       QPalette::ToolTipText,      QPalette::Disabled, false},
        {PaletteText,                      QPalette::Text,             QPalette::All,      true},
        {PaletteTextDisabled,              QPalette::Text,             QPalette::Disabled, true},
        {PaletteButton,                    QPalette::Button,           QPalette::All,      false},
        {PaletteButtonDisabled,            QPalette::Button,           QPalette::Disabled, false},
        {PaletteButtonText,                QPalette::ButtonText,       QPalette::All,      true},
        {PaletteButtonTextDisabled,        QPalette::ButtonText,       QPalette::Disabled, true},
        {PaletteBrightText,                QPalette::BrightText,       QPalette::All,      false},
        {PaletteBrightTextDisabled,        QPalette::BrightText,       QPalette::Disabled, false},
        {PaletteHighlight,                 QPalette::Highlight,        QPalette::All,      true},
        {PaletteHighlightDisabled,         QPalette::Highlight,        QPalette::Disabled, true},
        {PaletteHighlightedText,           QPalette::HighlightedText,  QPalette::All,      true},
        {PaletteHighlightedTextDisabled,   QPalette::HighlightedText,  QPalette::Disabled, true},
        {PaletteLink,                      QPalette::Link,             QPalette::All,      false},
        {PaletteLinkDisabled,              QPalette::Link,             QPalette::Disabled, false},
        {PaletteLinkVisited,               QPalette::LinkVisited,      QPalette::All,      false},
        {PaletteLinkVisitedDisabled,       QPalette::LinkVisited,      QPalette::Disabled, false},
        {PaletteLight,                     QPalette::Light,            QPalette::All,      false},
        {PaletteLightDisabled,             QPalette::Light,            QPalette::Disabled, false},
        {PaletteMidlight,                  QPalette::Midlight,         QPalette::All,      false},
        {PaletteMidlightDisabled,          QPalette::Midlight,         QPalette::Disabled, false},
        {PaletteDark,                      QPalette::Dark,             QPalette::All,      false},
        {PaletteDarkDisabled,              QPalette::Dark,             QPalette::Disabled, false},
        {PaletteMid,                       QPalette::Mid,              QPalette::All,      false},
        {PaletteMidDisabled,               QPalette::Mid,              QPalette::Disabled, false},
        {PaletteShadow,                    QPalette::Shadow,           QPalette::All,      false},
        {PaletteShadowDisabled,            QPalette::Shadow,           QPalette::Disabled, false},
        {PalettePlaceholderText,           QPalette::PlaceholderText,  QPalette::All,      false},
        {PalettePlaceholderTextDisabled,   QPalette::PlaceholderText,  QPalette::Disabled, false},
    };

  for (auto entry : mapping) {
    const QColor themeColor = color(entry.themeColor);
    // Use original color if color is not defined in theme.
    if (themeColor.isValid()) {
      if (entry.setColorRoleAsBrush)
        // TODO: Find out why sometimes setBrush is used
        pal.setBrush(entry.paletteColorGroup, entry.paletteColorRole, themeColor);
      else
        pal.setColor(entry.paletteColorGroup, entry.paletteColorRole, themeColor);
    }
  }

  return pal;
}

} // namespace Utils
