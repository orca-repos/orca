// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "disassembly-factory.hpp"

#include "disassembly-constants.hpp"
#include "disassembly.hpp"

#include <aggregation/aggregate.hpp>

#include <core/core-constants.hpp>
#include <core/core-editor-manager.hpp>

namespace Orca::Plugin::Disassembly {

DisassemblyFactory::DisassemblyFactory()
{
  setId(C_DISASSEMBLY_ID);
  setDisplayName(QCoreApplication::translate("OpenWith::Editors", C_DISASSEMBLY_DISPLAY_NAME));
  addMimeType(C_DISASSEMBLY_MIMETYPE);

  setEditorCreator([] {
    const auto widget = new DisassemblyWidget();
    const auto editor = new Disassembly(widget);
    const auto aggregate = new Aggregation::Aggregate;
    aggregate->add(widget);
    return editor;
  });
}

auto FactoryServiceImpl::createDisassemblyService(const QString &title0, const bool wants_editor) -> DisassemblyService*
{
  DisassemblyWidget *widget = nullptr;

  if (wants_editor) {
    auto title = title0;
    const auto editor = Core::EditorManager::openEditorWithContents(Core::K_DEFAULT_BINARY_EDITOR_ID, &title);

    if (!editor)
      return nullptr;

    widget = qobject_cast<DisassemblyWidget*>(editor->widget());
    widget->setEditor(editor);
  }
  else {
    widget = new DisassemblyWidget;
    widget->setWindowTitle(title0);
  }

  return widget->disassemblyService();
}

} // namespace Orca::Plugin::Disassembly
