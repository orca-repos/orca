// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iplugin.h"

namespace ExtensionSystem {

class PluginSpec;

namespace Internal {

class IPluginPrivate {
public:
  PluginSpec *pluginSpec;
};

} // namespace Internal
} // namespace ExtensionSystem
