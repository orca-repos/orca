// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "disassembly-service.hpp"

#include <QAbstractScrollArea>

namespace Orca::Plugin {
namespace Core {

class IEditor;

} // namespace Core

namespace Disassembly {

class DisassemblyWidgetPrivate;

class DisassemblyWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  DisassemblyWidget(QWidget *parent = nullptr);
  ~DisassemblyWidget() override;

  // The disassembly widget is embed into a Core::IEditor.
  auto disassemblyService() const -> DisassemblyService*;
  auto editor() const -> Core::IEditor*;
  auto setEditor(Core::IEditor *editor_interface) -> void ;

private:
  friend class DisassemblyWidgetPrivate;
  DisassemblyWidgetPrivate *d;
  Core::IEditor *m_editor_interface;
};

} // namespace Disassembly
} // namespace Orca::Plugin
