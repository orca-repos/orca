// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-general-settings.hpp"
#include "ui_core-general-settings.h"

#include "core-constants.hpp"
#include "core-interface.hpp"
#include "core-restart-dialog.hpp"

#include <utils/algorithm.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/infobar.hpp>
#include <utils/stylehelper.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStyleHints>
#include <QTextCodec>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr char settingsKeyDPI[] = "Core/EnableHighDpiScaling";
constexpr char settingsKeyShortcutsInContextMenu[] = "General/ShowShortcutsInContextMenu";
constexpr char settingsKeyCodecForLocale[] = "General/OverrideCodecForLocale";

class GeneralSettingsWidget final : public IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::GeneralSettings)

public:
  explicit GeneralSettingsWidget(GeneralSettings *q);

  auto apply() -> void final;
  auto resetInterfaceColor() const -> void;
  auto resetWarnings() const -> void;
  auto resetLanguage() const -> void;
  static auto canResetWarnings() -> bool;
  auto fillLanguageBox() const -> void;
  static auto language() -> QString;
  static auto setLanguage(const QString &) -> void;
  auto fillCodecBox() const -> void;
  static auto codecForLocale() -> QByteArray;
  static auto setCodecForLocale(const QByteArray &) -> void;

  GeneralSettings *q;
  Ui::GeneralSettings m_ui{};
};

GeneralSettingsWidget::GeneralSettingsWidget(GeneralSettings *q) : q(q)
{
  m_ui.setupUi(this);

  fillLanguageBox();
  fillCodecBox();

  m_ui.colorButton->setColor(StyleHelper::requestedBaseColor());
  m_ui.resetWarningsButton->setEnabled(canResetWarnings());
  m_ui.showShortcutsInContextMenus->setText(tr("Show keyboard shortcuts in context menus (default: %1)").arg(q->m_default_show_shortcuts_in_context_menu ? tr("on") : tr("off")));
  m_ui.showShortcutsInContextMenus->setChecked(GeneralSettings::showShortcutsInContextMenu());

  if constexpr (HostOsInfo::isMacHost()) {
    m_ui.dpiCheckbox->setVisible(false);
  } else {
    constexpr auto default_value = HostOsInfo::isWindowsHost();
    m_ui.dpiCheckbox->setChecked(ICore::settings()->value(settingsKeyDPI, default_value).toBool());
    connect(m_ui.dpiCheckbox, &QCheckBox::toggled, this, [default_value](const bool checked) {
      ICore::settings()->setValueWithDefault(settingsKeyDPI, checked, default_value);
      QMessageBox::information(ICore::dialogParent(), tr("Restart Required"), tr("The high DPI settings will take effect after restart."));
    });
  }

  connect(m_ui.resetColorButton, &QAbstractButton::clicked, this, &GeneralSettingsWidget::resetInterfaceColor);
  connect(m_ui.resetWarningsButton, &QAbstractButton::clicked, this, &GeneralSettingsWidget::resetWarnings);
}

static auto hasQmFilesForLocale(const QString &locale, const QString &creator_tr_path) -> bool
{
  static const auto qt_tr_path = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
  const QString tr_file = QLatin1String("/qt_") + locale + QLatin1String(".qm");
  return QFile::exists(qt_tr_path + tr_file) || QFile::exists(creator_tr_path + tr_file);
}

auto GeneralSettingsWidget::fillLanguageBox() const -> void
{
  const auto current_locale = language();

  m_ui.languageBox->addItem(tr("<System Language>"), QString());
  // need to add this explicitly, since there is no qm file for English
  m_ui.languageBox->addItem(QLatin1String("English"), QLatin1String("C"));

  if (current_locale == QLatin1String("C"))
    m_ui.languageBox->setCurrentIndex(m_ui.languageBox->count() - 1);

  const auto creator_tr_path = ICore::resourcePath("translations");

  for (const auto language_files = creator_tr_path.toDir().entryList(QStringList(QLatin1String("orca*.qm"))); const auto &language_file : language_files) {
    const auto start = static_cast<int>(language_file.indexOf('_') + 1);
    const auto end = static_cast<int>(language_file.lastIndexOf('.'));

    // no need to show a language that creator will not load anyway
    if (const auto locale = language_file.mid(start, end - start); hasQmFilesForLocale(locale, creator_tr_path.toString())) {
      QLocale tmp_locale(locale);
      QString language_item = QLocale::languageToString(tmp_locale.language()) + QLatin1String(" (") + QLocale::countryToString(tmp_locale.country()) + QLatin1Char(')');
      m_ui.languageBox->addItem(language_item, locale);
      if (locale == current_locale)
        m_ui.languageBox->setCurrentIndex(m_ui.languageBox->count() - 1);
    }
  }
}

auto GeneralSettingsWidget::apply() -> void
{
  auto current_index = m_ui.languageBox->currentIndex();
  setLanguage(m_ui.languageBox->itemData(current_index, Qt::UserRole).toString());
  current_index = m_ui.codecBox->currentIndex();
  setCodecForLocale(m_ui.codecBox->itemText(current_index).toLocal8Bit());
  q->setShowShortcutsInContextMenu(m_ui.showShortcutsInContextMenus->isChecked());
  // Apply the new base color if accepted
  StyleHelper::setBaseColor(m_ui.colorButton->color());
  m_ui.themeChooser->apply();
}

auto GeneralSettings::showShortcutsInContextMenu() -> bool
{
  return ICore::settings()->value(settingsKeyShortcutsInContextMenu, QGuiApplication::styleHints()->showShortcutsInContextMenus()).toBool();
}

auto GeneralSettingsWidget::resetInterfaceColor() const -> void
{
  m_ui.colorButton->setColor(StyleHelper::DEFAULT_BASE_COLOR);
}

auto GeneralSettingsWidget::resetWarnings() const -> void
{
  InfoBar::clearGloballySuppressed();
  CheckableMessageBox::resetAllDoNotAskAgainQuestions(ICore::settings());
  m_ui.resetWarningsButton->setEnabled(false);
}

auto GeneralSettingsWidget::canResetWarnings() -> bool
{
  return InfoBar::anyGloballySuppressed() || CheckableMessageBox::hasSuppressedQuestions(ICore::settings());
}

auto GeneralSettingsWidget::resetLanguage() const -> void
{
  // system language is default
  m_ui.languageBox->setCurrentIndex(0);
}

auto GeneralSettingsWidget::language() -> QString
{
  const QSettings *settings = ICore::settings();
  return settings->value(QLatin1String("General/OverrideLanguage")).toString();
}

auto GeneralSettingsWidget::setLanguage(const QString &locale) -> void
{
  const auto settings = ICore::settings();

  if (settings->value(QLatin1String("General/OverrideLanguage")).toString() != locale) {
    RestartDialog dialog(ICore::dialogParent(), tr("The language change will take effect after restart."));
    dialog.exec();
  }

  settings->setValueWithDefault(QLatin1String("General/OverrideLanguage"), locale, {});
}

auto GeneralSettingsWidget::fillCodecBox() const -> void
{
  const auto current_codec = codecForLocale();
  auto codecs = QTextCodec::availableCodecs();
  sort(codecs);

  for (const auto &codec : qAsConst(codecs)) {
    m_ui.codecBox->addItem(QString::fromLocal8Bit(codec));
    if (codec == current_codec)
      m_ui.codecBox->setCurrentIndex(m_ui.codecBox->count() - 1);
  }
}

auto GeneralSettingsWidget::codecForLocale() -> QByteArray
{
  const QSettings *settings = ICore::settings();
  auto codec = settings->value(settingsKeyCodecForLocale).toByteArray();

  if (codec.isEmpty())
    codec = QTextCodec::codecForLocale()->name();

  return codec;
}

auto GeneralSettingsWidget::setCodecForLocale(const QByteArray &codec) -> void
{
  const auto settings = ICore::settings();
  settings->setValueWithDefault(settingsKeyCodecForLocale, codec, {});
  QTextCodec::setCodecForLocale(QTextCodec::codecForName(codec));
}

auto GeneralSettings::setShowShortcutsInContextMenu(const bool show) const -> void
{
  ICore::settings()->setValueWithDefault(settingsKeyShortcutsInContextMenu, show, m_default_show_shortcuts_in_context_menu);
  QGuiApplication::styleHints()->setShowShortcutsInContextMenus(show);
}

GeneralSettings::GeneralSettings()
{
  setId(SETTINGS_ID_INTERFACE);
  setDisplayName(GeneralSettingsWidget::tr("Interface"));
  setCategory(SETTINGS_CATEGORY_CORE);
  setDisplayCategory(QCoreApplication::translate("Core", "Environment"));
  setCategoryIconPath(":/core/images/settingscategory_orca.png");
  setWidgetCreator([this] { return new GeneralSettingsWidget(this); });
  m_default_show_shortcuts_in_context_menu = QGuiApplication::styleHints()->showShortcutsInContextMenus();
}

} // namespace Orca::Plugin::Core
