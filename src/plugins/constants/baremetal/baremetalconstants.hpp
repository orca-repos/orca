// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace BareMetal {
namespace Constants {

constexpr char BareMetalOsType[] = "BareMetalOsType";

constexpr char ACTION_ID[] = "BareMetal.Action";
constexpr char MENU_ID[] = "BareMetal.Menu";
constexpr char DEBUG_SERVER_PROVIDERS_SETTINGS_ID[] = "EE.BareMetal.DebugServerProvidersOptions";

// GDB Debugger Server Provider Ids.
constexpr char GDBSERVER_OPENOCD_PROVIDER_ID[] = "BareMetal.GdbServerProvider.OpenOcd";
constexpr char GDBSERVER_JLINK_PROVIDER_ID[] = "BareMetal.GdbServerProvider.JLink";
constexpr char GDBSERVER_GENERIC_PROVIDER_ID[] = "BareMetal.GdbServerProvider.Generic";
constexpr char GDBSERVER_STLINK_UTIL_PROVIDER_ID[] = "BareMetal.GdbServerProvider.STLinkUtil";
constexpr char GDBSERVER_EBLINK_PROVIDER_ID[] = "BareMetal.GdbServerProvider.EBlink";

// uVision Debugger Server Provider Ids.
constexpr char UVSC_SIMULATOR_PROVIDER_ID[] = "BareMetal.UvscServerProvider.Simulator";
constexpr char UVSC_STLINK_PROVIDER_ID[] = "BareMetal.UvscServerProvider.StLink";
constexpr char UVSC_JLINK_PROVIDER_ID[] = "BareMetal.UvscServerProvider.JLink";

// Toolchain types.
constexpr char IAREW_TOOLCHAIN_TYPEID[] = "BareMetal.ToolChain.Iar";
constexpr char KEIL_TOOLCHAIN_TYPEID[] = "BareMetal.ToolChain.Keil";
constexpr char SDCC_TOOLCHAIN_TYPEID[] = "BareMetal.ToolChain.Sdcc";

} // namespace BareMetal
} // namespace Constants
