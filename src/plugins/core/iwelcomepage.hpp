// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/id.hpp>
#include <utils/porting.hpp>

#include <QWidget>
#include <QObject>

#include <functional>

QT_BEGIN_NAMESPACE
class QPixmap;
QT_END_NAMESPACE

namespace Core {

class CORE_EXPORT IWelcomePage : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString title READ title CONSTANT)
  Q_PROPERTY(int priority READ priority CONSTANT)

public:
  IWelcomePage();
  ~IWelcomePage() override;

  virtual auto title() const -> QString = 0;
  virtual auto priority() const -> int { return 0; }
  virtual auto id() const -> Utils::Id = 0;
  virtual auto createWidget() const -> QWidget* = 0;
  static auto allWelcomePages() -> QList<IWelcomePage*>;
};

class WelcomePageButtonPrivate;

class CORE_EXPORT WelcomePageFrame : public QWidget {
public:
  explicit WelcomePageFrame(QWidget *parent);

  auto paintEvent(QPaintEvent *event) -> void override;
  static auto buttonPalette(bool is_active, bool is_cursor_inside, bool for_text) -> QPalette;
};

class CORE_EXPORT WelcomePageButton final : public WelcomePageFrame {
public:
  enum Size {
    SizeSmall,
    SizeLarge,
  };

  explicit WelcomePageButton(QWidget *parent);
  ~WelcomePageButton() override;

  auto mousePressEvent(QMouseEvent *) -> void override;
  auto enterEvent(Utils::EnterEvent *) -> void override;
  auto leaveEvent(QEvent *) -> void override;
  auto setText(const QString &text) const -> void;
  auto setSize(Size) const -> void;
  auto setWithAccentColor(bool with_accent) -> void;
  auto setOnClicked(const std::function<void ()> &value) const -> void;
  auto setActiveChecker(const std::function<bool ()> &value) const -> void;
  auto recheckActive() const -> void;
  auto click() const -> void;

private:
  WelcomePageButtonPrivate *d;
};

} // Core
