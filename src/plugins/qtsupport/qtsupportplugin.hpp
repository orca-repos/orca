// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace QtSupport {
namespace Internal {

class QtSupportPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "qtsupport.json")

public:
  ~QtSupportPlugin() final;

private:
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool final;
  auto extensionsInitialized() -> void final;

  class QtSupportPluginPrivate *d = nullptr;

#ifdef WITH_TESTS
private slots:
    void testQtOutputParser_data();
    void testQtOutputParser();
    void testQtTestOutputParser();
    void testQtOutputFormatter_data();
    void testQtOutputFormatter();
    void testQtOutputFormatter_appendMessage_data();
    void testQtOutputFormatter_appendMessage();
    void testQtOutputFormatter_appendMixedAssertAndAnsi();

    void testQtProjectImporter_oneProject_data();
    void testQtProjectImporter_oneProject();

    void testQtBuildStringParsing_data();
    void testQtBuildStringParsing();

#if 0
    void testQtProjectImporter_oneProjectExistingKit();
    void testQtProjectImporter_oneProjectNewKitExistingQt();
    void testQtProjectImporter_oneProjectNewKitNewQt();
    void testQtProjectImporter_oneProjectTwoNewKitSameNewQt_pc();
    void testQtProjectImporter_oneProjectTwoNewKitSameNewQt_cp();
    void testQtProjectImporter_oneProjectTwoNewKitSameNewQt_cc();
#endif
#endif
};

} // namespace Internal
} // namespace QtSupport
