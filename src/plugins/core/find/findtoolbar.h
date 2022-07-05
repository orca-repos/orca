// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_findwidget.h"
#include "currentdocumentfind.h"

#include <utils/id.h>
#include <utils/styledbar.h>

#include <QTimer>

namespace Core {

class FindToolBarPlaceHolder;

namespace Internal {

class FindToolBar final : public Utils::StyledBar {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(FindToolBar)

public:
  enum open_flag {
    update_focus_and_select = 0x01,
    update_find_scope = 0x02,
    update_find_text = 0x04,
    update_highlight = 0x08,
    update_all = 0x0F
  };

  Q_DECLARE_FLAGS(OpenFlags, open_flag)

  explicit FindToolBar(CurrentDocumentFind *current_document_find);
  ~FindToolBar() override;

  auto readSettings() -> void;
  auto writeSettings() const -> void;
  auto openFindToolBar(OpenFlags flags = update_all) -> void;
  auto setUseFakeVim(bool on) -> void;
  auto setLightColoredIcon(bool light_colored) const -> void;

public slots:
  auto setBackward(bool backward) -> void;

protected:
  auto focusNextPrevChild(bool next) -> bool override;
  auto resizeEvent(QResizeEvent *event) -> void override;

private:
  enum class control_style {
    text,
    icon,
    hidden
  };

  auto invokeFindNext() -> void;
  auto invokeGlobalFindNext() -> void;
  auto invokeFindPrevious() -> void;
  auto invokeGlobalFindPrevious() -> void;
  auto invokeFindStep() -> void;
  auto invokeReplace() -> void;
  auto invokeGlobalReplace() -> void;
  auto invokeReplaceNext() -> void;
  auto invokeGlobalReplaceNext() -> void;
  auto invokeReplacePrevious() -> void;
  auto invokeGlobalReplacePrevious() -> void;
  auto invokeReplaceStep() const -> void;
  auto invokeReplaceAll() const -> void;
  auto invokeGlobalReplaceAll() -> void;
  auto invokeResetIncrementalSearch() -> void;
  auto invokeFindIncremental() -> void;
  auto invokeFindEnter() -> void;
  auto invokeReplaceEnter() -> void;
  auto putSelectionToFindClipboard() -> void;
  auto updateFromFindClipboard() -> void;
  auto hideAndResetFocus() -> void;
  auto openFind(bool focus = true) -> void;
  auto findNextSelected() -> void;
  auto findPreviousSelected() -> void;
  auto selectAll() const -> void;
  auto updateActions() -> void;
  auto updateToolBar() const -> void;
  auto findFlagsChanged() const -> void;
  auto findEditButtonClicked() const -> void;
  auto findCompleterActivated(const QModelIndex &) -> void;
  auto setCaseSensitive(bool sensitive) -> void;
  auto setWholeWord(bool whole_only) -> void;
  auto setRegularExpressions(bool regexp) -> void;
  auto setPreserveCase(bool preserve_case) -> void;
  auto adaptToCandidate() -> void;
  auto setFocusToCurrentFindSupport() const -> void;
  auto installEventFilters() -> void;
  auto invokeClearResults() const -> void;
  auto setFindFlag(FindFlag flag, bool enabled) -> void;
  auto hasFindFlag(FindFlag flag) const -> bool;
  auto effectiveFindFlags() const -> FindFlags;
  static auto findToolBarPlaceHolder() -> FindToolBarPlaceHolder*;
  auto toolBarHasFocus() const -> bool;
  auto controlStyle(bool replace_is_visible) const -> control_style;
  auto setFindButtonStyle(Qt::ToolButtonStyle style) const -> void;
  auto acceptCandidateAndMoveToolBar() -> void;
  auto indicateSearchState(IFindSupport::Result search_state) -> void;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;
  auto setFindText(const QString &text) -> void;
  auto getFindText() const -> QString;
  auto getReplaceText() const -> QString;
  auto selectFindText() const -> void;
  auto updateIcons() const -> void;
  auto updateFlagMenus() const -> void;
  auto updateFindReplaceEnabled() -> void;
  auto updateReplaceEnabled() const -> void;

  CurrentDocumentFind *m_current_document_find = nullptr;
  Ui::FindWidget m_ui{};
  QCompleter *m_find_completer = nullptr;
  QCompleter *m_replace_completer = nullptr;
  QAction *m_go_to_current_find_action = nullptr;
  QAction *m_find_in_document_action = nullptr;
  QAction *m_find_next_selected_action = nullptr;
  QAction *m_find_previous_selected_action = nullptr;
  QAction *m_select_all_action = nullptr;
  QAction *m_enter_find_string_action = nullptr;
  QAction *m_find_next_action = nullptr;
  QAction *m_find_previous_action = nullptr;
  QAction *m_replace_action = nullptr;
  QAction *m_replace_next_action = nullptr;
  QAction *m_replace_previous_action = nullptr;
  QAction *m_replace_all_action = nullptr;
  QAction *m_case_sensitive_action = nullptr;
  QAction *m_whole_word_action = nullptr;
  QAction *m_regular_expression_action = nullptr;
  QAction *m_preserve_case_action = nullptr;
  QAction *m_local_find_next_action = nullptr;
  QAction *m_local_find_previous_action = nullptr;
  QAction *m_local_select_all_action = nullptr;
  QAction *m_local_replace_action = nullptr;
  QAction *m_local_replace_next_action = nullptr;
  QAction *m_local_replace_previous_action = nullptr;
  QAction *m_local_replace_all_action = nullptr;
  FindFlags m_find_flags;
  QTimer m_find_incremental_timer;
  QTimer m_find_step_timer;
  IFindSupport::Result m_last_result = IFindSupport::NotYetFound;
  bool m_use_fake_vim = false;
  bool m_event_filters_installed = false;
  bool m_find_enabled = true;
};

} // namespace Internal
} // namespace Core
