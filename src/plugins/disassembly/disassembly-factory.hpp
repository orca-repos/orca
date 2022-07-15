// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "disassembly-service.hpp"

#include <core/core-editor-factory-interface.hpp>

namespace Orca::Plugin::Disassembly {

class DisassemblyFactory final : public Core::IEditorFactory {
public:
  DisassemblyFactory();
};

class FactoryServiceImpl : public QObject, public FactoryService {
  Q_OBJECT
  Q_INTERFACES(Orca::Plugin::Disassembly::FactoryService)

public:
  auto createDisassemblyService(const QString &title0, bool wants_editor) -> DisassemblyService* override;
};

} // namespace Orca::Plugin::Disassembly
