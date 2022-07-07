// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchresultwidget.hpp"
#include "searchresulttreeview.hpp"
#include "searchresulttreemodel.hpp"
#include "searchresulttreeitems.hpp"
#include "searchresulttreeitemroles.hpp"
#include "findplugin.hpp"
#include "itemviewfind.hpp"

#include <core/coreplugin.hpp>

#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>
#include <utils/fancylineedit.hpp>

#include <aggregation/aggregate.hpp>

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QCheckBox>
#include <QVBoxLayout>

static constexpr int  searchresult_warning_limit = 200000;
static constexpr char size_warning_label[] = "sizeWarningLabel";

using namespace Utils;

namespace Core {
namespace Internal {

class WideEnoughLineEdit final : public FancyLineEdit {
  Q_OBJECT

public:
  explicit WideEnoughLineEdit(QWidget *parent) : FancyLineEdit(parent)
  {
    setFiltering(true);
    setPlaceholderText(QString());
    connect(this, &QLineEdit::textChanged, this, &QLineEdit::updateGeometry);
  }

  auto sizeHint() const -> QSize override
  {
    auto sh = QLineEdit::minimumSizeHint();
    sh.rwidth() += qMax(25 * fontMetrics().horizontalAdvance(QLatin1Char('x')), fontMetrics().horizontalAdvance(text()));
    return sh;
  }
};

SearchResultWidget::SearchResultWidget(QWidget *parent) : QWidget(parent)
{
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  setLayout(layout);

  auto top_widget = new QFrame;
  QPalette pal;
  pal.setColor(QPalette::Window, orcaTheme()->color(Theme::InfoBarBackground));
  pal.setColor(QPalette::WindowText, orcaTheme()->color(Theme::InfoBarText));
  top_widget->setPalette(pal);

  if (orcaTheme()->flag(Theme::DrawSearchResultWidgetFrame)) {
    top_widget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    top_widget->setLineWidth(1);
  }

  top_widget->setAutoFillBackground(true);
  auto top_layout = new QVBoxLayout(top_widget);
  top_layout->setContentsMargins(2, 2, 2, 2);
  top_layout->setSpacing(2);
  top_widget->setLayout(top_layout);
  layout->addWidget(top_widget);

  auto top_find_widget = new QWidget(top_widget);
  auto top_find_layout = new QHBoxLayout(top_find_widget);
  top_find_layout->setContentsMargins(0, 0, 0, 0);
  top_find_widget->setLayout(top_find_layout);
  top_layout->addWidget(top_find_widget);

  m_top_replace_widget = new QWidget(top_widget);
  auto top_replace_layout = new QHBoxLayout(m_top_replace_widget);
  top_replace_layout->setContentsMargins(0, 0, 0, 0);
  m_top_replace_widget->setLayout(top_replace_layout);
  top_layout->addWidget(m_top_replace_widget);

  m_message_widget = new QFrame;
  pal.setColor(QPalette::WindowText, orcaTheme()->color(Theme::CanceledSearchTextColor));
  m_message_widget->setPalette(pal);

  if (orcaTheme()->flag(Theme::DrawSearchResultWidgetFrame)) {
    m_message_widget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    m_message_widget->setLineWidth(1);
  }

  m_message_widget->setAutoFillBackground(true);
  auto message_layout = new QHBoxLayout(m_message_widget);
  message_layout->setContentsMargins(2, 2, 2, 2);
  m_message_widget->setLayout(message_layout);
  auto message_label = new QLabel(tr("Search was canceled."));
  message_label->setPalette(pal);
  message_layout->addWidget(message_label);
  layout->addWidget(m_message_widget);
  m_message_widget->setVisible(false);

  m_search_result_tree_view = new SearchResultTreeView(this);
  m_search_result_tree_view->setFrameStyle(QFrame::NoFrame);
  m_search_result_tree_view->setAttribute(Qt::WA_MacShowFocusRect, false);
  connect(m_search_result_tree_view, &SearchResultTreeView::filterInvalidated, this, &SearchResultWidget::filterInvalidated);
  connect(m_search_result_tree_view, &SearchResultTreeView::filterChanged, this, &SearchResultWidget::filterChanged);

  auto agg = new Aggregation::Aggregate;
  agg->add(m_search_result_tree_view);
  agg->add(new ItemViewFind(m_search_result_tree_view, ItemDataRoles::ResultLineRole));
  layout->addWidget(m_search_result_tree_view);

  m_info_bar_display.setTarget(layout, 2);
  m_info_bar_display.setInfoBar(&m_info_bar);

  m_description_container = new QWidget(top_find_widget);
  auto description_layout = new QHBoxLayout(m_description_container);
  m_description_container->setLayout(description_layout);
  description_layout->setContentsMargins(0, 0, 0, 0);
  m_description_container->setMinimumWidth(200);
  m_description_container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  m_label = new QLabel(m_description_container);
  m_label->setVisible(false);

  m_search_term = new QLabel(m_description_container);
  m_search_term->setTextFormat(Qt::PlainText);
  m_search_term->setVisible(false);

  description_layout->addWidget(m_label);
  description_layout->addWidget(m_search_term);

  m_cancel_button = new QToolButton(top_find_widget);
  m_cancel_button->setText(tr("Cancel"));
  m_cancel_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(m_cancel_button, &QAbstractButton::clicked, this, &SearchResultWidget::cancel);

  m_search_again_button = new QToolButton(top_find_widget);
  m_search_again_button->setToolTip(tr("Repeat the search with same parameters."));
  m_search_again_button->setText(tr("&Search Again"));
  m_search_again_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_search_again_button->setVisible(false);
  connect(m_search_again_button, &QAbstractButton::clicked, this, &SearchResultWidget::searchAgain);

  m_replace_label = new QLabel(tr("Repla&ce with:"), m_top_replace_widget);
  m_replace_text_edit = new WideEnoughLineEdit(m_top_replace_widget);
  m_replace_label->setBuddy(m_replace_text_edit);
  m_replace_text_edit->setMinimumWidth(120);
  m_replace_text_edit->setEnabled(false);

  setTabOrder(m_replace_text_edit, m_search_result_tree_view);
  m_preserve_case_check = new QCheckBox(m_top_replace_widget);
  m_preserve_case_check->setText(tr("Preser&ve case"));
  m_preserve_case_check->setEnabled(false);

  m_additional_replace_widget = new QWidget(m_top_replace_widget);
  m_additional_replace_widget->setVisible(false);

  m_replace_button = new QToolButton(m_top_replace_widget);
  m_replace_button->setToolTip(tr("Replace all occurrences."));
  m_replace_button->setText(tr("&Replace"));
  m_replace_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_replace_button->setEnabled(false);

  m_preserve_case_check->setChecked(Find::hasFindFlag(FindPreserveCase));
  connect(m_preserve_case_check, &QAbstractButton::clicked, Find::instance(), &Find::setPreserveCase);

  m_matches_found_label = new QLabel(top_find_widget);
  updateMatchesFoundLabel();

  top_find_layout->addWidget(m_description_container);
  top_find_layout->addWidget(m_cancel_button);
  top_find_layout->addWidget(m_search_again_button);
  top_find_layout->addStretch(2);
  top_find_layout->addWidget(m_matches_found_label);

  top_replace_layout->addWidget(m_replace_label);
  top_replace_layout->addWidget(m_replace_text_edit);
  top_replace_layout->addWidget(m_preserve_case_check);
  top_replace_layout->addWidget(m_additional_replace_widget);
  top_replace_layout->addWidget(m_replace_button);
  top_replace_layout->addStretch(2);

  setShowReplaceUi(m_replace_supported);
  setSupportPreserveCase(true);

  connect(m_search_result_tree_view, &SearchResultTreeView::jumpToSearchResult, this, &SearchResultWidget::handleJumpToSearchResult);
  connect(m_replace_text_edit, &QLineEdit::returnPressed, this, &SearchResultWidget::handleReplaceButton);
  connect(m_replace_text_edit, &QLineEdit::textChanged, this, &SearchResultWidget::replaceTextChanged);
  connect(m_replace_button, &QAbstractButton::clicked, this, &SearchResultWidget::handleReplaceButton);
}

SearchResultWidget::~SearchResultWidget()
{
  if (m_info_bar.containsInfo(Id(size_warning_label)))
    cancelAfterSizeWarning();
}

auto SearchResultWidget::setInfo(const QString &label, const QString &tool_tip, const QString &term) const -> void
{
  m_label->setText(label);
  m_label->setVisible(!label.isEmpty());
  m_description_container->setToolTip(tool_tip);
  m_search_term->setText(term);
  m_search_term->setVisible(!term.isEmpty());
}

auto SearchResultWidget::additionalReplaceWidget() const -> QWidget*
{
  return m_additional_replace_widget;
}

auto SearchResultWidget::setAdditionalReplaceWidget(QWidget *widget) -> void
{
  if (const auto item = m_top_replace_widget->layout()->replaceWidget(m_additional_replace_widget, widget))
    delete item;

  delete m_additional_replace_widget;
  m_additional_replace_widget = widget;
}

auto SearchResultWidget::addResults(const QList<SearchResultItem> &items, const SearchResult::AddMode mode) -> void
{
  const auto first_items = m_count == 0;
  m_count += static_cast<int>(items.size());
  m_search_result_tree_view->addResults(items, mode);
  updateMatchesFoundLabel();

  if (first_items) {
    if (!m_dont_ask_again_group.isEmpty()) {
      if (const auto undo_warning_id = Id("warninglabel/").withSuffix(m_dont_ask_again_group); m_info_bar.canInfoBeAdded(undo_warning_id)) {
        const InfoBarEntry info(undo_warning_id, tr("This change cannot be undone."), InfoBarEntry::GlobalSuppression::Enabled);
        m_info_bar.addInfo(info);
      }
    }

    m_replace_text_edit->setEnabled(true);
    // We didn't have an item before, set the focus to the search widget or replace text edit
    setShowReplaceUi(m_replace_supported);
    if (m_replace_supported) {
      m_replace_text_edit->setFocus();
      m_replace_text_edit->selectAll();
    } else {
      m_search_result_tree_view->setFocus();
    }
    m_search_result_tree_view->selectionModel()->select(m_search_result_tree_view->model()->index(0, 0, QModelIndex()), QItemSelectionModel::Select);
    emit navigateStateChanged();
  } else if (m_count > searchresult_warning_limit) {

    const Id size_warning_id(size_warning_label);

    if (!m_info_bar.canInfoBeAdded(size_warning_id))
      return;

    emit paused(true);

    InfoBarEntry info(size_warning_id, tr("The search resulted in more than %n items, do you still want to continue?", nullptr, searchresult_warning_limit));
    info.setCancelButtonInfo(tr("Cancel"), [this] { cancelAfterSizeWarning(); });
    info.addCustomButton(tr("Continue"), [this] { continueAfterSizeWarning(); });
    m_info_bar.addInfo(info);

    emit requestPopup(false/*no focus*/);
  }
}

auto SearchResultWidget::count() const -> int
{
  return m_count;
}

auto SearchResultWidget::setSupportsReplace(const bool replace_supported, const QString &group) -> void
{
  m_replace_supported = replace_supported;
  setShowReplaceUi(replace_supported);
  m_dont_ask_again_group = group;
}

auto SearchResultWidget::supportsReplace() const -> bool
{
  return m_replace_supported;
}

auto SearchResultWidget::setTextToReplace(const QString &text_to_replace) const -> void
{
  m_replace_text_edit->setText(text_to_replace);
}

auto SearchResultWidget::textToReplace() const -> QString
{
  return m_replace_text_edit->text();
}

auto SearchResultWidget::setSupportPreserveCase(const bool enabled) -> void
{
  m_preserve_case_supported = enabled;
  m_preserve_case_check->setVisible(m_preserve_case_supported);
}

auto SearchResultWidget::setShowReplaceUi(const bool visible) -> void
{
  m_search_result_tree_view->model()->setShowReplaceUi(visible);
  m_top_replace_widget->setVisible(visible);
  m_is_showing_replace_ui = visible;
}

auto SearchResultWidget::hasFocusInternally() const -> bool
{
  return m_search_result_tree_view->hasFocus() || m_is_showing_replace_ui && m_replace_text_edit->hasFocus();
}

auto SearchResultWidget::setFocusInternally() const -> void
{
  if (m_count > 0) {
    if (m_is_showing_replace_ui) {
      if (!focusWidget() || focusWidget() == m_replace_text_edit) {
        m_replace_text_edit->setFocus();
        m_replace_text_edit->selectAll();
      } else {
        m_search_result_tree_view->setFocus();
      }
    } else {
      m_search_result_tree_view->setFocus();
    }
  }
}

auto SearchResultWidget::canFocusInternally() const -> bool
{
  return m_count > 0;
}

auto SearchResultWidget::notifyVisibilityChanged(const bool visible) -> void
{
  emit visibilityChanged(visible);
}

auto SearchResultWidget::setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void
{
  m_search_result_tree_view->setTextEditorFont(font, colors);
}

auto SearchResultWidget::setTabWidth(const int tab_width) const -> void
{
  m_search_result_tree_view->setTabWidth(tab_width);
}

auto SearchResultWidget::setAutoExpandResults(const bool expand) const -> void
{
  m_search_result_tree_view->setAutoExpandResults(expand);
}

auto SearchResultWidget::expandAll() const -> void
{
  m_search_result_tree_view->expandAll();
}

auto SearchResultWidget::collapseAll() const -> void
{
  m_search_result_tree_view->collapseAll();
}

auto SearchResultWidget::goToNext() const -> void
{
  if (m_count == 0)
    return;

  if (const auto idx = m_search_result_tree_view->model()->next(m_search_result_tree_view->currentIndex()); idx.isValid()) {
    m_search_result_tree_view->setCurrentIndex(idx);
    m_search_result_tree_view->emitJumpToSearchResult(idx);
  }
}

auto SearchResultWidget::goToPrevious() const -> void
{
  if (!m_search_result_tree_view->model()->rowCount())
    return;

  if (const auto idx = m_search_result_tree_view->model()->prev(m_search_result_tree_view->currentIndex()); idx.isValid()) {
    m_search_result_tree_view->setCurrentIndex(idx);
    m_search_result_tree_view->emitJumpToSearchResult(idx);
  }
}

auto SearchResultWidget::restart() -> void
{
  m_replace_text_edit->setEnabled(false);
  m_replace_button->setEnabled(false);
  m_search_result_tree_view->clear();
  m_searching = true;
  m_count = 0;
  const Id size_warning_id(size_warning_label);
  m_info_bar.removeInfo(size_warning_id);
  m_info_bar.unsuppressInfo(size_warning_id);
  m_cancel_button->setVisible(true);
  m_search_again_button->setVisible(false);
  m_message_widget->setVisible(false);
  updateMatchesFoundLabel();
  emit restarted();
}

auto SearchResultWidget::setSearchAgainSupported(const bool supported) -> void
{
  m_search_again_supported = supported;
  m_search_again_button->setVisible(supported && !m_cancel_button->isVisible());
}

auto SearchResultWidget::setSearchAgainEnabled(const bool enabled) const -> void
{
  m_search_again_button->setEnabled(enabled);
}

auto SearchResultWidget::setFilter(SearchResultFilter *filter) const -> void
{
  m_search_result_tree_view->setFilter(filter);
}

auto SearchResultWidget::hasFilter() const -> bool
{
  return m_search_result_tree_view->hasFilter();
}

auto SearchResultWidget::showFilterWidget(QWidget *parent) const -> void
{
  m_search_result_tree_view->showFilterWidget(parent);
}

auto SearchResultWidget::setReplaceEnabled(const bool enabled) const -> void
{
  m_replace_button->setEnabled(enabled);
}

auto SearchResultWidget::finishSearch(const bool canceled) -> void
{
  const Id size_warning_id(size_warning_label);
  m_info_bar.removeInfo(size_warning_id);
  m_info_bar.unsuppressInfo(size_warning_id);
  m_replace_text_edit->setEnabled(m_count > 0);
  m_replace_button->setEnabled(m_count > 0);
  m_preserve_case_check->setEnabled(m_count > 0);
  m_cancel_button->setVisible(false);
  m_message_widget->setVisible(canceled);
  m_search_again_button->setVisible(m_search_again_supported);
  m_searching = false;
  updateMatchesFoundLabel();
}

auto SearchResultWidget::sendRequestPopup() -> void
{
  emit requestPopup(true/*focus*/);
}

auto SearchResultWidget::continueAfterSizeWarning() -> void
{
  m_info_bar.suppressInfo(Id(size_warning_label));
  emit paused(false);
}

auto SearchResultWidget::cancelAfterSizeWarning() -> void
{
  m_info_bar.suppressInfo(Id(size_warning_label));
  emit cancelled();
  emit paused(false);
}

auto SearchResultWidget::handleJumpToSearchResult(const SearchResultItem &item) -> void
{
  emit activated(item);
}

auto SearchResultWidget::handleReplaceButton() -> void
{
  // check if button is actually enabled, because this is also triggered
  // by pressing return in replace line edit
  if (m_replace_button->isEnabled()) {
    m_info_bar.clear();
    setShowReplaceUi(false);
    emit replaceButtonClicked(m_replace_text_edit->text(), checkedItems(), m_preserve_case_supported && m_preserve_case_check->isChecked());
  }
}

auto SearchResultWidget::cancel() -> void
{
  m_cancel_button->setVisible(false);

  if (m_info_bar.containsInfo(Id(size_warning_label)))
    cancelAfterSizeWarning();

  else emit cancelled();
}

auto SearchResultWidget::searchAgain() -> void
{
  emit searchAgainRequested();
}

auto SearchResultWidget::checkedItems() const -> QList<SearchResultItem>
{
  QList<SearchResultItem> result;
  const auto model = m_search_result_tree_view->model();
  const auto file_count = model->rowCount();

  for (auto i = 0; i < file_count; ++i) {
    const auto file_index = model->index(i, 0);
    const auto item_count = model->rowCount(file_index);
    for (auto row_index = 0; row_index < item_count; ++row_index) {
      const auto text_index = model->index(row_index, 0, file_index);
      const SearchResultTreeItem *const row_item = model->itemForIndex(text_index);
      QTC_ASSERT(row_item != nullptr, continue);
      if (row_item->checkState())
        result << row_item->item;
    }
  }
  return result;
}

auto SearchResultWidget::updateMatchesFoundLabel() const -> void
{
  if (m_count > 0) {
    m_matches_found_label->setText(tr("%n matches found.", nullptr, m_count));
  } else if (m_searching) {
    m_matches_found_label->setText(tr("Searching..."));
  } else {
    m_matches_found_label->setText(tr("No matches found."));
  }
}

} // namespace Internal
} // namespace Core

#include "searchresultwidget.moc"
