// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-edit-mode.hpp"

#include "core-constants.hpp"
#include "core-editor-interface.hpp"
#include "core-editor-manager.hpp"
#include "core-icons.hpp"
#include "core-interface.hpp"
#include "core-mini-splitter.hpp"
#include "core-mode-manager.hpp"
#include "core-navigation-widget.hpp"
#include "core-output-pane.hpp"
#include "core-right-pane.hpp"

#include <QHBoxLayout>
#include <QLatin1String>
#include <QWidget>

namespace Orca::Plugin::Core {

EditMode::EditMode() : m_splitter(new MiniSplitter), m_right_split_widget_layout(new QVBoxLayout)
{
  setObjectName(QLatin1String("EditMode"));
  setDisplayName(tr("Edit"));
  setIcon(Utils::Icon::modeIcon(MODE_EDIT_CLASSIC, MODE_EDIT_FLAT, MODE_EDIT_FLAT_ACTIVE));
  setPriority(P_MODE_EDIT);
  setId(MODE_EDIT);

  m_right_split_widget_layout->setSpacing(0);
  m_right_split_widget_layout->setContentsMargins(0, 0, 0, 0);

  const auto right_split_widget = new QWidget;
  right_split_widget->setLayout(m_right_split_widget_layout);

  const auto editor_place_holder = new EditorManagerPlaceHolder;
  m_right_split_widget_layout->insertWidget(0, editor_place_holder);

  const auto right_pane_splitter = new MiniSplitter;
  right_pane_splitter->insertWidget(0, right_split_widget);
  right_pane_splitter->insertWidget(1, new RightPanePlaceHolder(MODE_EDIT));
  right_pane_splitter->setStretchFactor(0, 1);
  right_pane_splitter->setStretchFactor(1, 0);

  const auto splitter = new MiniSplitter;
  splitter->setOrientation(Qt::Vertical);
  splitter->insertWidget(0, right_pane_splitter);

  QWidget *output_pane = new OutputPanePlaceHolder(MODE_EDIT, splitter);
  output_pane->setObjectName(QLatin1String("EditModeOutputPanePlaceHolder"));
  splitter->insertWidget(1, output_pane);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 0);

  m_splitter->insertWidget(0, new NavigationWidgetPlaceHolder(MODE_EDIT, Side::Left));
  m_splitter->insertWidget(1, splitter);
  m_splitter->insertWidget(2, new NavigationWidgetPlaceHolder(MODE_EDIT, Side::Right));
  m_splitter->setStretchFactor(0, 0);
  m_splitter->setStretchFactor(1, 1);
  m_splitter->setStretchFactor(2, 0);

  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &EditMode::grabEditorManager);
  m_splitter->setFocusProxy(editor_place_holder);

  const auto mode_context_object = new IContext(this);
  mode_context_object->setContext(Context(C_EDITORMANAGER));
  mode_context_object->setWidget(m_splitter);

  ICore::addContextObject(mode_context_object);

  IContext::setWidget(m_splitter);
  IContext::setContext(Context(C_EDIT_MODE, C_NAVIGATION_PANE));
}

EditMode::~EditMode()
{
  delete m_splitter;
}

auto EditMode::grabEditorManager(const Utils::Id mode) const -> void
{
  if (mode != id())
    return;

  if (EditorManager::currentEditor())
    EditorManager::currentEditor()->widget()->setFocus();
}

} // namespace Orca::Plugin::Core
