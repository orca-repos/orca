// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace Orca::Plugin {
namespace Core {

class IEditor;

} // namespace Core

namespace Disassembly {

class DisassemblyService {
public:
  virtual ~DisassemblyService() = default;

  virtual auto widget() -> QWidget* = 0;
  virtual auto editor() -> Core::IEditor* = 0;
};

// Create a Disassembly widget. Embed into a Core::IEditor iff wantsEditor == true.
class FactoryService {
public:
  virtual ~FactoryService() = default;
  virtual auto createDisassemblyService(const QString &title, bool wants_editor) -> DisassemblyService* = 0;
};

} // namespace Disassembly
} // namespace Orca::Plugin

Q_DECLARE_INTERFACE(Orca::Plugin::Disassembly::FactoryService, "org.orca-repos.orca.disassembly.factory.service")
