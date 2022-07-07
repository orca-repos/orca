// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "locatorwidget.hpp"
#include "ilocatorfilter.hpp"
#include "locator.hpp"
#include "locatorconstants.hpp"
#include "locatorsearchutils.hpp"

#include <core/icore.hpp>
#include <core/modemanager.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/fileiconprovider.hpp>
#include <core/icontext.hpp>
#include <core/mainwindow.hpp>

#include <utils/algorithm.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/highlightingitemdelegate.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/itemviews.hpp>
#include <utils/progressindicator.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QToolTip>
#include <QTreeView>

Q_DECLARE_METATYPE(Core::LocatorFilterEntry)

using namespace Utils;

constexpr int locator_entry_role = static_cast<int>(HighlightingItemRole::User);

namespace Core {
namespace Internal {

bool LocatorWidget::m_shutting_down = false;
QFuture<void> LocatorWidget::m_shared_future;
LocatorWidget *LocatorWidget::m_shared_future_origin = nullptr;

/* A model to represent the Locator results. */
class LocatorModel final : public QAbstractListModel {
public:
  enum Columns {
    DisplayNameColumn,
    ExtraInfoColumn,
    ColumnCount
  };

  LocatorModel(QObject *parent = nullptr) : QAbstractListModel(parent), m_background_color(orcaTheme()->color(Theme::TextColorHighlightBackground)), m_foreground_color(orcaTheme()->color(Theme::TextColorNormal)) {}

  auto clear() -> void;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto addEntries(const QList<LocatorFilterEntry> &entries) -> void;

private:
  mutable QList<LocatorFilterEntry> m_entries;
  bool has_extra_info = false;
  QColor m_background_color;
  QColor m_foreground_color;
};

class CompletionDelegate final : public HighlightingItemDelegate {
public:
  explicit CompletionDelegate(QObject *parent);

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override;
};

class CompletionList final : public TreeView {
public:
  explicit CompletionList(QWidget *parent = nullptr);

  auto setModel(QAbstractItemModel *new_model) -> void override;
  auto resizeHeaders() const -> void;
  auto next() -> void;
  auto previous() -> void;
  auto showCurrentItemToolTip() const -> void;
  auto keyPressEvent(QKeyEvent *event) -> void override;
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

private:
  QMetaObject::Connection m_update_size_connection;
};

class TopLeftLocatorPopup final : public LocatorPopup {
public:
  explicit TopLeftLocatorPopup(LocatorWidget *locator_widget) : LocatorPopup(locator_widget, locator_widget)
  {
    doUpdateGeometry();
  }

protected:
  auto doUpdateGeometry() -> void override;
  auto inputLostFocus() -> void override;
};

class CenteredLocatorPopup final : public LocatorPopup {
public:
  CenteredLocatorPopup(LocatorWidget *locator_widget, QWidget *parent) : LocatorPopup(locator_widget, parent)
  {
    doUpdateGeometry();
  }

protected:
  auto doUpdateGeometry() -> void override;
};

auto LocatorModel::clear() -> void
{
  beginResetModel();
  m_entries.clear();
  has_extra_info = false;
  endResetModel();
}

auto LocatorModel::rowCount(const QModelIndex &parent) const -> int
{
  if (parent.isValid())
    return 0;

  return static_cast<int>(m_entries.size());
}

auto LocatorModel::columnCount(const QModelIndex &parent) const -> int
{
  if (parent.isValid())
    return 0;

  return has_extra_info ? ColumnCount : 1;
}

auto LocatorModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (!index.isValid() || index.row() >= m_entries.size())
    return {};

  switch (role) {
  case Qt::DisplayRole:
    if (index.column() == DisplayNameColumn)
      return m_entries.at(index.row()).display_name;
    else if (index.column() == ExtraInfoColumn)
      return m_entries.at(index.row()).extra_info;
    break;
  case Qt::ToolTipRole: {
    const auto &entry = m_entries.at(index.row());
    auto tool_tip = entry.display_name;
    if (!entry.extra_info.isEmpty())
      tool_tip += "\n\n" + entry.extra_info;
    if (!entry.tool_tip.isEmpty())
      tool_tip += "\n\n" + entry.tool_tip;
    return {tool_tip};
  }
  case Qt::DecorationRole:
    if (index.column() == DisplayNameColumn) {
      auto &entry = m_entries[index.row()];
      if (!entry.display_icon && !entry.file_path.isEmpty())
        entry.display_icon = FileIconProvider::icon(entry.file_path);
      return entry.display_icon ? entry.display_icon.value() : QIcon();
    }
    break;
  case Qt::ForegroundRole:
    if (index.column() == ExtraInfoColumn)
      return QColor(Qt::darkGray);
    break;
  case locator_entry_role:
    return QVariant::fromValue(m_entries.at(index.row()));
  case static_cast<int>(HighlightingItemRole::StartColumn):
  case static_cast<int>(HighlightingItemRole::Length): {
    const auto &entry = m_entries[index.row()];
    if (const int highlight_column = entry.highlight_info.dataType == LocatorFilterEntry::HighlightInfo::DisplayName ? DisplayNameColumn : ExtraInfoColumn; highlight_column == index.column()) {
      const auto start_index_role = role == static_cast<int>(HighlightingItemRole::StartColumn);
      return start_index_role ? QVariant::fromValue(entry.highlight_info.starts) : QVariant::fromValue(entry.highlight_info.lengths);
    }
    break;
  }
  case static_cast<int>(HighlightingItemRole::Background):
    return m_background_color;
  case static_cast<int>(HighlightingItemRole::Foreground):
    return m_foreground_color;
  default:
    break;
  }

  return {};
}

auto LocatorModel::addEntries(const QList<LocatorFilterEntry> &entries) -> void
{
  beginInsertRows(QModelIndex(), static_cast<int>(m_entries.size()), static_cast<int>(m_entries.size() + entries.size() - 1));
  m_entries.append(entries);
  endInsertRows();

  if (has_extra_info)
    return;

  if (anyOf(entries, [](const LocatorFilterEntry &e) { return !e.extra_info.isEmpty(); })) {
    beginInsertColumns(QModelIndex(), 1, 1);
    has_extra_info = true;
    endInsertColumns();
  }
}

// =========== CompletionList ===========

CompletionList::CompletionList(QWidget *parent) : TreeView(parent)
{
  // on macOS and Windows the popup doesn't really get focus, so fake the selection color
  // which would then just be a very light gray, but should look as if it had focus
  auto p = palette();
  p.setBrush(QPalette::Inactive, QPalette::Highlight, p.brush(QPalette::Normal, QPalette::Highlight));
  setPalette(p);

  setItemDelegate(new CompletionDelegate(this));
  setRootIsDecorated(false);
  setUniformRowHeights(true);
  header()->hide();
  header()->setStretchLastSection(true);

  // This is too slow when done on all results
  if constexpr (HostOsInfo::isMacHost()) {
    if (horizontalScrollBar())
      horizontalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
    if (verticalScrollBar())
      verticalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
  }

  installEventFilter(this);
}

auto CompletionList::setModel(QAbstractItemModel *new_model) -> void
{
  const auto update_size = [this] {
    if (model() && model()->rowCount() > 0) {
      const auto shint = sizeHintForIndex(model()->index(0, 0));
      setFixedHeight(shint.height() * 17 + frameWidth() * 2);
      disconnect(m_update_size_connection);
    }
  };

  if (model()) {
    disconnect(model(), nullptr, this, nullptr);
  }

  QTreeView::setModel(new_model);

  if (new_model) {
    connect(new_model, &QAbstractItemModel::columnsInserted, this, &CompletionList::resizeHeaders);
    m_update_size_connection = connect(new_model, &QAbstractItemModel::rowsInserted, this, update_size);
  }
}

auto LocatorPopup::doUpdateGeometry() -> void
{
  m_tree->resizeHeaders();
}

auto TopLeftLocatorPopup::doUpdateGeometry() -> void
{
  QTC_ASSERT(parentWidget(), return);
  const auto size = preferredSize();
  const auto border = m_tree->frameWidth();
  const QRect rect(parentWidget()->mapToGlobal(QPoint(-border, -size.height() - border)), size);
  setGeometry(rect);
  LocatorPopup::doUpdateGeometry();
}

auto CenteredLocatorPopup::doUpdateGeometry() -> void
{
  QTC_ASSERT(parentWidget(), return);
  const auto size = preferredSize();
  const auto parent_size = parentWidget()->size();
  const QPoint local((parent_size.width() - size.width()) / 2, parent_size.height() / 2 - size.height());
  const auto pos = parentWidget()->mapToGlobal(local);
  QRect rect(pos, size);

  // invisible widget doesn't have the right screen set yet, so use the parent widget to
  // check for available geometry
  const auto available = parentWidget()->screen()->availableGeometry();

  if (rect.right() > available.right())
    rect.moveRight(available.right());

  if (rect.bottom() > available.bottom())
    rect.moveBottom(available.bottom());

  if (rect.top() < available.top())
    rect.moveTop(available.top());

  if (rect.left() < available.left())
    rect.moveLeft(available.left());

  setGeometry(rect);
  LocatorPopup::doUpdateGeometry();
}

auto LocatorPopup::updateWindow() -> void
{
  if (const auto w = parentWidget() ? parentWidget()->window() : nullptr; m_window != w) {
    if (m_window)
      m_window->removeEventFilter(this);
    m_window = w;
    if (m_window)
      m_window->installEventFilter(this);
  }
}

auto LocatorPopup::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ParentChange) {
    updateWindow();
  } else if (event->type() == QEvent::Show) {
    // make sure the popup has correct position before it becomes visible
    doUpdateGeometry();
  } else if (event->type() == QEvent::LayoutRequest) {
    // completion list resizes after first items are shown --> LayoutRequest
    QMetaObject::invokeMethod(this, &LocatorPopup::doUpdateGeometry, Qt::QueuedConnection);
  } else if (event->type() == QEvent::ShortcutOverride) {
    // if we (the popup) has focus, we need to handle escape manually (Windows)
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->modifiers() == Qt::NoModifier && ke->key() == Qt::Key_Escape)
      event->accept();
  } else if (event->type() == QEvent::KeyPress) {
    // if we (the popup) has focus, we need to handle escape manually (Windows)
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->modifiers() == Qt::NoModifier && ke->key() == Qt::Key_Escape)
      hide();
  }
  return QWidget::event(event);
}

auto LocatorPopup::eventFilter(QObject *watched, QEvent *event) -> bool
{
  if (watched == m_tree && event->type() == QEvent::FocusOut) {
    // if the tree had focus and another application is brought to foreground,
    // we need to hide the popup because it otherwise stays on top of
    // everything else (even other applications) (Windows)
    if (const auto fe = dynamic_cast<QFocusEvent*>(event); fe->reason() == Qt::ActiveWindowFocusReason && !QApplication::activeWindow())
      hide();
  } else if (watched == m_window && event->type() == QEvent::Resize) {
    doUpdateGeometry();
  }
  return QWidget::eventFilter(watched, event);
}

auto LocatorPopup::preferredSize() const -> QSize
{
  static constexpr auto min_width = 730;
  const auto window_size = m_window ? m_window->size() : QSize(min_width, 0);
  const auto width = qMax(min_width, window_size.width() * 2 / 3);
  return {width, sizeHint().height()};
}

auto TopLeftLocatorPopup::inputLostFocus() -> void
{
  if (!isActiveWindow())
    hide();
}

auto LocatorPopup::inputLostFocus() -> void {}

auto CompletionList::resizeHeaders() const -> void
{
  header()->resizeSection(0, width() / 2);
  header()->resizeSection(1, 0); // last section is auto resized because of stretchLastSection
}

LocatorPopup::LocatorPopup(LocatorWidget *locator_widget, QWidget *parent) : QWidget(parent), m_tree(new CompletionList(this)), m_input_widget(locator_widget)
{
  if constexpr (HostOsInfo::isMacHost())
    m_tree->setFrameStyle(QFrame::NoFrame); // tool tip already includes a frame

  m_tree->setModel(locator_widget->model());
  m_tree->setTextElideMode(Qt::ElideMiddle);
  m_tree->installEventFilter(this);

  const auto layout = new QVBoxLayout;
  layout->setSizeConstraint(QLayout::SetMinimumSize);
  setLayout(layout);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_tree);

  connect(locator_widget, &LocatorWidget::parentChanged, this, &LocatorPopup::updateWindow);
  connect(locator_widget, &LocatorWidget::showPopup, this, &LocatorPopup::show);
  connect(locator_widget, &LocatorWidget::hidePopup, this, &LocatorPopup::close);
  connect(locator_widget, &LocatorWidget::lostFocus, this, &LocatorPopup::inputLostFocus, Qt::QueuedConnection);
  connect(locator_widget, &LocatorWidget::selectRow, m_tree, [this](const int row) {
    m_tree->setCurrentIndex(m_tree->model()->index(row, 0));
  });
  connect(locator_widget, &LocatorWidget::showCurrentItemToolTip, m_tree, &CompletionList::showCurrentItemToolTip);
  connect(locator_widget, &LocatorWidget::handleKey, this, [this](QKeyEvent *key_event) {
    QApplication::sendEvent(m_tree, key_event);
  }, Qt::DirectConnection); // must be handled directly before event is deleted
  connect(m_tree, &QAbstractItemView::activated, locator_widget, [this, locator_widget](const QModelIndex &index) {
    if (isVisible())
      locator_widget->scheduleAcceptEntry(index);
  });
}

auto LocatorPopup::completionList() const -> CompletionList*
{
  return m_tree;
}

auto LocatorPopup::inputWidget() const -> LocatorWidget*
{
  return m_input_widget;
}

auto LocatorPopup::focusOutEvent(QFocusEvent *event) -> void
{
  if (event->reason() == Qt::ActiveWindowFocusReason)
    hide();
  QWidget::focusOutEvent(event);
}

auto CompletionList::next() -> void
{
  auto index = currentIndex().row();
  ++index;

  if (index >= model()->rowCount(QModelIndex())) {
    // wrap
    index = 0;
  }

  setCurrentIndex(model()->index(index, 0));
}

auto CompletionList::previous() -> void
{
  auto index = currentIndex().row();
  --index;

  if (index < 0) {
    // wrap
    index = model()->rowCount(QModelIndex()) - 1;
  }

  setCurrentIndex(model()->index(index, 0));
}

auto CompletionList::showCurrentItemToolTip() const -> void
{
  QTC_ASSERT(model(), return);

  if (!isVisible())
    return;

  if (const auto index = currentIndex(); index.isValid()) {
    QToolTip::showText(mapToGlobal(pos() + visualRect(index).topRight()), model()->data(index, Qt::ToolTipRole).toString());
  }
}

auto CompletionList::keyPressEvent(QKeyEvent *event) -> void
{
  switch (event->key()) {
  case Qt::Key_Tab:
  case Qt::Key_Down:
    next();
    return;
  case Qt::Key_Backtab:
  case Qt::Key_Up:
    previous();
    return;
  case Qt::Key_P:
  case Qt::Key_N:
    if (event->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
      if (event->key() == Qt::Key_P)
        previous();
      else
        next();
      return;
    }
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    // emit activated even if current index is not valid
    // if there are no results yet, this allows activating the first entry when it is available
    // (see LocatorWidget::addSearchResults)
    if (event->modifiers() == 0) {
      emit activated(currentIndex());
      return;
    }
    break;
  default:
    break;
  }
  TreeView::keyPressEvent(event);
}

auto CompletionList::eventFilter(QObject *watched, QEvent *event) -> bool
{
  if (watched == this && event->type() == QEvent::ShortcutOverride) {
    switch (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key()) {
    case Qt::Key_Escape:
      if (!ke->modifiers()) {
        event->accept();
        return true;
      }
      break;
    case Qt::Key_P:
    case Qt::Key_N:
      if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        event->accept();
        return true;
      }
      break;
    default:
      break;
    }
  }
  return TreeView::eventFilter(watched, event);
}

LocatorWidget::LocatorWidget(Locator *locator) : m_locator_model(new LocatorModel(this)), m_filter_menu(new QMenu(this)), m_refresh_action(new QAction(tr("Refresh"), this)), m_configure_action(new QAction(ICore::msgShowOptionsDialog(), this)), m_file_line_edit(new FancyLineEdit)
{
  setAttribute(Qt::WA_Hover);
  setFocusProxy(m_file_line_edit);
  resize(200, 90);

  QSizePolicy size_policy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  size_policy.setHorizontalStretch(0);
  size_policy.setVerticalStretch(0);

  setSizePolicy(size_policy);
  setMinimumSize(QSize(200, 0));

  const auto layout = new QHBoxLayout(this);
  setLayout(layout);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_file_line_edit);

  const auto icon = Icons::MAGNIFIER.icon();
  m_file_line_edit->setFiltering(true);
  m_file_line_edit->setButtonIcon(FancyLineEdit::Left, icon);
  m_file_line_edit->setButtonToolTip(FancyLineEdit::Left, tr("Options"));
  m_file_line_edit->setFocusPolicy(Qt::ClickFocus);
  m_file_line_edit->setButtonVisible(FancyLineEdit::Left, true);
  // We set click focus since otherwise you will always get two popups
  m_file_line_edit->setButtonFocusPolicy(FancyLineEdit::Left, Qt::ClickFocus);
  m_file_line_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_file_line_edit->installEventFilter(this);

  this->installEventFilter(this);

  m_filter_menu->addAction(m_refresh_action);
  m_filter_menu->addAction(m_configure_action);
  m_file_line_edit->setButtonMenu(FancyLineEdit::Left, m_filter_menu);

  connect(m_refresh_action, &QAction::triggered, locator, [locator] {
    locator->refresh(Locator::filters());
  });
  connect(m_configure_action, &QAction::triggered, this, &LocatorWidget::showConfigureDialog);
  connect(m_file_line_edit, &QLineEdit::textChanged, this, &LocatorWidget::showPopupDelayed);

  m_entries_watcher = new QFutureWatcher<LocatorFilterEntry>(this);
  connect(m_entries_watcher, &QFutureWatcher<LocatorFilterEntry>::resultsReadyAt, this, &LocatorWidget::addSearchResults);
  connect(m_entries_watcher, &QFutureWatcher<LocatorFilterEntry>::finished, this, &LocatorWidget::handleSearchFinished);

  m_show_popup_timer.setInterval(100);
  m_show_popup_timer.setSingleShot(true);
  connect(&m_show_popup_timer, &QTimer::timeout, this, &LocatorWidget::showPopupNow);

  m_progress_indicator = new ProgressIndicator(ProgressIndicatorSize::Small, m_file_line_edit);
  m_progress_indicator->raise();
  m_progress_indicator->hide();
  m_show_progress_timer.setSingleShot(true);
  m_show_progress_timer.setInterval(50); // don't show progress for < 50ms tasks
  connect(&m_show_progress_timer, &QTimer::timeout, [this] { setProgressIndicatorVisible(true); });

  if (auto locate_cmd = ActionManager::command(Constants::locate); QTC_GUARD(locate_cmd)) {
    connect(locate_cmd, &Command::keySequenceChanged, this, [this,locate_cmd] {
      updatePlaceholderText(locate_cmd);
    });
    updatePlaceholderText(locate_cmd);
  }

  connect(qApp, &QApplication::focusChanged, this, &LocatorWidget::updatePreviousFocusWidget);
  connect(locator, &Locator::filtersChanged, this, &LocatorWidget::updateFilterList);
  updateFilterList();
}

LocatorWidget::~LocatorWidget()
{
  // no need to completely finish a running search, cancel it
  if (m_entries_watcher->future().isRunning())
    m_entries_watcher->future().cancel();
}

auto LocatorWidget::updatePlaceholderText(const Command *command) const -> void
{
  QTC_ASSERT(command, return);
  if (command->keySequence().isEmpty())
    m_file_line_edit->setPlaceholderText(tr("Type to locate"));
  else
    m_file_line_edit->setPlaceholderText(tr("Type to locate (%1)").arg(command->keySequence().toString(QKeySequence::NativeText)));
}

auto LocatorWidget::updateFilterList() const -> void
{
  m_filter_menu->clear();

  for (const auto filters = Locator::filters(); const auto filter : filters) {
    if (const auto cmd = ActionManager::command(filter->actionId()))
      m_filter_menu->addAction(cmd->action());
  }

  m_filter_menu->addSeparator();
  m_filter_menu->addAction(m_refresh_action);
  m_filter_menu->addAction(m_configure_action);
}

auto LocatorWidget::isInMainWindow() const -> bool
{
  return window() == ICore::mainWindow();
}

auto LocatorWidget::updatePreviousFocusWidget(QWidget *previous, const QWidget *current) -> void
{
  if (const auto is_in_locator = [this](const QWidget *w) { return w == this || isAncestorOf(w); }; is_in_locator(current) && !is_in_locator(previous))
    m_previous_focus_widget = previous;
}

static auto resetFocus(const QPointer<QWidget> &previous_focus, const bool is_in_main_window) -> void
{
  if (previous_focus) {
    previous_focus->setFocus();
    ICore::raiseWindow(previous_focus);
  } else if (is_in_main_window) {
    ModeManager::setFocusToCurrentMode();
  }
}

auto LocatorWidget::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == m_file_line_edit && event->type() == QEvent::ShortcutOverride) {
    switch (const auto key_event = dynamic_cast<QKeyEvent*>(event); key_event->key()) {
    case Qt::Key_P:
    case Qt::Key_N:
      if (key_event->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        event->accept();
        return true;
      }
    default:
      break;
    }
  } else if (obj == m_file_line_edit && event->type() == QEvent::KeyPress) {
    if (m_possible_tool_tip_request)
      m_possible_tool_tip_request = false;
    if (QToolTip::isVisible())
      QToolTip::hideText();

    switch (const auto key_event = dynamic_cast<QKeyEvent*>(event); key_event->key()) {
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Down:
    case Qt::Key_Tab:
    case Qt::Key_Up:
    case Qt::Key_Backtab: emit showPopup();
      emit handleKey(key_event);
      return true;
    case Qt::Key_Home:
    case Qt::Key_End:
      if (HostOsInfo::isMacHost() != (key_event->modifiers() == Qt::KeyboardModifiers(Qt::ControlModifier))) {
        emit showPopup();
        emit handleKey(key_event);
        return true;
      }
      break;
    case Qt::Key_Enter:
    case Qt::Key_Return: emit handleKey(key_event);
      return true;
    case Qt::Key_Escape: emit hidePopup();
      return true;
    case Qt::Key_Alt:
      if (key_event->modifiers() == Qt::AltModifier) {
        m_possible_tool_tip_request = true;
        return true;
      }
      break;
    case Qt::Key_P:
    case Qt::Key_N:
      if (key_event->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        emit showPopup();
        emit handleKey(key_event);
        return true;
      }
      break;
    default:
      break;
    }
  } else if (obj == m_file_line_edit && event->type() == QEvent::KeyRelease) {
    const auto key_event = dynamic_cast<QKeyEvent*>(event);
    if (m_possible_tool_tip_request) {
      m_possible_tool_tip_request = false;
      if ((key_event->key() == Qt::Key_Alt) && (key_event->modifiers() == Qt::NoModifier)) {
        emit showCurrentItemToolTip();
        return true;
      }
    }
  } else if (obj == m_file_line_edit && event->type() == QEvent::FocusOut) {
    emit lostFocus();
  } else if (obj == m_file_line_edit && event->type() == QEvent::FocusIn) {
    if (const auto fev = dynamic_cast<QFocusEvent*>(event); fev->reason() != Qt::ActiveWindowFocusReason)
      showPopupNow();
  } else if (obj == this && event->type() == QEvent::ParentChange) {
    emit parentChanged();
  } else if (obj == this && event->type() == QEvent::ShortcutOverride) {
    switch (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key()) {
    case Qt::Key_Escape:
      if (!ke->modifiers()) {
        event->accept();
        QMetaObject::invokeMethod(this, [focus = m_previous_focus_widget, isInMainWindow = isInMainWindow()] {
          resetFocus(focus, isInMainWindow);
        }, Qt::QueuedConnection);
        return true;
      }
      break;
    case Qt::Key_Alt:
      if (ke->modifiers() == Qt::AltModifier) {
        event->accept();
        return true;
      }
      break;
    default:
      break;
    }
  }
  return QWidget::eventFilter(obj, event);
}

auto LocatorWidget::showPopupDelayed() -> void
{
  m_update_requested = true;
  m_show_popup_timer.start();
}

auto LocatorWidget::showPopupNow() -> void
{
  m_show_popup_timer.stop();
  updateCompletionList(m_file_line_edit->text());
  emit showPopup();
}

auto LocatorWidget::filtersFor(const QString &text, QString &search_text) -> QList<ILocatorFilter*>
{
  const auto length = static_cast<int>(text.size());
  int first_non_space;

  for (first_non_space = 0; first_non_space < length; ++first_non_space) {
    if (!text.at(first_non_space).isSpace())
      break;
  }

  const auto whitespace = static_cast<int>(text.indexOf(QChar::Space, first_non_space));
  const auto filters = filtered(Locator::filters(), &ILocatorFilter::isEnabled);

  if (whitespace >= 0) {
    const auto prefix = text.mid(first_non_space, whitespace - first_non_space).toLower();
    QList<ILocatorFilter*> prefix_filters;
    for (const auto filter : filters) {
      if (prefix == filter->shortcutString()) {
        search_text = text.mid(whitespace).trimmed();
        prefix_filters << filter;
      }
    }
    if (!prefix_filters.isEmpty())
      return prefix_filters;
  }

  search_text = text.trimmed();
  return filtered(filters, &ILocatorFilter::isIncludedByDefault);
}

auto LocatorWidget::setProgressIndicatorVisible(const bool visible) const -> void
{
  if (!visible) {
    m_progress_indicator->hide();
    return;
  }

  const auto icon_size = m_progress_indicator->sizeHint();

  m_progress_indicator->setGeometry(m_file_line_edit->button(FancyLineEdit::Right)->geometry().x() - icon_size.width(), (m_file_line_edit->height() - icon_size.height()) / 2 /*center*/, icon_size.width(), icon_size.height());
  m_progress_indicator->show();
}

auto LocatorWidget::updateCompletionList(const QString &text) -> void
{
  if (m_shutting_down)
    return;

  m_update_requested = true;

  if (m_shared_future.isRunning()) {
    // Cancel the old future. We may not just block the UI thread to wait for the search to
    // actually cancel.
    m_requested_completion_text = text;
    if (m_shared_future_origin == this) {
      // This locator widget is currently running. Make handleSearchFinished trigger another
      // update.
      m_rerun_after_finished = true;
    } else {
      // Another locator widget is running. Trigger another update when that is finished.
      onFinished(m_shared_future, this, [this](const QFuture<void> &) {
        const auto text = m_requested_completion_text;
        m_requested_completion_text.clear();
        updateCompletionList(text);
      });
    }
    m_shared_future.cancel();
    return;
  }

  m_show_progress_timer.start();
  m_needs_clear_result = true;

  QString search_text;
  const auto filters = filtersFor(text, search_text);

  for (const auto filter : filters)
    filter->prepareSearch(search_text);

  const auto future = runAsync(&runSearch, filters, search_text);

  m_shared_future = QFuture<void>(future);
  m_shared_future_origin = this;
  m_entries_watcher->setFuture(future);
}

auto LocatorWidget::handleSearchFinished() -> void
{
  m_show_progress_timer.stop();
  setProgressIndicatorVisible(false);
  m_update_requested = false;

  if (m_row_requested_for_accept) {
    acceptEntry(m_row_requested_for_accept.value());
    m_row_requested_for_accept.reset();
    return;
  }

  if (m_rerun_after_finished) {
    m_rerun_after_finished = false;
    const auto text = m_requested_completion_text;
    m_requested_completion_text.clear();
    updateCompletionList(text);
    return;
  }

  if (m_needs_clear_result) {
    m_locator_model->clear();
    m_needs_clear_result = false;
  }
}

auto LocatorWidget::scheduleAcceptEntry(const QModelIndex &index) -> void
{
  if (m_update_requested) {
    // don't just accept the selected entry, since the list is not up to date
    // accept will be called after the update finished
    m_row_requested_for_accept = index.row();
    // do not wait for the rest of the search to finish
    m_entries_watcher->future().cancel();
  } else {
    acceptEntry(index.row());
  }
}

auto LocatorWidget::aboutToShutdown(const std::function<void()> &emit_asynchronous_shutdown_finished) -> ExtensionSystem::IPlugin::ShutdownFlag
{
  m_shutting_down = true;

  if (m_shared_future.isRunning()) {
    onFinished(m_shared_future, Locator::instance(), [emit_asynchronous_shutdown_finished](const QFuture<void> &) {
      emit_asynchronous_shutdown_finished();
    });
    m_shared_future.cancel();
    return ExtensionSystem::IPlugin::AsynchronousShutdown;
  }

  return ExtensionSystem::IPlugin::SynchronousShutdown;
}

auto LocatorWidget::acceptEntry(const int row) -> void
{
  if (row < 0 || row >= m_locator_model->rowCount())
    return;

  const auto index = m_locator_model->index(row, 0);

  if (!index.isValid())
    return;

  const auto entry = m_locator_model->data(index, locator_entry_role).value<LocatorFilterEntry>();
  Q_ASSERT(entry.filter != nullptr);
  QString new_text;
  auto selection_start = -1;
  auto selection_length = 0;
  const auto focus_before_accept = QApplication::focusWidget();

  entry.filter->accept(entry, &new_text, &selection_start, &selection_length);
  if (new_text.isEmpty()) {
    emit hidePopup();
    if (QApplication::focusWidget() == focus_before_accept)
      resetFocus(m_previous_focus_widget, isInMainWindow());
  } else {
    showText(new_text, selection_start, selection_length);
  }
}

auto LocatorWidget::showText(const QString &text, const int selection_start, const int selection_length) -> void
{
  if (!text.isEmpty())
    m_file_line_edit->setText(text);

  m_file_line_edit->setFocus();

  showPopupNow();
  ICore::raiseWindow(window());

  if (selection_start >= 0) {
    m_file_line_edit->setSelection(selection_start, selection_length);
    if (selection_length == 0) // make sure the cursor is at the right position (Mac-vs.-rest difference)
      m_file_line_edit->setCursorPosition(selection_start);
  } else {
    m_file_line_edit->selectAll();
  }
}

auto LocatorWidget::currentText() const -> QString
{
  return m_file_line_edit->text();
}

auto LocatorWidget::model() const -> QAbstractItemModel*
{
  return m_locator_model;
}

auto LocatorWidget::showConfigureDialog() -> void
{
  ICore::showOptionsDialog(Constants::filter_options_page);
}

auto LocatorWidget::addSearchResults(const int first_index, const int end_index) -> void
{
  if (m_needs_clear_result) {
    m_locator_model->clear();
    m_needs_clear_result = false;
  }

  const auto select_first = m_locator_model->rowCount() == 0;
  QList<LocatorFilterEntry> entries;

  for (auto i = first_index; i < end_index; ++i)
    entries.append(m_entries_watcher->resultAt(i));

  m_locator_model->addEntries(entries);

  if (select_first) {
    emit selectRow(0);
    if (m_row_requested_for_accept)
      m_row_requested_for_accept = 0;
  }
}

auto createStaticLocatorWidget(Locator *locator) -> LocatorWidget*
{
  const auto widget = new LocatorWidget(locator);
  const auto popup = new TopLeftLocatorPopup(widget); // owned by widget
  popup->setWindowFlags(Qt::ToolTip);
  return widget;
}

auto createLocatorPopup(Locator *locator, QWidget *parent) -> LocatorPopup*
{
  const auto widget = new LocatorWidget(locator);
  const auto popup = new CenteredLocatorPopup(widget, parent);
  popup->layout()->addWidget(widget);
  popup->setWindowFlags(Qt::Popup);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  return popup;
}

CompletionDelegate::CompletionDelegate(QObject *parent) : HighlightingItemDelegate(0, parent) {}

auto CompletionDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize
{
  return HighlightingItemDelegate::sizeHint(option, index) + QSize(0, 2);
}

} // namespace Internal
} // namespace Core
