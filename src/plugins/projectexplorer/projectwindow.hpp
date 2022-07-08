// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/fancymainwindow.hpp>
#include <utils/id.hpp>

#include <memory>

namespace Core {
class OutputWindow;
}

namespace ProjectExplorer {
namespace Internal {

enum {
  // To augment a context menu, data has a QMenu*
  ContextMenuItemAdderRole = Qt::UserRole + 1,
  ProjectDisplayNameRole,
  // Shown in the project selection combobox
  ItemActivatedDirectlyRole,
  // This item got activated through user interaction and
  // is now responsible for the central widget.
  ItemActivatedFromBelowRole,
  // A subitem gots activated and gives us the opportunity to adjust
  ItemActivatedFromAboveRole,
  // A parent item gots activated and makes us its active child.
  ItemDeactivatedFromBelowRole,
  // A subitem got deactivated and gives us the opportunity to adjust
  ItemUpdatedFromBelowRole,
  // A subitem got updated, re-expansion is necessary.
  ActiveItemRole,
  // The index of the currently selected item in the tree view
  KitIdRole,
  // The kit id in case the item is associated with a kit.
  PanelWidgetRole // This item's widget to be shown as central widget.
};

class ProjectWindowPrivate;

class ProjectWindow : public Utils::FancyMainWindow {
  friend class ProjectWindowPrivate;
  Q_OBJECT

public:
  ProjectWindow();
  ~ProjectWindow() override;

  auto activateProjectPanel(Utils::Id panelId) -> void;
  auto buildSystemOutput() const -> Core::OutputWindow*;

private:
  auto hideEvent(QHideEvent *event) -> void override;
  auto showEvent(QShowEvent *event) -> void override;
  auto savePersistentSettings() const -> void;
  auto loadPersistentSettings() -> void;

  const std::unique_ptr<ProjectWindowPrivate> d;
};

} // namespace Internal
} // namespace ProjectExplorer
