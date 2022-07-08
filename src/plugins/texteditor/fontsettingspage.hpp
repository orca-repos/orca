// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "texteditorconstants.hpp"
#include "colorscheme.hpp"

#include <core/dialogs/ioptionspage.hpp>

#include <QString>

QT_BEGIN_NAMESPACE
class QColor;
QT_END_NAMESPACE

namespace TextEditor {

class Format;
class FontSettings;

namespace Internal {
class FontSettingsPagePrivate;
}

// GUI description of a format consisting of id (settings key)
// and displayName to be displayed
class TEXTEDITOR_EXPORT FormatDescription {
public:
  enum ShowControls {
    ShowForegroundControl = 0x1,
    ShowBackgroundControl = 0x2,
    ShowFontControls = 0x4,
    ShowUnderlineControl = 0x8,
    ShowRelativeForegroundControl = 0x10,
    ShowRelativeBackgroundControl = 0x20,
    ShowRelativeControls = ShowRelativeForegroundControl | ShowRelativeBackgroundControl,
    ShowFontUnderlineAndRelativeControls = ShowFontControls | ShowUnderlineControl | ShowRelativeControls,
    ShowAllAbsoluteControls = ShowForegroundControl | ShowBackgroundControl | ShowFontControls | ShowUnderlineControl,
    ShowAllAbsoluteControlsExceptUnderline = ShowAllAbsoluteControls & ~ShowUnderlineControl,
    ShowAllControls = ShowAllAbsoluteControls | ShowRelativeControls
  };

  FormatDescription() = default;
  FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, ShowControls showControls = ShowAllAbsoluteControls);
  FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const QColor &foreground, ShowControls showControls = ShowAllAbsoluteControls);
  FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const Format &format, ShowControls showControls = ShowAllAbsoluteControls);
  FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const QColor &underlineColor, const QTextCharFormat::UnderlineStyle underlineStyle, ShowControls showControls = ShowAllAbsoluteControls);

  auto id() const -> TextStyle { return m_id; }
  auto displayName() const -> QString { return m_displayName; }
  static auto defaultForeground(TextStyle id) -> QColor;
  static auto defaultBackground(TextStyle id) -> QColor;
  auto format() const -> const Format& { return m_format; }
  auto format() -> Format& { return m_format; }
  auto tooltipText() const -> QString { return m_tooltipText; }
  auto showControl(ShowControls showControl) const -> bool;

private:
  TextStyle m_id;        // Name of the category
  Format m_format;       // Default format
  QString m_displayName; // Displayed name of the category
  QString m_tooltipText; // Description text for category
  ShowControls m_showControls = ShowAllAbsoluteControls;
};

using FormatDescriptions = std::vector<FormatDescription>;

class TEXTEDITOR_EXPORT FontSettingsPage final : public Core::IOptionsPage {
public:
  FontSettingsPage(FontSettings *fontSettings, const FormatDescriptions &fd);

  auto setFontZoom(int zoom) -> void;
};

} // namespace TextEditor
