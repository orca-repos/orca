// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-search-result-window.hpp"

#include <utils/infobar.hpp>

#include <QWidget>

QT_BEGIN_NAMESPACE
class QFrame;
class QLabel;
class QLineEdit;
class QToolButton;
class QCheckBox;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class SearchResultTreeView;

class SearchResultWidget final : public QWidget {
  Q_OBJECT

public:
  explicit SearchResultWidget(QWidget *parent = nullptr);
  ~SearchResultWidget() override;

  auto setInfo(const QString &label, const QString &tool_tip, const QString &term) const -> void;
  auto additionalReplaceWidget() const -> QWidget*;
  auto setAdditionalReplaceWidget(QWidget *widget) -> void;
  auto addResults(const QList<SearchResultItem> &items, SearchResult::AddMode mode) -> void;
  auto count() const -> int;
  auto setSupportsReplace(bool replace_supported, const QString &group) -> void;
  auto supportsReplace() const -> bool;
  auto setTextToReplace(const QString &text_to_replace) const -> void;
  auto textToReplace() const -> QString;
  auto setSupportPreserveCase(bool enabled) -> void;
  auto hasFocusInternally() const -> bool;
  auto setFocusInternally() const -> void;
  auto canFocusInternally() const -> bool;
  auto notifyVisibilityChanged(bool visible) -> void;
  auto setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void;
  auto setTabWidth(int tab_width) const -> void;
  auto setAutoExpandResults(bool expand) const -> void;
  auto expandAll() const -> void;
  auto collapseAll() const -> void;
  auto goToNext() const -> void;
  auto goToPrevious() const -> void;
  auto restart() -> void;
  auto setSearchAgainSupported(bool supported) -> void;
  auto setSearchAgainEnabled(bool enabled) const -> void;
  auto setFilter(SearchResultFilter *filter) const -> void;
  auto hasFilter() const -> bool;
  auto showFilterWidget(QWidget *parent) const -> void;
  auto setReplaceEnabled(bool enabled) const -> void;

public slots:
  auto finishSearch(bool canceled) -> void;
  auto sendRequestPopup() -> void;

signals:
  auto activated(const Core::SearchResultItem &item) -> void;
  auto replaceButtonClicked(const QString &replace_text, const QList<Core::SearchResultItem> &checked_items, bool preserve_case) -> void;
  auto replaceTextChanged(const QString &replace_text) -> void;
  auto searchAgainRequested() -> void;
  auto cancelled() -> void;
  auto paused(bool paused) -> void;
  auto restarted() -> void;
  auto visibilityChanged(bool visible) -> void;
  auto requestPopup(bool focus) -> void;
  auto filterInvalidated() -> void;
  auto filterChanged() -> void;
  auto navigateStateChanged() -> void;

private:
  auto handleJumpToSearchResult(const SearchResultItem &item) -> void;
  auto handleReplaceButton() -> void;
  auto cancel() -> void;
  auto searchAgain() -> void;

  auto setShowReplaceUi(bool visible) -> void;
  auto continueAfterSizeWarning() -> void;
  auto cancelAfterSizeWarning() -> void;

  auto checkedItems() const -> QList<SearchResultItem>;
  auto updateMatchesFoundLabel() const -> void;

  SearchResultTreeView *m_search_result_tree_view = nullptr;
  bool m_searching = true;
  int m_count = 0;
  QString m_dont_ask_again_group;
  QFrame *m_message_widget = nullptr;
  Utils::InfoBar m_info_bar;
  Utils::InfoBarDisplay m_info_bar_display;
  QWidget *m_top_replace_widget = nullptr;
  QLabel *m_replace_label = nullptr;
  QLineEdit *m_replace_text_edit = nullptr;
  QToolButton *m_replace_button = nullptr;
  QToolButton *m_search_again_button = nullptr;
  QCheckBox *m_preserve_case_check = nullptr;
  QWidget *m_additional_replace_widget = nullptr;
  QWidget *m_description_container = nullptr;
  QLabel *m_label = nullptr;
  QLabel *m_search_term = nullptr;
  QToolButton *m_cancel_button = nullptr;
  QLabel *m_matches_found_label = nullptr;
  bool m_preserve_case_supported = true;
  bool m_is_showing_replace_ui = false;
  bool m_search_again_supported = false;
  bool m_replace_supported = false;
};

} // Orca::Plugin::Core
