// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "coreconstants.h"
#include "icore.h"
#include "themechooser.h"

#include <core/dialogs/restartdialog.h>

#include <utils/algorithm.h>
#include <utils/theme/theme.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSpacerItem>

using namespace Utils;

static constexpr char g_theme_name_key[] = "ThemeName";

namespace Core {
namespace Internal {

ThemeEntry::ThemeEntry(const Id id, QString file_path) : m_id(id), m_filePath(std::move(file_path)) {}

auto ThemeEntry::id() const -> Id
{
  return m_id;
}

auto ThemeEntry::displayName() const -> QString
{
  if (m_displayName.isEmpty() && !m_filePath.isEmpty()) {
    const QSettings settings(m_filePath, QSettings::IniFormat);
    m_displayName = settings.value(QLatin1String(g_theme_name_key), QCoreApplication::tr("unnamed")).toString();
  }
  return m_displayName;
}

auto ThemeEntry::filePath() const -> QString
{
  return m_filePath;
}

class ThemeListModel final : public QAbstractListModel {
public:
  explicit ThemeListModel(QObject *parent = nullptr): QAbstractListModel(parent) { }

  auto rowCount(const QModelIndex &parent) const -> int override
  {
    return parent.isValid() ? 0 : static_cast<int>(m_themes.size());
  }

  auto data(const QModelIndex &index, const int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole)
      return m_themes.at(index.row()).displayName();
    return {};
  }

  auto removeTheme(const int index) -> void
  {
    beginRemoveRows(QModelIndex(), index, index);
    m_themes.removeAt(index);
    endRemoveRows();
  }

  auto setThemes(const QList<ThemeEntry> &themes) -> void
  {
    beginResetModel();
    m_themes = themes;
    endResetModel();
  }

  auto themeAt(const int index) const -> const ThemeEntry&
  {
    return m_themes.at(index);
  }

private:
  QList<ThemeEntry> m_themes;
};

class ThemeChooserPrivate {
public:
  explicit ThemeChooserPrivate(QWidget *widget);
  ~ThemeChooserPrivate();

  ThemeListModel *m_theme_list_model;
  QComboBox *m_theme_combo_box;
};

ThemeChooserPrivate::ThemeChooserPrivate(QWidget *widget) : m_theme_list_model(new ThemeListModel), m_theme_combo_box(new QComboBox)
{
  const auto layout = new QHBoxLayout(widget);
  layout->addWidget(m_theme_combo_box);

  const auto overridden_label = new QLabel;
  overridden_label->setText(ThemeChooser::tr("Current theme: %1").arg(orcaTheme()->displayName()));

  layout->addWidget(overridden_label);
  layout->setContentsMargins(0, 0, 0, 0);

  const auto horizontal_spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout->addSpacerItem(horizontal_spacer);
  m_theme_combo_box->setModel(m_theme_list_model);

  const auto themes = ThemeEntry::availableThemes();
  const auto theme_setting = ThemeEntry::themeSetting();
  const auto selected = indexOf(themes, equal(&ThemeEntry::id, theme_setting));

  m_theme_list_model->setThemes(themes);
  if (selected >= 0)
    m_theme_combo_box->setCurrentIndex(selected);
}

ThemeChooserPrivate::~ThemeChooserPrivate()
{
  delete m_theme_list_model;
}

ThemeChooser::ThemeChooser(QWidget *parent) : QWidget(parent)
{
  d = new ThemeChooserPrivate(this);
}

ThemeChooser::~ThemeChooser()
{
  delete d;
}

static auto defaultThemeId() -> QString
{
  return Theme::systemUsesDarkMode() ? QString(Constants::DEFAULT_DARK_THEME) : QString(Constants::DEFAULT_THEME);
}

auto ThemeChooser::apply() const -> void
{
  const auto index = d->m_theme_combo_box->currentIndex();

  if (index == -1)
    return;

  const auto theme_id = d->m_theme_list_model->themeAt(index).id().toString();
  const auto settings = ICore::settings();

  if (const auto current_theme_id = ThemeEntry::themeSetting().toString(); current_theme_id != theme_id) {
    // save filename of selected theme in global config
    settings->setValueWithDefault(Constants::SETTINGS_THEME, theme_id, defaultThemeId());
    RestartDialog restart_dialog(ICore::dialogParent(), tr("The theme change will take effect after restart."));
    restart_dialog.exec();
  }
}

static auto addThemesFromPath(const QString &path, QList<ThemeEntry> *themes) -> void
{
  static const QLatin1String extension("*.theme");
  QDir theme_dir(path);
  theme_dir.setNameFilters({extension});
  theme_dir.setFilter(QDir::Files);

  for(const auto theme_list = theme_dir.entryList(); const auto &filename: theme_list) {
    auto id = QFileInfo(filename).completeBaseName();
    themes->append(ThemeEntry(Id::fromString(id), theme_dir.absoluteFilePath(filename)));
  }
}

auto ThemeEntry::availableThemes() -> QList<ThemeEntry>
{
  QList<ThemeEntry> themes;

  static const auto install_theme_dir = ICore::resourcePath("themes");
  static const auto user_theme_dir = ICore::userResourcePath("themes");

  addThemesFromPath(install_theme_dir.toString(), &themes);

  if (themes.isEmpty())
    qWarning() << "Warning: No themes found in installation: " << install_theme_dir.toUserOutput();

  // move default theme to front
  if (const auto default_index = indexOf(themes, equal(&ThemeEntry::id, Id(Constants::DEFAULT_THEME))); default_index > 0) {
    // == exists and not at front
    const auto default_entry = themes.takeAt(default_index);
    themes.prepend(default_entry);
  }

  addThemesFromPath(user_theme_dir.toString(), &themes);
  return themes;
}

auto ThemeEntry::themeSetting() -> Id
{
  const auto setting = Id::fromSetting(ICore::settings()->value(Constants::SETTINGS_THEME, defaultThemeId()));
  const auto themes = availableThemes();

  if (themes.empty())
    return {};

  const auto setting_valid = contains(themes, equal(&ThemeEntry::id, setting));
  return setting_valid ? setting : themes.first().id();
}

auto ThemeEntry::createTheme(const Id id) -> Theme*
{
  if (!id.isValid())
    return nullptr;

  const auto entry = findOrDefault(availableThemes(), equal(&ThemeEntry::id, id));

  if (!entry.id().isValid())
    return nullptr;

  QSettings theme_settings(entry.filePath(), QSettings::IniFormat);
  const auto theme = new Theme(entry.id().toString());
  theme->readSettings(theme_settings);
  return theme;
}

} // namespace Internal
} // namespace Core
