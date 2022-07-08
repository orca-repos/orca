// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "projectimporter.hpp"
#include "task.hpp"

#include <utils/wizardpage.hpp>

#include <QPointer>
#include <QString>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QSpacerItem)

namespace Utils {
class FilePath;
}

namespace ProjectExplorer {
class Kit;
class Project;

namespace Internal {
class ImportWidget;
class TargetSetupPageUi;
class TargetSetupWidget;
} // namespace Internal

/// \internal
class PROJECTEXPLORER_EXPORT TargetSetupPage : public Utils::WizardPage {
  Q_OBJECT

public:
  explicit TargetSetupPage(QWidget *parent = nullptr);
  ~TargetSetupPage() override;

  /// Initializes the TargetSetupPage
  /// \note The import information is gathered in initializePage(), make sure that the right projectPath is set before
  auto initializePage() -> void override;

  // Call these before initializePage!
  auto setTasksGenerator(const TasksGenerator &tasksGenerator) -> void;
  auto setProjectPath(const Utils::FilePath &dir) -> void;
  auto setProjectImporter(ProjectImporter *importer) -> void;
  auto importLineEditHasFocus() const -> bool;
  /// Sets whether the targetsetupage uses a scrollarea
  /// to host the widgets from the factories
  /// call this before \sa initializePage()
  auto setUseScrollArea(bool b) -> void;
  auto isComplete() const -> bool override;
  auto setupProject(Project *project) -> bool;
  auto selectedKits() const -> QList<Utils::Id>;
  auto openOptions() -> void;
  auto changeAllKitsSelections() -> void;
  auto kitFilterChanged(const QString &filterText) -> void;

private:
  auto doInitializePage() -> void;
  auto showEvent(QShowEvent *event) -> void final;
  auto handleKitAddition(Kit *k) -> void;
  auto handleKitRemoval(Kit *k) -> void;
  auto handleKitUpdate(Kit *k) -> void;
  auto updateVisibility() -> void;
  auto reLayout() -> void;
  static auto compareKits(const Kit *k1, const Kit *k2) -> bool;
  auto sortedWidgetList() const -> std::vector<Internal::TargetSetupWidget*>;
  auto kitSelectionChanged() -> void;
  auto isUpdating() const -> bool;
  auto selectAtLeastOneEnabledKit() -> void;
  auto removeWidget(Kit *k) -> void { removeWidget(widget(k)); }
  auto removeWidget(Internal::TargetSetupWidget *w) -> void;
  auto addWidget(Kit *k) -> Internal::TargetSetupWidget*;
  auto addAdditionalWidgets() -> void;
  auto removeAdditionalWidgets(QLayout *layout) -> void;
  auto removeAdditionalWidgets() -> void { removeAdditionalWidgets(m_baseLayout); }
  auto updateWidget(Internal::TargetSetupWidget *widget) -> void;
  auto isUsable(const Kit *kit) const -> bool;
  auto setupImports() -> void;
  auto import(const Utils::FilePath &path, bool silent = false) -> void;
  auto setupWidgets(const QString &filterText = QString()) -> void;
  auto reset() -> void;

  auto widget(const Kit *k, Internal::TargetSetupWidget *fallback = nullptr) const -> Internal::TargetSetupWidget*
  {
    return k ? widget(k->id(), fallback) : fallback;
  }

  auto widget(const Utils::Id kitId, Internal::TargetSetupWidget *fallback = nullptr) const -> Internal::TargetSetupWidget*;

  TasksGenerator m_tasksGenerator;
  QPointer<ProjectImporter> m_importer;
  QLayout *m_baseLayout = nullptr;
  Utils::FilePath m_projectPath;
  QString m_defaultShadowBuildLocation;
  std::vector<Internal::TargetSetupWidget*> m_widgets;
  Internal::TargetSetupPageUi *m_ui;
  Internal::ImportWidget *m_importWidget;
  QSpacerItem *m_spacer;
  QList<QWidget*> m_potentialWidgets;
  bool m_widgetsWereSetUp = false;
};

} // namespace ProjectExplorer
