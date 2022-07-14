// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/id.hpp>

#include <QFuture>
#include <QString>
#include <QWidget>

namespace Orca::Plugin::Core {

class FutureProgressPrivate;

class CORE_EXPORT FutureProgress final : public QWidget {
  Q_OBJECT

public:
  enum KeepOnFinishType {
    HideOnFinish = 0,
    KeepOnFinishTillUserInteraction = 1,
    KeepOnFinish = 2
  };

  explicit FutureProgress(QWidget *parent = nullptr);
  ~FutureProgress() override;

  auto eventFilter(QObject *object, QEvent *) -> bool override;
  auto setFuture(const QFuture<void> &future) const -> void;
  auto future() const -> QFuture<void>;
  auto setTitle(const QString &title) const -> void;
  auto title() const -> QString;
  auto setSubtitle(const QString &subtitle) -> void;
  auto subtitle() const -> QString;
  auto setSubtitleVisibleInStatusBar(bool visible) -> void;
  auto isSubtitleVisibleInStatusBar() const -> bool;
  auto setType(Utils::Id type) const -> void;
  auto type() const -> Utils::Id;
  auto setKeepOnFinish(KeepOnFinishType keep_type) const -> void;
  auto keepOnFinish() const -> bool;
  auto hasError() const -> bool;
  auto setWidget(QWidget *widget) const -> void;
  auto widget() const -> QWidget*;
  auto setStatusBarWidget(QWidget *widget) -> void;
  auto statusBarWidget() const -> QWidget*;
  auto isFading() const -> bool;
  auto sizeHint() const -> QSize override;

signals:
  auto clicked() -> void;
  auto finished() -> void;
  auto canceled() -> void;
  auto removeMe() -> void;
  auto hasErrorChanged() -> void;
  auto fadeStarted() -> void;
  auto statusBarWidgetChanged() -> void;
  auto subtitleInStatusBarChanged() -> void;

protected:
  auto mousePressEvent(QMouseEvent *event) -> void override;
  auto paintEvent(QPaintEvent *) -> void override;

private:
  auto updateToolTip(const QString &) -> void;
  auto cancel() const -> void;
  auto setStarted() const -> void;
  auto setFinished() -> void;
  auto setProgressRange(int min, int max) const -> void;
  auto setProgressValue(int val) const -> void;
  auto setProgressText(const QString &text) -> void;

  FutureProgressPrivate *d;
};

} // namespace Orca::Plugin::Core
