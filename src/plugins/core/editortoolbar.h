// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <utils/styledbar.h>

#include <functional>

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

namespace Core {

class IEditor;
class IDocument;
struct EditorToolBarPrivate;

/**
  * Fakes an IEditor-like toolbar for design mode widgets such as Qt Designer and Bauhaus.
  * Creates a combobox for open files and lock and close buttons on the right.
  */
class CORE_EXPORT EditorToolBar : public Utils::StyledBar {
  Q_OBJECT

public:
  explicit EditorToolBar(QWidget *parent = nullptr);
  ~EditorToolBar() override;

  using menu_provider = std::function<void(QMenu *)>;

  enum ToolbarCreationFlags {
    FlagsNone = 0,
    FlagsStandalone = 1
  };

  /**
    * Adds an editor whose state is listened to, so that the toolbar can be kept up to date
    * with regards to locked status and tooltips.
    */
  auto addEditor(IEditor *editor) -> void;

  /**
    * Sets the editor and adds its custom toolbar to the widget.
    */
  auto setCurrentEditor(IEditor *editor) -> void;
  auto setToolbarCreationFlags(ToolbarCreationFlags flags) -> void;
  auto setMenuProvider(const menu_provider &provider) const -> void;

  /**
    * Adds a toolbar to the widget and sets invisible by default.
    */
  auto addCenterToolBar(QWidget *tool_bar) const -> void;
  auto setNavigationVisible(bool is_visible) const -> void;
  auto setCanGoBack(bool can_go_back) const -> void;
  auto setCanGoForward(bool can_go_forward) const -> void;
  auto removeToolbarForEditor(IEditor *editor) -> void;
  auto setCloseSplitEnabled(bool enable) const -> void;
  auto setCloseSplitIcon(const QIcon &icon) const -> void;

signals:
  auto closeClicked() -> void;
  auto goBackClicked() -> void;
  auto goForwardClicked() -> void;
  auto horizontalSplitClicked() -> void;
  auto verticalSplitClicked() -> void;
  auto splitNewWindowClicked() -> void;
  auto closeSplitClicked() -> void;
  auto listSelectionActivated(int row) -> void;
  auto currentDocumentMoved() -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  static auto changeActiveEditor(int row) -> void;
  static auto makeEditorWritable() -> void;
  auto checkDocumentStatus() const -> void;
  auto closeEditor() -> void;
  auto updateActionShortcuts() const -> void;
  auto updateDocumentStatus(const IDocument *document) const -> void;
  auto fillListContextMenu(QMenu *menu) const -> void;
  auto updateToolBar(QWidget *tool_bar) const -> void;
  EditorToolBarPrivate *d;
};

} // namespace Core
