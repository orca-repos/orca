// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "porting.hpp"

#include <QWidget>

namespace Utils {

class DetailsWidgetPrivate;
class FadingPanel;

class ORCA_UTILS_EXPORT DetailsWidget : public QWidget {
  Q_OBJECT Q_PROPERTY(QString summaryText READ summaryText WRITE setSummaryText DESIGNABLE true)
  Q_PROPERTY(QString additionalSummaryText READ additionalSummaryText WRITE setAdditionalSummaryText DESIGNABLE true)
  Q_PROPERTY(bool useCheckBox READ useCheckBox WRITE setUseCheckBox DESIGNABLE true)
  Q_PROPERTY(bool checked READ isChecked WRITE setChecked DESIGNABLE true)
  Q_PROPERTY(State state READ state WRITE setState)

public:
  enum State {
    Expanded,
    Collapsed,
    NoSummary,
    OnlySummary
  };

  Q_ENUM(State)

  explicit DetailsWidget(QWidget *parent = nullptr);
  ~DetailsWidget() override;

  auto setSummaryText(const QString &text) -> void;
  auto summaryText() const -> QString;
  auto setAdditionalSummaryText(const QString &text) -> void;
  auto additionalSummaryText() const -> QString;
  auto setState(State state) -> void;
  auto state() const -> State;
  auto setWidget(QWidget *widget) -> void;
  auto widget() const -> QWidget*;
  auto takeWidget() -> QWidget*;
  auto setToolWidget(FadingPanel *widget) -> void;
  auto toolWidget() const -> QWidget*;
  auto setSummaryFontBold(bool b) -> void;
  auto isChecked() const -> bool;
  auto setChecked(bool b) -> void;
  auto useCheckBox() -> bool;
  auto setUseCheckBox(bool b) -> void;
  auto setCheckable(bool b) -> void;
  auto setExpandable(bool b) -> void;
  auto setIcon(const QIcon &icon) -> void;
  static auto createBackground(const QSize &size, int topHeight, QWidget *widget) -> QPixmap;

signals:
  auto checked(bool) -> void;
  auto linkActivated(const QString &link) -> void;
  auto expanded(bool) -> void;

private:
  auto setExpanded(bool) -> void;

protected:
  auto paintEvent(QPaintEvent *paintEvent) -> void override;
  auto enterEvent(EnterEvent *event) -> void override;
  auto leaveEvent(QEvent *event) -> void override;

private:
  DetailsWidgetPrivate *d;
};

} // namespace Utils
