// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/id.hpp>

#include <QKeySequence>
#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QSettings;
class QToolButton;
class QWidget;
QT_END_NAMESPACE

namespace Utils {
class QtcSettings;
}

namespace Orca::Plugin::Core {

struct NavigationView {
  QWidget *widget{};
  QList<QToolButton*> dock_tool_bar_widgets;
};

class CORE_EXPORT INavigationWidgetFactory : public QObject {
  Q_OBJECT

public:
  INavigationWidgetFactory();
  ~INavigationWidgetFactory() override;

  static auto allNavigationFactories() -> QList<INavigationWidgetFactory*>;
  auto setDisplayName(const QString &display_name) -> void;
  auto setPriority(int priority) -> void;
  auto setId(Utils::Id id) -> void;
  auto setActivationSequence(const QKeySequence &keys) -> void;
  auto displayName() const -> QString { return m_display_name; }
  auto priority() const -> int { return m_priority; }
  auto id() const -> Utils::Id { return m_id; }
  auto activationSequence() const -> QKeySequence;

  // This design is not optimal, think about it again once we need to extend it
  // It could be implemented as returning an object which has both the widget
  // and the docktoolbar widgets
  // Similar to how IView
  virtual auto createWidget() -> NavigationView = 0;
  virtual auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void;
  virtual auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void;

private:
  QString m_display_name;
  int m_priority = 0;
  Utils::Id m_id;
  QKeySequence m_activation_sequence;
};

} // namespace Orca::Plugin::Core
