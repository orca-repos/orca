// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "colorscheme.hpp"

#include "texteditorconstants.hpp"

#include <utils/fileutils.hpp>

#include <QFile>
#include <QCoreApplication>
#include <QXmlStreamWriter>

using namespace TextEditor;

static constexpr char trueString[] = "true";
static constexpr char falseString[] = "false";

// Format

Format::Format(const QColor &foreground, const QColor &background) : m_foreground(foreground), m_background(background) {}

auto Format::setForeground(const QColor &foreground) -> void
{
  m_foreground = foreground;
}

auto Format::setBackground(const QColor &background) -> void
{
  m_background = background;
}

auto Format::setRelativeForegroundSaturation(double relativeForegroundSaturation) -> void
{
  m_relativeForegroundSaturation = relativeForegroundSaturation;
}

auto Format::setRelativeForegroundLightness(double relativeForegroundLightness) -> void
{
  m_relativeForegroundLightness = relativeForegroundLightness;
}

auto Format::setRelativeBackgroundSaturation(double relativeBackgroundSaturation) -> void
{
  m_relativeBackgroundSaturation = relativeBackgroundSaturation;
}

auto Format::setRelativeBackgroundLightness(double relativeBackgroundLightness) -> void
{
  m_relativeBackgroundLightness = relativeBackgroundLightness;
}

auto Format::setBold(bool bold) -> void
{
  m_bold = bold;
}

auto Format::setItalic(bool italic) -> void
{
  m_italic = italic;
}

auto Format::setUnderlineColor(const QColor &underlineColor) -> void
{
  m_underlineColor = underlineColor;
}

auto Format::underlineColor() const -> QColor
{
  return m_underlineColor;
}

auto Format::setUnderlineStyle(QTextCharFormat::UnderlineStyle underlineStyle) -> void
{
  m_underlineStyle = underlineStyle;
}

auto Format::underlineStyle() const -> QTextCharFormat::UnderlineStyle
{
  return m_underlineStyle;
}

static auto stringToColor(const QString &string) -> QColor
{
  if (string == QLatin1String("invalid"))
    return QColor();
  return QColor(string);
}

static auto stringToUnderlineStyle(const QString &string) -> QTextCharFormat::UnderlineStyle
{
  if (string.isEmpty() || string == QStringLiteral("NoUnderline"))
    return QTextCharFormat::NoUnderline;
  if (string == QStringLiteral("SingleUnderline"))
    return QTextCharFormat::SingleUnderline;
  if (string == QStringLiteral("DashUnderline"))
    return QTextCharFormat::DashUnderline;
  if (string == QStringLiteral("DotLine"))
    return QTextCharFormat::DotLine;
  if (string == QStringLiteral("DashDotLine"))
    return QTextCharFormat::DashDotLine;
  if (string == QStringLiteral("DashDotDotLine"))
    return QTextCharFormat::DashDotDotLine;
  if (string == QStringLiteral("WaveUnderline"))
    return QTextCharFormat::WaveUnderline;

  return QTextCharFormat::NoUnderline;
}

static auto underlineStyleToString(QTextCharFormat::UnderlineStyle underlineStyle) -> QString
{
  switch (underlineStyle) {
  case QTextCharFormat::NoUnderline:
    return QStringLiteral("NoUnderline");
  case QTextCharFormat::SingleUnderline:
    return QStringLiteral("SingleUnderline");
  case QTextCharFormat::DashUnderline:
    return QStringLiteral("DashUnderline");
  case QTextCharFormat::DotLine:
    return QStringLiteral("DotLine");
  case QTextCharFormat::DashDotLine:
    return QStringLiteral("DashDotLine");
  case QTextCharFormat::DashDotDotLine:
    return QStringLiteral("DashDotDotLine");
  case QTextCharFormat::WaveUnderline:
    return QStringLiteral("WaveUnderline");
  case QTextCharFormat::SpellCheckUnderline:
    return QString();
  }

  return QString();
}

auto Format::equals(const Format &other) const -> bool
{
  return m_foreground == other.m_foreground && m_background == other.m_background && m_underlineColor == other.m_underlineColor && m_underlineStyle == other.m_underlineStyle && m_bold == other.m_bold && m_italic == other.m_italic && qFuzzyCompare(m_relativeForegroundSaturation, other.m_relativeForegroundSaturation) && qFuzzyCompare(m_relativeForegroundLightness, other.m_relativeForegroundLightness) && qFuzzyCompare(m_relativeBackgroundSaturation, other.m_relativeBackgroundSaturation) && qFuzzyCompare(m_relativeBackgroundLightness, other.m_relativeBackgroundLightness);
}

auto Format::toString() const -> QString
{
  const QStringList text({m_foreground.name(), m_background.name(), m_bold ? QLatin1String(trueString) : QLatin1String(falseString), m_italic ? QLatin1String(trueString) : QLatin1String(falseString), m_underlineColor.name(), underlineStyleToString(m_underlineStyle), QString::number(m_relativeForegroundSaturation), QString::number(m_relativeForegroundLightness), QString::number(m_relativeBackgroundSaturation), QString::number(m_relativeBackgroundLightness)});

  return text.join(QLatin1Char(';'));
}

auto Format::fromString(const QString &str) -> bool
{
  *this = Format();

  const auto lst = str.split(QLatin1Char(';'));
  if (lst.size() != 4 && lst.size() != 6 && lst.size() != 10)
    return false;

  m_foreground = stringToColor(lst.at(0));
  m_background = stringToColor(lst.at(1));
  m_bold = lst.at(2) == QLatin1String(trueString);
  m_italic = lst.at(3) == QLatin1String(trueString);
  if (lst.size() > 4) {
    m_underlineColor = stringToColor(lst.at(4));
    m_underlineStyle = stringToUnderlineStyle(lst.at(5));
  }
  if (lst.size() > 6) {
    m_relativeForegroundSaturation = lst.at(6).toDouble();
    m_relativeForegroundLightness = lst.at(7).toDouble();
    m_relativeBackgroundSaturation = lst.at(8).toDouble();
    m_relativeBackgroundLightness = lst.at(9).toDouble();
  }

  return true;
}

// ColorScheme

auto ColorScheme::contains(TextStyle category) const -> bool
{
  return m_formats.contains(category);
}

auto ColorScheme::formatFor(TextStyle category) -> Format&
{
  return m_formats[category];
}

auto ColorScheme::formatFor(TextStyle category) const -> Format
{
  return m_formats.value(category);
}

auto ColorScheme::setFormatFor(TextStyle category, const Format &format) -> void
{
  m_formats[category] = format;
}

auto ColorScheme::clear() -> void
{
  m_formats.clear();
}

auto ColorScheme::save(const QString &fileName, QWidget *parent) const -> bool
{
  Utils::FileSaver saver(Utils::FilePath::fromString(fileName));
  if (!saver.hasError()) {
    QXmlStreamWriter w(saver.file());
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);

    w.writeStartDocument();
    w.writeStartElement(QLatin1String("style-scheme"));
    w.writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
    if (!m_displayName.isEmpty())
      w.writeAttribute(QLatin1String("name"), m_displayName);

    for (auto i = m_formats.cbegin(), end = m_formats.cend(); i != end; ++i) {
      const auto &format = i.value();
      w.writeStartElement(QLatin1String("style"));
      w.writeAttribute(QLatin1String("name"), QString::fromLatin1(Constants::nameForStyle(i.key())));
      if (format.foreground().isValid())
        w.writeAttribute(QLatin1String("foreground"), format.foreground().name().toLower());
      if (format.background().isValid())
        w.writeAttribute(QLatin1String("background"), format.background().name().toLower());
      if (format.bold())
        w.writeAttribute(QLatin1String("bold"), QLatin1String(trueString));
      if (format.italic())
        w.writeAttribute(QLatin1String("italic"), QLatin1String(trueString));
      if (format.underlineColor().isValid())
        w.writeAttribute(QStringLiteral("underlineColor"), format.underlineColor().name().toLower());
      if (format.underlineStyle() != QTextCharFormat::NoUnderline)
        w.writeAttribute(QLatin1String("underlineStyle"), underlineStyleToString(format.underlineStyle()));
      if (!qFuzzyIsNull(format.relativeForegroundSaturation()))
        w.writeAttribute(QLatin1String("relativeForegroundSaturation"), QString::number(format.relativeForegroundSaturation()));
      if (!qFuzzyIsNull(format.relativeForegroundLightness()))
        w.writeAttribute(QLatin1String("relativeForegroundLightness"), QString::number(format.relativeForegroundLightness()));
      if (!qFuzzyIsNull(format.relativeBackgroundSaturation()))
        w.writeAttribute(QLatin1String("relativeBackgroundSaturation"), QString::number(format.relativeBackgroundSaturation()));
      if (!qFuzzyIsNull(format.relativeBackgroundLightness()))
        w.writeAttribute(QLatin1String("relativeBackgroundLightness"), QString::number(format.relativeBackgroundLightness()));
      w.writeEndElement();
    }

    w.writeEndElement();
    w.writeEndDocument();

    saver.setResult(&w);
  }
  return saver.finalize(parent);
}

namespace {

class ColorSchemeReader : public QXmlStreamReader {
public:
  auto read(const QString &fileName, ColorScheme *scheme) -> bool;
  auto readName(const QString &fileName) -> QString;

private:
  auto readNextStartElement() -> bool;
  auto skipCurrentElement() -> void;
  auto readStyleScheme() -> void;
  auto readStyle() -> void;

  ColorScheme *m_scheme = nullptr;
  QString m_name;
};

auto ColorSchemeReader::read(const QString &fileName, ColorScheme *scheme) -> bool
{
  m_scheme = scheme;

  if (m_scheme)
    m_scheme->clear();

  QFile file(fileName);
  if (!file.open(QFile::ReadOnly | QFile::Text))
    return false;

  setDevice(&file);

  if (readNextStartElement() && name() == QLatin1String("style-scheme"))
    readStyleScheme();
  else
    raiseError(QCoreApplication::translate("TextEditor::Internal::ColorScheme", "Not a color scheme file."));

  return true;
}

auto ColorSchemeReader::readName(const QString &fileName) -> QString
{
  read(fileName, nullptr);
  return m_name;
}

auto ColorSchemeReader::readNextStartElement() -> bool
{
  while (readNext() != Invalid) {
    if (isStartElement())
      return true;
    if (isEndElement())
      return false;
  }
  return false;
}

auto ColorSchemeReader::skipCurrentElement() -> void
{
  while (readNextStartElement())
    skipCurrentElement();
}

auto ColorSchemeReader::readStyleScheme() -> void
{
  Q_ASSERT(isStartElement() && name() == QLatin1String("style-scheme"));

  const auto attr = attributes();
  m_name = attr.value(QLatin1String("name")).toString();
  if (!m_scheme)
    // We're done
    raiseError(QLatin1String("name loaded"));
  else
    m_scheme->setDisplayName(m_name);

  while (readNextStartElement()) {
    if (name() == QLatin1String("style"))
      readStyle();
    else
      skipCurrentElement();
  }
}

auto ColorSchemeReader::readStyle() -> void
{
  Q_ASSERT(isStartElement() && name() == QLatin1String("style"));

  const auto attr = attributes();
  const auto name = attr.value(QLatin1String("name")).toString().toLatin1();
  const auto foreground = attr.value(QLatin1String("foreground")).toString();
  const auto background = attr.value(QLatin1String("background")).toString();
  const auto bold = attr.value(QLatin1String("bold")) == QLatin1String(trueString);
  const auto italic = attr.value(QLatin1String("italic")) == QLatin1String(trueString);
  const auto underlineColor = attr.value(QLatin1String("underlineColor")).toString();
  const auto underlineStyle = attr.value(QLatin1String("underlineStyle")).toString();
  const auto relativeForegroundSaturation = attr.value(QLatin1String("relativeForegroundSaturation")).toDouble();
  const auto relativeForegroundLightness = attr.value(QLatin1String("relativeForegroundLightness")).toDouble();
  const auto relativeBackgroundSaturation = attr.value(QLatin1String("relativeBackgroundSaturation")).toDouble();
  const auto relativeBackgroundLightness = attr.value(QLatin1String("relativeBackgroundLightness")).toDouble();

  Format format;

  if (QColor::isValidColor(foreground))
    format.setForeground(QColor(foreground));
  else
    format.setForeground(QColor());

  if (QColor::isValidColor(background))
    format.setBackground(QColor(background));
  else
    format.setBackground(QColor());

  format.setBold(bold);
  format.setItalic(italic);

  if (QColor::isValidColor(underlineColor))
    format.setUnderlineColor(QColor(underlineColor));
  else
    format.setUnderlineColor(QColor());

  format.setUnderlineStyle(stringToUnderlineStyle(underlineStyle));

  format.setRelativeForegroundSaturation(relativeForegroundSaturation);
  format.setRelativeForegroundLightness(relativeForegroundLightness);
  format.setRelativeBackgroundSaturation(relativeBackgroundSaturation);
  format.setRelativeBackgroundLightness(relativeBackgroundLightness);

  m_scheme->setFormatFor(Constants::styleFromName(name), format);

  skipCurrentElement();
}

} // anonymous namespace

auto ColorScheme::load(const QString &fileName) -> bool
{
  ColorSchemeReader reader;
  return reader.read(fileName, this) && !reader.hasError();
}

auto ColorScheme::readNameOfScheme(const QString &fileName) -> QString
{
  return ColorSchemeReader().readName(fileName);
}
