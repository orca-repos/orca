// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "disassembly-widget.hpp"

#include <core/core-document-interface.hpp>

namespace Orca::Plugin::Disassembly {

class DisassemblyDocument : public Core::IDocument {
  Q_OBJECT

public:
  DisassemblyDocument(DisassemblyWidget *parent);

  auto open(QString *error_string, const Utils::FilePath &file_path, const Utils::FilePath &real_file_path) -> OpenResult override;
  static auto openImpl(QString *error_string, const Utils::FilePath &file_path, quint64 offset = 0) -> OpenResult;

private:
    DisassemblyWidget *m_widget;
};

} // namespace Orca::Plugin::Disassembly
