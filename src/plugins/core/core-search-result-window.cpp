// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-search-result-window.hpp"

#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-interface.hpp"
#include "core-search-result-widget.hpp"
#include "core-text-find-constants.hpp"

#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>

static constexpr char settings_key_section_name[] = "SearchResults";
static constexpr char settings_key_expand_results[] = "ExpandResults";
static constexpr int max_search_history = 12;

namespace Orca::Plugin::Core {

/*!
    \namespace Orca::Plugin::Core::Search
    \inmodule Orca
    \internal
*/

/*!
    \class Orca::Plugin::Core::TextPosition
    \inmodule Orca
    \internal
*/

/*!
    \class Orca::Plugin::Core::TextRange
    \inmodule Orca
    \internal
*/

/*!
    \class Orca::Plugin::Core::SearchResultItem
    \inmodule Orca
    \internal
*/

class InternalScrollArea final : public QScrollArea {
  Q_OBJECT public:
  explicit InternalScrollArea(QWidget *parent) : QScrollArea(parent)
  {
    setFrameStyle(NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  }

  auto sizeHint() const -> QSize override
  {
    if (widget())
      return widget()->size();

    return QScrollArea::sizeHint();
  }
};

class SearchResultWindowPrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::SearchResultWindow)

public:
  SearchResultWindowPrivate(SearchResultWindow *window, QWidget *new_search_panel);
  auto isSearchVisible() const -> bool { return m_current_index > 0; }
  auto visibleSearchIndex() const -> int { return m_current_index - 1; }
  auto setCurrentIndex(int index, bool focus) -> void;
  auto setCurrentIndexWithFocus(const int index) -> void { setCurrentIndex(index, true); }
  auto moveWidgetToTop() -> void;
  auto popupRequested(bool focus) -> void;
  auto handleExpandCollapseToolButton(bool checked) const -> void;
  auto updateFilterButton() const -> void;
  auto toolBarWidgets() -> QList<QWidget*>;

  SearchResultWindow *q;
  QList<SearchResultWidget*> m_search_result_widgets;
  QToolButton *m_expand_collapse_button;
  QToolButton *m_filter_button;
  QToolButton *m_new_search_button;
  QAction *m_expand_collapse_action;
  static const bool m_initially_expand;
  QWidget *m_spacer;
  QLabel *m_history_label = nullptr;
  QWidget *m_spacer2;
  QComboBox *m_recent_searches_box = nullptr;
  QStackedWidget *m_widget;
  QList<SearchResult*> m_search_results;
  int m_current_index;
  QFont m_font;
  search_result_colors m_colors;
  int m_tab_width;

};

constexpr bool SearchResultWindowPrivate::m_initially_expand = false;

SearchResultWindowPrivate::SearchResultWindowPrivate(SearchResultWindow *window, QWidget *new_search_panel) : q(window), m_expand_collapse_button(nullptr), m_expand_collapse_action(new QAction(tr("Expand All"), window)), m_spacer(new QWidget), m_spacer2(new QWidget), m_widget(new QStackedWidget), m_current_index(0), m_tab_width(8)
{
  m_spacer->setMinimumWidth(30);
  m_spacer2->setMinimumWidth(5);
  m_widget->setWindowTitle(q->displayName());

  const auto new_search_area = new InternalScrollArea(m_widget);
  new_search_area->setWidget(new_search_panel);
  new_search_area->setFocusProxy(new_search_panel);
  m_widget->addWidget(new_search_area);

  m_expand_collapse_button = new QToolButton(m_widget);
  m_expand_collapse_action->setCheckable(true);
  m_expand_collapse_action->setIcon(Utils::Icons::EXPAND_ALL_TOOLBAR.icon());
  m_expand_collapse_action->setEnabled(false);

  auto cmd = ActionManager::registerAction(m_expand_collapse_action, "Find.ExpandAll");
  cmd->setAttribute(Command::CA_UpdateText);
  m_expand_collapse_button->setDefaultAction(cmd->action());

  m_filter_button = new QToolButton(m_widget);
  m_filter_button->setText(tr("Filter Results"));
  m_filter_button->setIcon(Utils::Icons::FILTER.icon());
  m_filter_button->setEnabled(false);

  const auto new_search_action = new QAction(tr("New Search"), this);
  new_search_action->setIcon(Utils::Icons::NEWSEARCH_TOOLBAR.icon());
  cmd = ActionManager::command(ADVANCED_FIND);
  m_new_search_button = Command::toolButtonWithAppendedShortcut(new_search_action, cmd);

  if (QTC_GUARD(cmd && cmd->action()))
    connect(m_new_search_button, &QToolButton::triggered, cmd->action(), &QAction::trigger);

  connect(m_expand_collapse_action, &QAction::toggled, this, &SearchResultWindowPrivate::handleExpandCollapseToolButton);
  connect(m_filter_button, &QToolButton::clicked, this, [this] {
    if (!isSearchVisible())
      return;
    m_search_result_widgets.at(visibleSearchIndex())->showFilterWidget(m_filter_button);
  });
}

auto SearchResultWindowPrivate::setCurrentIndex(const int index, const bool focus) -> void
{
  QTC_ASSERT(m_recent_searches_box, return);

  if (isSearchVisible())
    m_search_result_widgets.at(visibleSearchIndex())->notifyVisibilityChanged(false);

  m_current_index = index;
  m_widget->setCurrentIndex(index);
  m_recent_searches_box->setCurrentIndex(index);

  if (!isSearchVisible()) {
    if (focus)
      m_widget->currentWidget()->setFocus();
    m_expand_collapse_action->setEnabled(false);
    m_new_search_button->setEnabled(false);
  } else {
    if (focus)
      m_search_result_widgets.at(visibleSearchIndex())->setFocusInternally();
    m_search_result_widgets.at(visibleSearchIndex())->notifyVisibilityChanged(true);
    m_expand_collapse_action->setEnabled(true);
    m_new_search_button->setEnabled(true);
  }

  q->navigateStateChanged();
  updateFilterButton();
}

auto SearchResultWindowPrivate::moveWidgetToTop() -> void
{
  QTC_ASSERT(m_recent_searches_box, return);
  const auto widget = qobject_cast<SearchResultWidget*>(sender());
  QTC_ASSERT(widget, return);
  const auto index = static_cast<int>(m_search_result_widgets.indexOf(widget));

  if (index == 0)
    return; // nothing to do

  const auto internal_index = index + 1/*account for "new search" entry*/;
  const auto search_entry = m_recent_searches_box->itemText(internal_index);

  m_search_result_widgets.removeAt(index);
  m_widget->removeWidget(widget);
  m_recent_searches_box->removeItem(internal_index);

  const auto result = m_search_results.takeAt(index);

  m_search_result_widgets.prepend(widget);
  m_widget->insertWidget(1, widget);
  m_recent_searches_box->insertItem(1, search_entry);
  m_search_results.prepend(result);

  // adapt the current index
  if (index == visibleSearchIndex()) {
    // was visible, so we switch
    // this is the default case
    m_current_index = 1;
    m_widget->setCurrentIndex(1);
    m_recent_searches_box->setCurrentIndex(1);
  } else if (visibleSearchIndex() < index) {
    // academical case where the widget moved before the current widget
    // only our internal book keeping needed
    ++m_current_index;
  }
}

auto SearchResultWindowPrivate::popupRequested(const bool focus) -> void
{
  const auto widget = qobject_cast<SearchResultWidget*>(sender());
  QTC_ASSERT(widget, return);
  const auto internal_index = static_cast<int>(m_search_result_widgets.indexOf(widget) + 1) /*account for "new search" entry*/;
  setCurrentIndex(internal_index, focus);
  q->popup(focus ? IOutputPane::ModeSwitch | IOutputPane::WithFocus : IOutputPane::NoModeSwitch);
}

/*!
    \enum Orca::Plugin::Core::SearchResultWindow::SearchMode
    This enum type specifies whether a search should show the replace UI or not:

    \value SearchOnly
           The search does not support replace.
    \value SearchAndReplace
           The search supports replace, so show the UI for it.
*/

/*!
    \class Orca::Plugin::Core::SearchResult
    \inheaderfile coreplugin/find/searchresultwindow.h
    \inmodule Orca

    \brief The SearchResult class reports user interaction, such as the
    activation of a search result item.

    Whenever a new search is initiated via startNewSearch, an instance of this
    class is returned to provide the initiator with the hooks for handling user
    interaction.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::activated(const Orca::Plugin::Core::SearchResultItem &item)
    Indicates that the user activated the search result \a item by
    double-clicking it, for example.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::replaceButtonClicked(const QString &replaceText,
                           const QList<Orca::Plugin::Core::SearchResultItem> &checkedItems,
                           bool preserveCase)

    Indicates that the user initiated a text replace by selecting
    \uicontrol {Replace All}, for example.

    The signal reports the text to use for replacement in \a replaceText,
    the list of search result items that were selected by the user
    in \a checkedItems, and whether a search and replace should preserve the
    case of the replaced strings in \a preserveCase.
    The handler of this signal should apply the replace only on the selected
    items.
*/

/*!
    \enum Orca::Plugin::Core::SearchResult::AddMode
    This enum type specifies whether the search results should be sorted or
    ordered:

    \value AddSorted
           The search results are sorted.
    \value AddOrdered
           The search results are ordered.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::cancelled()
    This signal is emitted if the user cancels the search.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::countChanged(int count)
    This signal is emitted when the number of search hits changes to \a count.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::paused(bool paused)
    This signal is emitted when the search status is set to \a paused.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::requestEnabledCheck()

    This signal is emitted when the enabled status of search results is
    requested.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::searchAgainRequested()

    This signal is emitted when the \uicontrol {Search Again} button is
    selected.
*/

/*!
    \fn void Orca::Plugin::Core::SearchResult::visibilityChanged(bool visible)

    This signal is emitted when the visibility of the search results changes
    to \a visible.
*/

/*!
    \class Orca::Plugin::Core::SearchResultWindow
    \inheaderfile coreplugin/find/searchresultwindow.h
    \inmodule Orca

    \brief The SearchResultWindow class is the implementation of a commonly
    shared \uicontrol{Search Results} output pane.

    \image core-searchresults.png

    Whenever you want to show the user a list of search results, or want
    to present UI for a global search and replace, use the single instance
    of this class.

    In addition to being an implementation of an output pane, the
    SearchResultWindow has functions and enums that enable other
    plugins to show their search results and hook into the user actions for
    selecting an entry and performing a global replace.

    Whenever you start a search, call startNewSearch(SearchMode) to initialize
    the \uicontrol {Search Results} output pane. The parameter determines if the GUI for
    replacing should be shown.
    The function returns a SearchResult object that is your
    hook into the signals from user interaction for this search.
    When you produce search results, call addResults() or addResult() to add them
    to the \uicontrol {Search Results} output pane.
    After the search has finished call finishSearch() to inform the
    \uicontrol {Search Results} output pane about it.

    You will get activated() signals via your SearchResult instance when
    the user selects a search result item. If you started the search
    with the SearchAndReplace option, the replaceButtonClicked() signal
    is emitted when the user requests a replace.
*/

/*!
    \fn QString Orca::Plugin::Core::SearchResultWindow::displayName() const
    \internal
*/

/*!
    \enum Orca::Plugin::Core::SearchResultWindow::PreserveCaseMode
    This enum type specifies whether a search and replace should preserve the
    case of the replaced strings:

    \value PreserveCaseEnabled
           The case is preserved when replacings strings.
    \value PreserveCaseDisabled
           The given case is used when replacing strings.
*/

SearchResultWindow *SearchResultWindow::m_instance = nullptr;

/*!
    \internal
*/
SearchResultWindow::SearchResultWindow(QWidget *new_search_panel) : d(new SearchResultWindowPrivate(this, new_search_panel))
{
  m_instance = this;
  readSettings();
}

/*!
    \internal
*/
SearchResultWindow::~SearchResultWindow()
{
  qDeleteAll(d->m_search_results);
  delete d->m_widget;
  d->m_widget = nullptr;
  delete d;
}

/*!
    Returns the single shared instance of the \uicontrol {Search Results} output pane.
*/
auto SearchResultWindow::instance() -> SearchResultWindow*
{
  return m_instance;
}

/*!
    \internal
*/
auto SearchResultWindow::visibilityChanged(const bool visible) -> void
{
  if (d->isSearchVisible())
    d->m_search_result_widgets.at(d->visibleSearchIndex())->notifyVisibilityChanged(visible);
}

/*!
    \internal
*/
auto SearchResultWindow::outputWidget(QWidget *) -> QWidget*
{
  return d->m_widget;
}

/*!
    \internal
*/
auto SearchResultWindow::toolBarWidgets() const -> QList<QWidget*>
{
  return d->toolBarWidgets();
}

/*!
    Tells the \uicontrol {Search Results} output pane to start a new search.

    The \a label should be a string that shortly describes the type of the
    search, that is, the search filter and possibly the most relevant search
    option, followed by a colon (:). For example: \c {Project 'myproject':}
    The \a searchTerm is shown after the colon.

    The \a toolTip should elaborate on the search parameters, like file patterns
    that are searched and find flags.

    If \a cfgGroup is not empty, it will be used for storing the \e {do not ask again}
    setting of a \e {this change cannot be undone} warning (which is implicitly requested
    by passing a non-empty group).

    The \a searchOrSearchAndReplace parameter holds whether the search
    results pane should show a UI for a global search and replace action.
    The \a preserveCaseMode parameter holds whether the case of the search
    string should be preserved when replacing strings.

    Returns a SearchResult object that is used for signaling user interaction
    with the results of this search.
    The search result window owns the returned SearchResult
    and might delete it any time, even while the search is running.
    For example, when the user clears the \uicontrol {Search Results} pane, or when
    the user opens so many other searches that this search falls out of the history.

*/
auto SearchResultWindow::startNewSearch(const QString &label, const QString &tool_tip, const QString &search_term, const SearchMode search_or_search_and_replace, const PreserveCaseMode preserve_case_mode, const QString &cfg_group) -> SearchResult*
{
  if (QTC_GUARD(d->m_recent_searches_box)) {
    if (d->m_search_results.size() >= max_search_history) {
      if (d->m_current_index >= d->m_recent_searches_box->count() - 1) {
        // temporarily set the index to the last but one existing
        d->m_current_index = d->m_recent_searches_box->count() - 2;
      }
      d->m_search_result_widgets.last()->notifyVisibilityChanged(false);
      // widget first, because that might send interesting signals to SearchResult
      delete d->m_search_result_widgets.takeLast();
      delete d->m_search_results.takeLast();
      d->m_recent_searches_box->removeItem(d->m_recent_searches_box->count() - 1);
    }
    d->m_recent_searches_box->insertItem(1, tr("%1 %2").arg(label, search_term));
  }

  auto widget = new SearchResultWidget;
  connect(widget, &SearchResultWidget::filterInvalidated, this, [this, widget] {
    if (widget == d->m_search_result_widgets.at(d->visibleSearchIndex()))
      d->handleExpandCollapseToolButton(d->m_expand_collapse_button->isChecked());
  });

  connect(widget, &SearchResultWidget::filterChanged, d, &SearchResultWindowPrivate::updateFilterButton);
  d->m_search_result_widgets.prepend(widget);
  d->m_widget->insertWidget(1, widget);
  connect(widget, &SearchResultWidget::navigateStateChanged, this, &SearchResultWindow::navigateStateChanged);
  connect(widget, &SearchResultWidget::restarted, d, &SearchResultWindowPrivate::moveWidgetToTop);
  connect(widget, &SearchResultWidget::requestPopup, d, &SearchResultWindowPrivate::popupRequested);

  widget->setTextEditorFont(d->m_font, d->m_colors);
  widget->setTabWidth(d->m_tab_width);
  widget->setSupportPreserveCase(preserve_case_mode == PreserveCaseEnabled);
  const auto supports_replace = search_or_search_and_replace != SearchOnly;
  widget->setSupportsReplace(supports_replace, supports_replace ? cfg_group : QString());
  widget->setAutoExpandResults(d->m_expand_collapse_action->isChecked());
  widget->setInfo(label, tool_tip, search_term);
  const auto result = new SearchResult(widget);

  d->m_search_results.prepend(result);
  if (d->m_current_index > 0)
    ++d->m_current_index; // so setCurrentIndex still knows about the right "currentIndex" and its widget
  d->setCurrentIndexWithFocus(1);

  return result;
}

/*!
    Clears the current contents of the \uicontrol {Search Results} output pane.
*/
auto SearchResultWindow::clearContents() -> void
{
  if (QTC_GUARD(d->m_recent_searches_box)) {
    for (auto i = d->m_recent_searches_box->count() - 1; i > 0 /* don't want i==0 */; --i)
      d->m_recent_searches_box->removeItem(i);
  }

  for(const auto widget: d->m_search_result_widgets)
    widget->notifyVisibilityChanged(false);

  qDeleteAll(d->m_search_result_widgets);
  d->m_search_result_widgets.clear();
  qDeleteAll(d->m_search_results);
  d->m_search_results.clear();
  d->m_current_index = 0;
  d->m_widget->currentWidget()->setFocus();
  d->m_expand_collapse_action->setEnabled(false);
  navigateStateChanged();
  d->m_new_search_button->setEnabled(false);
}

/*!
    \internal
*/
auto SearchResultWindow::hasFocus() const -> bool
{
  const auto widget = d->m_widget->focusWidget();

  if (!widget)
    return false;

  return widget->window()->focusWidget() == widget;
}

/*!
    \internal
*/
auto SearchResultWindow::canFocus() const -> bool
{
  if (d->isSearchVisible())
    return d->m_search_result_widgets.at(d->visibleSearchIndex())->canFocusInternally();

  return true;
}

/*!
    \internal
*/
auto SearchResultWindow::setFocus() -> void
{
  if (!d->isSearchVisible())
    d->m_widget->currentWidget()->setFocus();
  else
    d->m_search_result_widgets.at(d->visibleSearchIndex())->setFocusInternally();
}

/*!
    \internal
*/
auto SearchResultWindow::setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void
{
  d->m_font = font;
  d->m_colors = colors;

  for(const auto widget: d->m_search_result_widgets)
    widget->setTextEditorFont(font, colors);
}

/*!
    Sets the \uicontrol {Search Results} tab width to \a tabWidth.
*/
auto SearchResultWindow::setTabWidth(const int tab_width) const -> void
{
  d->m_tab_width = tab_width;
  for(const auto widget: d->m_search_result_widgets)
    widget->setTabWidth(tab_width);
}

/*!
    Opens a new search panel.
*/
auto SearchResultWindow::openNewSearchPanel() -> void
{
  d->setCurrentIndexWithFocus(0);
  popup(ModeSwitch | WithFocus | EnsureSizeHint);
}

auto SearchResultWindowPrivate::handleExpandCollapseToolButton(const bool checked) const -> void
{
  if (!isSearchVisible())
    return;

  m_search_result_widgets.at(visibleSearchIndex())->setAutoExpandResults(checked);

  if (checked) {
    m_expand_collapse_action->setText(tr("Collapse All"));
    m_search_result_widgets.at(visibleSearchIndex())->expandAll();
  } else {
    m_expand_collapse_action->setText(tr("Expand All"));
    m_search_result_widgets.at(visibleSearchIndex())->collapseAll();
  }
}

auto SearchResultWindowPrivate::updateFilterButton() const -> void
{
  m_filter_button->setEnabled(isSearchVisible() && m_search_result_widgets.at(visibleSearchIndex())->hasFilter());
}

auto SearchResultWindowPrivate::toolBarWidgets() -> QList<QWidget*>
{
  if (!m_history_label)
    m_history_label = new QLabel(tr("History:"));

  if (!m_recent_searches_box) {
    m_recent_searches_box = new QComboBox;
    m_recent_searches_box->setProperty("drawleftborder", true);
    m_recent_searches_box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_recent_searches_box->addItem(tr("New Search"));
    connect(m_recent_searches_box, QOverload<int>::of(&QComboBox::activated), this, &SearchResultWindowPrivate::setCurrentIndexWithFocus);
  }

  return {m_expand_collapse_button, m_filter_button, m_new_search_button, m_spacer, m_history_label, m_spacer2, m_recent_searches_box};
}

/*!
    \internal
*/
auto SearchResultWindow::readSettings() const -> void
{
  QSettings *s = ICore::settings();
  s->beginGroup(QLatin1String(settings_key_section_name));
  d->m_expand_collapse_action->setChecked(s->value(QLatin1String(settings_key_expand_results), SearchResultWindowPrivate::m_initially_expand).toBool());
  s->endGroup();
}

/*!
    \internal
*/
auto SearchResultWindow::writeSettings() const -> void
{
  auto s = ICore::settings();
  s->beginGroup(settings_key_section_name);
  s->setValueWithDefault(settings_key_expand_results, d->m_expand_collapse_action->isChecked(), SearchResultWindowPrivate::m_initially_expand);
  s->endGroup();
}

/*!
    \internal
*/
auto SearchResultWindow::priorityInStatusBar() const -> int
{
  return 80;
}

/*!
    \internal
*/
auto SearchResultWindow::canNext() const -> bool
{
  if (d->isSearchVisible())
    return d->m_search_result_widgets.at(d->visibleSearchIndex())->count() > 0;

  return false;
}

/*!
    \internal
*/
auto SearchResultWindow::canPrevious() const -> bool
{
  return canNext();
}

/*!
    \internal
*/
auto SearchResultWindow::goToNext() -> void
{
  if (const auto index = static_cast<qsizetype>(d->m_widget->currentIndex()); index != 0)
    d->m_search_result_widgets.at(index - 1)->goToNext();
}

/*!
    \internal
*/
auto SearchResultWindow::goToPrev() -> void
{
  if (const auto index = static_cast<qsizetype>(d->m_widget->currentIndex()); index != 0)
    d->m_search_result_widgets.at(index - 1)->goToPrevious();
}

/*!
    \internal
*/
auto SearchResultWindow::canNavigate() const -> bool
{
  return true;
}

/*!
    \internal
*/
SearchResult::SearchResult(SearchResultWidget *widget) : m_widget(widget)
{
  connect(widget, &SearchResultWidget::activated, this, &SearchResult::activated);
  connect(widget, &SearchResultWidget::replaceButtonClicked, this, &SearchResult::replaceButtonClicked);
  connect(widget, &SearchResultWidget::replaceTextChanged, this, &SearchResult::replaceTextChanged);
  connect(widget, &SearchResultWidget::cancelled, this, &SearchResult::cancelled);
  connect(widget, &SearchResultWidget::paused, this, &SearchResult::paused);
  connect(widget, &SearchResultWidget::visibilityChanged, this, &SearchResult::visibilityChanged);
  connect(widget, &SearchResultWidget::searchAgainRequested, this, &SearchResult::searchAgainRequested);
}

/*!
    Attaches some random \a data to this search, that you can use later.

    \sa userData()
*/
auto SearchResult::setUserData(const QVariant &data) -> void
{
  m_userData = data;
}

/*!
    Returns the data that was attached to this search by calling
    setUserData().

    \sa setUserData()
*/
auto SearchResult::userData() const -> QVariant
{
  return m_userData;
}

auto SearchResult::supportsReplace() const -> bool
{
  return m_widget->supportsReplace();
}

/*!
    Returns the text that should replace the text in search results.
*/
auto SearchResult::textToReplace() const -> QString
{
  return m_widget->textToReplace();
}

/*!
    Returns the number of search hits.
*/
auto SearchResult::count() const -> int
{
  return m_widget->count();
}

/*!
    Sets whether the \uicontrol {Seach Again} button is enabled to \a supported.
*/
auto SearchResult::setSearchAgainSupported(const bool supported) const -> void
{
  m_widget->setSearchAgainSupported(supported);
}

/*!
    Returns a UI for a global search and replace action.
*/
auto SearchResult::additionalReplaceWidget() const -> QWidget*
{
  return m_widget->additionalReplaceWidget();
}

/*!
    Sets a \a widget as UI for a global search and replace action.
*/
auto SearchResult::setAdditionalReplaceWidget(QWidget *widget) const -> void
{
  m_widget->setAdditionalReplaceWidget(widget);
}

/*!
    Adds a single result line to the \uicontrol {Search Results} output pane.

    \a {item}.mainRange() specifies the region from the beginning of the search term
    through its length that should be visually marked.
    \a {item}.path(), \a {item}.text() are shown on the result line.
    You can attach arbitrary \a {item}.userData() to the search result, which can
    be used, for example, when reacting to the signals of the search results
    for your search.

    \sa addResults()
*/
auto SearchResult::addResult(const SearchResultItem &item) const -> void
{
  m_widget->addResults({item}, AddOrdered);
}

/*!
    Adds the search result \a items to the \uicontrol {Search Results} output
    pane using \a mode.

    \sa addResult()
*/
auto SearchResult::addResults(const QList<SearchResultItem> &items, AddMode mode) -> void
{
  m_widget->addResults(items, mode);
  emit countChanged(m_widget->count());
}

auto SearchResult::setFilter(SearchResultFilter *filter) const -> void
{
  m_widget->setFilter(filter);
}

/*!
    Notifies the \uicontrol {Search Results} output pane that the current search
    has been \a canceled, and the UI should reflect that.
*/
auto SearchResult::finishSearch(const bool canceled) const -> void
{
  m_widget->finishSearch(canceled);
}

/*!
    Sets the value in the UI element that allows the user to type
    the text that should replace text in search results to \a textToReplace.
*/
auto SearchResult::setTextToReplace(const QString &text_to_replace) const -> void
{
  m_widget->setTextToReplace(text_to_replace);
}

/*!
    Sets whether replace is \a enabled and can be triggered by the user.
*/
auto SearchResult::setReplaceEnabled(const bool enabled) const -> void
{
  m_widget->setReplaceEnabled(enabled);
}

/*!
 * Removes all search results.
 */
auto SearchResult::restart() const -> void
{
  m_widget->restart();
}

/*!
    Sets whether the \uicontrol {Seach Again} button is enabled to \a enabled.
*/
auto SearchResult::setSearchAgainEnabled(const bool enabled) const -> void
{
  m_widget->setSearchAgainEnabled(enabled);
}

/*!
 * Opens the \uicontrol {Search Results} output pane with this search.
 */
auto SearchResult::popup() const -> void
{
  m_widget->sendRequestPopup();
}

} // namespace Orca::Plugin::Core

#include "core-search-result-window.moc"
