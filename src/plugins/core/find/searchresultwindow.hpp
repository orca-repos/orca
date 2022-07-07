// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchresultcolor.hpp"
#include "searchresultitem.hpp"

#include <core/ioutputpane.hpp>

#include <QVariant>

QT_BEGIN_NAMESPACE
class QFont;
QT_END_NAMESPACE

namespace Core {

namespace Internal {
class SearchResultWindowPrivate;
class SearchResultWidget;
}

class SearchResultWindow;

class CORE_EXPORT SearchResultFilter : public QObject {
  Q_OBJECT

public:
  virtual auto createWidget() -> QWidget* = 0;
  virtual auto matches(const SearchResultItem &item) const -> bool = 0;

signals:
  auto filterChanged() -> void;
};

class CORE_EXPORT SearchResult final : public QObject {
  Q_OBJECT

public:
  enum AddMode {
    AddSorted,
    AddOrdered
  };

  auto setUserData(const QVariant &data) -> void;
  auto userData() const -> QVariant;
  auto supportsReplace() const -> bool;
  auto textToReplace() const -> QString;
  auto count() const -> int;
  auto setSearchAgainSupported(bool supported) const -> void;
  auto additionalReplaceWidget() const -> QWidget*;
  auto setAdditionalReplaceWidget(QWidget *widget) const -> void;

public slots:
  auto addResult(const SearchResultItem &item) const -> void;
  auto addResults(const QList<SearchResultItem> &items, AddMode mode) -> void;
  auto setFilter(SearchResultFilter *filter) const -> void; // Takes ownership
  auto finishSearch(bool canceled) const -> void;
  auto setTextToReplace(const QString &text_to_replace) const -> void;
  auto restart() const -> void;
  auto setReplaceEnabled(bool enabled) const -> void;
  auto setSearchAgainEnabled(bool enabled) const -> void;
  auto popup() const -> void;

signals:
  auto activated(const SearchResultItem &item) -> void;
  auto replaceButtonClicked(const QString &replace_text, const QList<SearchResultItem> &checked_items, bool preserve_case) -> void;
  auto replaceTextChanged(const QString &replace_text) -> void;
  auto cancelled() -> void;
  auto paused(bool paused) -> void;
  auto visibilityChanged(bool visible) -> void;
  auto countChanged(int count) -> void;
  auto searchAgainRequested() -> void;
  auto requestEnabledCheck() -> void;

private:
  SearchResult(Internal::SearchResultWidget *widget);
  friend class SearchResultWindow; // for the constructor
  Internal::SearchResultWidget *m_widget;
  QVariant m_userData;
};

class CORE_EXPORT SearchResultWindow : public IOutputPane {
  Q_OBJECT

public:
  enum SearchMode {
    SearchOnly,
    SearchAndReplace
  };

  enum PreserveCaseMode {
    PreserveCaseEnabled,
    PreserveCaseDisabled
  };

  SearchResultWindow(QWidget *new_search_panel);
  ~SearchResultWindow() override;

  static auto instance() -> SearchResultWindow*;
  auto outputWidget(QWidget *) -> QWidget* override;
  auto toolBarWidgets() const -> QList<QWidget*> override;
  auto displayName() const -> QString override { return tr("Search Results"); }
  auto priorityInStatusBar() const -> int override;
  auto visibilityChanged(bool visible) -> void override;
  auto hasFocus() const -> bool override;
  auto canFocus() const -> bool override;
  auto setFocus() -> void override;
  auto canNext() const -> bool override;
  auto canPrevious() const -> bool override;
  auto goToNext() -> void override;
  auto goToPrev() -> void override;
  auto canNavigate() const -> bool override;
  auto setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void;
  auto setTabWidth(int tab_width) const -> void;
  auto openNewSearchPanel() -> void;

  // The search result window owns the returned SearchResult
  // and might delete it any time, even while the search is running
  // (e.g. when the user clears the search result pane, or if the user opens so many other searches
  // that this search falls out of the history).
  auto startNewSearch(const QString &label, const QString &tool_tip, const QString &search_term, SearchMode search_or_search_and_replace = SearchOnly, PreserveCaseMode preserve_case_mode = PreserveCaseEnabled, const QString &cfg_group = QString()) -> SearchResult*;

public slots:
  auto clearContents() -> void;

public: // Used by plugin, do not use
  auto writeSettings() const -> void;

private:
  auto readSettings() const -> void;

  Internal::SearchResultWindowPrivate *d;
  static SearchResultWindow *m_instance;
};

} // namespace Core
