// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "taskwindow.hpp"

#include "itaskhandler.hpp"
#include "projectexplorericons.hpp"
#include "session.hpp"
#include "task.hpp"
#include "taskhub.hpp"
#include "taskmodel.hpp"

#include <aggregation/aggregate.hpp>

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/find/itemviewfind.hpp>
#include <core/icore.hpp>
#include <core/icontext.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileinprojectfinder.hpp>
#include <utils/itemviews.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QDir>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QMenu>
#include <QToolButton>
#include <QScrollBar>

using namespace Utils;

namespace {
constexpr int  ELLIPSIS_GRADIENT_WIDTH = 16;
constexpr char SESSION_FILTER_CATEGORIES[] = "TaskWindow.Categories";
constexpr char SESSION_FILTER_WARNINGS[] = "TaskWindow.IncludeWarnings";
}

namespace ProjectExplorer {

static QList<ITaskHandler *> g_taskHandlers;

ITaskHandler::ITaskHandler(bool isMultiHandler) : m_isMultiHandler(isMultiHandler)
{
  g_taskHandlers.append(this);
}

ITaskHandler::~ITaskHandler()
{
  g_taskHandlers.removeOne(this);
}

auto ITaskHandler::handle(const Task &task) -> void
{
  QTC_ASSERT(m_isMultiHandler, return);
  handle(Tasks{task});
}

auto ITaskHandler::handle(const Tasks &tasks) -> void
{
  QTC_ASSERT(canHandle(tasks), return);
  QTC_ASSERT(!m_isMultiHandler, return);
  handle(tasks.first());
}

auto ITaskHandler::canHandle(const Tasks &tasks) const -> bool
{
  if (tasks.isEmpty())
    return false;
  if (m_isMultiHandler)
    return true;
  if (tasks.size() > 1)
    return false;
  return canHandle(tasks.first());
}

namespace Internal {

class TaskView : public ListView {
public:
  TaskView(QWidget *parent = nullptr);
  ~TaskView() override;

private:
  auto resizeEvent(QResizeEvent *e) -> void override;
  auto mousePressEvent(QMouseEvent *e) -> void override;
  auto mouseReleaseEvent(QMouseEvent *e) -> void override;
  auto mouseMoveEvent(QMouseEvent *e) -> void override;
  auto locationForPos(const QPoint &pos) -> Link;

  bool m_linksActive = true;
  Qt::MouseButton m_mouseButtonPressed = Qt::NoButton;
};

class TaskDelegate : public QStyledItemDelegate {
  Q_OBJECT
  friend class TaskView; // for using Positions::minimumSize()

public:
  TaskDelegate(QObject *parent = nullptr);
  ~TaskDelegate() override;

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override;
  // TaskView uses this method if the size of the taskview changes
  auto emitSizeHintChanged(const QModelIndex &index) -> void;
  auto currentChanged(const QModelIndex &current, const QModelIndex &previous) -> void;
  auto hrefForPos(const QPointF &pos) -> QString;

private:
  auto generateGradientPixmap(int width, int height, QColor color, bool selected) const -> void;

  mutable int m_cachedHeight = 0;
  mutable QFont m_cachedFont;
  mutable QList<QPair<QRectF, QString>> m_hrefs;

  /*
    Collapsed:
    +----------------------------------------------------------------------------------------------------+
    | TASKICONAREA  TEXTAREA                                                           FILEAREA LINEAREA |
    +----------------------------------------------------------------------------------------------------+

    Expanded:
    +----------------------------------------------------------------------------------------------------+
    | TASKICONICON  TEXTAREA                                                           FILEAREA LINEAREA |
    |               more text -------------------------------------------------------------------------> |
    +----------------------------------------------------------------------------------------------------+
   */
  class Positions {
  public:
    Positions(const QStyleOptionViewItem &options, TaskModel *model) : m_totalWidth(options.rect.width()), m_maxFileLength(model->sizeOfFile(options.font)), m_maxLineLength(model->sizeOfLineNumber(options.font)), m_realFileLength(m_maxFileLength), m_top(options.rect.top()), m_bottom(options.rect.bottom())
    {
      const auto flexibleArea = lineAreaLeft() - textAreaLeft() - ITEM_SPACING;
      if (m_maxFileLength > flexibleArea / 2)
        m_realFileLength = flexibleArea / 2;
      m_fontHeight = QFontMetrics(options.font).height();
    }

    auto top() const -> int { return m_top + ITEM_MARGIN; }
    auto left() const -> int { return ITEM_MARGIN; }
    auto right() const -> int { return m_totalWidth - ITEM_MARGIN; }
    auto bottom() const -> int { return m_bottom; }
    auto firstLineHeight() const -> int { return m_fontHeight + 1; }
    static auto minimumHeight() -> int { return taskIconHeight() + 2 * ITEM_MARGIN; }
    auto taskIconLeft() const -> int { return left(); }
    static auto taskIconWidth() -> int { return TASK_ICON_SIZE; }
    static auto taskIconHeight() -> int { return TASK_ICON_SIZE; }
    auto taskIconRight() const -> int { return taskIconLeft() + taskIconWidth(); }
    auto taskIcon() const -> QRect { return QRect(taskIconLeft(), top(), taskIconWidth(), taskIconHeight()); }
    auto textAreaLeft() const -> int { return taskIconRight() + ITEM_SPACING; }
    auto textAreaWidth() const -> int { return textAreaRight() - textAreaLeft(); }
    auto textAreaRight() const -> int { return fileAreaLeft() - ITEM_SPACING; }
    auto textArea() const -> QRect { return QRect(textAreaLeft(), top(), textAreaWidth(), firstLineHeight()); }
    auto fileAreaLeft() const -> int { return fileAreaRight() - fileAreaWidth(); }
    auto fileAreaWidth() const -> int { return m_realFileLength; }
    auto fileAreaRight() const -> int { return lineAreaLeft() - ITEM_SPACING; }
    auto fileArea() const -> QRect { return QRect(fileAreaLeft(), top(), fileAreaWidth(), firstLineHeight()); }
    auto lineAreaLeft() const -> int { return lineAreaRight() - lineAreaWidth(); }
    auto lineAreaWidth() const -> int { return m_maxLineLength; }
    auto lineAreaRight() const -> int { return right(); }
    auto lineArea() const -> QRect { return QRect(lineAreaLeft(), top(), lineAreaWidth(), firstLineHeight()); }

  private:
    int m_totalWidth;
    int m_maxFileLength;
    int m_maxLineLength;
    int m_realFileLength;
    int m_top;
    int m_bottom;
    int m_fontHeight;
    static const int TASK_ICON_SIZE = 16;
    static const int ITEM_MARGIN = 2;
    static const int ITEM_SPACING = 2 * ITEM_MARGIN;
  };
};

TaskView::TaskView(QWidget *parent) : ListView(parent)
{
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollMode(ScrollPerPixel);
  setMouseTracking(true);
  setAutoScroll(false); // QTCREATORBUG-25101

  const QFontMetrics fm(font());
  auto vStepSize = fm.height() + 3;
  if (vStepSize < TaskDelegate::Positions::minimumHeight())
    vStepSize = TaskDelegate::Positions::minimumHeight();

  verticalScrollBar()->setSingleStep(vStepSize);
}

TaskView::~TaskView() = default;

auto TaskView::resizeEvent(QResizeEvent *e) -> void
{
  Q_UNUSED(e)
  static_cast<TaskDelegate*>(itemDelegate())->emitSizeHintChanged(selectionModel()->currentIndex());
}

auto TaskView::mousePressEvent(QMouseEvent *e) -> void
{
  m_mouseButtonPressed = e->button();
  ListView::mousePressEvent(e);
}

auto TaskView::mouseReleaseEvent(QMouseEvent *e) -> void
{
  if (m_linksActive && m_mouseButtonPressed == Qt::LeftButton) {
    const auto loc = locationForPos(e->pos());
    if (!loc.targetFilePath.isEmpty()) {
      Core::EditorManager::openEditorAt(loc, {}, Core::EditorManager::SwitchSplitIfAlreadyVisible);
    }
  }

  // Mouse was released, activate links again
  m_linksActive = true;
  m_mouseButtonPressed = Qt::NoButton;
  ListView::mouseReleaseEvent(e);
}

auto TaskView::mouseMoveEvent(QMouseEvent *e) -> void
{
  // Cursor was dragged, deactivate links
  if (m_mouseButtonPressed != Qt::NoButton)
    m_linksActive = false;

  viewport()->setCursor(m_linksActive && !locationForPos(e->pos()).targetFilePath.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
  ListView::mouseMoveEvent(e);
}

auto TaskView::locationForPos(const QPoint &pos) -> Link
{
  const auto delegate = qobject_cast<TaskDelegate*>(itemDelegate(indexAt(pos)));
  if (!delegate)
    return {};
  OutputFormatter formatter;
  Link loc;
  connect(&formatter, &OutputFormatter::openInEditorRequested, this, [&loc](const Link &link) {
    loc = link;
  });

  const auto href = delegate->hrefForPos(pos);
  if (!href.isEmpty())
    formatter.handleLink(href);
  return loc;
}

/////
// TaskWindow
/////

class TaskWindowPrivate {
public:
  auto handler(const QAction *action) -> ITaskHandler*
  {
    const auto handler = m_actionToHandlerMap.value(action, nullptr);
    return g_taskHandlers.contains(handler) ? handler : nullptr;
  }

  TaskModel *m_model;
  TaskFilterModel *m_filter;
  TaskView *m_listview;
  Core::IContext *m_taskWindowContext;
  QMenu *m_contextMenu;
  QMap<const QAction*, ITaskHandler*> m_actionToHandlerMap;
  ITaskHandler *m_defaultHandler = nullptr;
  QToolButton *m_filterWarningsButton;
  QToolButton *m_categoriesButton;
  QMenu *m_categoriesMenu;
  QList<QAction*> m_actions;
  int m_visibleIssuesCount = 0;
};

static auto createFilterButton(const QIcon &icon, const QString &toolTip, QObject *receiver, std::function<void(bool)> lambda) -> QToolButton*
{
  const auto button = new QToolButton;
  button->setIcon(icon);
  button->setToolTip(toolTip);
  button->setCheckable(true);
  button->setChecked(true);
  button->setEnabled(true);
  QObject::connect(button, &QToolButton::toggled, receiver, lambda);
  return button;
}

TaskWindow::TaskWindow() : d(std::make_unique<TaskWindowPrivate>())
{
  d->m_model = new TaskModel(this);
  d->m_filter = new TaskFilterModel(d->m_model);
  d->m_listview = new TaskView;

  const auto agg = new Aggregation::Aggregate;
  agg->add(d->m_listview);
  agg->add(new Core::ItemViewFind(d->m_listview, TaskModel::Description));

  d->m_listview->setModel(d->m_filter);
  d->m_listview->setFrameStyle(QFrame::NoFrame);
  d->m_listview->setWindowTitle(displayName());
  d->m_listview->setSelectionMode(QAbstractItemView::ExtendedSelection);
  auto *tld = new TaskDelegate(this);
  d->m_listview->setItemDelegate(tld);
  d->m_listview->setWindowIcon(Icons::WINDOW.icon());
  d->m_listview->setContextMenuPolicy(Qt::ActionsContextMenu);
  d->m_listview->setAttribute(Qt::WA_MacShowFocusRect, false);

  d->m_taskWindowContext = new Core::IContext(d->m_listview);
  d->m_taskWindowContext->setWidget(d->m_listview);
  d->m_taskWindowContext->setContext(Core::Context(Core::Constants::C_PROBLEM_PANE));
  Core::ICore::addContextObject(d->m_taskWindowContext);

  connect(d->m_listview->selectionModel(), &QItemSelectionModel::currentChanged, tld, &TaskDelegate::currentChanged);
  connect(d->m_listview->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) { d->m_listview->scrollTo(index); });
  connect(d->m_listview, &QAbstractItemView::activated, this, &TaskWindow::triggerDefaultHandler);
  connect(d->m_listview->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
    const auto tasks = d->m_filter->tasks(d->m_listview->selectionModel()->selectedIndexes());
    for (const auto action : qAsConst(d->m_actions)) {
      const auto h = d->handler(action);
      action->setEnabled(h && h->canHandle(tasks));
    }
  });

  d->m_contextMenu = new QMenu(d->m_listview);

  d->m_listview->setContextMenuPolicy(Qt::ActionsContextMenu);

  d->m_filterWarningsButton = createFilterButton(Utils::Icons::WARNING_TOOLBAR.icon(), tr("Show Warnings"), this, [this](bool show) { setShowWarnings(show); });

  d->m_categoriesButton = new QToolButton;
  d->m_categoriesButton->setIcon(Utils::Icons::FILTER.icon());
  d->m_categoriesButton->setToolTip(tr("Filter by categories"));
  d->m_categoriesButton->setProperty("noArrow", true);
  d->m_categoriesButton->setPopupMode(QToolButton::InstantPopup);

  d->m_categoriesMenu = new QMenu(d->m_categoriesButton);
  connect(d->m_categoriesMenu, &QMenu::aboutToShow, this, &TaskWindow::updateCategoriesMenu);

  d->m_categoriesButton->setMenu(d->m_categoriesMenu);

  setupFilterUi("IssuesPane.Filter");
  setFilteringEnabled(true);

  const auto hub = TaskHub::instance();
  connect(hub, &TaskHub::categoryAdded, this, &TaskWindow::addCategory);
  connect(hub, &TaskHub::taskAdded, this, &TaskWindow::addTask);
  connect(hub, &TaskHub::taskRemoved, this, &TaskWindow::removeTask);
  connect(hub, &TaskHub::taskLineNumberUpdated, this, &TaskWindow::updatedTaskLineNumber);
  connect(hub, &TaskHub::taskFileNameUpdated, this, &TaskWindow::updatedTaskFileName);
  connect(hub, &TaskHub::tasksCleared, this, &TaskWindow::clearTasks);
  connect(hub, &TaskHub::categoryVisibilityChanged, this, &TaskWindow::setCategoryVisibility);
  connect(hub, &TaskHub::popupRequested, this, &TaskWindow::popup);
  connect(hub, &TaskHub::showTask, this, &TaskWindow::showTask);
  connect(hub, &TaskHub::openTask, this, &TaskWindow::openTask);

  connect(d->m_filter, &TaskFilterModel::rowsAboutToBeRemoved, [this](const QModelIndex &, int first, int last) {
    d->m_visibleIssuesCount -= d->m_filter->issuesCount(first, last);
    emit setBadgeNumber(d->m_visibleIssuesCount);
  });
  connect(d->m_filter, &TaskFilterModel::rowsInserted, [this](const QModelIndex &, int first, int last) {
    d->m_visibleIssuesCount += d->m_filter->issuesCount(first, last);
    emit setBadgeNumber(d->m_visibleIssuesCount);
  });
  connect(d->m_filter, &TaskFilterModel::modelReset, [this] {
    d->m_visibleIssuesCount = d->m_filter->issuesCount(0, d->m_filter->rowCount());
    emit setBadgeNumber(d->m_visibleIssuesCount);
  });

  const auto session = SessionManager::instance();
  connect(session, &SessionManager::aboutToSaveSession, this, &TaskWindow::saveSettings);
  connect(session, &SessionManager::sessionLoaded, this, &TaskWindow::loadSettings);
}

TaskWindow::~TaskWindow()
{
  delete d->m_filterWarningsButton;
  delete d->m_listview;
  delete d->m_filter;
  delete d->m_model;
}

auto TaskWindow::delayedInitialization() -> void
{
  static auto alreadyDone = false;
  if (alreadyDone)
    return;

  alreadyDone = true;

  for (auto h : qAsConst(g_taskHandlers)) {
    if (h->isDefaultHandler() && !d->m_defaultHandler)
      d->m_defaultHandler = h;

    auto action = h->createAction(this);
    action->setEnabled(false);
    QTC_ASSERT(action, continue);
    d->m_actionToHandlerMap.insert(action, h);
    connect(action, &QAction::triggered, this, &TaskWindow::actionTriggered);
    d->m_actions << action;

    auto id = h->actionManagerId();
    if (id.isValid()) {
      const auto cmd = Core::ActionManager::registerAction(action, id, d->m_taskWindowContext->context(), true);
      action = cmd->action();
    }
    d->m_listview->addAction(action);
  }
}

auto TaskWindow::toolBarWidgets() const -> QList<QWidget*>
{
  return {d->m_filterWarningsButton, d->m_categoriesButton, filterWidget()};
}

auto TaskWindow::outputWidget(QWidget *) -> QWidget*
{
  return d->m_listview;
}

auto TaskWindow::clearTasks(Id categoryId) -> void
{
  d->m_model->clearTasks(categoryId);

  emit tasksChanged();
  navigateStateChanged();
}

auto TaskWindow::setCategoryVisibility(Id categoryId, bool visible) -> void
{
  if (!categoryId.isValid())
    return;

  auto categories = d->m_filter->filteredCategories();

  if (visible)
    categories.removeOne(categoryId);
  else
    categories.append(categoryId);

  d->m_filter->setFilteredCategories(categories);
}

auto TaskWindow::saveSettings() -> void
{
  const auto categories = transform(d->m_filter->filteredCategories(), &Id::toString);
  SessionManager::setValue(QLatin1String(SESSION_FILTER_CATEGORIES), categories);
  SessionManager::setValue(QLatin1String(SESSION_FILTER_WARNINGS), d->m_filter->filterIncludesWarnings());
}

auto TaskWindow::loadSettings() -> void
{
  auto value = SessionManager::value(QLatin1String(SESSION_FILTER_CATEGORIES));
  if (value.isValid()) {
    const auto categories = transform(value.toStringList(), &Id::fromString);
    d->m_filter->setFilteredCategories(categories);
  }
  value = SessionManager::value(QLatin1String(SESSION_FILTER_WARNINGS));
  if (value.isValid()) {
    const auto includeWarnings = value.toBool();
    d->m_filter->setFilterIncludesWarnings(includeWarnings);
    d->m_filterWarningsButton->setChecked(d->m_filter->filterIncludesWarnings());
  }
}

auto TaskWindow::visibilityChanged(bool visible) -> void
{
  if (visible)
    delayedInitialization();
}

auto TaskWindow::addCategory(Id categoryId, const QString &displayName, bool visible, int priority) -> void
{
  d->m_model->addCategory(categoryId, displayName, priority);
  if (!visible) {
    auto filters = d->m_filter->filteredCategories();
    filters += categoryId;
    d->m_filter->setFilteredCategories(filters);
  }
}

auto TaskWindow::addTask(const Task &task) -> void
{
  d->m_model->addTask(task);

  emit tasksChanged();
  navigateStateChanged();

  if ((task.options & Task::FlashWorthy) && task.type == Task::Error && d->m_filter->filterIncludesErrors() && !d->m_filter->filteredCategories().contains(task.category)) {
    flash();
  }
}

auto TaskWindow::removeTask(const Task &task) -> void
{
  d->m_model->removeTask(task.taskId);

  emit tasksChanged();
  navigateStateChanged();
}

auto TaskWindow::updatedTaskFileName(const Task &task, const QString &fileName) -> void
{
  d->m_model->updateTaskFileName(task, fileName);
  emit tasksChanged();
}

auto TaskWindow::updatedTaskLineNumber(const Task &task, int line) -> void
{
  d->m_model->updateTaskLineNumber(task, line);
  emit tasksChanged();
}

auto TaskWindow::showTask(const Task &task) -> void
{
  const auto sourceRow = d->m_model->rowForTask(task);
  const auto sourceIdx = d->m_model->index(sourceRow, 0);
  const auto filterIdx = d->m_filter->mapFromSource(sourceIdx);
  d->m_listview->setCurrentIndex(filterIdx);
  popup(ModeSwitch);
}

auto TaskWindow::openTask(const Task &task) -> void
{
  const auto sourceRow = d->m_model->rowForTask(task);
  const auto sourceIdx = d->m_model->index(sourceRow, 0);
  const auto filterIdx = d->m_filter->mapFromSource(sourceIdx);
  triggerDefaultHandler(filterIdx);
}

auto TaskWindow::triggerDefaultHandler(const QModelIndex &index) -> void
{
  if (!index.isValid() || !d->m_defaultHandler)
    return;

  auto task(d->m_filter->task(index));
  if (task.isNull())
    return;

  if (!task.file.isEmpty() && !task.file.toFileInfo().isAbsolute() && !task.fileCandidates.empty()) {
    const auto userChoice = chooseFileFromList(task.fileCandidates);
    if (!userChoice.isEmpty()) {
      task.file = userChoice;
      updatedTaskFileName(task, task.file.toString());
    }
  }

  if (d->m_defaultHandler->canHandle(task)) {
    d->m_defaultHandler->handle(task);
  } else {
    if (!task.file.exists())
      d->m_model->setFileNotFound(index, true);
  }
}

auto TaskWindow::actionTriggered() -> void
{
  const auto action = qobject_cast<QAction*>(sender());
  if (!action || !action->isEnabled())
    return;
  const auto h = d->handler(action);
  if (!h)
    return;

  h->handle(d->m_filter->tasks(d->m_listview->selectionModel()->selectedIndexes()));
}

auto TaskWindow::setShowWarnings(bool show) -> void
{
  d->m_filter->setFilterIncludesWarnings(show);
}

auto TaskWindow::updateCategoriesMenu() -> void
{
  using NameToIdsConstIt = QMap<QString, Id>::ConstIterator;

  d->m_categoriesMenu->clear();

  const auto filteredCategories = d->m_filter->filteredCategories();

  QMap<QString, Id> nameToIds;
  foreach(Utils::Id categoryId, d->m_model->categoryIds())
    nameToIds.insert(d->m_model->categoryDisplayName(categoryId), categoryId);

  const auto cend = nameToIds.constEnd();
  for (auto it = nameToIds.constBegin(); it != cend; ++it) {
    const auto &displayName = it.key();
    const auto categoryId = it.value();
    auto action = new QAction(d->m_categoriesMenu);
    action->setCheckable(true);
    action->setText(displayName);
    action->setChecked(!filteredCategories.contains(categoryId));
    connect(action, &QAction::triggered, this, [this, action, categoryId] {
      setCategoryVisibility(categoryId, action->isChecked());
    });
    d->m_categoriesMenu->addAction(action);
  }
}

auto TaskWindow::taskCount(Id category) const -> int
{
  return d->m_model->taskCount(category);
}

auto TaskWindow::errorTaskCount(Id category) const -> int
{
  return d->m_model->errorTaskCount(category);
}

auto TaskWindow::warningTaskCount(Id category) const -> int
{
  return d->m_model->warningTaskCount(category);
}

auto TaskWindow::priorityInStatusBar() const -> int
{
  return 90;
}

auto TaskWindow::clearContents() -> void
{
  // clear all tasks in all displays
  // Yeah we are that special
  TaskHub::clearTasks();
}

auto TaskWindow::hasFocus() const -> bool
{
  return d->m_listview->window()->focusWidget() == d->m_listview;
}

auto TaskWindow::canFocus() const -> bool
{
  return d->m_filter->rowCount();
}

auto TaskWindow::setFocus() -> void
{
  if (d->m_filter->rowCount()) {
    d->m_listview->setFocus();
    if (d->m_listview->currentIndex() == QModelIndex())
      d->m_listview->setCurrentIndex(d->m_filter->index(0, 0, QModelIndex()));
  }
}

auto TaskWindow::canNext() const -> bool
{
  return d->m_filter->rowCount();
}

auto TaskWindow::canPrevious() const -> bool
{
  return d->m_filter->rowCount();
}

auto TaskWindow::goToNext() -> void
{
  if (!canNext())
    return;
  const auto startIndex = d->m_listview->currentIndex();
  auto currentIndex = startIndex;

  if (startIndex.isValid()) {
    do {
      auto row = currentIndex.row() + 1;
      if (row == d->m_filter->rowCount())
        row = 0;
      currentIndex = d->m_filter->index(row, 0);
      if (d->m_filter->hasFile(currentIndex))
        break;
    } while (startIndex != currentIndex);
  } else {
    currentIndex = d->m_filter->index(0, 0);
  }
  d->m_listview->setCurrentIndex(currentIndex);
  triggerDefaultHandler(currentIndex);
}

auto TaskWindow::goToPrev() -> void
{
  if (!canPrevious())
    return;
  const auto startIndex = d->m_listview->currentIndex();
  auto currentIndex = startIndex;

  if (startIndex.isValid()) {
    do {
      auto row = currentIndex.row() - 1;
      if (row < 0)
        row = d->m_filter->rowCount() - 1;
      currentIndex = d->m_filter->index(row, 0);
      if (d->m_filter->hasFile(currentIndex))
        break;
    } while (startIndex != currentIndex);
  } else {
    currentIndex = d->m_filter->index(0, 0);
  }
  d->m_listview->setCurrentIndex(currentIndex);
  triggerDefaultHandler(currentIndex);
}

auto TaskWindow::updateFilter() -> void
{
  d->m_filter->updateFilterProperties(filterText(), filterCaseSensitivity(), filterUsesRegexp(), filterIsInverted());
}

auto TaskWindow::canNavigate() const -> bool
{
  return true;
}

/////
// Delegate
/////

TaskDelegate::TaskDelegate(QObject *parent) : QStyledItemDelegate(parent) { }

TaskDelegate::~TaskDelegate() = default;

auto TaskDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize
{
  auto opt = option;
  initStyleOption(&opt, index);

  const auto view = qobject_cast<const QAbstractItemView*>(opt.widget);
  const auto current = view->selectionModel()->currentIndex() == index;
  QSize s;
  s.setWidth(option.rect.width());

  if (!current && option.font == m_cachedFont && m_cachedHeight > 0) {
    s.setHeight(m_cachedHeight);
    return s;
  }

  const QFontMetrics fm(option.font);
  const auto fontHeight = fm.height();
  const auto fontLeading = fm.leading();

  const auto model = static_cast<TaskFilterModel*>(view->model())->taskModel();
  const Positions positions(option, model);

  if (current) {
    auto description = index.data(TaskModel::Description).toString();
    // Layout the description
    const auto leading = fontLeading;
    auto height = 0;
    description.replace(QLatin1Char('\n'), QChar::LineSeparator);
    QTextLayout tl(description);
    tl.setFormats(index.data(TaskModel::Task_t).value<Task>().formats);
    tl.beginLayout();
    while (true) {
      auto line = tl.createLine();
      if (!line.isValid())
        break;
      line.setLineWidth(positions.textAreaWidth());
      height += leading;
      line.setPosition(QPoint(0, height));
      height += static_cast<int>(line.height());
    }
    tl.endLayout();

    s.setHeight(height + leading + fontHeight + 3);
  } else {
    s.setHeight(fontHeight + 3);
  }
  if (s.height() < Positions::minimumHeight())
    s.setHeight(Positions::minimumHeight());

  if (!current) {
    m_cachedHeight = s.height();
    m_cachedFont = option.font;
  }

  return s;
}

auto TaskDelegate::emitSizeHintChanged(const QModelIndex &index) -> void
{
  emit sizeHintChanged(index);
}

auto TaskDelegate::currentChanged(const QModelIndex &current, const QModelIndex &previous) -> void
{
  emit sizeHintChanged(current);
  emit sizeHintChanged(previous);
}

auto TaskDelegate::hrefForPos(const QPointF &pos) -> QString
{
  for (const auto &link : qAsConst(m_hrefs)) {
    if (link.first.contains(pos))
      return link.second;
  }
  return {};
}

auto TaskDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  auto opt = option;
  initStyleOption(&opt, index);
  painter->save();

  QFontMetrics fm(opt.font);
  QColor backgroundColor;
  QColor textColor;

  auto view = qobject_cast<const QAbstractItemView*>(opt.widget);
  const auto selected = view->selectionModel()->isSelected(index);
  const auto current = view->selectionModel()->currentIndex() == index;

  if (selected) {
    painter->setBrush(opt.palette.highlight().color());
    backgroundColor = opt.palette.highlight().color();
  } else {
    painter->setBrush(opt.palette.window().color());
    backgroundColor = opt.palette.window().color();
  }
  painter->setPen(Qt::NoPen);
  painter->drawRect(opt.rect);

  // Set Text Color
  if (selected)
    textColor = opt.palette.highlightedText().color();
  else
    textColor = opt.palette.text().color();

  painter->setPen(textColor);

  auto model = static_cast<TaskFilterModel*>(view->model())->taskModel();
  Positions positions(opt, model);

  // Paint TaskIconArea:
  auto icon = index.data(TaskModel::Icon).value<QIcon>();
  painter->drawPixmap(positions.left(), positions.top(), icon.pixmap(Positions::taskIconWidth(), Positions::taskIconHeight()));

  // Paint TextArea:
  if (!current) {
    // in small mode we lay out differently
    auto bottom = index.data(TaskModel::Description).toString().split(QLatin1Char('\n')).first();
    painter->setClipRect(positions.textArea());
    painter->drawText(positions.textAreaLeft(), positions.top() + fm.ascent(), bottom);
    if (fm.horizontalAdvance(bottom) > positions.textAreaWidth()) {
      // draw a gradient to mask the text
      auto gradientStart = positions.textAreaRight() - ELLIPSIS_GRADIENT_WIDTH + 1;
      QLinearGradient lg(gradientStart, 0, gradientStart + ELLIPSIS_GRADIENT_WIDTH, 0);
      lg.setColorAt(0, Qt::transparent);
      lg.setColorAt(1, backgroundColor);
      painter->fillRect(gradientStart, positions.top(), ELLIPSIS_GRADIENT_WIDTH, positions.firstLineHeight(), lg);
    }
  } else {
    // Description
    auto description = index.data(TaskModel::Description).toString();
    // Layout the description
    auto leading = fm.leading();
    auto height = 0;
    description.replace(QLatin1Char('\n'), QChar::LineSeparator);
    QTextLayout tl(description);
    auto formats = index.data(TaskModel::Task_t).value<Task>().formats;
    for (auto &format : formats)
      format.format.setForeground(textColor);
    tl.setFormats(formats);
    tl.beginLayout();
    while (true) {
      auto line = tl.createLine();
      if (!line.isValid())
        break;
      line.setLineWidth(positions.textAreaWidth());
      height += leading;
      line.setPosition(QPoint(0, height));
      height += static_cast<int>(line.height());
    }
    tl.endLayout();
    const auto indexPos = view->visualRect(index).topLeft();
    tl.draw(painter, QPoint(positions.textAreaLeft(), positions.top()));
    m_hrefs.clear();
    for (const auto &range : tl.formats()) {
      if (!range.format.isAnchor())
        continue;
      const auto &firstLinkLine = tl.lineForTextPosition(range.start);
      const auto &lastLinkLine = tl.lineForTextPosition(range.start + range.length - 1);
      for (auto i = firstLinkLine.lineNumber(); i <= lastLinkLine.lineNumber(); ++i) {
        const auto &linkLine = tl.lineAt(i);
        if (!linkLine.isValid())
          break;
        const auto linePos = linkLine.position();
        const auto linkStartPos = i == firstLinkLine.lineNumber() ? range.start : linkLine.textStart();
        const auto startOffset = linkLine.cursorToX(linkStartPos);
        const auto linkEndPos = i == lastLinkLine.lineNumber() ? range.start + range.length : linkLine.textStart() + linkLine.textLength();
        const auto endOffset = linkLine.cursorToX(linkEndPos);
        const QPointF linkPos(indexPos.x() + positions.textAreaLeft() + linePos.x() + startOffset, positions.top() + linePos.y());
        const QSize linkSize(endOffset - startOffset, linkLine.height());
        const QRectF linkRect(linkPos, linkSize);
        m_hrefs << qMakePair(linkRect, range.format.anchorHref());
      }
    }

    QColor mix;
    mix.setRgb(static_cast<int>(0.7 * textColor.red() + 0.3 * backgroundColor.red()), static_cast<int>(0.7 * textColor.green() + 0.3 * backgroundColor.green()), static_cast<int>(0.7 * textColor.blue() + 0.3 * backgroundColor.blue()));
    painter->setPen(mix);

    const auto directory = QDir::toNativeSeparators(index.data(TaskModel::File).toString());
    auto secondBaseLine = positions.top() + fm.ascent() + height + leading;
    if (index.data(TaskModel::FileNotFound).toBool() && !directory.isEmpty()) {
      auto fileNotFound = tr("File not found: %1").arg(directory);
      painter->setPen(Qt::red);
      painter->drawText(positions.textAreaLeft(), secondBaseLine, fileNotFound);
    } else {
      painter->drawText(positions.textAreaLeft(), secondBaseLine, directory);
    }
  }
  painter->setPen(textColor);

  // Paint FileArea
  auto file = index.data(TaskModel::File).toString();
  const int pos = file.lastIndexOf(QLatin1Char('/'));
  if (pos != -1)
    file = file.mid(pos + 1);
  const auto realFileWidth = fm.horizontalAdvance(file);
  painter->setClipRect(positions.fileArea());
  painter->drawText(qMin(positions.fileAreaLeft(), positions.fileAreaRight() - realFileWidth), positions.top() + fm.ascent(), file);
  if (realFileWidth > positions.fileAreaWidth()) {
    // draw a gradient to mask the text
    auto gradientStart = positions.fileAreaLeft() - 1;
    QLinearGradient lg(gradientStart + ELLIPSIS_GRADIENT_WIDTH, 0, gradientStart, 0);
    lg.setColorAt(0, Qt::transparent);
    lg.setColorAt(1, backgroundColor);
    painter->fillRect(gradientStart, positions.top(), ELLIPSIS_GRADIENT_WIDTH, positions.firstLineHeight(), lg);
  }

  // Paint LineArea
  auto line = index.data(TaskModel::Line).toInt();
  auto movedLine = index.data(TaskModel::MovedLine).toInt();
  QString lineText;

  if (line == -1) {
    // No line information at all
  } else if (movedLine == -1) {
    // removed the line, but we had line information, show the line in ()
    auto f = painter->font();
    f.setItalic(true);
    painter->setFont(f);
    lineText = QLatin1Char('(') + QString::number(line) + QLatin1Char(')');
  } else if (movedLine != line) {
    // The line was moved
    auto f = painter->font();
    f.setItalic(true);
    painter->setFont(f);
    lineText = QString::number(movedLine);
  } else {
    lineText = QString::number(line);
  }

  painter->setClipRect(positions.lineArea());
  const auto realLineWidth = fm.horizontalAdvance(lineText);
  painter->drawText(positions.lineAreaRight() - realLineWidth, positions.top() + fm.ascent(), lineText);
  painter->setClipRect(opt.rect);

  // Separator lines
  painter->setPen(QColor::fromRgb(150, 150, 150));
  const auto borderRect = QRectF(opt.rect).adjusted(0.5, 0.5, -0.5, -0.5);
  painter->drawLine(borderRect.bottomLeft(), borderRect.bottomRight());
  painter->restore();
}

} // namespace Internal
} // namespace ProjectExplorer

#include "taskwindow.moc"
