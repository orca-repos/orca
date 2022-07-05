// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.h>

#include <QToolButton>

QT_BEGIN_NAMESPACE
class QAction;
class QLabel;
class QStackedWidget;
class QTimeLine;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

class MainWindow;
class OutputPaneToggleButton;
class OutputPaneManageButton;

class OutputPaneManager : public QWidget {
  Q_OBJECT

public:
  static auto instance() -> OutputPaneManager*;
  auto updateStatusButtons(bool visible) const -> void;
  static auto updateMaximizeButton(bool maximized) -> void;
  static auto outputPaneHeightSetting() -> int;
  static auto setOutputPaneHeightSetting(int value) -> void;

public slots:
  auto slotHide() const -> void;
  auto slotNext() -> void;
  auto slotPrev() -> void;
  static auto toggleMaximized() -> void;

protected:
  auto focusInEvent(QFocusEvent *e) -> void override;

private:
  // the only class that is allowed to create and destroy
  friend class MainWindow;
  friend class OutputPaneManageButton;

  static auto create() -> void;
  static auto initialize() -> void;
  static auto destroy() -> void;

  explicit OutputPaneManager(QWidget *parent = nullptr);
  ~OutputPaneManager() override;

  auto shortcutTriggered(int idx) -> void;
  auto clearPage() const -> void;
  auto popupMenu() -> void;
  auto saveSettings() const -> void;
  auto showPage(int idx, int flags) -> void;
  auto ensurePageVisible(int idx) -> void;
  auto currentIndex() const -> int;
  auto setCurrentIndex(int idx) const -> void;
  auto buttonTriggered(int idx) -> void;
  auto readSettings() -> void;

  QLabel *m_titleLabel = nullptr;
  OutputPaneManageButton *m_manageButton = nullptr;
  QAction *m_clearAction = nullptr;
  QToolButton *m_clearButton = nullptr;
  QToolButton *m_closeButton = nullptr;
  QAction *m_minMaxAction = nullptr;
  QToolButton *m_minMaxButton = nullptr;
  QAction *m_nextAction = nullptr;
  QAction *m_prevAction = nullptr;
  QToolButton *m_prevToolButton = nullptr;
  QToolButton *m_nextToolButton = nullptr;
  QWidget *m_toolBar = nullptr;
  QStackedWidget *m_outputWidgetPane = nullptr;
  QStackedWidget *m_opToolBarWidgets = nullptr;
  QWidget *m_buttonsWidget = nullptr;
  QIcon m_minimizeIcon;
  QIcon m_maximizeIcon;
  int m_outputPaneHeightSetting = 0;
};

class BadgeLabel {
public:
  BadgeLabel();
  auto paint(QPainter *p, int x, int y, bool is_checked) const -> void;
  auto setText(const QString &text) -> void;
  auto text() const -> QString;
  auto sizeHint() const -> QSize;

private:
  auto calculateSize() -> void;

  QSize m_size;
  QString m_text;
  QFont m_font;
  static const int m_padding = 6;
};

class OutputPaneToggleButton : public QToolButton {
  Q_OBJECT public:
  OutputPaneToggleButton(int number, QString text, QAction *action, QWidget *parent = nullptr);
  auto sizeHint() const -> QSize override;
  auto paintEvent(QPaintEvent *) -> void override;
  auto flash(int count = 3) -> void;
  auto setIconBadgeNumber(int number) -> void;
  auto isPaneVisible() const -> bool;

private:
  auto updateToolTip() -> void;
  auto checkStateSet() -> void override;

  QString m_number;
  QString m_text;
  QAction *m_action;
  QTimeLine *m_flashTimer;
  BadgeLabel m_badgeNumberLabel;
};

class OutputPaneManageButton : public QToolButton {
  Q_OBJECT public:
  OutputPaneManageButton();
  auto sizeHint() const -> QSize override;
  auto paintEvent(QPaintEvent *) -> void override;
};

} // namespace Internal
} // namespace Core
