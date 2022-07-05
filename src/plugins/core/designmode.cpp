// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "designmode.h"

#include <core/icore.h>
#include <core/idocument.h>
#include <core/modemanager.h>
#include <core/editormanager/editormanager.h>
#include <core/editormanager/ieditor.h>
#include <core/coreconstants.h>
#include <core/coreicons.h>

#include <extensionsystem/pluginmanager.h>

#include <QPointer>
#include <QStackedWidget>

namespace Core {

struct DesignEditorInfo {
  int widget_index{};
  QStringList mime_types;
  Context context;
  QWidget *widget{};
};

class DesignModePrivate {
public:
  DesignModePrivate();
  ~DesignModePrivate();
  
  QPointer<IEditor> m_current_editor;
  bool m_is_active = false;
  QList<DesignEditorInfo*> m_editors;
  QStackedWidget *m_stack_widget;
  Context m_active_context;
};

DesignModePrivate::DesignModePrivate() : m_stack_widget(new QStackedWidget) {}

DesignModePrivate::~DesignModePrivate()
{
  delete m_stack_widget;
}

static DesignMode *m_instance = nullptr;
static DesignModePrivate *d = nullptr;

DesignMode::DesignMode()
{
  ICore::addPreCloseListener([]() -> bool {
    m_instance->currentEditorChanged(nullptr);
    return true;
  });

  setObjectName(QLatin1String("DesignMode"));
  setEnabled(false);
  setContext(Context(Constants::C_DESIGN_MODE));
  setWidget(d->m_stack_widget);
  setDisplayName(tr("Design"));
  setIcon(Utils::Icon::modeIcon(Icons::MODE_DESIGN_CLASSIC, Icons::MODE_DESIGN_FLAT, Icons::MODE_DESIGN_FLAT_ACTIVE));
  setPriority(Constants::P_MODE_DESIGN);
  setId(Constants::MODE_DESIGN);

  connect(EditorManager::instance(), &EditorManager::currentEditorChanged, this, &DesignMode::currentEditorChanged);
  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &DesignMode::updateContext);
}

DesignMode::~DesignMode()
{
  qDeleteAll(d->m_editors);
}

auto DesignMode::instance() -> DesignMode*
{
  return m_instance;
}

auto DesignMode::setDesignModeIsRequired() -> void
{
  // d != nullptr indicates "isRequired".
  if (!d)
    d = new DesignModePrivate;
}

/**
  * Registers a widget to be displayed when an editor with a file specified in
  * mimeTypes is opened. This also appends the additionalContext in ICore to
  * the context, specified here.
  */
auto DesignMode::registerDesignWidget(QWidget *widget, const QStringList &mime_types, const Context &context) -> void
{
  setDesignModeIsRequired();

  const auto index = d->m_stack_widget->addWidget(widget);
  const auto info = new DesignEditorInfo;

  info->mime_types = mime_types;
  info->context = context;
  info->widget_index = index;
  info->widget = widget;

  d->m_editors.append(info);
}

auto DesignMode::unregisterDesignWidget(QWidget *widget) -> void
{
  d->m_stack_widget->removeWidget(widget);

  for(const auto info: d->m_editors) {
    if (info->widget == widget) {
      d->m_editors.removeAll(info);
      delete info;
      break;
    }
  }
}

// if editor changes, check if we have valid mimetype registered.
auto DesignMode::currentEditorChanged(IEditor *editor) -> void
{
  if (editor && (d->m_current_editor.data() == editor))
    return;

  auto mime_editor_available = false;

  if (editor) {
    if (const auto mime_type = editor->document()->mimeType(); !mime_type.isEmpty()) {
      for(const auto editor_info: d->m_editors) {
        for(const auto &mime: editor_info->mime_types) {
          if (mime == mime_type) {
            d->m_stack_widget->setCurrentIndex(editor_info->widget_index);
            setActiveContext(editor_info->context);
            mime_editor_available = true;
            setEnabled(true);
            break;
          }
        } // foreach mime
        if (mime_editor_available)
          break;
      } // foreach editorInfo
    }
  }

  if (d->m_current_editor)
    disconnect(d->m_current_editor.data()->document(), &IDocument::changed, this, &DesignMode::updateActions);

  if (!mime_editor_available) {
    setActiveContext(Context());
    if (ModeManager::currentModeId() == id())
      ModeManager::activateMode(Constants::MODE_EDIT);
    setEnabled(false);
    d->m_current_editor = nullptr;
    emit actionsUpdated(d->m_current_editor.data());
  } else {
    d->m_current_editor = editor;
    if (d->m_current_editor)
      connect(d->m_current_editor.data()->document(), &IDocument::changed, this, &DesignMode::updateActions);
    emit actionsUpdated(d->m_current_editor.data());
  }
}

auto DesignMode::updateActions() -> void
{
  emit actionsUpdated(d->m_current_editor.data());
}

auto DesignMode::updateContext(const Utils::Id new_mode, const Utils::Id old_mode) const -> void
{
  if (new_mode == id())
    ICore::addAdditionalContext(d->m_active_context);
  else if (old_mode == id())
    ICore::removeAdditionalContext(d->m_active_context);
}

auto DesignMode::setActiveContext(const Context &context) const -> void
{
  if (d->m_active_context == context)
    return;

  if (ModeManager::currentModeId() == id())
    ICore::updateAdditionalContexts(d->m_active_context, context);

  d->m_active_context = context;
}

auto DesignMode::createModeIfRequired() -> void
{
  if (d) {
    m_instance = new DesignMode;
    ExtensionSystem::PluginManager::addObject(m_instance);
  }
}

auto DesignMode::destroyModeIfRequired() -> void
{
  if (m_instance) {
    ExtensionSystem::PluginManager::removeObject(m_instance);
    delete m_instance;
  }
  delete d;
}

} // namespace Core
