// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "behaviorsettingswidget.hpp"
#include "ui_behaviorsettingswidget.h"

#include "tabsettingswidget.hpp"

#include <texteditor/typingsettings.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/behaviorsettings.hpp>
#include <texteditor/extraencodingsettings.hpp>

#include <core/core-constants.hpp>
#include <core/core-interface.hpp>

#include <utils/algorithm.hpp>

#include <QList>
#include <QString>
#include <QByteArray>
#include <QTextCodec>

namespace TextEditor {

struct BehaviorSettingsWidgetPrivate {
  Internal::Ui::BehaviorSettingsWidget m_ui;
  QList<QTextCodec*> m_codecs;
};

BehaviorSettingsWidget::BehaviorSettingsWidget(QWidget *parent) : QWidget(parent), d(new BehaviorSettingsWidgetPrivate)
{
  d->m_ui.setupUi(this);

  auto mibs = QTextCodec::availableMibs();
  Utils::sort(mibs);
  const auto firstNonNegative = std::find_if(mibs.begin(), mibs.end(), [](int n) { return n >= 0; });
  if (firstNonNegative != mibs.end())
    std::rotate(mibs.begin(), firstNonNegative, mibs.end());
  foreach(int mib, mibs) {
    if (const auto codec = QTextCodec::codecForMib(mib)) {
      QString compoundName = QLatin1String(codec->name());
      foreach(const QByteArray &alias, codec->aliases()) {
        compoundName += QLatin1String(" / ");
        compoundName += QString::fromLatin1(alias);
      }
      d->m_ui.encodingBox->addItem(compoundName);
      d->m_codecs.append(codec);
    }
  }

  // Qt5 doesn't list the system locale (QTBUG-34283), so add it manually
  const QString system(QLatin1String("System"));
  if (d->m_ui.encodingBox->findText(system) == -1) {
    d->m_ui.encodingBox->insertItem(0, system);
    d->m_codecs.prepend(QTextCodec::codecForLocale());
  }

  d->m_ui.defaultLineEndings->addItems(ExtraEncodingSettings::lineTerminationModeNames());

  auto currentIndexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);
  connect(d->m_ui.autoIndent, &QAbstractButton::toggled, this, &BehaviorSettingsWidget::slotTypingSettingsChanged);
  connect(d->m_ui.smartBackspaceBehavior, currentIndexChanged, this, &BehaviorSettingsWidget::slotTypingSettingsChanged);
  connect(d->m_ui.tabKeyBehavior, currentIndexChanged, this, &BehaviorSettingsWidget::slotTypingSettingsChanged);
  connect(d->m_ui.cleanWhitespace, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotStorageSettingsChanged);
  connect(d->m_ui.inEntireDocument, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotStorageSettingsChanged);
  connect(d->m_ui.addFinalNewLine, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotStorageSettingsChanged);
  connect(d->m_ui.cleanIndentation, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotStorageSettingsChanged);
  connect(d->m_ui.skipTrailingWhitespace, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotStorageSettingsChanged);
  connect(d->m_ui.mouseHiding, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.mouseNavigation, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.scrollWheelZooming, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.camelCaseNavigation, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.utf8BomBox, currentIndexChanged, this, &BehaviorSettingsWidget::slotExtraEncodingChanged);
  connect(d->m_ui.encodingBox, currentIndexChanged, this, &BehaviorSettingsWidget::slotEncodingBoxChanged);
  connect(d->m_ui.constrainTooltipsBox, currentIndexChanged, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.keyboardTooltips, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
  connect(d->m_ui.smartSelectionChanging, &QAbstractButton::clicked, this, &BehaviorSettingsWidget::slotBehaviorSettingsChanged);
}

BehaviorSettingsWidget::~BehaviorSettingsWidget()
{
  delete d;
}

auto BehaviorSettingsWidget::setActive(bool active) -> void
{
  d->m_ui.tabPreferencesWidget->setEnabled(active);
  d->m_ui.groupBoxTyping->setEnabled(active);
  d->m_ui.groupBoxEncodings->setEnabled(active);
  d->m_ui.groupBoxMouse->setEnabled(active);
  d->m_ui.groupBoxStorageSettings->setEnabled(active);
}

auto BehaviorSettingsWidget::setAssignedCodec(QTextCodec *codec) -> void
{
  const QString codecName = Orca::Plugin::Core::ICore::settings()->value(Orca::Plugin::Core::SETTINGS_DEFAULTTEXTENCODING).toString();

  auto rememberedSystemPosition = -1;
  for (auto i = 0; i < d->m_codecs.size(); ++i) {
    if (codec == d->m_codecs.at(i)) {
      if (d->m_ui.encodingBox->itemText(i) == codecName) {
        d->m_ui.encodingBox->setCurrentIndex(i);
        return;
      }
      // we've got System matching encoding - but have explicitly set the codec
      rememberedSystemPosition = i;
    }
  }
  if (rememberedSystemPosition != -1)
    d->m_ui.encodingBox->setCurrentIndex(rememberedSystemPosition);
}

auto BehaviorSettingsWidget::assignedCodecName() const -> QByteArray
{
  return d->m_ui.encodingBox->currentIndex() == 0
           ? QByteArray("System") // we prepend System to the available codecs
           : d->m_codecs.at(d->m_ui.encodingBox->currentIndex())->name();
}

auto BehaviorSettingsWidget::setCodeStyle(ICodeStylePreferences *preferences) -> void
{
  d->m_ui.tabPreferencesWidget->setPreferences(preferences);
}

auto BehaviorSettingsWidget::setAssignedTypingSettings(const TypingSettings &typingSettings) -> void
{
  d->m_ui.autoIndent->setChecked(typingSettings.m_autoIndent);
  d->m_ui.smartBackspaceBehavior->setCurrentIndex(typingSettings.m_smartBackspaceBehavior);
  d->m_ui.tabKeyBehavior->setCurrentIndex(typingSettings.m_tabKeyBehavior);

  d->m_ui.preferSingleLineComments->setChecked(typingSettings.m_preferSingleLineComments);
}

auto BehaviorSettingsWidget::assignedTypingSettings(TypingSettings *typingSettings) const -> void
{
  typingSettings->m_autoIndent = d->m_ui.autoIndent->isChecked();
  typingSettings->m_smartBackspaceBehavior = (TypingSettings::SmartBackspaceBehavior)d->m_ui.smartBackspaceBehavior->currentIndex();
  typingSettings->m_tabKeyBehavior = (TypingSettings::TabKeyBehavior)d->m_ui.tabKeyBehavior->currentIndex();

  typingSettings->m_preferSingleLineComments = d->m_ui.preferSingleLineComments->isChecked();
}

auto BehaviorSettingsWidget::setAssignedStorageSettings(const StorageSettings &storageSettings) -> void
{
  d->m_ui.cleanWhitespace->setChecked(storageSettings.m_cleanWhitespace);
  d->m_ui.inEntireDocument->setChecked(storageSettings.m_inEntireDocument);
  d->m_ui.cleanIndentation->setChecked(storageSettings.m_cleanIndentation);
  d->m_ui.addFinalNewLine->setChecked(storageSettings.m_addFinalNewLine);
  d->m_ui.skipTrailingWhitespace->setChecked(storageSettings.m_skipTrailingWhitespace);
  d->m_ui.ignoreFileTypes->setText(storageSettings.m_ignoreFileTypes);
  d->m_ui.ignoreFileTypes->setEnabled(d->m_ui.skipTrailingWhitespace->isChecked());
}

auto BehaviorSettingsWidget::assignedStorageSettings(StorageSettings *storageSettings) const -> void
{
  storageSettings->m_cleanWhitespace = d->m_ui.cleanWhitespace->isChecked();
  storageSettings->m_inEntireDocument = d->m_ui.inEntireDocument->isChecked();
  storageSettings->m_cleanIndentation = d->m_ui.cleanIndentation->isChecked();
  storageSettings->m_addFinalNewLine = d->m_ui.addFinalNewLine->isChecked();
  storageSettings->m_skipTrailingWhitespace = d->m_ui.skipTrailingWhitespace->isChecked();
  storageSettings->m_ignoreFileTypes = d->m_ui.ignoreFileTypes->text();
}

auto BehaviorSettingsWidget::updateConstrainTooltipsBoxTooltip() const -> void
{
  if (d->m_ui.constrainTooltipsBox->currentIndex() == 0) {
    d->m_ui.constrainTooltipsBox->setToolTip(tr("Displays context-sensitive help or type information on mouseover."));
  } else {
    d->m_ui.constrainTooltipsBox->setToolTip(tr("Displays context-sensitive help or type information on Shift+Mouseover."));
  }
}

auto BehaviorSettingsWidget::setAssignedBehaviorSettings(const BehaviorSettings &behaviorSettings) -> void
{
  d->m_ui.mouseHiding->setChecked(behaviorSettings.m_mouseHiding);
  d->m_ui.mouseNavigation->setChecked(behaviorSettings.m_mouseNavigation);
  d->m_ui.scrollWheelZooming->setChecked(behaviorSettings.m_scrollWheelZooming);
  d->m_ui.constrainTooltipsBox->setCurrentIndex(behaviorSettings.m_constrainHoverTooltips ? 1 : 0);
  d->m_ui.camelCaseNavigation->setChecked(behaviorSettings.m_camelCaseNavigation);
  d->m_ui.keyboardTooltips->setChecked(behaviorSettings.m_keyboardTooltips);
  d->m_ui.smartSelectionChanging->setChecked(behaviorSettings.m_smartSelectionChanging);
  updateConstrainTooltipsBoxTooltip();
}

auto BehaviorSettingsWidget::assignedBehaviorSettings(BehaviorSettings *behaviorSettings) const -> void
{
  behaviorSettings->m_mouseHiding = d->m_ui.mouseHiding->isChecked();
  behaviorSettings->m_mouseNavigation = d->m_ui.mouseNavigation->isChecked();
  behaviorSettings->m_scrollWheelZooming = d->m_ui.scrollWheelZooming->isChecked();
  behaviorSettings->m_constrainHoverTooltips = d->m_ui.constrainTooltipsBox->currentIndex() == 1;
  behaviorSettings->m_camelCaseNavigation = d->m_ui.camelCaseNavigation->isChecked();
  behaviorSettings->m_keyboardTooltips = d->m_ui.keyboardTooltips->isChecked();
  behaviorSettings->m_smartSelectionChanging = d->m_ui.smartSelectionChanging->isChecked();
}

auto BehaviorSettingsWidget::setAssignedExtraEncodingSettings(const ExtraEncodingSettings &encodingSettings) -> void
{
  d->m_ui.utf8BomBox->setCurrentIndex(encodingSettings.m_utf8BomSetting);
}

auto BehaviorSettingsWidget::assignedExtraEncodingSettings(ExtraEncodingSettings *encodingSettings) const -> void
{
  encodingSettings->m_utf8BomSetting = (ExtraEncodingSettings::Utf8BomSetting)d->m_ui.utf8BomBox->currentIndex();
}

auto BehaviorSettingsWidget::setAssignedLineEnding(int lineEnding) -> void
{
  d->m_ui.defaultLineEndings->setCurrentIndex(lineEnding);
}

auto BehaviorSettingsWidget::assignedLineEnding() const -> int
{
  return d->m_ui.defaultLineEndings->currentIndex();
}

auto BehaviorSettingsWidget::tabSettingsWidget() const -> TabSettingsWidget*
{
  return d->m_ui.tabPreferencesWidget->tabSettingsWidget();
}

auto BehaviorSettingsWidget::slotTypingSettingsChanged() -> void
{
  TypingSettings settings;
  assignedTypingSettings(&settings);
  emit typingSettingsChanged(settings);
}

auto BehaviorSettingsWidget::slotStorageSettingsChanged() -> void
{
  StorageSettings settings;
  assignedStorageSettings(&settings);

  bool ignoreFileTypesEnabled = d->m_ui.cleanWhitespace->isChecked() && d->m_ui.skipTrailingWhitespace->isChecked();
  d->m_ui.ignoreFileTypes->setEnabled(ignoreFileTypesEnabled);

  emit storageSettingsChanged(settings);
}

auto BehaviorSettingsWidget::slotBehaviorSettingsChanged() -> void
{
  BehaviorSettings settings;
  assignedBehaviorSettings(&settings);
  updateConstrainTooltipsBoxTooltip();
  emit behaviorSettingsChanged(settings);
}

auto BehaviorSettingsWidget::slotExtraEncodingChanged() -> void
{
  ExtraEncodingSettings settings;
  assignedExtraEncodingSettings(&settings);
  emit extraEncodingSettingsChanged(settings);
}

auto BehaviorSettingsWidget::slotEncodingBoxChanged(int index) -> void
{
  emit textCodecChanged(d->m_codecs.at(index));
}

} // TextEditor
