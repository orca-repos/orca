// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/osspecificaspects.hpp>

#include <QList>
#include <QHash>

#include <vector>

namespace Utils { class FilePath; }

namespace ProjectExplorer {

// --------------------------------------------------------------------------
// ABI (documentation inside)
// --------------------------------------------------------------------------

class Abi;
using Abis = QVector<Abi>;

class PROJECTEXPLORER_EXPORT Abi {
public:
  enum Architecture {
    ArmArchitecture,
    X86Architecture,
    ItaniumArchitecture,
    MipsArchitecture,
    PowerPCArchitecture,
    ShArchitecture,
    AvrArchitecture,
    Avr32Architecture,
    XtensaArchitecture,
    Mcs51Architecture,
    Mcs251Architecture,
    AsmJsArchitecture,
    Stm8Architecture,
    Msp430Architecture,
    Rl78Architecture,
    C166Architecture,
    V850Architecture,
    Rh850Architecture,
    RxArchitecture,
    K78Architecture,
    M68KArchitecture,
    M32CArchitecture,
    M16CArchitecture,
    M32RArchitecture,
    R32CArchitecture,
    CR16Architecture,
    RiscVArchitecture,
    UnknownArchitecture
  };

  enum OS {
    BsdOS,
    LinuxOS,
    DarwinOS,
    UnixOS,
    WindowsOS,
    VxWorks,
    QnxOS,
    BareMetalOS,
    UnknownOS
  };

  enum OSFlavor {
    // BSDs
    FreeBsdFlavor,
    NetBsdFlavor,
    OpenBsdFlavor,

    // Linux
    AndroidLinuxFlavor,

    // Unix
    SolarisUnixFlavor,

    // Windows
    WindowsMsvc2005Flavor,
    WindowsMsvc2008Flavor,
    WindowsMsvc2010Flavor,
    WindowsMsvc2012Flavor,
    WindowsMsvc2013Flavor,
    WindowsMsvc2015Flavor,
    WindowsMsvc2017Flavor,
    WindowsMsvc2019Flavor,
    WindowsMsvc2022Flavor,
    WindowsLastMsvcFlavor = WindowsMsvc2022Flavor,
    WindowsMSysFlavor,
    WindowsCEFlavor,

    // Embedded
    VxWorksFlavor,

    // Generic:
    RtosFlavor,
    GenericFlavor,

    UnknownFlavor // keep last in this enum!
  };

  enum BinaryFormat {
    ElfFormat,
    MachOFormat,
    PEFormat,
    RuntimeQmlFormat,
    UbrofFormat,
    OmfFormat,
    EmscriptenFormat,
    UnknownFormat
  };

  Abi(const Architecture &a = UnknownArchitecture, const OS &o = UnknownOS, const OSFlavor &so = UnknownFlavor, const BinaryFormat &f = UnknownFormat, unsigned char w = 0, const QString &p = {});

  static auto abiFromTargetTriplet(const QString &machineTriple) -> Abi;
  static auto abiOsToOsType(const OS os) -> Utils::OsType;

  auto operator !=(const Abi &other) const -> bool;
  auto operator ==(const Abi &other) const -> bool;
  auto isCompatibleWith(const Abi &other) const -> bool;
  auto isValid() const -> bool;
  auto isNull() const -> bool;
  auto architecture() const -> Architecture { return m_architecture; }
  auto os() const -> OS { return m_os; }
  auto osFlavor() const -> OSFlavor { return m_osFlavor; }
  auto binaryFormat() const -> BinaryFormat { return m_binaryFormat; }
  auto wordWidth() const -> unsigned char { return m_wordWidth; }
  auto toString() const -> QString;
  auto param() const -> QString;

  static auto toString(const Architecture &a) -> QString;
  static auto toString(const OS &o) -> QString;
  static auto toString(const OSFlavor &of) -> QString;
  static auto toString(const BinaryFormat &bf) -> QString;
  static auto toString(int w) -> QString;
  static auto architectureFromString(const QString &a) -> Architecture;
  static auto osFromString(const QString &o) -> OS;
  static auto osFlavorFromString(const QString &of, const OS os) -> OSFlavor;
  static auto binaryFormatFromString(const QString &bf) -> BinaryFormat;
  static auto wordWidthFromString(const QString &w) -> unsigned char;
  static auto registerOsFlavor(const std::vector<OS> &oses, const QString &flavorName) -> OSFlavor;
  static auto flavorsForOs(const OS &o) -> QList<OSFlavor>;
  static auto allOsFlavors() -> QList<OSFlavor>;
  static auto osSupportsFlavor(const OS &os, const OSFlavor &flavor) -> bool;
  static auto flavorForMsvcVersion(int version) -> OSFlavor;
  static auto fromString(const QString &abiString) -> Abi;
  static auto hostAbi() -> Abi;
  static auto abisOfBinary(const Utils::FilePath &path) -> Abis;

  friend auto qHash(const Abi &abi)
  {
    const auto h = abi.architecture() + (abi.os() << 3) + (abi.osFlavor() << 6) + (abi.binaryFormat() << 10) + (abi.wordWidth() << 13);
    return QT_PREPEND_NAMESPACE(qHash)(h);
  }

private:
  Architecture m_architecture;
  OS m_os;
  OSFlavor m_osFlavor;
  BinaryFormat m_binaryFormat;
  unsigned char m_wordWidth;
  QString m_param;
};

} // namespace ProjectExplorer
