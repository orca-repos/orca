// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "coreconstants.hpp"
#include "coreicons.hpp"
#include "editmode.hpp"
#include "icore.hpp"
#include "modemanager.hpp"
#include "minisplitter.hpp"
#include "navigationwidget.hpp"
#include "outputpane.hpp"
#include "rightpane.hpp"
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditor.hpp>

#include <QLatin1String>
#include <QHBoxLayout>
#include <QWidget>

namespace Core {
namespace Internal {

EditMode::EditMode() : m_splitter(new MiniSplitter), m_right_split_widget_layout(new QVBoxLayout)
{
  setObjectName(QLatin1String("EditMode"));
  setDisplayName(tr("Edit"));
  setIcon(Utils::Icon::modeIcon(Icons::MODE_EDIT_CLASSIC, Icons::MODE_EDIT_FLAT, Icons::MODE_EDIT_FLAT_ACTIVE));
  setPriority(Constants::P_MODE_EDIT);
  setId(Constants::MODE_EDIT);

  m_right_split_widget_layout->setSpacing(0);
  m_right_split_widget_layout->setContentsMargins(0, 0, 0, 0);

  const auto right_split_widget = new QWidget;
  right_split_widget->setLayout(m_right_split_widget_layout);

  const auto editor_place_holder = new EditorManagerPlaceHolder;
  m_right_split_widget_layout->insertWidget(0, editor_place_holder);

  const auto right_pane_splitter = new MiniSplitter;
  right_pane_splitter->insertWidget(0, right_split_widget);
  right_pane_splitter->insertWidget(1, new RightPanePlaceHolder(Constants::MODE_EDIT));
  right_pane_splitter->setStretchFactor(0, 1);
  right_pane_splitter->setStretchFactor(1, 0);

  const auto splitter = new MiniSplitter;
  splitter->setOrientation(Qt::Vertical);
  splitter->insertWidget(0, right_pane_splitter);

  QWidget *output_pane = new OutputPanePlaceHolder(Constants::MODE_EDIT, splitter);
  output_pane->setObjectName(QLatin1String("EditModeOutputPanePlaceHolder"));
  splitter->insertWidget(1, output_pane);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 0);

  m_splitter->insertWidget(0, new NavigationWidgetPlaceHolder(Constants::MODE_EDIT, Side::Left));
  m_splitter->insertWidget(1, splitter);
  m_splitter->insertWidget(2, new NavigationWidgetPlaceHolder(Constants::MODE_EDIT, Side::Right));
  m_splitter->setStretchFactor(0, 0);
  m_splitter->setStretchFactor(1, 1);
  m_splitter->setStretchFactor(2, 0);

  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &EditMode::grabEditorManager);
  m_splitter->setFocusProxy(editor_place_holder);

  const auto mode_context_object = new IContext(this);
  mode_context_object->setContext(Context(Constants::C_EDITORMANAGER));
  mode_context_object->setWidget(m_splitter);

  ICore::addContextObject(mode_context_object);

  IContext::setWidget(m_splitter);
  IContext::setContext(Context(Constants::C_EDIT_MODE, Constants::C_NAVIGATION_PANE));
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

} // namespace Internal
} // namespace Core
