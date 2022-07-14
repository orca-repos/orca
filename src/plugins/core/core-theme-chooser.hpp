// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-options-page-interface.hpp"

#include <utils/id.hpp>

#include <QWidget>

namespace Utils {
class Theme;
}

namespace Orca::Plugin::Core {

class ThemeChooserPrivate;

class ThemeEntry {
public:
  ThemeEntry() = default;
  ThemeEntry(Utils::Id id, QString file_path);

  auto id() const -> Utils::Id;
  auto displayName() const -> QString;
  auto filePath() const -> QString;
  static auto availableThemes() -> QList<ThemeEntry>;
  static auto themeSetting() -> Utils::Id;
  static auto createTheme(Utils::Id id) -> Utils::Theme*;

private:
  Utils::Id m_id;
  QString m_filePath;
  mutable QString m_displayName;
};

class ThemeChooser : public QWidget {
  Q_OBJECT

public:
  ThemeChooser(QWidget *parent = nullptr);
  ~ThemeChooser() override;

  auto apply() const -> void;

private:
  ThemeChooserPrivate *d;
};

} // namespace Orca::Plugin::Core
