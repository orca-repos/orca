// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fontsettingspage.hpp"

#include "fontsettings.hpp"
#include "texteditorsettings.hpp"
#include "ui_fontsettingspage.h"

#include <core/core-interface.hpp>

#include <utils/fileutils.hpp>
#include <utils/filepath.hpp>
#include <utils/stringutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>

#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QSettings>
#include <QDebug>

using namespace TextEditor::Internal;

namespace TextEditor {
namespace Internal {

struct ColorSchemeEntry {
  ColorSchemeEntry(const QString &fileName, bool readOnly): fileName(fileName), name(ColorScheme::readNameOfScheme(fileName)), readOnly(readOnly) { }

  QString fileName;
  QString name;
  QString id;
  bool readOnly;
};

class SchemeListModel : public QAbstractListModel {
public:
  SchemeListModel(QObject *parent = nullptr): QAbstractListModel(parent) { }

  auto rowCount(const QModelIndex &parent) const -> int override
  {
    return parent.isValid() ? 0 : m_colorSchemes.size();
  }

  auto data(const QModelIndex &index, int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole)
      return m_colorSchemes.at(index.row()).name;

    return QVariant();
  }

  auto removeColorScheme(int index) -> void
  {
    beginRemoveRows(QModelIndex(), index, index);
    m_colorSchemes.removeAt(index);
    endRemoveRows();
  }

  auto setColorSchemes(const QList<ColorSchemeEntry> &colorSchemes) -> void
  {
    beginResetModel();
    m_colorSchemes = colorSchemes;
    endResetModel();
  }

  auto colorSchemeAt(int index) const -> const ColorSchemeEntry& { return m_colorSchemes.at(index); }

private:
  QList<ColorSchemeEntry> m_colorSchemes;
};

class FontSettingsPageWidget : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::FontSettingsPageWidget)
public:
  FontSettingsPageWidget(FontSettingsPage *q, const FormatDescriptions &fd, FontSettings *fontSettings) : q(q), m_value(*fontSettings), m_descriptions(fd)
  {
    m_lastValue = m_value;

    m_ui.setupUi(this);
    m_ui.colorSchemeGroupBox->setTitle(tr("Color Scheme for Theme \"%1\"").arg(Utils::orcaTheme()->displayName()));
    m_ui.schemeComboBox->setModel(&m_schemeListModel);

    m_ui.fontComboBox->setCurrentFont(m_value.family());

    m_ui.antialias->setChecked(m_value.antialias());
    m_ui.zoomSpinBox->setValue(m_value.fontZoom());

    m_ui.schemeEdit->setFormatDescriptions(fd);
    m_ui.schemeEdit->setBaseFont(m_value.font());
    m_ui.schemeEdit->setColorScheme(m_value.colorScheme());

    auto sizeValidator = new QIntValidator(m_ui.sizeComboBox);
    sizeValidator->setBottom(0);
    m_ui.sizeComboBox->setValidator(sizeValidator);

    connect(m_ui.fontComboBox, &QFontComboBox::currentFontChanged, this, &FontSettingsPageWidget::fontSelected);
    connect(m_ui.sizeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FontSettingsPageWidget::fontSizeSelected);
    connect(m_ui.zoomSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FontSettingsPageWidget::fontZoomChanged);
    connect(m_ui.antialias, &QCheckBox::toggled, this, &FontSettingsPageWidget::antialiasChanged);
    connect(m_ui.schemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FontSettingsPageWidget::colorSchemeSelected);
    connect(m_ui.copyButton, &QPushButton::clicked, this, &FontSettingsPageWidget::openCopyColorSchemeDialog);
    connect(m_ui.schemeEdit, &ColorSchemeEdit::copyScheme, this, &FontSettingsPageWidget::openCopyColorSchemeDialog);
    connect(m_ui.deleteButton, &QPushButton::clicked, this, &FontSettingsPageWidget::confirmDeleteColorScheme);
    connect(m_ui.importButton, &QPushButton::clicked, this, &FontSettingsPageWidget::importScheme);
    connect(m_ui.exportButton, &QPushButton::clicked, this, &FontSettingsPageWidget::exportScheme);

    updatePointSizes();
    refreshColorSchemeList();
  }

  auto apply() -> void final;
  auto finish() -> void final;
  auto saveSettings() -> void;
  auto fontSelected(const QFont &font) -> void;
  auto fontSizeSelected(int index) -> void;
  auto fontZoomChanged() -> void;
  auto antialiasChanged() -> void;
  auto colorSchemeSelected(int index) -> void;
  auto openCopyColorSchemeDialog() -> void;
  auto copyColorScheme(const QString &name) -> void;
  auto confirmDeleteColorScheme() -> void;
  auto importScheme() -> void;
  auto exportScheme() -> void;
  auto deleteColorScheme() -> void;
  auto maybeSaveColorScheme() -> void;
  auto updatePointSizes() -> void;
  auto pointSizesForSelectedFont() const -> QList<int>;
  auto refreshColorSchemeList() -> void;

  FontSettingsPage *q;
  Ui::FontSettingsPage m_ui;
  bool m_refreshingSchemeList = false;
  FontSettings &m_value;
  FontSettings m_lastValue;
  SchemeListModel m_schemeListModel;
  FormatDescriptions m_descriptions;
};

} // namespace Internal

static auto customStylesPath() -> Utils::FilePath
{
  return Orca::Plugin::Core::ICore::userResourcePath("styles");
}

static auto createColorSchemeFileName(const QString &pattern) -> Utils::FilePath
{
  const auto stylesPath = customStylesPath();

  // Find an available file name
  auto i = 1;
  Utils::FilePath filePath;
  do {
    filePath = stylesPath.pathAppended(pattern.arg(i == 1 ? QString() : QString::number(i)));
    ++i;
  } while (filePath.exists());

  // Create the base directory when it doesn't exist
  if (!stylesPath.exists() && !stylesPath.createDir()) {
    qWarning() << "Failed to create color scheme directory:" << stylesPath;
    return {};
  }

  return filePath;
}

// ------- FormatDescription
FormatDescription::FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const QColor &foreground, ShowControls showControls) : m_id(id), m_displayName(displayName), m_tooltipText(tooltipText), m_showControls(showControls)
{
  m_format.setForeground(foreground);
  m_format.setBackground(defaultBackground(id));
}

FormatDescription::FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const Format &format, ShowControls showControls) : m_id(id), m_format(format), m_displayName(displayName), m_tooltipText(tooltipText), m_showControls(showControls) {}

FormatDescription::FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, const QColor &underlineColor, const QTextCharFormat::UnderlineStyle underlineStyle, ShowControls showControls) : m_id(id), m_displayName(displayName), m_tooltipText(tooltipText), m_showControls(showControls)
{
  m_format.setForeground(defaultForeground(id));
  m_format.setBackground(defaultBackground(id));
  m_format.setUnderlineColor(underlineColor);
  m_format.setUnderlineStyle(underlineStyle);
}

FormatDescription::FormatDescription(TextStyle id, const QString &displayName, const QString &tooltipText, ShowControls showControls) : m_id(id), m_displayName(displayName), m_tooltipText(tooltipText), m_showControls(showControls)
{
  m_format.setForeground(defaultForeground(id));
  m_format.setBackground(defaultBackground(id));
}

auto FormatDescription::defaultForeground(TextStyle id) -> QColor
{
  if (id == C_TEXT) {
    return Qt::black;
  }
  if (id == C_LINE_NUMBER) {
    const auto palette = Utils::Theme::initialPalette();
    const auto bg = palette.window().color();
    if (bg.value() < 128)
      return palette.windowText().color();
    return palette.dark().color();
  }
  if (id == C_CURRENT_LINE_NUMBER) {
    const auto palette = Utils::Theme::initialPalette();
    const auto bg = palette.window().color();
    if (bg.value() < 128)
      return palette.windowText().color();
    return QColor();
  }
  if (id == C_PARENTHESES) {
    return QColor(Qt::red);
  }
  if (id == C_AUTOCOMPLETE) {
    return QColor(Qt::darkBlue);
  }
  if (id == C_SEARCH_RESULT_ALT1) {
    return QColor(0x00, 0x00, 0x33);
  }
  if (id == C_SEARCH_RESULT_ALT2) {
    return QColor(0x33, 0x00, 0x00);
  }
  return QColor();
}

auto FormatDescription::defaultBackground(TextStyle id) -> QColor
{
  if (id == C_TEXT) {
    return Qt::white;
  }
  if (id == C_LINE_NUMBER) {
    return Utils::Theme::initialPalette().window().color();
  }
  if (id == C_SEARCH_RESULT) {
    return QColor(0xffef0b);
  }
  if (id == C_SEARCH_RESULT_ALT1) {
    return QColor(0xb6, 0xcc, 0xff);
  }
  if (id == C_SEARCH_RESULT_ALT2) {
    return QColor(0xff, 0xb6, 0xcc);
  }
  if (id == C_PARENTHESES) {
    return QColor(0xb4, 0xee, 0xb4);
  }
  if (id == C_PARENTHESES_MISMATCH) {
    return QColor(Qt::magenta);
  }
  if (id == C_AUTOCOMPLETE) {
    return QColor(192, 192, 255);
  }
  if (id == C_CURRENT_LINE || id == C_SEARCH_SCOPE) {
    const auto palette = Utils::Theme::initialPalette();
    const auto &fg = palette.color(QPalette::Highlight);
    const auto &bg = palette.color(QPalette::Base);

    qreal smallRatio;
    qreal largeRatio;
    if (id == C_CURRENT_LINE) {
      smallRatio = .3;
      largeRatio = .6;
    } else {
      smallRatio = .05;
      largeRatio = .4;
    }
    const auto ratio = palette.color(QPalette::Text).value() < 128 ^ palette.color(QPalette::HighlightedText).value() < 128 ? smallRatio : largeRatio;

    const auto &col = QColor::fromRgbF(fg.redF() * ratio + bg.redF() * (1 - ratio), fg.greenF() * ratio + bg.greenF() * (1 - ratio), fg.blueF() * ratio + bg.blueF() * (1 - ratio));
    return col;
  }
  if (id == C_SELECTION) {
    return Utils::Theme::initialPalette().color(QPalette::Highlight);
  }
  if (id == C_OCCURRENCES) {
    return QColor(180, 180, 180);
  }
  if (id == C_OCCURRENCES_RENAME) {
    return QColor(255, 100, 100);
  }
  if (id == C_DISABLED_CODE) {
    return QColor(239, 239, 239);
  }
  return QColor(); // invalid color
}

auto FormatDescription::showControl(ShowControls showControl) const -> bool
{
  return m_showControls & showControl;
}

auto FontSettingsPageWidget::fontSelected(const QFont &font) -> void
{
  m_value.setFamily(font.family());
  m_ui.schemeEdit->setBaseFont(font);
  updatePointSizes();
}

namespace Internal {

auto FontSettingsPageWidget::updatePointSizes() -> void
{
  // Update point sizes
  const auto oldSize = m_value.fontSize();
  m_ui.sizeComboBox->clear();
  const auto sizeLst = pointSizesForSelectedFont();
  auto idx = -1;
  auto i = 0;
  for (; i < sizeLst.count(); ++i) {
    if (idx == -1 && sizeLst.at(i) >= oldSize) {
      idx = i;
      if (sizeLst.at(i) != oldSize)
        m_ui.sizeComboBox->addItem(QString::number(oldSize));
    }
    m_ui.sizeComboBox->addItem(QString::number(sizeLst.at(i)));
  }
  if (idx != -1)
    m_ui.sizeComboBox->setCurrentIndex(idx);
}

auto FontSettingsPageWidget::pointSizesForSelectedFont() const -> QList<int>
{
  QFontDatabase db;
  const QString familyName = m_ui.fontComboBox->currentFont().family();
  QList<int> sizeLst = db.pointSizes(familyName);
  if (!sizeLst.isEmpty())
    return sizeLst;

  QStringList styles = db.styles(familyName);
  if (!styles.isEmpty())
    sizeLst = db.pointSizes(familyName, styles.first());
  if (sizeLst.isEmpty())
    sizeLst = QFontDatabase::standardSizes();

  return sizeLst;
}

auto FontSettingsPageWidget::fontSizeSelected(int index) -> void
{
  const QString sizeString = m_ui.sizeComboBox->itemText(index);
  auto ok = true;
  const int size = sizeString.toInt(&ok);
  if (ok) {
    m_value.setFontSize(size);
    m_ui.schemeEdit->setBaseFont(m_value.font());
  }
}

auto FontSettingsPageWidget::fontZoomChanged() -> void
{
  m_value.setFontZoom(m_ui.zoomSpinBox->value());
}

auto FontSettingsPageWidget::antialiasChanged() -> void
{
  m_value.setAntialias(m_ui.antialias->isChecked());
  m_ui.schemeEdit->setBaseFont(m_value.font());
}

auto FontSettingsPageWidget::colorSchemeSelected(int index) -> void
{
  auto readOnly = true;
  if (index != -1) {
    // Check whether we're switching away from a changed color scheme
    if (!m_refreshingSchemeList)
      maybeSaveColorScheme();

    const auto &entry = m_schemeListModel.colorSchemeAt(index);
    readOnly = entry.readOnly;
    m_value.loadColorScheme(entry.fileName, m_descriptions);
    m_ui.schemeEdit->setColorScheme(m_value.colorScheme());
  }
  m_ui.copyButton->setEnabled(index != -1);
  m_ui.deleteButton->setEnabled(!readOnly);
  m_ui.schemeEdit->setReadOnly(readOnly);
}

auto FontSettingsPageWidget::openCopyColorSchemeDialog() -> void
{
  QInputDialog *dialog = new QInputDialog(m_ui.copyButton->window());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setInputMode(QInputDialog::TextInput);
  dialog->setWindowTitle(tr("Copy Color Scheme"));
  dialog->setLabelText(tr("Color scheme name:"));
  dialog->setTextValue(tr("%1 (copy)").arg(m_value.colorScheme().displayName()));

  connect(dialog, &QInputDialog::textValueSelected, this, &FontSettingsPageWidget::copyColorScheme);
  dialog->open();
}

auto FontSettingsPageWidget::copyColorScheme(const QString &name) -> void
{
  int index = m_ui.schemeComboBox->currentIndex();
  if (index == -1)
    return;

  const ColorSchemeEntry &entry = m_schemeListModel.colorSchemeAt(index);

  QString baseFileName = QFileInfo(entry.fileName).completeBaseName();
  baseFileName += QLatin1String("_copy%1.xml");
  const Utils::FilePath fileName = createColorSchemeFileName(baseFileName);

  if (!fileName.isEmpty()) {
    // Ask about saving any existing modifications
    maybeSaveColorScheme();

    // Make sure we're copying the current version
    m_value.setColorScheme(m_ui.schemeEdit->colorScheme());

    auto scheme = m_value.colorScheme();
    scheme.setDisplayName(name);
    if (scheme.save(fileName.path(), Orca::Plugin::Core::ICore::dialogParent()))
      m_value.setColorSchemeFileName(fileName.path());

    refreshColorSchemeList();
  }
}

auto FontSettingsPageWidget::confirmDeleteColorScheme() -> void
{
  const int index = m_ui.schemeComboBox->currentIndex();
  if (index == -1)
    return;

  const ColorSchemeEntry &entry = m_schemeListModel.colorSchemeAt(index);
  if (entry.readOnly)
    return;

  QMessageBox *messageBox = new QMessageBox(QMessageBox::Warning, tr("Delete Color Scheme"), tr("Are you sure you want to delete this color scheme permanently?"), QMessageBox::Discard | QMessageBox::Cancel, m_ui.deleteButton->window());

  // Change the text and role of the discard button
  auto deleteButton = static_cast<QPushButton*>(messageBox->button(QMessageBox::Discard));
  deleteButton->setText(tr("Delete"));
  messageBox->addButton(deleteButton, QMessageBox::AcceptRole);
  messageBox->setDefaultButton(deleteButton);

  connect(messageBox, &QDialog::accepted, this, &FontSettingsPageWidget::deleteColorScheme);
  messageBox->setAttribute(Qt::WA_DeleteOnClose);
  messageBox->open();
}

auto FontSettingsPageWidget::deleteColorScheme() -> void
{
  const int index = m_ui.schemeComboBox->currentIndex();
  QTC_ASSERT(index != -1, return);

  const ColorSchemeEntry &entry = m_schemeListModel.colorSchemeAt(index);
  QTC_ASSERT(!entry.readOnly, return);

  if (QFile::remove(entry.fileName))
    m_schemeListModel.removeColorScheme(index);
}

auto FontSettingsPageWidget::importScheme() -> void
{
  const auto importedFile = Utils::FileUtils::getOpenFilePath(this, tr("Import Color Scheme"), {}, tr("Color scheme (*.xml);;All files (*)"));

  if (importedFile.isEmpty())
    return;

  auto fileName = createColorSchemeFileName(importedFile.baseName() + "%1." + importedFile.suffix());

  // Ask about saving any existing modifications
  maybeSaveColorScheme();

  QInputDialog *dialog = new QInputDialog(m_ui.copyButton->window());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setInputMode(QInputDialog::TextInput);
  dialog->setWindowTitle(tr("Import Color Scheme"));
  dialog->setLabelText(tr("Color scheme name:"));
  dialog->setTextValue(importedFile.baseName());

  connect(dialog, &QInputDialog::textValueSelected, this, [this, fileName](const QString &name) {
    m_value.setColorScheme(m_ui.schemeEdit->colorScheme());

    auto scheme = m_value.colorScheme();
    scheme.setDisplayName(name);
    if (scheme.save(fileName.path(), Orca::Plugin::Core::ICore::dialogParent()))
      m_value.setColorSchemeFileName(fileName.path());

    refreshColorSchemeList();
  });
  dialog->open();
}

auto FontSettingsPageWidget::exportScheme() -> void
{
  int index = m_ui.schemeComboBox->currentIndex();
  if (index == -1)
    return;

  const ColorSchemeEntry &entry = m_schemeListModel.colorSchemeAt(index);

  const Utils::FilePath filePath = Utils::FileUtils::getSaveFilePath(this, tr("Export Color Scheme"), Utils::FilePath::fromString(entry.fileName), tr("Color scheme (*.xml);;All files (*)"));

  if (!filePath.isEmpty())
    m_value.colorScheme().save(filePath.toString(), Orca::Plugin::Core::ICore::dialogParent());
}

auto FontSettingsPageWidget::maybeSaveColorScheme() -> void
{
  if (m_value.colorScheme() == m_ui.schemeEdit->colorScheme())
    return;

  QMessageBox messageBox(QMessageBox::Warning, tr("Color Scheme Changed"), tr("The color scheme \"%1\" was modified, do you want to save the changes?").arg(m_ui.schemeEdit->colorScheme().displayName()), QMessageBox::Discard | QMessageBox::Save, m_ui.schemeComboBox->window());

  // Change the text of the discard button
  auto discardButton = static_cast<QPushButton*>(messageBox.button(QMessageBox::Discard));
  discardButton->setText(tr("Discard"));
  messageBox.addButton(discardButton, QMessageBox::DestructiveRole);
  messageBox.setDefaultButton(QMessageBox::Save);

  if (messageBox.exec() == QMessageBox::Save) {
    const ColorScheme &scheme = m_ui.schemeEdit->colorScheme();
    scheme.save(m_value.colorSchemeFileName(), Orca::Plugin::Core::ICore::dialogParent());
  }
}

auto FontSettingsPageWidget::refreshColorSchemeList() -> void
{
  QList<ColorSchemeEntry> colorSchemes;

  QDir styleDir(Orca::Plugin::Core::ICore::resourcePath("styles").toDir());
  styleDir.setNameFilters(QStringList() << QLatin1String("*.xml"));
  styleDir.setFilter(QDir::Files);

  auto selected = 0;

  QStringList schemeList = styleDir.entryList();
  const auto defaultScheme = Utils::FilePath::fromString(FontSettings::defaultSchemeFileName()).fileName();
  if (schemeList.removeAll(defaultScheme))
    schemeList.prepend(defaultScheme);
  foreach(const QString &file, schemeList) {
    const QString fileName = styleDir.absoluteFilePath(file);
    if (m_value.colorSchemeFileName() == fileName)
      selected = colorSchemes.size();
    colorSchemes.append(ColorSchemeEntry(fileName, true));
  }

  if (colorSchemes.isEmpty())
    qWarning() << "Warning: no color schemes found in path:" << styleDir.path();

  styleDir.setPath(customStylesPath().path());

  foreach(const QString &file, styleDir.entryList()) {
    const QString fileName = styleDir.absoluteFilePath(file);
    if (m_value.colorSchemeFileName() == fileName)
      selected = colorSchemes.size();
    colorSchemes.append(ColorSchemeEntry(fileName, false));
  }

  m_refreshingSchemeList = true;
  m_schemeListModel.setColorSchemes(colorSchemes);
  m_ui.schemeComboBox->setCurrentIndex(selected);
  m_refreshingSchemeList = false;
}

auto FontSettingsPageWidget::apply() -> void
{
  if (m_value.colorScheme() != m_ui.schemeEdit->colorScheme()) {
    // Update the scheme and save it under the name it already has
    m_value.setColorScheme(m_ui.schemeEdit->colorScheme());
    const auto &scheme = m_value.colorScheme();
    scheme.save(m_value.colorSchemeFileName(), Orca::Plugin::Core::ICore::dialogParent());
  }

  bool ok;
  int fontSize = m_ui.sizeComboBox->currentText().toInt(&ok);
  if (ok && m_value.fontSize() != fontSize) {
    m_value.setFontSize(fontSize);
    m_ui.schemeEdit->setBaseFont(m_value.font());
  }

  int index = m_ui.schemeComboBox->currentIndex();
  if (index != -1) {
    const ColorSchemeEntry &entry = m_schemeListModel.colorSchemeAt(index);
    if (entry.fileName != m_value.colorSchemeFileName())
      m_value.loadColorScheme(entry.fileName, m_descriptions);
  }

  saveSettings();
}

auto FontSettingsPageWidget::saveSettings() -> void
{
  m_lastValue = m_value;
  m_value.toSettings(Orca::Plugin::Core::ICore::settings());
  emit TextEditorSettings::instance()->fontSettingsChanged(m_value);
}

auto FontSettingsPageWidget::finish() -> void
{
  // If changes were applied, these are equal. Otherwise restores last value.
  m_value = m_lastValue;
}

} // namespace Internal

// FontSettingsPage

FontSettingsPage::FontSettingsPage(FontSettings *fontSettings, const FormatDescriptions &fd)
{
  const QSettings *settings = Orca::Plugin::Core::ICore::settings();
  if (settings)
    fontSettings->fromSettings(fd, settings);

  if (fontSettings->colorSchemeFileName().isEmpty())
    fontSettings->loadColorScheme(FontSettings::defaultSchemeFileName(), fd);

  setId(Constants::TEXT_EDITOR_FONT_SETTINGS);
  setDisplayName(FontSettingsPageWidget::tr("Font && Colors"));
  setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
  setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
  setWidgetCreator([this, fontSettings, fd] { return new FontSettingsPageWidget(this, fd, fontSettings); });
}

auto FontSettingsPage::setFontZoom(int zoom) -> void
{
  if (m_widget)
    static_cast<FontSettingsPageWidget*>(m_widget.data())->m_ui.zoomSpinBox->setValue(zoom);
}

} // TextEditor
