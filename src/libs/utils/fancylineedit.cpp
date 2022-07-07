// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "camelcasecursor.hpp"
#include "execmenu.hpp"
#include "fancylineedit.hpp"
#include "historycompleter.hpp"
#include "hostosinfo.hpp"
#include "optional.hpp"
#include "qtcassert.hpp"
#include "stylehelper.hpp"
#include "utilsicons.hpp"

#include <QAbstractItemView>
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QShortcut>
#include <QStylePainter>
#include <QPropertyAnimation>
#include <QStyle>
#include <QWindow>

/*!
    \class Utils::FancyLineEdit

    \brief The FancyLineEdit class is an enhanced line edit with several
    opt-in features.

    A FancyLineEdit instance can have:

    \list
    \li An embedded pixmap on one side that is connected to a menu.

    \li A grayed hintText (like "Type Here to")
    when not focused and empty. When connecting to the changed signals and
    querying text, one has to be aware that the text is set to that hint
    text if isShowingHintText() returns true (that is, does not contain
    valid user input).

    \li A history completer.

    \li The ability to validate the contents of the text field by overriding
    virtual \c validate() function in derived clases.
    \endlist

    When invalid, the text color will turn red and a tooltip will
    contain the error message. This approach is less intrusive than a
    QValidator which will prevent the user from entering certain characters.

    A visible hint text results validation to be in state 'DisplayingInitialText',
    which is not valid, but is not marked red.

 */

enum { margin = 6 };

#define ICONBUTTON_HEIGHT 18
#define FADE_TIME 160

namespace Utils {

static bool camelCaseNavigation = false;

class CompletionShortcut : public QObject {
  Q_OBJECT

public:
  auto setKeySequence(const QKeySequence &key) -> void
  {
    if (m_key != key) {
      m_key = key;
      emit keyChanged(key);
    }
  }

  auto key() const -> QKeySequence { return m_key; }

signals:
  auto keyChanged(const QKeySequence &key) -> void;

private:
  QKeySequence m_key = Qt::Key_Space + HostOsInfo::controlModifier();
};

Q_GLOBAL_STATIC(CompletionShortcut, completionShortcut)

class FancyLineEditPrivate : public QObject {
public:
  explicit FancyLineEditPrivate(FancyLineEdit *parent);

  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

  FancyLineEdit *m_lineEdit;
  IconButton *m_iconbutton[2];
  HistoryCompleter *m_historyCompleter = nullptr;
  QShortcut m_completionShortcut;
  FancyLineEdit::ValidationFunction m_validationFunction = &FancyLineEdit::validateWithValidator;
  QString m_oldText;
  QMenu *m_menu[2];
  FancyLineEdit::State m_state = FancyLineEdit::Invalid;
  bool m_menuTabFocusTrigger[2];
  bool m_iconEnabled[2];
  bool m_isFiltering = false;
  bool m_firstChange = true;
  bool m_toolTipSet = false;
  QString m_lastFilterText;
  const QColor m_okTextColor;
  const QColor m_errorTextColor;
  QString m_errorMessage;
};

FancyLineEditPrivate::FancyLineEditPrivate(FancyLineEdit *parent) : QObject(parent), m_lineEdit(parent), m_completionShortcut(completionShortcut()->key(), parent), m_okTextColor(orcaTheme()->color(Theme::TextColorNormal)), m_errorTextColor(orcaTheme()->color(Theme::TextColorError))
{
  m_completionShortcut.setContext(Qt::WidgetShortcut);
  connect(completionShortcut(), &CompletionShortcut::keyChanged, &m_completionShortcut, &QShortcut::setKey);

  for (int i = 0; i < 2; ++i) {
    m_iconbutton[i] = new IconButton(parent);
    m_iconbutton[i]->installEventFilter(this);
    m_iconbutton[i]->hide();
    m_iconbutton[i]->setAutoHide(false);

    m_menu[i] = nullptr;

    m_menuTabFocusTrigger[i] = false;
    m_iconEnabled[i] = false;
  }
}

auto FancyLineEditPrivate::eventFilter(QObject *obj, QEvent *event) -> bool
{
  int buttonIndex = -1;
  for (int i = 0; i < 2; ++i) {
    if (obj == m_iconbutton[i]) {
      buttonIndex = i;
      break;
    }
  }
  if (buttonIndex == -1)
    return QObject::eventFilter(obj, event);
  switch (event->type()) {
  case QEvent::FocusIn:
    if (m_menuTabFocusTrigger[buttonIndex] && m_menu[buttonIndex]) {
      m_lineEdit->setFocus();
      execMenuAtWidget(m_menu[buttonIndex], m_iconbutton[buttonIndex]);
      return true;
    }
  default:
    break;
  }
  return QObject::eventFilter(obj, event);
}

FancyLineEdit::FancyLineEdit(QWidget *parent) : CompletingLineEdit(parent), d(new FancyLineEditPrivate(this))
{
  ensurePolished();
  updateMargins();

  connect(d->m_iconbutton[Left], &QAbstractButton::clicked, this, &FancyLineEdit::iconClicked);
  connect(d->m_iconbutton[Right], &QAbstractButton::clicked, this, &FancyLineEdit::iconClicked);
  connect(this, &QLineEdit::textChanged, this, &FancyLineEdit::validate);
  connect(&d->m_completionShortcut, &QShortcut::activated, this, [this] {
    if (!completer())
      return;
    completer()->setCompletionPrefix(text().left(cursorPosition()));
    completer()->complete();
  });
}

FancyLineEdit::~FancyLineEdit()
{
  if (d->m_historyCompleter) {
    // When dialog with FancyLineEdit widget closed by <Escape>
    // the QueuedConnection don't have enough time to call slot callback
    // because edit widget and all of its connections are destroyed before
    // QCoreApplicationPrivate::sendPostedEvents dispatch our queued signal.
    if (!text().isEmpty())
      d->m_historyCompleter->addEntry(text());
  }
}

auto FancyLineEdit::setTextKeepingActiveCursor(const QString &text) -> void
{
  optional<int> cursor = hasFocus() ? make_optional(cursorPosition()) : nullopt;
  setText(text);
  if (cursor)
    setCursorPosition(*cursor);
}

auto FancyLineEdit::setButtonVisible(Side side, bool visible) -> void
{
  d->m_iconbutton[side]->setVisible(visible);
  d->m_iconEnabled[side] = visible;
  updateMargins();
}

auto FancyLineEdit::isButtonVisible(Side side) const -> bool
{
  return d->m_iconEnabled[side];
}

auto FancyLineEdit::button(FancyLineEdit::Side side) const -> QAbstractButton*
{
  return d->m_iconbutton[side];
}

auto FancyLineEdit::iconClicked() -> void
{
  auto button = qobject_cast<IconButton*>(sender());
  int index = -1;
  for (int i = 0; i < 2; ++i)
    if (d->m_iconbutton[i] == button)
      index = i;
  if (index == -1)
    return;
  if (d->m_menu[index]) {
    execMenuAtWidget(d->m_menu[index], button);
  } else {
    emit buttonClicked((Side)index);
    if (index == Left) emit leftButtonClicked();
    else if (index == Right) emit rightButtonClicked();
  }
}

auto FancyLineEdit::updateMargins() -> void
{
  bool leftToRight = (layoutDirection() == Qt::LeftToRight);
  Side realLeft = (leftToRight ? Left : Right);
  Side realRight = (leftToRight ? Right : Left);

  int leftMargin = d->m_iconbutton[realLeft]->sizeHint().width() + 8;
  int rightMargin = d->m_iconbutton[realRight]->sizeHint().width() + 8;
  // Note KDE does not reserve space for the highlight color
  if (style()->inherits("OxygenStyle")) {
    leftMargin = qMax(24, leftMargin);
    rightMargin = qMax(24, rightMargin);
  }

  QMargins margins((d->m_iconEnabled[realLeft] ? leftMargin : 0), 0, (d->m_iconEnabled[realRight] ? rightMargin : 0), 0);

  setTextMargins(margins);
}

auto FancyLineEdit::updateButtonPositions() -> void
{
  QRect contentRect = rect();
  for (int i = 0; i < 2; ++i) {
    Side iconpos = Side(i);
    if (layoutDirection() == Qt::RightToLeft)
      iconpos = (iconpos == Left ? Right : Left);

    if (iconpos == FancyLineEdit::Right) {
      const int iconoffset = textMargins().right() + 4;
      d->m_iconbutton[i]->setGeometry(contentRect.adjusted(width() - iconoffset, 0, 0, 0));
    } else {
      const int iconoffset = textMargins().left() + 4;
      d->m_iconbutton[i]->setGeometry(contentRect.adjusted(0, 0, -width() + iconoffset, 0));
    }
  }
}

auto FancyLineEdit::resizeEvent(QResizeEvent *) -> void
{
  updateButtonPositions();
}

auto FancyLineEdit::setButtonIcon(Side side, const QIcon &icon) -> void
{
  d->m_iconbutton[side]->setIcon(icon);
  updateMargins();
  updateButtonPositions();
  update();
}

auto FancyLineEdit::buttonIcon(Side side) const -> QIcon
{
  return d->m_iconbutton[side]->icon();
}

auto FancyLineEdit::setButtonMenu(Side side, QMenu *buttonMenu) -> void
{
  d->m_menu[side] = buttonMenu;
  d->m_iconbutton[side]->setIconOpacity(1.0);
}

auto FancyLineEdit::buttonMenu(Side side) const -> QMenu*
{
  return d->m_menu[side];
}

auto FancyLineEdit::hasMenuTabFocusTrigger(Side side) const -> bool
{
  return d->m_menuTabFocusTrigger[side];
}

auto FancyLineEdit::setMenuTabFocusTrigger(Side side, bool v) -> void
{
  if (d->m_menuTabFocusTrigger[side] == v)
    return;

  d->m_menuTabFocusTrigger[side] = v;
  d->m_iconbutton[side]->setFocusPolicy(v ? Qt::TabFocus : Qt::NoFocus);
}

auto FancyLineEdit::hasAutoHideButton(Side side) const -> bool
{
  return d->m_iconbutton[side]->hasAutoHide();
}

auto FancyLineEdit::setHistoryCompleter(const QString &historyKey, bool restoreLastItemFromHistory) -> void
{
  QTC_ASSERT(!d->m_historyCompleter, return);
  d->m_historyCompleter = new HistoryCompleter(historyKey, this);
  if (restoreLastItemFromHistory && d->m_historyCompleter->hasHistory())
    setText(d->m_historyCompleter->historyItem());
  QLineEdit::setCompleter(d->m_historyCompleter);

  // Hitting <Return> in the popup first causes editingFinished()
  // being emitted and more updates finally calling setText() (again).
  // To make sure we report the "final" content delay the addEntry()
  // "a bit".
  connect(this, &QLineEdit::editingFinished, this, &FancyLineEdit::onEditingFinished, Qt::QueuedConnection);
}

auto FancyLineEdit::onEditingFinished() -> void
{
  d->m_historyCompleter->addEntry(text());
}

auto FancyLineEdit::keyPressEvent(QKeyEvent *event) -> void
{
  if (camelCaseNavigation) {
    if (event == QKeySequence::MoveToPreviousWord)
      CamelCaseCursor::left(this, QTextCursor::MoveAnchor);
    else if (event == QKeySequence::SelectPreviousWord)
      CamelCaseCursor::left(this, QTextCursor::KeepAnchor);
    else if (event == QKeySequence::MoveToNextWord)
      CamelCaseCursor::right(this, QTextCursor::MoveAnchor);
    else if (event == QKeySequence::SelectNextWord)
      CamelCaseCursor::right(this, QTextCursor::KeepAnchor);
    else
      CompletingLineEdit::keyPressEvent(event);
  } else {
    CompletingLineEdit::keyPressEvent(event);
  }
}

auto FancyLineEdit::setCamelCaseNavigationEnabled(bool enabled) -> void
{
  camelCaseNavigation = enabled;
}

auto FancyLineEdit::setCompletionShortcut(const QKeySequence &shortcut) -> void
{
  completionShortcut()->setKeySequence(shortcut);
}

auto FancyLineEdit::setSpecialCompleter(QCompleter *completer) -> void
{
  QTC_ASSERT(!d->m_historyCompleter, return);
  QLineEdit::setCompleter(completer);
}

auto FancyLineEdit::setAutoHideButton(Side side, bool h) -> void
{
  d->m_iconbutton[side]->setAutoHide(h);
  if (h)
    d->m_iconbutton[side]->setIconOpacity(text().isEmpty() ? 0.0 : 1.0);
  else
    d->m_iconbutton[side]->setIconOpacity(1.0);
}

auto FancyLineEdit::setButtonToolTip(Side side, const QString &tip) -> void
{
  d->m_iconbutton[side]->setToolTip(tip);
}

auto FancyLineEdit::setButtonFocusPolicy(Side side, Qt::FocusPolicy policy) -> void
{
  d->m_iconbutton[side]->setFocusPolicy(policy);
}

auto FancyLineEdit::setFiltering(bool on) -> void
{
  if (on == d->m_isFiltering)
    return;

  d->m_isFiltering = on;
  if (on) {
    d->m_lastFilterText = text();
    // KDE has custom icons for this. Notice that icon namings are counter intuitive.
    // If these icons are not available we use the freedesktop standard name before
    // falling back to a bundled resource.
    QIcon icon = QIcon::fromTheme(layoutDirection() == Qt::LeftToRight ? QLatin1String("edit-clear-locationbar-rtl") : QLatin1String("edit-clear-locationbar-ltr"), QIcon::fromTheme(QLatin1String("edit-clear"), Icons::EDIT_CLEAR.icon()));

    setButtonIcon(Right, icon);
    setButtonVisible(Right, true);
    setPlaceholderText(tr("Filter"));
    setButtonToolTip(Right, tr("Clear text"));
    setAutoHideButton(Right, true);
    connect(this, &FancyLineEdit::rightButtonClicked, this, &QLineEdit::clear);
  } else {
    disconnect(this, &FancyLineEdit::rightButtonClicked, this, &QLineEdit::clear);
  }
}

auto FancyLineEdit::setValidationFunction(const FancyLineEdit::ValidationFunction &fn) -> void
{
  d->m_validationFunction = fn;
  validate();
}

auto FancyLineEdit::defaultValidationFunction() -> FancyLineEdit::ValidationFunction
{
  return &FancyLineEdit::validateWithValidator;
}

auto FancyLineEdit::validateWithValidator(FancyLineEdit *edit, QString *errorMessage) -> bool
{
  Q_UNUSED(errorMessage)
  if (const QValidator *v = edit->validator()) {
    QString tmp = edit->text();
    int pos = edit->cursorPosition();
    return v->validate(tmp, pos) == QValidator::Acceptable;
  }
  return true;
}

auto FancyLineEdit::state() const -> FancyLineEdit::State
{
  return d->m_state;
}

auto FancyLineEdit::isValid() const -> bool
{
  return d->m_state == Valid;
}

auto FancyLineEdit::errorMessage() const -> QString
{
  return d->m_errorMessage;
}

auto FancyLineEdit::validate() -> void
{
  const QString t = text();

  if (d->m_isFiltering) {
    if (t != d->m_lastFilterText) {
      d->m_lastFilterText = t;
      emit filterChanged(t);
    }
  }

  d->m_errorMessage.clear();
  // Are we displaying the placeholder text?
  const bool isDisplayingPlaceholderText = !placeholderText().isEmpty() && t.isEmpty();
  const bool validates = d->m_validationFunction(this, &d->m_errorMessage);
  const State newState = isDisplayingPlaceholderText ? DisplayingPlaceholderText : (validates ? Valid : Invalid);
  if (!validates || d->m_toolTipSet) {
    setToolTip(d->m_errorMessage);
    d->m_toolTipSet = true;
  }
  // Changed..figure out if valid changed. DisplayingPlaceholderText is not valid,
  // but should not show error color. Also trigger on the first change.
  if (newState != d->m_state || d->m_firstChange) {
    const bool validHasChanged = (d->m_state == Valid) != (newState == Valid);
    d->m_state = newState;
    d->m_firstChange = false;

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Text, newState == Invalid ? d->m_errorTextColor : d->m_okTextColor);
    setPalette(p);

    if (validHasChanged) emit validChanged(newState == Valid);
  }
  const QString fixedString = fixInputString(t);
  if (t != fixedString) {
    const int cursorPos = cursorPosition();
    QSignalBlocker blocker(this);
    setText(fixedString);
    setCursorPosition(qMin(cursorPos, fixedString.length()));
  }

  // Check buttons.
  if (d->m_oldText.isEmpty() || t.isEmpty()) {
    for (auto &button : qAsConst(d->m_iconbutton)) {
      if (button->hasAutoHide())
        button->animateShow(!t.isEmpty());
    }
    d->m_oldText = t;
  }

  handleChanged(t);
}

auto FancyLineEdit::fixInputString(const QString &string) -> QString
{
  return string;
}

//
// IconButton - helper class to represent a clickable icon
//

IconButton::IconButton(QWidget *parent) : QAbstractButton(parent), m_autoHide(false)
{
  setCursor(Qt::ArrowCursor);
  setFocusPolicy(Qt::NoFocus);
}

auto IconButton::paintEvent(QPaintEvent *) -> void
{
  QWindow *window = this->window()->windowHandle();
  const QPixmap iconPixmap = icon().pixmap(window, sizeHint(), isEnabled() ? QIcon::Normal : QIcon::Disabled);
  QStylePainter painter(this);
  QRect pixmapRect(QPoint(), iconPixmap.size() / window->devicePixelRatio());
  pixmapRect.moveCenter(rect().center());

  if (m_autoHide)
    painter.setOpacity(m_iconOpacity);

  painter.drawPixmap(pixmapRect, iconPixmap);

  if (hasFocus()) {
    QStyleOptionFocusRect focusOption;
    focusOption.initFrom(this);
    focusOption.rect = pixmapRect;
    if (HostOsInfo::isMacHost()) {
      focusOption.rect.adjust(-4, -4, 4, 4);
      painter.drawControl(QStyle::CE_FocusFrame, focusOption);
    } else {
      painter.drawPrimitive(QStyle::PE_FrameFocusRect, focusOption);
    }
  }
}

auto IconButton::animateShow(bool visible) -> void
{
  QPropertyAnimation *animation = new QPropertyAnimation(this, "iconOpacity");
  animation->setDuration(FADE_TIME);
  animation->setEndValue(visible ? 1.0 : 0.0);
  animation->start(QAbstractAnimation::DeleteWhenStopped);
}

auto IconButton::sizeHint() const -> QSize
{
  QWindow *window = this->window()->windowHandle();
  return icon().actualSize(window, QSize(32, 16)); // Find flags icon can be wider than 16px
}

auto IconButton::keyPressEvent(QKeyEvent *ke) -> void
{
  QAbstractButton::keyPressEvent(ke);
  if (!ke->modifiers() && (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return))
    click();
  // do not forward to line edit
  ke->accept();
}

auto IconButton::keyReleaseEvent(QKeyEvent *ke) -> void
{
  QAbstractButton::keyReleaseEvent(ke);
  // do not forward to line edit
  ke->accept();
}

} // namespace Utils

#include <fancylineedit.moc>
