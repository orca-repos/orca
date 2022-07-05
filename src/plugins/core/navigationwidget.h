// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/minisplitter.h>

#include <utils/id.h>

#include <QHash>

QT_BEGIN_NAMESPACE
class QSettings;
class QAbstractItemModel;
QT_END_NAMESPACE

namespace Utils {
class QtcSettings;
}

namespace Core {
class INavigationWidgetFactory;
class Command;
class NavigationWidget;
struct NavigationWidgetPrivate;

namespace Internal {
class NavigationSubWidget;
}

enum class Side {
  Left,
  Right
};

class CORE_EXPORT NavigationWidgetPlaceHolder : public QWidget {
  Q_OBJECT friend class NavigationWidget;

public:
  explicit NavigationWidgetPlaceHolder(Utils::Id mode, Side side, QWidget *parent = nullptr);
  ~NavigationWidgetPlaceHolder() override;
  static auto current(Side side) -> NavigationWidgetPlaceHolder*;
  static auto setCurrent(Side side, NavigationWidgetPlaceHolder *nav_widget) -> void;
  auto applyStoredSize() -> void;

private:
  auto currentModeAboutToChange(Utils::Id mode) -> void;
  auto storedWidth() const -> int;

  Utils::Id m_mode;
  Side m_side;
  static NavigationWidgetPlaceHolder *s_current_left;
  static NavigationWidgetPlaceHolder *s_current_right;
};

class CORE_EXPORT NavigationWidget final : public MiniSplitter {
  Q_OBJECT

public:
  enum FactoryModelRoles {
    FactoryObjectRole = Qt::UserRole,
    FactoryIdRole,
    FactoryActionIdRole,
    FactoryPriorityRole
  };

  explicit NavigationWidget(QAction *toggle_side_bar_action, Side side);
  ~NavigationWidget() override;

  auto setFactories(const QList<INavigationWidgetFactory*> &factories) -> void;
  auto settingsGroup() const -> QString;
  auto saveSettings(Utils::QtcSettings *settings) const -> void;
  auto restoreSettings(QSettings *settings) -> void;
  auto activateSubWidget(Utils::Id factory_id, int preferred_position) -> QWidget*;
  auto closeSubWidgets() const -> void;
  auto isShown() const -> bool;
  auto setShown(bool b) const -> void;
  static auto instance(Side side) -> NavigationWidget*;
  static auto activateSubWidget(Utils::Id factory_id, Side fallback_side) -> QWidget*;
  auto storedWidth() const -> int;

  // Called from the place holders
  auto placeHolderChanged(const NavigationWidgetPlaceHolder *holder) const -> void;
  auto commandMap() const -> QHash<Utils::Id, Command*>;
  auto factoryModel() const -> QAbstractItemModel*;

protected:
  auto resizeEvent(QResizeEvent *) -> void override;

private:
  auto splitSubWidget(int factory_index) -> void;
  auto closeSubWidget() -> void;
  auto updateToggleText() const -> void;
  auto insertSubItem(int position, int factory_index) -> Internal::NavigationSubWidget*;
  auto factoryIndex(Utils::Id id) const -> int;
  auto settingsKey(const QString &key) const -> QString;
  auto onSubWidgetFactoryIndexChanged(int factory_index) const -> void;

  NavigationWidgetPrivate *d;
};

} // namespace Core
