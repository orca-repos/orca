// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "buildinfo.hpp"
#include "kit.hpp"
#include "task.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QPushButton;
class QSpacerItem;
QT_END_NAMESPACE

namespace Utils {
class DetailsWidget;
class PathChooser;
} // namespace Utils

namespace ProjectExplorer {

class BuildInfo;

namespace Internal {

class TargetSetupWidget : public QWidget {
  Q_OBJECT

public:
  TargetSetupWidget(Kit *k, const Utils::FilePath &projectPath);

  auto kit() const -> Kit*;
  auto clearKit() -> void;
  auto isKitSelected() const -> bool;
  auto setKitSelected(bool b) -> void;
  auto addBuildInfo(const BuildInfo &info, bool isImport) -> void;
  auto selectedBuildInfoList() const -> const QList<BuildInfo>;
  auto setProjectPath(const Utils::FilePath &projectPath) -> void;
  auto expandWidget() -> void;
  auto update(const TasksGenerator &generator) -> void;

signals:
  auto selectedToggled() const -> void;

private:
  static auto buildInfoList(const Kit *k, const Utils::FilePath &projectPath) -> const QList<BuildInfo>;
  auto hasSelectedBuildConfigurations() const -> bool;
  auto toggleEnabled(bool enabled) -> void;
  auto checkBoxToggled(bool b) -> void;
  auto pathChanged() -> void;
  auto targetCheckBoxToggled(bool b) -> void;
  auto manageKit() -> void;
  auto reportIssues(int index) -> void;
  auto findIssues(const BuildInfo &info) -> QPair<Task::TaskType, QString>;
  auto clear() -> void;
  auto updateDefaultBuildDirectories() -> void;

  Kit *m_kit;
  Utils::FilePath m_projectPath;
  bool m_haveImported = false;
  Utils::DetailsWidget *m_detailsWidget;
  QPushButton *m_manageButton;
  QGridLayout *m_newBuildsLayout;

  struct BuildInfoStore {
    ~BuildInfoStore();
    BuildInfoStore() = default;
    BuildInfoStore(const BuildInfoStore &other) = delete;
    BuildInfoStore(BuildInfoStore &&other);
    auto operator=(const BuildInfoStore &other) -> BuildInfoStore& = delete;
    auto operator=(BuildInfoStore &&other) -> BuildInfoStore& = delete;

    BuildInfo buildInfo;
    QCheckBox *checkbox = nullptr;
    QLabel *label = nullptr;
    QLabel *issuesLabel = nullptr;
    Utils::PathChooser *pathChooser = nullptr;
    bool isEnabled = false;
    bool hasIssues = false;
    bool customBuildDir = false;
  };

  std::vector<BuildInfoStore> m_infoStore;
  bool m_ignoreChange = false;
  int m_selected = 0; // Number of selected "buildconfigurations"
};

} // namespace Internal
} // namespace ProjectExplorer
