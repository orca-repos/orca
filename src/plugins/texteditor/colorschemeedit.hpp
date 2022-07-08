// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "colorscheme.hpp"
#include "fontsettingspage.hpp"

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace TextEditor {
namespace Internal {

namespace Ui {
class ColorSchemeEdit;
}

class FormatsModel;

/*!
  A widget for editing a color scheme. Used in the FontSettingsPage.
  */
class ColorSchemeEdit : public QWidget {
  Q_OBJECT

public:
  ColorSchemeEdit(QWidget *parent = nullptr);
  ~ColorSchemeEdit() override;

  auto setFormatDescriptions(const FormatDescriptions &descriptions) -> void;
  auto setBaseFont(const QFont &font) -> void;
  auto setReadOnly(bool readOnly) -> void;

  auto setColorScheme(const ColorScheme &colorScheme) -> void;
  auto colorScheme() const -> const ColorScheme&;

signals:
  auto copyScheme() -> void;

private:
  auto currentItemChanged(const QModelIndex &index) -> void;
  auto changeForeColor() -> void;
  auto changeBackColor() -> void;
  auto eraseForeColor() -> void;
  auto eraseBackColor() -> void;
  auto changeRelativeForeColor() -> void;
  auto changeRelativeBackColor() -> void;
  auto eraseRelativeForeColor() -> void;
  auto eraseRelativeBackColor() -> void;
  auto checkCheckBoxes() -> void;
  auto changeUnderlineColor() -> void;
  auto eraseUnderlineColor() -> void;
  auto changeUnderlineStyle(int index) -> void;
  auto updateControls() -> void;
  auto updateForegroundControls() -> void;
  auto updateBackgroundControls() -> void;
  auto updateRelativeForegroundControls() -> void;
  auto updateRelativeBackgroundControls() -> void;
  auto updateFontControls() -> void;
  auto updateUnderlineControls() -> void;
  auto setItemListBackground(const QColor &color) -> void;
  auto populateUnderlineStyleComboBox() -> void;

  FormatDescriptions m_descriptions;
  ColorScheme m_scheme;
  int m_curItem = -1;
  Ui::ColorSchemeEdit *m_ui;
  FormatsModel *m_formatsModel;
  bool m_readOnly = false;
};

} // namespace Internal
} // namespace TextEditor
