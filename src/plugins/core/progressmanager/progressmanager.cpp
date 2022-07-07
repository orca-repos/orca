// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "futureprogress.hpp"
#include "progressmanager_p.hpp"
#include "progressbar.hpp"
#include "progressview.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/icore.hpp>
#include <core/statusbarmanager.hpp>

#include <utils/qtcassert.hpp>
#include <utils/stylehelper.hpp>
#include <utils/utilsicons.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariant>

#include <cmath>

static constexpr char k_settings_group[] = "Progress";
static constexpr char k_details_pinned[] = "DetailsPinned";
static constexpr bool k_details_pinned_default = true;
static constexpr int  timer_interval = 100; // 100 ms

using namespace Utils;

namespace Core {
namespace Internal {

/*!
    \class Core::ProgressManager
    \inheaderfile coreplugin/progressmanager/progressmanager.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The ProgressManager class is used to show a user interface
    for running tasks in Qt Creator.

    The progress manager tracks the progress of a task that it is told
    about, and shows a progress indicator in the lower right corner
    of Qt Creator's main window to the user.
    The progress indicator also allows the user to cancel the task.

    You get the single instance of this class via the
    ProgressManager::instance() function.

    \section1 Registering a task
    The ProgressManager API uses QtConcurrent as the basis for defining
    tasks. A task consists of the following properties:

    \table
    \header
        \li Property
        \li Type
        \li Description
    \row
        \li Task abstraction
        \li \c QFuture<void>
        \li A \l QFuture object that represents the task which is
           responsible for reporting the state of the task. See below
           for coding patterns how to create this object for your
           specific task.
    \row
        \li Title
        \li \c QString
        \li A very short title describing your task. This is shown
           as a title over the progress bar.
    \row
        \li Type
        \li \c QString
        \li A string identifier that is used to group different tasks that
           belong together.
           For example, all the search operations use the same type
           identifier.
    \row
        \li Flags
        \li \l ProgressManager::ProgressFlags
        \li Additional flags that specify how the progress bar should
           be presented to the user.
    \endtable

    To register a task you create your \c QFuture<void> object, and call
    addTask(). This function returns a
    \l{Core::FutureProgress}{FutureProgress}
    object that you can use to further customize the progress bar's appearance.
    See the \l{Core::FutureProgress}{FutureProgress} documentation for
    details.

    In the following you will learn about two common patterns how to
    create the \c QFuture<void> object for your task.

    \section2 Create a threaded task with QtConcurrent
    The first option is to directly use QtConcurrent to actually
    start a task concurrently in a different thread.
    QtConcurrent has several different functions to run e.g.
    a class function in a different thread. Qt Creator itself
    adds a few more in \c{src/libs/qtconcurrent/runextensions.h}.
    The QtConcurrent functions to run a concurrent task return a
    \c QFuture object. This is what you want to give the
    ProgressManager in the addTask() function.

    Have a look at e.g Core::ILocatorFilter. Locator filters implement
    a function \c refresh() which takes a \c QFutureInterface object
    as a parameter. These functions look something like:
    \code
    void Filter::refresh(QFutureInterface<void> &future) {
        future.setProgressRange(0, MAX);
        ...
        while (!future.isCanceled()) {
            // Do a part of the long stuff
            ...
            future.setProgressValue(currentProgress);
            ...
        }
    }
    \endcode

    The actual refresh, which calls all the filters' refresh functions
    in a different thread, looks like this:
    \code
    QFuture<void> task = Utils::map(filters, &ILocatorFilter::refresh);
    Core::FutureProgress *progress = Core::ProgressManager::addTask(task, tr("Indexing"),
                                                                    Locator::Constants::TASK_INDEX);
    \endcode
    First, we to start an asynchronous operation which calls all the filters'
    refresh function. After that we register the returned QFuture object
    with the ProgressManager.

    \section2 Manually create QtConcurrent objects for your thread
    If your task has its own means to create and run a thread,
    you need to create the necessary objects yourselves, and
    report the start/stop state.

    \code
    // We are already running in a different thread here
    QFutureInterface<void> *progressObject = new QFutureInterface<void>;
    progressObject->setProgressRange(0, MAX);
    Core::ProgressManager::addTask(progressObject->future(), tr("DoIt"), MYTASKTYPE);
    progressObject->reportStarted();
    // Do something
    ...
    progressObject->setProgressValue(currentProgress);
    ...
    // We have done what we needed to do
    progressObject->reportFinished();
    delete progressObject;
    \endcode
    In the first line we create the QFutureInterface object that will be
    our way for reporting the task's state.
    The first thing we report is the expected range of the progress values.
    We register the task with the ProgressManager, using the internal
    QFuture object that has been created for our QFutureInterface object.
    Next we report that the task has begun and start doing our actual
    work, regularly reporting the progress via the functions
    in QFutureInterface. After the long taking operation has finished,
    we report so through the QFutureInterface object, and delete it
    afterwards.

    \section1 Customizing progress appearance

    You can set a custom widget to show below the progress bar itself,
    using the FutureProgress object returned by the addTask() function.
    Also use this object to get notified when the user clicks on the
    progress indicator.
*/

/*!
    \enum Core::ProgressManager::ProgressFlag
    Additional flags that specify details in behavior. The
    default for a task is to not have any of these flags set.
    \value KeepOnFinish
        The progress indicator stays visible after the task has finished.
    \value ShowInApplicationIcon
        The progress indicator for this task is additionally
        shown in the application icon in the system's task bar or dock, on
        platforms that support that (at the moment Windows 7 and Mac OS X).
*/

/*!
    \fn void Core::ProgressManager::taskStarted(Utils::Id type)

    Sent whenever a task of a given \a type is started.
*/

/*!
    \fn void Core::ProgressManager::allTasksFinished(Utils::Id type)

    Sent when all tasks of a \a type have finished.
*/

static ProgressManagerPrivate *m_instance = nullptr;

ProgressManagerPrivate::ProgressManagerPrivate() : m_opacityEffect(new QGraphicsOpacityEffect(this))
{
  m_opacityEffect->setOpacity(.999);
  m_instance = this;
  m_progressView = new ProgressView;
  // withDelay, so the statusBarWidget has the chance to get the enter event
  connect(m_progressView.data(), &ProgressView::hoveredChanged, this, &ProgressManagerPrivate::updateVisibilityWithDelay);
  connect(ICore::instance(), &ICore::coreAboutToClose, this, &ProgressManagerPrivate::cancelAllRunningTasks);
}

ProgressManagerPrivate::~ProgressManagerPrivate()
{
  stopFadeOfSummaryProgress();
  qDeleteAll(m_taskList);
  m_taskList.clear();
  StatusBarManager::destroyStatusBarWidget(m_statusBarWidget);
  m_statusBarWidget = nullptr;
  cleanup();
  m_instance = nullptr;
}

auto ProgressManagerPrivate::readSettings() -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(k_settings_group);
  m_progressViewPinned = settings->value(k_details_pinned, k_details_pinned_default).toBool();
  settings->endGroup();
}

auto ProgressManagerPrivate::init() -> void
{
  readSettings();

  m_statusBarWidget = new QWidget;
  m_statusBarWidget->setObjectName("ProgressInfo"); // used for UI introduction
  const auto layout = new QHBoxLayout(m_statusBarWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  m_statusBarWidget->setLayout(layout);

  m_summaryProgressWidget = new QWidget(m_statusBarWidget);
  m_summaryProgressWidget->setVisible(!m_progressViewPinned);
  m_summaryProgressWidget->setGraphicsEffect(m_opacityEffect);

  const auto summary_progress_layout = new QHBoxLayout(m_summaryProgressWidget);
  summary_progress_layout->setContentsMargins(0, 0, 0, 2);
  summary_progress_layout->setSpacing(0);
  m_summaryProgressWidget->setLayout(summary_progress_layout);

  m_statusDetailsWidgetContainer = new QWidget(m_summaryProgressWidget);
  m_statusDetailsWidgetLayout = new QHBoxLayout(m_statusDetailsWidgetContainer);
  m_statusDetailsWidgetLayout->setContentsMargins(0, 0, 0, 0);
  m_statusDetailsWidgetLayout->setSpacing(0);
  m_statusDetailsWidgetLayout->addStretch(1);
  m_statusDetailsWidgetContainer->setLayout(m_statusDetailsWidgetLayout);

  summary_progress_layout->addWidget(m_statusDetailsWidgetContainer);
  m_summaryProgressBar = new ProgressBar(m_summaryProgressWidget);
  m_summaryProgressBar->setMinimumWidth(70);
  m_summaryProgressBar->setTitleVisible(false);
  m_summaryProgressBar->setSeparatorVisible(false);
  m_summaryProgressBar->setCancelEnabled(false);
  summary_progress_layout->addWidget(m_summaryProgressBar);

  layout->addWidget(m_summaryProgressWidget);

  const auto toggle_button = new QToolButton(m_statusBarWidget);
  layout->addWidget(toggle_button);

  m_statusBarWidget->installEventFilter(this);
  StatusBarManager::addStatusBarWidget(m_statusBarWidget, StatusBarManager::RightCorner);

  const auto toggle_progress_view = new QAction(tr("Toggle Progress Details"), this);
  toggle_progress_view->setCheckable(true);
  toggle_progress_view->setChecked(m_progressViewPinned);
  toggle_progress_view->setIcon(Utils::Icons::TOGGLE_PROGRESSDETAILS_TOOLBAR.icon());

  const auto cmd = ActionManager::registerAction(toggle_progress_view, "Orca.ToggleProgressDetails");
  connect(toggle_progress_view, &QAction::toggled, this, &ProgressManagerPrivate::progressDetailsToggled);

  toggle_button->setDefaultAction(cmd->action());
  m_progressView->setReferenceWidget(toggle_button);

  updateVisibility();
  initInternal();
}

auto ProgressManagerPrivate::doCancelTasks(const Id type) -> void
{
  auto found = false;
  auto task = m_runningTasks.begin();

  while (task != m_runningTasks.end()) {
    if (task.value() != type) {
      ++task;
      continue;
    }

    found = true;
    disconnect(task.key(), &QFutureWatcherBase::finished, this, &ProgressManagerPrivate::taskFinished);

    if (m_applicationTask == task.key())
      disconnectApplicationTask();

    task.key()->cancel();
    delete task.key();
    task = m_runningTasks.erase(task);
  }

  if (found) {
    updateSummaryProgressBar();
    emit allTasksFinished(type);
  }
}

auto ProgressManagerPrivate::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == m_statusBarWidget && event->type() == QEvent::Enter) {
    m_hovered = true;
    updateVisibility();
  } else if (obj == m_statusBarWidget && event->type() == QEvent::Leave) {
    m_hovered = false;
    // give the progress view the chance to get the mouse enter event
    updateVisibilityWithDelay();
  } else if (obj == m_statusBarWidget && event->type() == QEvent::MouseButtonPress && !m_taskList.isEmpty()) {
    if (const auto me = dynamic_cast<QMouseEvent*>(event); me->button() == Qt::LeftButton && !me->modifiers()) {
      FutureProgress *progress = m_currentStatusDetailsProgress;

      if (!progress)
        progress = m_taskList.last();

      // don't send signal directly from an event filter, event filters should
      // do as little a possible
      QMetaObject::invokeMethod(progress, &FutureProgress::clicked, Qt::QueuedConnection);
      event->accept();
      return true;
    }
  }

  return false;
}

auto ProgressManagerPrivate::cancelAllRunningTasks() -> void
{
  auto task = m_runningTasks.constBegin();

  while (task != m_runningTasks.constEnd()) {
    disconnect(task.key(), &QFutureWatcherBase::finished, this, &ProgressManagerPrivate::taskFinished);

    if (m_applicationTask == task.key())
      disconnectApplicationTask();

    task.key()->cancel();
    delete task.key();
    ++task;
  }

  m_runningTasks.clear();
  updateSummaryProgressBar();
}

auto ProgressManagerPrivate::doAddTask(const QFuture<void> &future, const QString &title, const Id type, const ProgressFlags flags) -> FutureProgress*
{
  // watch
  const auto watcher = new QFutureWatcher<void>();
  m_runningTasks.insert(watcher, type);

  connect(watcher, &QFutureWatcherBase::progressRangeChanged, this, &ProgressManagerPrivate::updateSummaryProgressBar);
  connect(watcher, &QFutureWatcherBase::progressValueChanged, this, &ProgressManagerPrivate::updateSummaryProgressBar);
  connect(watcher, &QFutureWatcherBase::finished, this, &ProgressManagerPrivate::taskFinished);

  // handle application task
  if (flags & ShowInApplicationIcon) {
    if (m_applicationTask)
      disconnectApplicationTask();

    m_applicationTask = watcher;

    setApplicationProgressRange(future.progressMinimum(), future.progressMaximum());
    setApplicationProgressValue(future.progressValue());
    connect(m_applicationTask, &QFutureWatcherBase::progressRangeChanged, this, &ProgressManagerPrivate::setApplicationProgressRange);
    connect(m_applicationTask, &QFutureWatcherBase::progressValueChanged, this, &ProgressManagerPrivate::setApplicationProgressValue);
    setApplicationProgressVisible(true);
  }

  watcher->setFuture(future);

  // create FutureProgress and manage task list
  removeOldTasks(type);

  if (m_taskList.size() == 10)
    removeOneOldTask();

  const auto progress = new FutureProgress;
  progress->setTitle(title);
  progress->setFuture(future);

  m_progressView->addProgressWidget(progress);
  m_taskList.append(progress);

  progress->setType(type);
  if (flags.testFlag(ProgressManager::KeepOnFinish))
    progress->setKeepOnFinish(FutureProgress::KeepOnFinishTillUserInteraction);
  else
    progress->setKeepOnFinish(FutureProgress::HideOnFinish);

  connect(progress, &FutureProgress::hasErrorChanged, this, &ProgressManagerPrivate::updateSummaryProgressBar);
  connect(progress, &FutureProgress::removeMe, this, &ProgressManagerPrivate::slotRemoveTask);
  connect(progress, &FutureProgress::fadeStarted, this, &ProgressManagerPrivate::updateSummaryProgressBar);
  connect(progress, &FutureProgress::statusBarWidgetChanged, this, &ProgressManagerPrivate::updateStatusDetailsWidget);
  connect(progress, &FutureProgress::subtitleInStatusBarChanged, this, &ProgressManagerPrivate::updateStatusDetailsWidget);
  updateStatusDetailsWidget();

  emit taskStarted(type);
  return progress;
}

auto ProgressManagerPrivate::progressView() -> ProgressView*
{
  return m_progressView;
}

auto ProgressManagerPrivate::taskFinished() -> void
{
  const auto task_object = sender();
  QTC_ASSERT(task_object, return);
  const auto task = dynamic_cast<QFutureWatcher<void>*>(task_object);

  if (m_applicationTask == task)
    disconnectApplicationTask();

  const auto type = m_runningTasks.value(task);
  m_runningTasks.remove(task);
  delete task;
  updateSummaryProgressBar();

  if (!m_runningTasks.key(type, nullptr))
    emit allTasksFinished(type);
}

auto ProgressManagerPrivate::disconnectApplicationTask() -> void
{
  disconnect(m_applicationTask, &QFutureWatcherBase::progressRangeChanged, this, &ProgressManagerPrivate::setApplicationProgressRange);
  disconnect(m_applicationTask, &QFutureWatcherBase::progressValueChanged, this, &ProgressManagerPrivate::setApplicationProgressValue);
  setApplicationProgressVisible(false);
  m_applicationTask = nullptr;
}

auto ProgressManagerPrivate::updateSummaryProgressBar() -> void
{
  m_summaryProgressBar->setError(hasError());
  updateVisibility();

  if (m_runningTasks.isEmpty()) {
    m_summaryProgressBar->setFinished(true);
    if (m_taskList.isEmpty() || isLastFading())
      fadeAwaySummaryProgress();
    return;
  }

  stopFadeOfSummaryProgress();

  m_summaryProgressBar->setFinished(false);
  static constexpr auto task_range = 100;
  auto value = 0;

  for (auto it = m_runningTasks.cbegin(), end = m_runningTasks.cend(); it != end; ++it) {
    const auto watcher = it.key();
    const auto min = watcher->progressMinimum();

    if (const auto range = watcher->progressMaximum() - min; range > 0)
      value += task_range * (watcher->progressValue() - min) / range;
  }

  m_summaryProgressBar->setRange(0, task_range * static_cast<int>(m_runningTasks.size()));
  m_summaryProgressBar->setValue(value);
}

auto ProgressManagerPrivate::fadeAwaySummaryProgress() -> void
{
  stopFadeOfSummaryProgress();
  m_opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity");
  m_opacityAnimation->setDuration(StyleHelper::progressFadeAnimationDuration);
  m_opacityAnimation->setEndValue(0.);
  connect(m_opacityAnimation.data(), &QAbstractAnimation::finished, this, &ProgressManagerPrivate::summaryProgressFinishedFading);
  m_opacityAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

auto ProgressManagerPrivate::stopFadeOfSummaryProgress() -> void
{
  if (m_opacityAnimation) {
    m_opacityAnimation->stop();
    m_opacityEffect->setOpacity(.999);
    delete m_opacityAnimation;
  }
}

auto ProgressManagerPrivate::hasError() const -> bool
{
 for (const FutureProgress *progress : qAsConst(m_taskList))
        if (progress->hasError())
            return true;

  return false;
}

auto ProgressManagerPrivate::isLastFading() const -> bool
{
  if (m_taskList.isEmpty())
    return false;

  for(const auto progress: m_taskList) {
    if (!progress->isFading()) // we still have progress bars that are not fading
      return false;
  }

  return true;
}

auto ProgressManagerPrivate::slotRemoveTask() -> void
{
  const auto progress = qobject_cast<FutureProgress*>(sender());
  QTC_ASSERT(progress, return);
  const auto type = progress->type();
  removeTask(progress);
  removeOldTasks(type, true);
}

auto ProgressManagerPrivate::removeOldTasks(const Id type, const bool keep_one) -> void
{
  auto first_found = !keep_one; // start with false if we want to keep one
  auto i = m_taskList.end();

  while (i != m_taskList.begin()) {
    --i;
    if ((*i)->type() == type) {
      if (first_found && ((*i)->future().isFinished() || (*i)->future().isCanceled())) {
        deleteTask(*i);
        i = m_taskList.erase(i);
      }
      first_found = true;
    }
  }

  updateSummaryProgressBar();
  updateStatusDetailsWidget();
}

auto ProgressManagerPrivate::removeOneOldTask() -> void
{
  if (m_taskList.isEmpty())
    return;

  // look for oldest ended process
  for (auto i = m_taskList.begin(); i != m_taskList.end(); ++i) {
    if ((*i)->future().isFinished()) {
      deleteTask(*i);
      i = m_taskList.erase(i);
      return;
    }
  }

  // no ended process, look for a task type with multiple running tasks and remove the oldest one
  for (auto i = m_taskList.begin(); i != m_taskList.end(); ++i) {
    const auto type = (*i)->type();

    auto task_count = 0;
    foreach(FutureProgress *p, m_taskList) if (p->type() == type)
      ++task_count;

    if (task_count > 1) {
      // don't care for optimizations it's only a handful of entries
      deleteTask(*i);
      i = m_taskList.erase(i);
      return;
    }
  }

  // no ended process, no type with multiple processes, just remove the oldest task
  const auto task = m_taskList.takeFirst();
  deleteTask(task);
  updateSummaryProgressBar();
  updateStatusDetailsWidget();
}

auto ProgressManagerPrivate::removeTask(FutureProgress *task) -> void
{
  m_taskList.removeAll(task);
  deleteTask(task);
  updateSummaryProgressBar();
  updateStatusDetailsWidget();
}

auto ProgressManagerPrivate::deleteTask(FutureProgress *progress) const -> void
{
  m_progressView->removeProgressWidget(progress);
  progress->hide();
  progress->deleteLater();
}

auto ProgressManagerPrivate::updateVisibility() const -> void
{
  m_progressView->setVisible(m_progressViewPinned || m_hovered || m_progressView->isHovered());
  m_summaryProgressWidget->setVisible((!m_runningTasks.isEmpty() || !m_taskList.isEmpty()) && !m_progressViewPinned);
}

auto ProgressManagerPrivate::updateVisibilityWithDelay() -> void
{
  QTimer::singleShot(150, this, &ProgressManagerPrivate::updateVisibility);
}

constexpr int raster = 20;

auto ProgressManagerPrivate::updateStatusDetailsWidget() -> void
{
  QWidget *candidate_widget = nullptr;
  // get newest progress with a status bar widget
  auto i = m_taskList.end();

  while (i != m_taskList.begin()) {
    --i;
    const auto progress = *i;
    candidate_widget = progress->statusBarWidget();

    if (candidate_widget) {
      m_currentStatusDetailsProgress = progress;
      break;
    }

    if (progress->isSubtitleVisibleInStatusBar() && !progress->subtitle().isEmpty()) {
      if (!m_statusDetailsLabel) {
        m_statusDetailsLabel = new QLabel(m_summaryProgressWidget);
        auto font(m_statusDetailsLabel->font());
        font.setPointSizeF(StyleHelper::sidebarFontSize());
        font.setBold(true);
        m_statusDetailsLabel->setFont(font);
      }
      m_statusDetailsLabel->setText(progress->subtitle());
      candidate_widget = m_statusDetailsLabel;
      m_currentStatusDetailsProgress = progress;
      break;
    }
  }

  // make size fit on raster, to avoid flickering in status bar
  // because the output pane buttons resize, if the widget changes a lot (like it is the case for
  // the language server indexing)
  if (candidate_widget) {
    const auto preferred_width = candidate_widget->sizeHint().width();
    const auto width = preferred_width + (raster - preferred_width % raster);
    m_statusDetailsWidgetContainer->setFixedWidth(width);
  }

  if (candidate_widget == m_currentStatusDetailsWidget)
    return;

  if (m_currentStatusDetailsWidget) {
    m_currentStatusDetailsWidget->hide();
    m_statusDetailsWidgetLayout->removeWidget(m_currentStatusDetailsWidget);
  }

  if (candidate_widget) {
    m_statusDetailsWidgetLayout->addWidget(candidate_widget);
    candidate_widget->show();
  }

  m_currentStatusDetailsWidget = candidate_widget;
}

auto ProgressManagerPrivate::summaryProgressFinishedFading() const -> void
{
  m_summaryProgressWidget->setVisible(false);
  m_opacityEffect->setOpacity(.999);
}

auto ProgressManagerPrivate::progressDetailsToggled(const bool checked) -> void
{
  m_progressViewPinned = checked;
  updateVisibility();

  const auto settings = ICore::settings();
  settings->beginGroup(k_settings_group);
  settings->setValueWithDefault(k_details_pinned, m_progressViewPinned, k_details_pinned_default);
  settings->endGroup();
}

} // namespace Internal

/*!
    \internal
*/
ProgressManager::ProgressManager() = default;

/*!
    \internal
*/
ProgressManager::~ProgressManager() = default;

/*!
    Returns a single progress manager instance.
*/
auto ProgressManager::instance() -> ProgressManager*
{
  return Internal::m_instance;
}

/*!
    Shows a progress indicator for the task given by the QFuture object
    \a future.

    The progress indicator shows the specified \a title along with the progress
    bar. The \a type of a task will specify a logical grouping with other
    running tasks. Via the \a flags parameter you can e.g. let the progress
    indicator stay visible after the task has finished.

    Returns an object that represents the created progress indicator, which
    can be used to further customize. The FutureProgress object's life is
    managed by the ProgressManager and is guaranteed to live only until
    the next event loop cycle, or until the next call of addTask.

    If you want to use the returned FutureProgress later than directly after
    calling this function, you will need to use protective functions (like
    wrapping the returned object in QPointer and checking for 0 whenever you
    use it).
*/
auto ProgressManager::addTask(const QFuture<void> &future, const QString &title, const Id type, const ProgressFlags flags) -> FutureProgress*
{
  return Internal::m_instance->doAddTask(future, title, type, flags);
}

/*!
    Shows a progress indicator for task given by the QFutureInterface object
    \a futureInterface.
    The progress indicator shows the specified \a title along with the progress bar.
    The progress indicator will increase monotonically with time, at \a expectedSeconds
    it will reach about 80%, and continue to increase with a decreasingly slower rate.

    The \a type of a task will specify a logical grouping with other
    running tasks. Via the \a flags parameter you can e.g. let the
    progress indicator stay visible after the task has finished.

    \sa addTask
*/

auto ProgressManager::addTimedTask(const QFutureInterface<void> &fi, const QString &title, const Id type, const int expected_seconds, const ProgressFlags flags) -> FutureProgress*
{
  auto dummy(fi); // Need mutable to access .future()
  const auto fp = Internal::m_instance->doAddTask(dummy.future(), title, type, flags);
  (void)new ProgressTimer(static_cast<QFutureInterfaceBase>(fi), expected_seconds, fp);
  return fp;
}

/*!
    Shows the given \a text in a platform dependent way in the application
    icon in the system's task bar or dock. This is used to show the number
    of build errors on Windows 7 and \macos.
*/
auto ProgressManager::setApplicationLabel(const QString &text) -> void
{
  Internal::m_instance->doSetApplicationLabel(text);
}

/*!
    Schedules the cancellation of all running tasks of the given \a type.
    The cancellation functionality depends on the running task actually
    checking the \l QFuture::isCanceled property.
*/

auto ProgressManager::cancelTasks(const Id type) -> void
{
  if (Internal::m_instance)
    Internal::m_instance->doCancelTasks(type);
}

ProgressTimer::ProgressTimer(QFutureInterfaceBase future_interface, const int expected_seconds, QObject *parent) : QObject(parent), m_future_interface(std::move(future_interface)), m_expected_time(expected_seconds)
{
  m_future_interface.setProgressRange(0, 100);
  m_future_interface.setProgressValue(0);
  m_timer = new QTimer(this);
  m_timer->setInterval(timer_interval);

  connect(m_timer, &QTimer::timeout, this, &ProgressTimer::handleTimeout);

  m_timer->start();
}

auto ProgressTimer::handleTimeout() -> void
{
  ++m_current_time;

  // This maps expectation to atan(1) to Pi/4 ~= 0.78, i.e. snaps
  // from 78% to 100% when expectations are met at the time the
  // future finishes. That's not bad for a random choice.
  const auto mapped = atan2(static_cast<double>(m_current_time) * timer_interval / 1000.0, static_cast<double>(m_expected_time));
  const auto progress = 100 * 2 * mapped / 3.14;
  m_future_interface.setProgressValue(static_cast<int>(progress));
}

} // namespace Core
