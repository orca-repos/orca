// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "futureprogress.hpp"
#include "progressbar.hpp"

#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QPainter>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QVBoxLayout>
#include <QMouseEvent>

constexpr int notification_timeout = 8000;
constexpr int short_notification_timeout = 1000;

using namespace Utils;

namespace Core {

class FutureProgressPrivate final : public QObject {
  Q_OBJECT

public:
  explicit FutureProgressPrivate(FutureProgress *q);

  auto fadeAway() -> void;
  auto tryToFadeAway() -> void;

  QFutureWatcher<void> m_watcher;
  Internal::ProgressBar *m_progress;
  QWidget *m_widget;
  QHBoxLayout *m_widget_layout;
  QWidget *m_status_bar_widget;
  Id m_type;
  FutureProgress::KeepOnFinishType m_keep;
  bool m_waiting_for_user_interaction;
  FutureProgress *m_q;
  bool m_fade_starting;
  bool m_is_fading;
  bool m_is_subtitle_visible_in_status_bar = false;
};

FutureProgressPrivate::FutureProgressPrivate(FutureProgress *q) : m_progress(new Internal::ProgressBar), m_widget(nullptr), m_widget_layout(new QHBoxLayout), m_status_bar_widget(nullptr), m_keep(FutureProgress::HideOnFinish), m_waiting_for_user_interaction(false), m_q(q), m_fade_starting(false), m_is_fading(false) {}

/*!
    \ingroup mainclasses
    \inheaderfile coreplugin/progressmanager/futureprogress.h
    \class Core::FutureProgress
    \inmodule Orca

    \brief The FutureProgress class is used to adapt the appearance of
    progress indicators that were created through the ProgressManager class.

    Use the instance of this class that was returned by
    ProgressManager::addTask() to define a widget that
    should be shown below the progress bar, or to change the
    progress title.
    Also use it to react on the event that the user clicks on
    the progress indicator (which can be used to e.g. open a more detailed
    view, or the results of the task).
*/

/*!
    \fn void Core::FutureProgress::clicked()
    Connect to this signal to get informed when the user clicks on the
    progress indicator.
*/

/*!
    \fn void Core::FutureProgress::canceled()
    Connect to this signal to get informed when the operation is canceled.
*/

/*!
    \fn void Core::FutureProgress::finished()
    Another way to get informed when the task has finished.
*/

/*!
    \fn QWidget Core::FutureProgress::widget() const
    Returns the custom widget that is shown below the progress indicator.
*/

/*!
    \internal
*/
FutureProgress::FutureProgress(QWidget *parent) : QWidget(parent), d(new FutureProgressPrivate(this))
{
  const auto layout = new QVBoxLayout;
  setLayout(layout);
  layout->addWidget(d->m_progress);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addLayout(d->m_widget_layout);

  d->m_widget_layout->setContentsMargins(7, 0, 7, 2);
  d->m_widget_layout->setSpacing(0);

  connect(&d->m_watcher, &QFutureWatcherBase::started, this, &FutureProgress::setStarted);
  connect(&d->m_watcher, &QFutureWatcherBase::finished, this, &FutureProgress::setFinished);
  connect(&d->m_watcher, &QFutureWatcherBase::canceled, this, &FutureProgress::canceled);
  connect(&d->m_watcher, &QFutureWatcherBase::progressRangeChanged, this, &FutureProgress::setProgressRange);
  connect(&d->m_watcher, &QFutureWatcherBase::progressValueChanged, this, &FutureProgress::setProgressValue);
  connect(&d->m_watcher, &QFutureWatcherBase::progressTextChanged, this, &FutureProgress::setProgressText);
  connect(d->m_progress, &Internal::ProgressBar::clicked, this, &FutureProgress::cancel);

  setMinimumWidth(100);
  setMaximumWidth(300);
}

/*!
    \internal
*/
FutureProgress::~FutureProgress()
{
  delete d->m_widget;
  delete d;
}

/*!
    Sets the \a widget to show below the progress bar.
    This will be destroyed when the progress indicator is destroyed.
    Default is to show no widget below the progress indicator.
*/
auto FutureProgress::setWidget(QWidget *widget) const -> void
{
  delete d->m_widget;
  auto sp = widget->sizePolicy();
  sp.setHorizontalPolicy(QSizePolicy::Ignored);
  widget->setSizePolicy(sp);
  d->m_widget = widget;

  if (d->m_widget)
    d->m_widget_layout->addWidget(d->m_widget);
}

/*!
    Changes the \a title of the progress indicator.
*/
auto FutureProgress::setTitle(const QString &title) const -> void
{
  d->m_progress->setTitle(title);
}

/*!
    Returns the title of the progress indicator.
*/
auto FutureProgress::title() const -> QString
{
  return d->m_progress->title();
}

auto FutureProgress::setSubtitle(const QString &subtitle) -> void
{
  if (subtitle != d->m_progress->subtitle()) {
    d->m_progress->setSubtitle(subtitle);
    if (d->m_is_subtitle_visible_in_status_bar) emit subtitleInStatusBarChanged();
  }
}

auto FutureProgress::subtitle() const -> QString
{
  return d->m_progress->subtitle();
}

auto FutureProgress::setSubtitleVisibleInStatusBar(const bool visible) -> void
{
  if (visible != d->m_is_subtitle_visible_in_status_bar) {
    d->m_is_subtitle_visible_in_status_bar = visible;
    emit subtitleInStatusBarChanged();
  }
}

auto FutureProgress::isSubtitleVisibleInStatusBar() const -> bool
{
  return d->m_is_subtitle_visible_in_status_bar;
}

auto FutureProgress::cancel() const -> void
{
  d->m_watcher.future().cancel();
}

auto FutureProgress::updateToolTip(const QString &text) -> void
{
  setToolTip(QLatin1String("<b>") + title() + QLatin1String("</b><br>") + text);
}

auto FutureProgress::setStarted() const -> void
{
  d->m_progress->reset();
  d->m_progress->setError(false);
  d->m_progress->setRange(d->m_watcher.progressMinimum(), d->m_watcher.progressMaximum());
  d->m_progress->setValue(d->m_watcher.progressValue());
}

auto FutureProgress::eventFilter(QObject *, QEvent *e) -> bool
{
  if (d->m_keep != KeepOnFinish && d->m_waiting_for_user_interaction && (e->type() == QEvent::MouseMove || e->type() == QEvent::KeyPress)) {
    qApp->removeEventFilter(this);
    QTimer::singleShot(notification_timeout, d, &FutureProgressPrivate::fadeAway);
  }
  return false;
}

auto FutureProgress::setFinished() -> void
{
  updateToolTip(d->m_watcher.future().progressText());
  d->m_progress->setFinished(true);

  if (d->m_watcher.future().isCanceled()) {
    d->m_progress->setError(true);
    emit hasErrorChanged();
  } else {
    d->m_progress->setError(false);
  }

  emit finished();
  d->tryToFadeAway();
}

auto FutureProgressPrivate::tryToFadeAway() -> void
{
  if (m_fade_starting)
    return;

  if (m_keep == FutureProgress::KeepOnFinishTillUserInteraction || (m_keep == FutureProgress::HideOnFinish && m_progress->hasError())) {
    m_waiting_for_user_interaction = true;
    //eventfilter is needed to get user interaction
    //events to start QTimer::singleShot later
    qApp->installEventFilter(m_q);
    m_fade_starting = true;
  } else if (m_keep == FutureProgress::HideOnFinish) {
    QTimer::singleShot(short_notification_timeout, this, &FutureProgressPrivate::fadeAway);
    m_fade_starting = true;
  }
}

auto FutureProgress::setProgressRange(const int min, const int max) const -> void
{
  d->m_progress->setRange(min, max);
}

auto FutureProgress::setProgressValue(const int val) const -> void
{
  d->m_progress->setValue(val);
}

auto FutureProgress::setProgressText(const QString &text) -> void
{
  updateToolTip(text);
}

/*!
    \internal
*/
auto FutureProgress::setFuture(const QFuture<void> &future) const -> void
{
  d->m_watcher.setFuture(future);
}

/*!
    Returns a QFuture object that represents this running task.
*/
auto FutureProgress::future() const -> QFuture<void>
{
  return d->m_watcher.future();
}

/*!
    \internal
*/
auto FutureProgress::mousePressEvent(QMouseEvent *event) -> void
{
  if (event->button() == Qt::LeftButton) emit clicked();
  QWidget::mousePressEvent(event);
}

auto FutureProgress::paintEvent(QPaintEvent *) -> void
{
  QPainter p(this);

  if (orcaTheme()->flag(Theme::FlatToolBars)) {
    p.fillRect(rect(), StyleHelper::baseColor());
  } else {
    const auto grad = StyleHelper::statusBarGradient(rect());
    p.fillRect(rect(), grad);
  }
}

/*!
    Returns the error state of this progress indicator.
*/
auto FutureProgress::hasError() const -> bool
{
  return d->m_progress->hasError();
}

auto FutureProgress::setType(const Id type) const -> void
{
  d->m_type = type;
}

auto FutureProgress::type() const -> Id
{
  return d->m_type;
}

auto FutureProgress::setKeepOnFinish(const KeepOnFinishType keep_type) const -> void
{
  if (d->m_keep == keep_type)
    return;

  d->m_keep = keep_type;

  //if it is not finished tryToFadeAway is called by setFinished at the end
  if (d->m_watcher.isFinished())
    d->tryToFadeAway();
}

auto FutureProgress::keepOnFinish() const -> bool
{
  return d->m_keep;
}

auto FutureProgress::widget() const -> QWidget*
{
  return d->m_widget;
}

auto FutureProgress::setStatusBarWidget(QWidget *widget) -> void
{
  if (widget == d->m_status_bar_widget)
    return;

  delete d->m_status_bar_widget;
  d->m_status_bar_widget = widget;
  emit statusBarWidgetChanged();
}

auto FutureProgress::statusBarWidget() const -> QWidget*
{
  return d->m_status_bar_widget;
}

auto FutureProgress::isFading() const -> bool
{
  return d->m_is_fading;
}

auto FutureProgress::sizeHint() const -> QSize
{
  return {QWidget::sizeHint().width(), minimumHeight()};
}

auto FutureProgressPrivate::fadeAway() -> void
{
  m_is_fading = true;

  const auto opacity_effect = new QGraphicsOpacityEffect;
  opacity_effect->setOpacity(.999);
  m_q->setGraphicsEffect(opacity_effect);

  const auto group = new QSequentialAnimationGroup(this);
  auto animation = new QPropertyAnimation(opacity_effect, "opacity");
  animation->setDuration(StyleHelper::progressFadeAnimationDuration);
  animation->setEndValue(0.);
  group->addAnimation(animation);
  animation = new QPropertyAnimation(m_q, "maximumHeight");
  animation->setDuration(120);
  animation->setEasingCurve(QEasingCurve::InCurve);
  animation->setStartValue(m_q->sizeHint().height());
  animation->setEndValue(0.0);
  group->addAnimation(animation);
  connect(group, &QAbstractAnimation::finished, m_q, &FutureProgress::removeMe);
  group->start(QAbstractAnimation::DeleteWhenStopped);

  emit m_q->fadeStarted();
}

} // namespace Core

#include "futureprogress.moc"
