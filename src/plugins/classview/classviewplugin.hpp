// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace ClassView {
namespace Internal {

class ClassViewPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.OrcaPlugin" FILE "ClassView.json")

public:
  ClassViewPlugin() = default;
  ~ClassViewPlugin() final;

private:
  auto initialize(const QStringList &arguments, QString *errorMessage = nullptr) -> bool final;
  auto extensionsInitialized() -> void final {}
};

} // namespace Internal
} // namespace ClassView