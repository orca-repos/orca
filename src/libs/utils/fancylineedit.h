// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "completinglineedit.h"

#include <QAbstractButton>

#include <functional>

QT_BEGIN_NAMESPACE
class QEvent;
class QKeySequence;
QT_END_NAMESPACE

namespace Utils {

class FancyLineEditPrivate;

class ORCA_UTILS_EXPORT IconButton : public QAbstractButton {
  Q_OBJECT
  Q_PROPERTY(float iconOpacity READ iconOpacity WRITE setIconOpacity)
  Q_PROPERTY(bool autoHide READ hasAutoHide WRITE setAutoHide)

public:
  explicit IconButton(QWidget *parent = nullptr);
  auto paintEvent(QPaintEvent *event) -> void override;
  auto iconOpacity() -> float { return m_iconOpacity; }

  auto setIconOpacity(float value) -> void
  {
    m_iconOpacity = value;
    update();
  }

  auto animateShow(bool visible) -> void;
  auto setAutoHide(bool hide) -> void { m_autoHide = hide; }
  auto hasAutoHide() const -> bool { return m_autoHide; }
  auto sizeHint() const -> QSize override;

protected:
  auto keyPressEvent(QKeyEvent *ke) -> void override;
  auto keyReleaseEvent(QKeyEvent *ke) -> void override;

private:
  float m_iconOpacity;
  bool m_autoHide;
  QIcon m_icon;
};

class ORCA_UTILS_EXPORT FancyLineEdit : public CompletingLineEdit {
  Q_OBJECT

public:
  enum Side {
    Left = 0,
    Right = 1
  };

  Q_ENUM(Side)

  explicit FancyLineEdit(QWidget *parent = nullptr);
  ~FancyLineEdit() override;

  auto setTextKeepingActiveCursor(const QString &text) -> void;
  auto buttonIcon(Side side) const -> QIcon;
  auto setButtonIcon(Side side, const QIcon &icon) -> void;
  auto buttonMenu(Side side) const -> QMenu*;
  auto setButtonMenu(Side side, QMenu *menu) -> void;
  auto setButtonVisible(Side side, bool visible) -> void;
  auto isButtonVisible(Side side) const -> bool;
  auto button(Side side) const -> QAbstractButton*;
  auto setButtonToolTip(Side side, const QString &) -> void;
  auto setButtonFocusPolicy(Side side, Qt::FocusPolicy policy) -> void;

  // Set whether tabbing in will trigger the menu.
  auto setMenuTabFocusTrigger(Side side, bool v) -> void;
  auto hasMenuTabFocusTrigger(Side side) const -> bool;

  // Set if icon should be hidden when text is empty
  auto setAutoHideButton(Side side, bool h) -> void;
  auto hasAutoHideButton(Side side) const -> bool;

  // Completion

  // Enable a history completer with a history of entries.
  auto setHistoryCompleter(const QString &historyKey, bool restoreLastItemFromHistory = false) -> void;
  // Sets a completer that is not a history completer.
  auto setSpecialCompleter(QCompleter *completer) -> void;

  // Filtering

  // Enables filtering
  auto setFiltering(bool on) -> void;

  //  Validation

  // line edit, (out)errorMessage -> valid?
  using ValidationFunction = std::function<bool(FancyLineEdit *, QString *)>;

  enum State {
    Invalid,
    DisplayingPlaceholderText,
    Valid
  };

  auto state() const -> State;
  auto isValid() const -> bool;
  auto errorMessage() const -> QString;
  auto setValidationFunction(const ValidationFunction &fn) -> void;
  static auto defaultValidationFunction() -> ValidationFunction;
  auto validate() -> void;
  auto onEditingFinished() -> void;
  static auto setCamelCaseNavigationEnabled(bool enabled) -> void;
  static auto setCompletionShortcut(const QKeySequence &shortcut) -> void;

protected:
  // Custom behaviour can be added here.
  virtual auto handleChanged(const QString &) -> void {}
  auto keyPressEvent(QKeyEvent *event) -> void override;

signals:
  auto buttonClicked(Utils::FancyLineEdit::Side side) -> void;
  auto leftButtonClicked() -> void;
  auto rightButtonClicked() -> void;
  auto filterChanged(const QString &) -> void;
  auto validChanged(bool validState) -> void;
  auto validReturnPressed() -> void;

protected:
  auto resizeEvent(QResizeEvent *e) -> void override;
  virtual auto fixInputString(const QString &string) -> QString;

private:
  auto iconClicked() -> void;

  static auto validateWithValidator(FancyLineEdit *edit, QString *errorMessage) -> bool;
  // Unimplemented, to force the user to make a decision on
  // whether to use setHistoryCompleter() or setSpecialCompleter().
  auto setCompleter(QCompleter *) -> void;
  auto updateMargins() -> void;
  auto updateButtonPositions() -> void;
  auto camelCaseBackward(bool mark) -> bool;
  auto camelCaseForward(bool mark) -> bool;
  friend class FancyLineEditPrivate;

  FancyLineEditPrivate *d;
};

} // namespace Utils
