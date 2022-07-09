// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppcodemodelsettings.hpp"

#include <core/dialogs/ioptionspage.hpp>

namespace CppEditor::Internal {

class CppCodeModelSettingsPage final : public Core::IOptionsPage {
public:
  explicit CppCodeModelSettingsPage(CppCodeModelSettings *settings);
};

class ClangdSettingsPage final : public Core::IOptionsPage {
public:
  explicit ClangdSettingsPage();
};

class ClangdSettingsWidget : public QWidget {
  Q_OBJECT

public:
  ClangdSettingsWidget(const ClangdSettings::Data &settingsData, bool isForProject);
  ~ClangdSettingsWidget();

  auto settingsData() const -> ClangdSettings::Data;

signals:
  auto settingsDataChanged() -> void;

private:
  class Private;
  Private *const d;
};

class ClangdProjectSettingsWidget : public QWidget {
  Q_OBJECT

public:
  ClangdProjectSettingsWidget(const ClangdProjectSettings &settings);
  ~ClangdProjectSettingsWidget();

private:
  class Private;
  Private *const d;
};

} // CppEditor::Internal namespace
