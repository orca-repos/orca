// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "disassembly-document.hpp"
#include "disassembly-widget.hpp"

#include <core/core-document-interface.hpp>
#include <core/core-editor-interface.hpp>

#include <QToolBar>

namespace Orca::Plugin::Disassembly {

class Disassembly : public Core::IEditor {
  Q_OBJECT

public:
  Disassembly(DisassemblyWidget *widget);
  ~Disassembly() override;

  auto document() const -> DisassemblyDocument* override;
  auto toolBar() -> QWidget* override;

private:
  DisassemblyDocument *m_file;
  QToolBar *m_tool_bar;
};

} // namespace Orca::Plugin::Disassembly
