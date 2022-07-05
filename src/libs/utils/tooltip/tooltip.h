// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../utils_global.h"

#include <QSharedPointer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QRect>
#include <QVariant>

/*
 * In its current form QToolTip is not extensible. So this is an attempt to provide a more
 * flexible and customizable tooltip mechanism for Creator. Part of the code here is duplicated
 * from QToolTip. This includes a private Qt header and the non-exported class QTipLabel, which
 * here serves as a base tip class. Please notice that Qt relies on this particular class name in
 * order to correctly apply the native styles for tooltips. Therefore the QTipLabel name should
 * not be changed.
 */

QT_BEGIN_NAMESPACE
class QPoint;
class QVariant;
class QLayout;
class QWidget;
QT_END_NAMESPACE

namespace Utils {
namespace Internal { class TipLabel; }

class ORCA_UTILS_EXPORT ToolTip : public QObject {
  Q_OBJECT

protected:
  ToolTip();

public:
  ~ToolTip() override;

  enum {
    ColorContent = 0,
    TextContent = 1,
    WidgetContent = 42
  };

  auto eventFilter(QObject *o, QEvent *event) -> bool override;
  static auto instance() -> ToolTip*;
  static auto show(const QPoint &pos, const QString &content, QWidget *w = nullptr, const QVariant &contextHelp = {}, const QRect &rect = QRect()) -> void;
  static auto show(const QPoint &pos, const QString &content, Qt::TextFormat format, QWidget *w = nullptr, const QVariant &contextHelp = {}, const QRect &rect = QRect()) -> void;
  static auto show(const QPoint &pos, const QColor &color, QWidget *w = nullptr, const QVariant &contextHelp = {}, const QRect &rect = QRect()) -> void;
  static auto show(const QPoint &pos, QWidget *content, QWidget *w = nullptr, const QVariant &contextHelp = {}, const QRect &rect = QRect()) -> void;
  static auto show(const QPoint &pos, QLayout *content, QWidget *w = nullptr, const QVariant &contextHelp = {}, const QRect &rect = QRect()) -> void;
  static auto move(const QPoint &pos) -> void;
  static auto hide() -> void;
  static auto hideImmediately() -> void;
  static auto isVisible() -> bool;
  static auto offsetFromPosition() -> QPoint;

  // Helper to 'pin' (show as real window) a tooltip shown
  // using WidgetContent
  static auto pinToolTip(QWidget *w, QWidget *parent) -> bool;
  static auto contextHelp() -> QVariant;

signals:
  auto shown() -> void;
  auto hidden() -> void;

private:
  auto showInternal(const QPoint &pos, const QVariant &content, int typeId, QWidget *w, const QVariant &contextHelp, const QRect &rect) -> void;
  auto hideTipImmediately() -> void;
  auto acceptShow(const QVariant &content, int typeId, const QPoint &pos, QWidget *w, const QVariant &contextHelp, const QRect &rect) -> bool;
  auto setUp(const QPoint &pos, QWidget *w, const QRect &rect) -> void;
  auto tipChanged(const QPoint &pos, const QVariant &content, int typeId, QWidget *w, const QVariant &contextHelp) const -> bool;
  auto setTipRect(QWidget *w, const QRect &rect) -> void;
  auto placeTip(const QPoint &pos) -> void;
  auto showTip() -> void;
  auto hideTipWithDelay() -> void;

  QPointer<Internal::TipLabel> m_tip;
  QWidget *m_widget;
  QRect m_rect;
  QTimer m_showTimer;
  QTimer m_hideDelayTimer;
  QVariant m_contextHelp;
};

} // namespace Utils
