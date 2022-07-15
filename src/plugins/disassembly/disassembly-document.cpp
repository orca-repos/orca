// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "disassembly-document.hpp"

#include <utils/qtcassert.hpp>

namespace Orca::Plugin::Disassembly {

DisassemblyDocument::DisassemblyDocument(DisassemblyWidget *parent): IDocument(parent)
{
  m_widget = parent;
}

auto DisassemblyDocument::open(QString *error_string, const Utils::FilePath &file_path, const Utils::FilePath &real_file_path) -> OpenResult
{
  QTC_CHECK(file_path == real_file_path);
  return openImpl(error_string, file_path);
}

auto DisassemblyDocument::openImpl(QString *error_string, const Utils::FilePath &file_path, quint64 offset) -> OpenResult
{
  return OpenResult::Success;
}

} // namespace Orca::Plugin::Disassembly
