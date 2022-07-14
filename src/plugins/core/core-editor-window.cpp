// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-editor-window.hpp"

#include "core-constants.hpp"
#include "core-context-interface.hpp"
#include "core-editor-area.hpp"
#include "core-editor-manager-private.hpp"
#include "core-interface.hpp"
#include "core-locator-manager.hpp"
#include "core-mini-splitter.hpp"

#include <aggregation/aggregate.hpp>

#include <utils/qtcassert.hpp>

#include <QStatusBar>
#include <QVBoxLayout>

constexpr char geometry_key[] = "geometry";
constexpr char split_state_key[] = "splitstate";

namespace Orca::Plugin::Core {

EditorWindow::EditorWindow(QWidget *parent) : QWidget(parent)
{
  m_area = new EditorArea;

  const auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  setLayout(layout);

  layout->addWidget(m_area);
  setFocusProxy(m_area);

  const auto status_bar = new QStatusBar;
  layout->addWidget(status_bar);

  const auto splitter = new NonResizingSplitter(status_bar);
  splitter->setChildrenCollapsible(false);
  status_bar->addPermanentWidget(splitter, 10);

  const auto locator_widget = LocatorManager::createLocatorInputWidget(this);
  splitter->addWidget(locator_widget);
  splitter->addWidget(new QWidget);

  setAttribute(Qt::WA_DeleteOnClose);
  setAttribute(Qt::WA_QuitOnClose, false); // don't prevent Qt Creator from closing
  resize(QSize(800, 600));

  static auto window_id = 0;
  ICore::registerWindow(this, Context(Utils::Id("EditorManager.ExternalWindow.").withSuffix(++window_id), C_EDITORMANAGER));

  connect(m_area, &EditorArea::windowTitleNeedsUpdate, this, &EditorWindow::updateWindowTitle);
  // editor area can be deleted by editor manager
  connect(m_area, &EditorArea::destroyed, this, [this]() {
    m_area = nullptr;
    deleteLater();
  });

  updateWindowTitle();
}

EditorWindow::~EditorWindow()
{
  if (m_area)
    disconnect(m_area, nullptr, this, nullptr);
}

auto EditorWindow::editorArea() const -> EditorArea*
{
  return m_area;
}

auto EditorWindow::saveState() const -> QVariantHash
{
  QVariantHash state;
  state.insert(geometry_key, saveGeometry());

  if (QTC_GUARD(m_area)) {
    const auto split_state = m_area->saveState();
    state.insert(split_state_key, split_state);
  }

  return state;
}

auto EditorWindow::restoreState(const QVariantHash &state) -> void
{
  if (state.contains(geometry_key))
    restoreGeometry(state.value(geometry_key).toByteArray());

  if (state.contains(split_state_key) && m_area)
    m_area->restoreState(state.value(split_state_key).toByteArray());
}

auto EditorWindow::updateWindowTitle() const -> void
{
  EditorManagerPrivate::updateWindowTitleForDocument(m_area->currentDocument(), this);
}

} // namespace Orca::Plugin::Core
