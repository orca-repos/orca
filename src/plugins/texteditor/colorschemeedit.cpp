// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "colorschemeedit.hpp"
#include "ui_colorschemeedit.h"

#include <QAbstractListModel>
#include <QColorDialog>

using namespace TextEditor;
using namespace Internal;

namespace {
const int layoutSpacing = 6;
} // namespace

static auto colorButtonStyleSheet(const QColor &bgColor) -> QString
{
  if (bgColor.isValid()) {
    QString rc = QLatin1String("border: 2px solid black; border-radius: 2px; background:");
    rc += bgColor.name();
    return rc;
  }
  return QLatin1String("border: 2px dotted black; border-radius: 2px;");
}

namespace TextEditor {
namespace Internal {

class FormatsModel : public QAbstractListModel {
public:
  FormatsModel(QObject *parent = nullptr): QAbstractListModel(parent) { }

  auto setFormatDescriptions(const FormatDescriptions *descriptions) -> void
  {
    beginResetModel();
    m_descriptions = descriptions;
    endResetModel();
  }

  auto setBaseFont(const QFont &font) -> void
  {
    emit layoutAboutToBeChanged(); // So the view adjust to new item height
    m_baseFont = font;
    emit layoutChanged();
    emitDataChanged(index(0));
  }

  auto setColorScheme(const ColorScheme *scheme) -> void
  {
    m_scheme = scheme;
    emitDataChanged(index(0));
  }

  auto rowCount(const QModelIndex &parent) const -> int override
  {
    return parent.isValid() || !m_descriptions ? 0 : int(m_descriptions->size());
  }

  auto data(const QModelIndex &index, int role) const -> QVariant override
  {
    if (!m_descriptions || !m_scheme)
      return QVariant();

    const auto &description = m_descriptions->at(index.row());

    switch (role) {
    case Qt::DisplayRole:
      return description.displayName();
    case Qt::ForegroundRole: {
      const auto foreground = m_scheme->formatFor(description.id()).foreground();
      if (foreground.isValid())
        return foreground;
      return m_scheme->formatFor(C_TEXT).foreground();
    }
    case Qt::BackgroundRole: {
      const auto background = m_scheme->formatFor(description.id()).background();
      if (background.isValid())
        return background;
      break;
    }
    case Qt::FontRole: {
      auto font = m_baseFont;
      const auto format = m_scheme->formatFor(description.id());
      font.setBold(format.bold());
      font.setItalic(format.italic());
      font.setUnderline(format.underlineStyle() != QTextCharFormat::NoUnderline);
      return font;
    }
    case Qt::ToolTipRole: {
      return description.tooltipText();
    }
    }
    return QVariant();
  }

  auto emitDataChanged(const QModelIndex &i) -> void
  {
    if (!m_descriptions)
      return;

    // If the text category changes, all indexes might have changed
    if (i.row() == 0) emit dataChanged(i, index(int(m_descriptions->size()) - 1));
    else emit dataChanged(i, i);
  }

private:
  const FormatDescriptions *m_descriptions = nullptr;
  const ColorScheme *m_scheme = nullptr;
  QFont m_baseFont;
};

} // namespace Internal
} // namespace TextEditor

ColorSchemeEdit::ColorSchemeEdit(QWidget *parent) : QWidget(parent), m_ui(new Ui::ColorSchemeEdit), m_formatsModel(new FormatsModel(this))
{
  setContentsMargins(0, layoutSpacing, 0, 0);
  m_ui->setupUi(this);
  m_ui->detailsScrollArea->viewport()->setAutoFillBackground(false);
  m_ui->scrollAreaWidgetContents->setAutoFillBackground(false);
  m_ui->itemList->setModel(m_formatsModel);
  m_ui->builtinSchemeLabel->setVisible(m_readOnly);

  populateUnderlineStyleComboBox();

  connect(m_ui->itemList->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ColorSchemeEdit::currentItemChanged);
  connect(m_ui->foregroundToolButton, &QAbstractButton::clicked, this, &ColorSchemeEdit::changeForeColor);
  connect(m_ui->backgroundToolButton, &QAbstractButton::clicked, this, &ColorSchemeEdit::changeBackColor);
  connect(m_ui->eraseBackgroundToolButton, &QAbstractButton::clicked, this, &ColorSchemeEdit::eraseBackColor);
  connect(m_ui->eraseForegroundToolButton, &QAbstractButton::clicked, this, &ColorSchemeEdit::eraseForeColor);
  connect(m_ui->foregroundSaturationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSchemeEdit::changeRelativeForeColor);
  connect(m_ui->foregroundLightnessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSchemeEdit::changeRelativeForeColor);
  connect(m_ui->backgroundSaturationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSchemeEdit::changeRelativeBackColor);
  connect(m_ui->backgroundLightnessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSchemeEdit::changeRelativeBackColor);
  connect(m_ui->boldCheckBox, &QAbstractButton::toggled, this, &ColorSchemeEdit::checkCheckBoxes);
  connect(m_ui->italicCheckBox, &QAbstractButton::toggled, this, &ColorSchemeEdit::checkCheckBoxes);
  connect(m_ui->underlineColorToolButton, &QToolButton::clicked, this, &ColorSchemeEdit::changeUnderlineColor);
  connect(m_ui->eraseUnderlineColorToolButton, &QToolButton::clicked, this, &ColorSchemeEdit::eraseUnderlineColor);
  connect(m_ui->underlineComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorSchemeEdit::changeUnderlineStyle);
  connect(m_ui->builtinSchemeLabel, &QLabel::linkActivated, this, &ColorSchemeEdit::copyScheme);
}

ColorSchemeEdit::~ColorSchemeEdit()
{
  delete m_ui;
}

auto ColorSchemeEdit::setFormatDescriptions(const FormatDescriptions &descriptions) -> void
{
  m_descriptions = descriptions;
  m_formatsModel->setFormatDescriptions(&m_descriptions);

  if (!m_descriptions.empty())
    m_ui->itemList->setCurrentIndex(m_formatsModel->index(0));
}

auto ColorSchemeEdit::setBaseFont(const QFont &font) -> void
{
  m_formatsModel->setBaseFont(font);
}

auto ColorSchemeEdit::setReadOnly(bool readOnly) -> void
{
  if (m_readOnly == readOnly)
    return;

  m_readOnly = readOnly;

  m_ui->detailsScrollArea->setVisible(!readOnly);
  m_ui->builtinSchemeLabel->setVisible(readOnly);
  updateControls();
}

auto ColorSchemeEdit::setColorScheme(const ColorScheme &colorScheme) -> void
{
  m_scheme = colorScheme;
  m_formatsModel->setColorScheme(&m_scheme);
  setItemListBackground(m_scheme.formatFor(C_TEXT).background());
  updateControls();
}

auto ColorSchemeEdit::colorScheme() const -> const ColorScheme&
{
  return m_scheme;
}

auto ColorSchemeEdit::currentItemChanged(const QModelIndex &index) -> void
{
  if (!index.isValid())
    return;

  m_curItem = index.row();
  updateControls();
}

auto ColorSchemeEdit::updateControls() -> void
{
  updateForegroundControls();
  updateBackgroundControls();
  updateRelativeForegroundControls();
  updateRelativeBackgroundControls();
  updateFontControls();
  updateUnderlineControls();
}

auto ColorSchemeEdit::updateForegroundControls() -> void
{
  const auto &formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowForegroundControl);

  m_ui->relativeForegroundHeadline->setEnabled(isVisible);
  m_ui->foregroundLabel->setVisible(isVisible);
  m_ui->foregroundToolButton->setVisible(isVisible);
  m_ui->eraseForegroundToolButton->setVisible(isVisible);
  m_ui->foregroundSpacer->setVisible(isVisible);

  m_ui->foregroundToolButton->setStyleSheet(colorButtonStyleSheet(format.foreground()));
  m_ui->eraseForegroundToolButton->setEnabled(!m_readOnly && m_curItem > 0 && format.foreground().isValid());
}

auto ColorSchemeEdit::updateBackgroundControls() -> void
{
  const auto formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowBackgroundControl);

  m_ui->relativeBackgroundHeadline->setVisible(isVisible);
  m_ui->backgroundLabel->setVisible(isVisible);
  m_ui->backgroundToolButton->setVisible(isVisible);
  m_ui->eraseBackgroundToolButton->setVisible(isVisible);
  m_ui->backgroundSpacer->setVisible(isVisible);

  m_ui->backgroundToolButton->setStyleSheet(colorButtonStyleSheet(format.background()));
  m_ui->eraseBackgroundToolButton->setEnabled(!m_readOnly && m_curItem > 0 && format.background().isValid());
}

auto ColorSchemeEdit::updateRelativeForegroundControls() -> void
{
  const auto &formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  QSignalBlocker saturationSignalBlocker(m_ui->foregroundSaturationSpinBox);
  QSignalBlocker lightnessSignalBlocker(m_ui->foregroundLightnessSpinBox);

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowRelativeForegroundControl);

  m_ui->relativeForegroundHeadline->setVisible(isVisible);
  m_ui->foregroundSaturationLabel->setVisible(isVisible);
  m_ui->foregroundLightnessLabel->setVisible(isVisible);
  m_ui->foregroundSaturationSpinBox->setVisible(isVisible);
  m_ui->foregroundLightnessSpinBox->setVisible(isVisible);
  m_ui->relativeForegroundSpacer1->setVisible(isVisible);
  m_ui->relativeForegroundSpacer2->setVisible(isVisible);
  m_ui->relativeForegroundSpacer3->setVisible(isVisible);

  auto isEnabled = !m_readOnly && !format.foreground().isValid();

  m_ui->relativeForegroundHeadline->setEnabled(isEnabled);
  m_ui->foregroundSaturationLabel->setEnabled(isEnabled);
  m_ui->foregroundLightnessLabel->setEnabled(isEnabled);
  m_ui->foregroundSaturationSpinBox->setEnabled(isEnabled);
  m_ui->foregroundLightnessSpinBox->setEnabled(isEnabled);

  m_ui->foregroundSaturationSpinBox->setValue(format.relativeForegroundSaturation());
  m_ui->foregroundLightnessSpinBox->setValue(format.relativeForegroundLightness());
}

auto ColorSchemeEdit::updateRelativeBackgroundControls() -> void
{
  const auto &formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  QSignalBlocker saturationSignalBlocker(m_ui->backgroundSaturationSpinBox);
  QSignalBlocker lightnessSignalBlocker(m_ui->backgroundLightnessSpinBox);

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowRelativeBackgroundControl);

  m_ui->relativeBackgroundHeadline->setVisible(isVisible);
  m_ui->backgroundSaturationLabel->setVisible(isVisible);
  m_ui->backgroundLightnessLabel->setVisible(isVisible);
  m_ui->backgroundSaturationSpinBox->setVisible(isVisible);
  m_ui->backgroundLightnessSpinBox->setVisible(isVisible);
  m_ui->relativeBackgroundSpacer1->setVisible(isVisible);
  m_ui->relativeBackgroundSpacer2->setVisible(isVisible);
  m_ui->relativeBackgroundSpacer3->setVisible(isVisible);

  auto isEnabled = !m_readOnly && !format.background().isValid();

  m_ui->relativeBackgroundHeadline->setEnabled(isEnabled);
  m_ui->backgroundSaturationLabel->setEnabled(isEnabled);
  m_ui->backgroundLightnessLabel->setEnabled(isEnabled);
  m_ui->backgroundSaturationSpinBox->setEnabled(isEnabled);
  m_ui->backgroundLightnessSpinBox->setEnabled(isEnabled);

  m_ui->backgroundSaturationSpinBox->setValue(format.relativeBackgroundSaturation());
  m_ui->backgroundLightnessSpinBox->setValue(format.relativeBackgroundLightness());
}

auto ColorSchemeEdit::updateFontControls() -> void
{
  const auto formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  QSignalBlocker boldSignalBlocker(m_ui->boldCheckBox);
  QSignalBlocker italicSignalBlocker(m_ui->italicCheckBox);

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowFontControls);

  m_ui->fontHeadline->setVisible(isVisible);
  m_ui->boldCheckBox->setVisible(isVisible);
  m_ui->italicCheckBox->setVisible(isVisible);
  m_ui->fontSpacer1->setVisible(isVisible);
  m_ui->fontSpacer2->setVisible(isVisible);

  m_ui->boldCheckBox->setChecked(format.bold());
  m_ui->italicCheckBox->setChecked(format.italic());
}

auto ColorSchemeEdit::updateUnderlineControls() -> void
{
  const auto formatDescription = m_descriptions[m_curItem];
  const auto &format = m_scheme.formatFor(formatDescription.id());

  QSignalBlocker comboBoxSignalBlocker(m_ui->underlineComboBox);

  auto isVisible = !m_readOnly && formatDescription.showControl(FormatDescription::ShowUnderlineControl);

  m_ui->underlineHeadline->setVisible(isVisible);
  m_ui->underlineLabel->setVisible(isVisible);
  m_ui->underlineColorToolButton->setVisible(isVisible);
  m_ui->eraseUnderlineColorToolButton->setVisible(isVisible);
  m_ui->underlineComboBox->setVisible(isVisible);
  m_ui->underlineSpacer1->setVisible(isVisible);
  m_ui->underlineSpacer2->setVisible(isVisible);

  m_ui->underlineColorToolButton->setStyleSheet(colorButtonStyleSheet(format.underlineColor()));
  m_ui->eraseUnderlineColorToolButton->setEnabled(!m_readOnly && m_curItem > 0 && format.underlineColor().isValid());
  int index = m_ui->underlineComboBox->findData(QVariant::fromValue(int(format.underlineStyle())));
  m_ui->underlineComboBox->setCurrentIndex(index);
}

auto ColorSchemeEdit::changeForeColor() -> void
{
  if (m_curItem == -1)
    return;
  const auto color = m_scheme.formatFor(m_descriptions[m_curItem].id()).foreground();
  const auto newColor = QColorDialog::getColor(color, m_ui->boldCheckBox->window());
  if (!newColor.isValid())
    return;
  m_ui->foregroundToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseForegroundToolButton->setEnabled(true);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setForeground(newColor);
    m_formatsModel->emitDataChanged(index);
  }

  updateControls();
}

auto ColorSchemeEdit::changeBackColor() -> void
{
  if (m_curItem == -1)
    return;
  const auto color = m_scheme.formatFor(m_descriptions[m_curItem].id()).background();
  const auto newColor = QColorDialog::getColor(color, m_ui->boldCheckBox->window());
  if (!newColor.isValid())
    return;
  m_ui->backgroundToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseBackgroundToolButton->setEnabled(true);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setBackground(newColor);
    m_formatsModel->emitDataChanged(index);
    // Synchronize item list background with text background
    if (index.row() == 0)
      setItemListBackground(newColor);
  }

  updateControls();
}

auto ColorSchemeEdit::eraseBackColor() -> void
{
  if (m_curItem == -1)
    return;
  const QColor newColor;
  m_ui->backgroundToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseBackgroundToolButton->setEnabled(false);

  foreach(const QModelIndex &index, m_ui->itemList->selectionModel()->selectedRows()) {
    const TextStyle category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setBackground(newColor);
    m_formatsModel->emitDataChanged(index);
  }

  updateControls();
}

auto ColorSchemeEdit::eraseForeColor() -> void
{
  if (m_curItem == -1)
    return;
  const QColor newColor;
  m_ui->foregroundToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseForegroundToolButton->setEnabled(false);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setForeground(newColor);
    m_formatsModel->emitDataChanged(index);
  }

  updateControls();
}

auto ColorSchemeEdit::changeRelativeForeColor() -> void
{
  if (m_curItem == -1)
    return;

  const double saturation = m_ui->foregroundSaturationSpinBox->value();
  const double lightness = m_ui->foregroundLightnessSpinBox->value();

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setRelativeForegroundSaturation(saturation);
    m_scheme.formatFor(category).setRelativeForegroundLightness(lightness);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::changeRelativeBackColor() -> void
{
  if (m_curItem == -1)
    return;

  const double saturation = m_ui->backgroundSaturationSpinBox->value();
  const double lightness = m_ui->backgroundLightnessSpinBox->value();

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setRelativeBackgroundSaturation(saturation);
    m_scheme.formatFor(category).setRelativeBackgroundLightness(lightness);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::eraseRelativeForeColor() -> void
{
  if (m_curItem == -1)
    return;

  m_ui->foregroundSaturationSpinBox->setValue(0.0);
  m_ui->foregroundLightnessSpinBox->setValue(0.0);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setRelativeForegroundSaturation(0.0);
    m_scheme.formatFor(category).setRelativeForegroundLightness(0.0);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::eraseRelativeBackColor() -> void
{
  if (m_curItem == -1)
    return;

  m_ui->backgroundSaturationSpinBox->setValue(0.0);
  m_ui->backgroundLightnessSpinBox->setValue(0.0);

  foreach(const QModelIndex &index, m_ui->itemList->selectionModel()->selectedRows()) {
    const TextStyle category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setRelativeBackgroundSaturation(0.0);
    m_scheme.formatFor(category).setRelativeBackgroundLightness(0.0);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::checkCheckBoxes() -> void
{
  if (m_curItem == -1)
    return;

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setBold(m_ui->boldCheckBox->isChecked());
    m_scheme.formatFor(category).setItalic(m_ui->italicCheckBox->isChecked());
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::changeUnderlineColor() -> void
{
  if (m_curItem == -1)
    return;
  const auto color = m_scheme.formatFor(m_descriptions[m_curItem].id()).underlineColor();
  const auto newColor = QColorDialog::getColor(color, m_ui->boldCheckBox->window());
  if (!newColor.isValid())
    return;
  m_ui->underlineColorToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseUnderlineColorToolButton->setEnabled(true);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setUnderlineColor(newColor);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::eraseUnderlineColor() -> void
{
  if (m_curItem == -1)
    return;
  const QColor newColor;
  m_ui->underlineColorToolButton->setStyleSheet(colorButtonStyleSheet(newColor));
  m_ui->eraseUnderlineColorToolButton->setEnabled(false);

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    m_scheme.formatFor(category).setUnderlineColor(newColor);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::changeUnderlineStyle(int comboBoxIndex) -> void
{
  if (m_curItem == -1)
    return;

  for (const QModelIndex &index : m_ui->itemList->selectionModel()->selectedRows()) {
    const auto category = m_descriptions[index.row()].id();
    auto value = m_ui->underlineComboBox->itemData(comboBoxIndex);
    auto enumeratorIndex = static_cast<QTextCharFormat::UnderlineStyle>(value.toInt());
    m_scheme.formatFor(category).setUnderlineStyle(enumeratorIndex);
    m_formatsModel->emitDataChanged(index);
  }
}

auto ColorSchemeEdit::setItemListBackground(const QColor &color) -> void
{
  QPalette pal;
  pal.setColor(QPalette::Base, color);
  m_ui->itemList->setPalette(pal);
}

auto ColorSchemeEdit::populateUnderlineStyleComboBox() -> void
{
  m_ui->underlineComboBox->addItem(tr("No Underline"), QVariant::fromValue(int(QTextCharFormat::NoUnderline)));
  m_ui->underlineComboBox->addItem(tr("Single Underline"), QVariant::fromValue(int(QTextCharFormat::SingleUnderline)));
  m_ui->underlineComboBox->addItem(tr("Wave Underline"), QVariant::fromValue(int(QTextCharFormat::WaveUnderline)));
  m_ui->underlineComboBox->addItem(tr("Dot Underline"), QVariant::fromValue(int(QTextCharFormat::DotLine)));
  m_ui->underlineComboBox->addItem(tr("Dash Underline"), QVariant::fromValue(int(QTextCharFormat::DashUnderline)));
  m_ui->underlineComboBox->addItem(tr("Dash-Dot Underline"), QVariant::fromValue(int(QTextCharFormat::DashDotLine)));
  m_ui->underlineComboBox->addItem(tr("Dash-Dot-Dot Underline"), QVariant::fromValue(int(QTextCharFormat::DashDotDotLine)));
}
