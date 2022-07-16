// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "outlinefactory.hpp"

#include <core/core-constants.hpp>
#include <core/core-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>

#include <utils/utilsicons.hpp>
#include <utils/qtcassert.hpp>

#include <QToolButton>
#include <QLabel>
#include <QStackedWidget>

#include <QDebug>

namespace TextEditor {

static QList<IOutlineWidgetFactory*> g_outlineWidgetFactories;
static QPointer<Internal::OutlineFactory> g_outlineFactory;

IOutlineWidgetFactory::IOutlineWidgetFactory()
{
  g_outlineWidgetFactories.append(this);
}

IOutlineWidgetFactory::~IOutlineWidgetFactory()
{
  g_outlineWidgetFactories.removeOne(this);
}

auto IOutlineWidgetFactory::updateOutline() -> void
{
  if (QTC_GUARD(!g_outlineFactory.isNull())) emit g_outlineFactory->updateOutline();
}

namespace Internal {

OutlineWidgetStack::OutlineWidgetStack(OutlineFactory *factory) : m_syncWithEditor(true), m_sorted(false)
{
  const auto label = new QLabel(tr("No outline available"), this);
  label->setAlignment(Qt::AlignCenter);

  // set background to be white
  label->setAutoFillBackground(true);
  label->setBackgroundRole(QPalette::Base);

  addWidget(label);

  m_toggleSync = new QToolButton(this);
  m_toggleSync->setIcon(Utils::Icons::LINK_TOOLBAR.icon());
  m_toggleSync->setCheckable(true);
  m_toggleSync->setChecked(true);
  m_toggleSync->setToolTip(tr("Synchronize with Editor"));
  connect(m_toggleSync, &QAbstractButton::clicked, this, &OutlineWidgetStack::toggleCursorSynchronization);

  m_filterButton = new QToolButton(this);
  // The ToolButton needs a parent because updateFilterMenu() sets
  // it visible. That would open a top-level window if the button
  // did not have a parent in that moment.

  m_filterButton->setIcon(Utils::Icons::FILTER.icon());
  m_filterButton->setToolTip(tr("Filter tree"));
  m_filterButton->setPopupMode(QToolButton::InstantPopup);
  m_filterButton->setProperty("noArrow", true);
  m_filterMenu = new QMenu(m_filterButton);
  m_filterButton->setMenu(m_filterMenu);

  m_toggleSort = new QToolButton(this);
  m_toggleSort->setIcon(Utils::Icons::SORT_ALPHABETICALLY_TOOLBAR.icon());
  m_toggleSort->setCheckable(true);
  m_toggleSort->setChecked(false);
  m_toggleSort->setToolTip(tr("Sort Alphabetically"));
  connect(m_toggleSort, &QAbstractButton::clicked, this, &OutlineWidgetStack::toggleSort);

  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::currentEditorChanged, this, &OutlineWidgetStack::updateEditor);
  connect(factory, &OutlineFactory::updateOutline, this, &OutlineWidgetStack::updateCurrentEditor);
  updateCurrentEditor();
}

auto OutlineWidgetStack::toolButtons() -> QList<QToolButton*>
{
  return {m_filterButton, m_toggleSort, m_toggleSync};
}

OutlineWidgetStack::~OutlineWidgetStack() = default;

auto OutlineWidgetStack::saveSettings(QSettings *settings, int position) -> void
{
  const auto baseKey = QStringLiteral("Outline.%1.").arg(position);
  settings->setValue(baseKey + QLatin1String("SyncWithEditor"), m_toggleSync->isChecked());
  for (auto iter = m_widgetSettings.constBegin(); iter != m_widgetSettings.constEnd(); ++iter)
    settings->setValue(baseKey + iter.key(), iter.value());
}

auto OutlineWidgetStack::restoreSettings(QSettings *settings, int position) -> void
{
  const auto baseKey = QStringLiteral("Outline.%1.").arg(position);

  auto syncWithEditor = true;
  m_widgetSettings.clear();
  foreach(const QString &longKey, settings->allKeys()) {
    if (!longKey.startsWith(baseKey))
      continue;

    const QString key = longKey.mid(baseKey.length());

    if (key == QLatin1String("SyncWithEditor")) {
      syncWithEditor = settings->value(longKey).toBool();
      continue;
    }
    m_widgetSettings.insert(key, settings->value(longKey));
  }

  m_toggleSync->setChecked(syncWithEditor);
  if (const auto outlineWidget = qobject_cast<IOutlineWidget*>(currentWidget()))
    outlineWidget->restoreSettings(m_widgetSettings);
}

auto OutlineWidgetStack::isCursorSynchronized() const -> bool
{
  return m_syncWithEditor;
}

auto OutlineWidgetStack::toggleCursorSynchronization() -> void
{
  m_syncWithEditor = !m_syncWithEditor;
  if (const auto outlineWidget = qobject_cast<IOutlineWidget*>(currentWidget()))
    outlineWidget->setCursorSynchronization(m_syncWithEditor);
}

auto OutlineWidgetStack::toggleSort() -> void
{
  m_sorted = !m_sorted;
  if (const auto outlineWidget = qobject_cast<IOutlineWidget*>(currentWidget()))
    outlineWidget->setSorted(m_sorted);
}

auto OutlineWidgetStack::updateFilterMenu() -> void
{
  m_filterMenu->clear();
  if (const auto outlineWidget = qobject_cast<IOutlineWidget*>(currentWidget())) {
    foreach(QAction *filterAction, outlineWidget->filterMenuActions()) {
      m_filterMenu->addAction(filterAction);
    }
  }
  m_filterButton->setVisible(!m_filterMenu->actions().isEmpty());
}

auto OutlineWidgetStack::updateCurrentEditor() -> void
{
  updateEditor(Orca::Plugin::Core::EditorManager::currentEditor());
}

auto OutlineWidgetStack::updateEditor(Orca::Plugin::Core::IEditor *editor) -> void
{
  IOutlineWidget *newWidget = nullptr;

  if (editor) {
    for (const auto widgetFactory : qAsConst(g_outlineWidgetFactories)) {
      if (widgetFactory->supportsEditor(editor)) {
        newWidget = widgetFactory->createWidget(editor);
        m_toggleSort->setVisible(widgetFactory->supportsSorting());
        break;
      }
    }
  }

  if (newWidget != currentWidget()) {
    // delete old widget
    if (const auto outlineWidget = qobject_cast<IOutlineWidget*>(currentWidget())) {
      const auto widgetSettings = outlineWidget->settings();
      for (auto iter = widgetSettings.constBegin(); iter != widgetSettings.constEnd(); ++iter)
        m_widgetSettings.insert(iter.key(), iter.value());
      removeWidget(outlineWidget);
      delete outlineWidget;
    }
    if (newWidget) {
      newWidget->restoreSettings(m_widgetSettings);
      newWidget->setCursorSynchronization(m_syncWithEditor);
      m_toggleSort->setChecked(newWidget->isSorted());
      addWidget(newWidget);
      setCurrentWidget(newWidget);
      setFocusProxy(newWidget);
    }

    updateFilterMenu();
  }
}

OutlineFactory::OutlineFactory()
{
  QTC_CHECK(g_outlineFactory.isNull());
  g_outlineFactory = this;
  setDisplayName(tr("Outline"));
  setId("Outline");
  setPriority(600);
}

auto OutlineFactory::createWidget() -> Orca::Plugin::Core::NavigationView
{
  auto placeHolder = new OutlineWidgetStack(this);
  return {placeHolder, placeHolder->toolButtons()};
}

auto OutlineFactory::saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void
{
  const auto widgetStack = qobject_cast<OutlineWidgetStack*>(widget);
  Q_ASSERT(widgetStack);
  widgetStack->saveSettings(settings, position);
}

auto OutlineFactory::restoreSettings(QSettings *settings, int position, QWidget *widget) -> void
{
  const auto widgetStack = qobject_cast<OutlineWidgetStack*>(widget);
  Q_ASSERT(widgetStack);
  widgetStack->restoreSettings(settings, position);
}

} // namespace Internal
} // namespace TextEditor
