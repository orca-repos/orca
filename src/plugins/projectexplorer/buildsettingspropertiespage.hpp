// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
QT_END_NAMESPACE

namespace ProjectExplorer {

class BuildConfiguration;
class BuildInfo;
class NamedWidget;
class Target;

namespace Internal {

class BuildSettingsWidget : public QWidget {
  Q_OBJECT

public:
  BuildSettingsWidget(Target *target);
  ~BuildSettingsWidget() override;

  auto clearWidgets() -> void;
  auto addSubWidget(NamedWidget *widget) -> void;

private:
  auto updateBuildSettings() -> void;
  auto currentIndexChanged(int index) -> void;
  auto renameConfiguration() -> void;
  auto updateAddButtonMenu() -> void;
  auto updateActiveConfiguration() -> void;
  auto createConfiguration(const BuildInfo &info) -> void;
  auto cloneConfiguration() -> void;
  auto deleteConfiguration(BuildConfiguration *toDelete) -> void;
  auto uniqueName(const QString &name) -> QString;

  Target *m_target = nullptr;
  BuildConfiguration *m_buildConfiguration = nullptr;
  QPushButton *m_addButton = nullptr;
  QPushButton *m_removeButton = nullptr;
  QPushButton *m_renameButton = nullptr;
  QPushButton *m_cloneButton = nullptr;
  QPushButton *m_makeActiveButton = nullptr;
  QComboBox *m_buildConfigurationComboBox = nullptr;
  QMenu *m_addButtonMenu = nullptr;
  QList<NamedWidget*> m_subWidgets;
  QList<QLabel*> m_labels;
};

} // namespace Internal
} // namespace ProjectExplorer
