// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/ioutlinewidget.hpp>

#include <core/core-navigation-widget-factory-interface.hpp>

#include <QStackedWidget>
#include <QMenu>

namespace Orca::Plugin::Core {
class IEditor;
}

namespace TextEditor {
namespace Internal {

class OutlineFactory;

class OutlineWidgetStack : public QStackedWidget {
  Q_OBJECT

public:
  OutlineWidgetStack(OutlineFactory *factory);
  ~OutlineWidgetStack() override;

  auto toolButtons() -> QList<QToolButton*>;
  auto saveSettings(QSettings *settings, int position) -> void;
  auto restoreSettings(QSettings *settings, int position) -> void;

private:
  auto isCursorSynchronized() const -> bool;
  auto dummyWidget() const -> QWidget*;
  auto updateFilterMenu() -> void;
  auto toggleCursorSynchronization() -> void;
  auto toggleSort() -> void;
  auto updateEditor(Orca::Plugin::Core::IEditor *editor) -> void;
  auto updateCurrentEditor() -> void;

  QToolButton *m_toggleSync;
  QToolButton *m_filterButton;
  QToolButton *m_toggleSort;
  QMenu *m_filterMenu;
  QVariantMap m_widgetSettings;
  bool m_syncWithEditor;
  bool m_sorted;
};

class OutlineFactory : public Orca::Plugin::Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  OutlineFactory();

  // from INavigationWidgetFactory
  auto createWidget() -> Orca::Plugin::Core::NavigationView override;
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;

signals:
  auto updateOutline() -> void;
};

} // namespace Internal
} // namespace TextEditor
