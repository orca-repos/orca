// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "msvctoolchain.hpp"

#include "gcctoolchain.hpp"
#include "msvcparser.hpp"
#include "projectexplorer.hpp"
#include "projectexplorerconstants.hpp"
#include "projectexplorersettings.hpp"
#include "taskhub.hpp"
#include "toolchainmanager.hpp"

#include <core/icore.hpp>

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/runextensions.hpp>
#include <utils/temporarydirectory.hpp>
#include <utils/pathchooser.hpp>
#include <utils/winutils.hpp>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCodec>
#include <QVector>
#include <QVersionNumber>

#include <QLabel>
#include <QComboBox>
#include <QFormLayout>

using namespace Utils;

#define KEY_ROOT "ProjectExplorer.MsvcToolChain."
static constexpr char varsBatKeyC[] = KEY_ROOT "VarsBat";
static constexpr char varsBatArgKeyC[] = KEY_ROOT "VarsBatArg";
static constexpr char environModsKeyC[] = KEY_ROOT "environmentModifications";

static Q_LOGGING_CATEGORY(Log, "qtc.projectexplorer.toolchain.msvc", QtWarningMsg);

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------------

static auto envModThreadPool() -> QThreadPool*
{
  static QThreadPool *pool = nullptr;
  if (!pool) {
    pool = new QThreadPool(ProjectExplorerPlugin::instance());
    pool->setMaxThreadCount(1);
  }
  return pool;
}

struct MsvcPlatform {
  MsvcToolChain::Platform platform;
  const char *name;
  const char *prefix; // VS up until 14.0 (MSVC2015)
  const char *bat;
};

const MsvcPlatform platforms[] = {{MsvcToolChain::x86, "x86", "/bin", "vcvars32.bat"}, {MsvcToolChain::amd64, "amd64", "/bin/amd64", "vcvars64.bat"}, {MsvcToolChain::x86_amd64, "x86_amd64", "/bin/x86_amd64", "vcvarsx86_amd64.bat"}, {MsvcToolChain::ia64, "ia64", "/bin/ia64", "vcvars64.bat"}, {MsvcToolChain::x86_ia64, "x86_ia64", "/bin/x86_ia64", "vcvarsx86_ia64.bat"}, {MsvcToolChain::arm, "arm", "/bin/arm", "vcvarsarm.bat"}, {MsvcToolChain::x86_arm, "x86_arm", "/bin/x86_arm", "vcvarsx86_arm.bat"}, {MsvcToolChain::amd64_arm, "amd64_arm", "/bin/amd64_arm", "vcvarsamd64_arm.bat"}, {MsvcToolChain::amd64_x86, "amd64_x86", "/bin/amd64_x86", "vcvarsamd64_x86.bat"}, {MsvcToolChain::x86_arm64, "x86_arm64", "/bin/x86_arm64", "vcvarsx86_arm64.bat"}, {MsvcToolChain::amd64_arm64, "amd64_arm64", "/bin/amd64_arm64", "vcvarsamd64_arm64.bat"}};

static QList<const MsvcToolChain*> g_availableMsvcToolchains;

static auto platformEntryFromName(const QString &name) -> const MsvcPlatform*
{
  for (const auto &p : platforms) {
    if (name == QLatin1String(p.name))
      return &p;
  }
  return nullptr;
}

static auto platformEntry(MsvcToolChain::Platform t) -> const MsvcPlatform*
{
  for (const auto &p : platforms) {
    if (p.platform == t)
      return &p;
  }
  return nullptr;
}

static auto platformName(MsvcToolChain::Platform t) -> QString
{
  if (const auto p = platformEntry(t))
    return QLatin1String(p->name);
  return QString();
}

static auto hostPrefersPlatform(MsvcToolChain::Platform platform) -> bool
{
  switch (HostOsInfo::hostArchitecture()) {
  case HostOsInfo::HostArchitectureAMD64:
    return platform == MsvcToolChain::amd64 || platform == MsvcToolChain::amd64_arm || platform == MsvcToolChain::amd64_x86 || platform == MsvcToolChain::amd64_arm64;
  case HostOsInfo::HostArchitectureX86:
    return platform == MsvcToolChain::x86 || platform == MsvcToolChain::x86_amd64 || platform == MsvcToolChain::x86_ia64 || platform == MsvcToolChain::x86_arm || platform == MsvcToolChain::x86_arm64;
  case HostOsInfo::HostArchitectureArm:
    return platform == MsvcToolChain::arm;
  case HostOsInfo::HostArchitectureItanium:
    return platform == MsvcToolChain::ia64;
  default:
    return false;
  }
}

static auto hostSupportsPlatform(MsvcToolChain::Platform platform) -> bool
{
  if (hostPrefersPlatform(platform))
    return true;
  // The x86 host toolchains are not the preferred toolchains on amd64 but they are still
  // supported by that host
  return HostOsInfo::hostArchitecture() == HostOsInfo::HostArchitectureAMD64 && (platform == MsvcToolChain::x86 || platform == MsvcToolChain::x86_amd64 || platform == MsvcToolChain::x86_ia64 || platform == MsvcToolChain::x86_arm || platform == MsvcToolChain::x86_arm64);
}

static auto fixRegistryPath(const QString &path) -> QString
{
  auto result = QDir::fromNativeSeparators(path);
  if (result.endsWith(QLatin1Char('/')))
    result.chop(1);
  return result;
}

struct VisualStudioInstallation {
  QString vsName;
  QVersionNumber version;
  QString path;       // Main installation path
  QString vcVarsPath; // Path under which the various vc..bat are to be found
  QString vcVarsAll;
};

auto operator<<(QDebug d, const VisualStudioInstallation &i) -> QDebug
{
  QDebugStateSaver saver(d);
  d.noquote();
  d.nospace();
  d << "VisualStudioInstallation(\"" << i.vsName << "\", v=" << i.version << ", path=\"" << QDir::toNativeSeparators(i.path) << "\", vcVarsPath=\"" << QDir::toNativeSeparators(i.vcVarsPath) << "\", vcVarsAll=\"" << QDir::toNativeSeparators(i.vcVarsAll) << "\")";
  return d;
}

static auto windowsProgramFilesDir() -> QString
{
  #ifdef Q_OS_WIN64
  const char programFilesC[] = "ProgramFiles(x86)";
  #else
    const char programFilesC[] = "ProgramFiles";
  #endif
  return QDir::fromNativeSeparators(QFile::decodeName(qgetenv(programFilesC)));
}

static auto installationFromPathAndVersion(const QString &installationPath, const QVersionNumber &version) -> optional<VisualStudioInstallation>
{
  auto vcVarsPath = QDir::fromNativeSeparators(installationPath);
  if (!vcVarsPath.endsWith('/'))
    vcVarsPath += '/';
  if (version.majorVersion() > 14)
    vcVarsPath += QStringLiteral("VC/Auxiliary/Build");
  else
    vcVarsPath += QStringLiteral("VC");

  const QString vcVarsAllPath = vcVarsPath + QStringLiteral("/vcvarsall.bat");
  if (!QFileInfo(vcVarsAllPath).isFile()) {
    qWarning().noquote() << "Unable to find MSVC setup script " << QDir::toNativeSeparators(vcVarsPath) << " in version " << version;
    return nullopt;
  }

  const auto versionString = version.toString();
  VisualStudioInstallation installation;
  installation.path = installationPath;
  installation.version = version;
  installation.vsName = versionString;
  installation.vcVarsPath = vcVarsPath;
  installation.vcVarsAll = vcVarsAllPath;
  return installation;
}

// Detect build tools introduced with MSVC2017
static auto detectCppBuildTools2017() -> optional<VisualStudioInstallation>
{
  const QString installPath = windowsProgramFilesDir() + "/Microsoft Visual Studio/2017/BuildTools";
  const QString vcVarsPath = installPath + "/VC/Auxiliary/Build";
  const QString vcVarsAllPath = vcVarsPath + "/vcvarsall.bat";

  if (!QFileInfo::exists(vcVarsAllPath))
    return nullopt;

  VisualStudioInstallation installation;
  installation.path = installPath;
  installation.vcVarsAll = vcVarsAllPath;
  installation.vcVarsPath = vcVarsPath;
  installation.version = QVersionNumber(15);
  installation.vsName = "15.0";

  return installation;
}

static auto detectVisualStudioFromVsWhere(const QString &vswhere) -> QVector<VisualStudioInstallation>
{
  QVector<VisualStudioInstallation> installations;
  QtcProcess vsWhereProcess;
  vsWhereProcess.setCodec(QTextCodec::codecForName("UTF-8"));
  const auto timeoutS = 5;
  vsWhereProcess.setTimeoutS(timeoutS);
  vsWhereProcess.setCommand({FilePath::fromString(vswhere), {"-products", "*", "-prerelease", "-legacy", "-format", "json", "-utf8"}});
  vsWhereProcess.runBlocking();
  switch (vsWhereProcess.result()) {
  case QtcProcess::FinishedWithSuccess:
    break;
  case QtcProcess::StartFailed: qWarning().noquote() << QDir::toNativeSeparators(vswhere) << "could not be started.";
    return installations;
  case QtcProcess::FinishedWithError: qWarning().noquote().nospace() << QDir::toNativeSeparators(vswhere) << " finished with exit code " << vsWhereProcess.exitCode() << ".";
    return installations;
  case QtcProcess::TerminatedAbnormally: qWarning().noquote().nospace() << QDir::toNativeSeparators(vswhere) << " crashed. Exit code: " << vsWhereProcess.exitCode();
    return installations;
  case QtcProcess::Hang: qWarning().noquote() << QDir::toNativeSeparators(vswhere) << "did not finish in" << timeoutS << "seconds.";
    return installations;
  }

  const auto output = vsWhereProcess.stdOut().toUtf8();
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(output, &error);
  if (error.error != QJsonParseError::NoError || doc.isNull()) {
    qWarning() << "Could not parse json document from vswhere output.";
    return installations;
  }

  const auto versions = doc.array();
  if (versions.isEmpty()) {
    qWarning() << "Could not detect any versions from vswhere output.";
    return installations;
  }

  for (const QJsonValue &vsVersion : versions) {
    const auto vsVersionObj = vsVersion.toObject();
    if (vsVersionObj.isEmpty()) {
      qWarning() << "Could not obtain object from vswhere version";
      continue;
    }

    auto value = vsVersionObj.value("installationVersion");
    if (value.isUndefined()) {
      qWarning() << "Could not obtain VS version from json output";
      continue;
    }
    const auto versionString = value.toString();
    auto version = QVersionNumber::fromString(versionString);
    value = vsVersionObj.value("installationPath");
    if (value.isUndefined()) {
      qWarning() << "Could not obtain VS installation path from json output";
      continue;
    }
    const auto installationPath = value.toString();
    auto installation = installationFromPathAndVersion(installationPath, version);

    if (installation)
      installations.append(*installation);
  }
  return installations;
}

static auto detectVisualStudioFromRegistry() -> QVector<VisualStudioInstallation>
{
  QVector<VisualStudioInstallation> result;
  #ifdef Q_OS_WIN64
  const auto keyRoot = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\");
  #else
    const QString keyRoot = QStringLiteral(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\");
  #endif
  QSettings vsRegistry(keyRoot + QStringLiteral("VS7"), QSettings::NativeFormat);
  QScopedPointer<QSettings> vcRegistry;
  foreach(const QString &vsName, vsRegistry.allKeys()) {
    const auto version = QVersionNumber::fromString(vsName);
    if (!version.isNull()) {
      const auto installationPath = fixRegistryPath(vsRegistry.value(vsName).toString());

      auto installation = installationFromPathAndVersion(installationPath, version);
      if (installation)
        result.append(*installation);
    }
  }

  // Detect VS 2017 Build Tools
  auto installation = detectCppBuildTools2017();
  if (installation)
    result.append(*installation);

  return result;
}

static auto detectVisualStudio() -> QVector<VisualStudioInstallation>
{
  const QString vswhere = windowsProgramFilesDir() + "/Microsoft Visual Studio/Installer/vswhere.exe";
  if (QFileInfo::exists(vswhere)) {
    const auto installations = detectVisualStudioFromVsWhere(vswhere);
    if (!installations.isEmpty())
      return installations;
  }

  return detectVisualStudioFromRegistry();
}

static auto wordWidthForPlatform(MsvcToolChain::Platform platform) -> unsigned char
{
  switch (platform) {
  case MsvcToolChain::x86:
  case MsvcToolChain::arm:
  case MsvcToolChain::x86_arm:
  case MsvcToolChain::amd64_arm:
  case MsvcToolChain::amd64_x86:
    return 32;
  case MsvcToolChain::amd64:
  case MsvcToolChain::x86_amd64:
  case MsvcToolChain::ia64:
  case MsvcToolChain::x86_ia64:
  case MsvcToolChain::amd64_arm64:
  case MsvcToolChain::x86_arm64:
    return 64;
  }

  return 0;
}

static auto archForPlatform(MsvcToolChain::Platform platform) -> Abi::Architecture
{
  switch (platform) {
  case MsvcToolChain::x86:
  case MsvcToolChain::amd64:
  case MsvcToolChain::x86_amd64:
  case MsvcToolChain::amd64_x86:
    return Abi::X86Architecture;
  case MsvcToolChain::arm:
  case MsvcToolChain::x86_arm:
  case MsvcToolChain::amd64_arm:
  case MsvcToolChain::x86_arm64:
  case MsvcToolChain::amd64_arm64:
    return Abi::ArmArchitecture;
  case MsvcToolChain::ia64:
  case MsvcToolChain::x86_ia64:
    return Abi::ItaniumArchitecture;
  }

  return Abi::UnknownArchitecture;
}

static auto findAbiOfMsvc(MsvcToolChain::Type type, MsvcToolChain::Platform platform, const QString &version) -> Abi
{
  auto flavor = Abi::UnknownFlavor;

  auto msvcVersionString = version;
  if (type == MsvcToolChain::WindowsSDK) {
    if (version == QLatin1String("v7.0") || version.startsWith(QLatin1String("6.")))
      msvcVersionString = QLatin1String("9.0");
    else if (version == QLatin1String("v7.0A") || version == QLatin1String("v7.1"))
      msvcVersionString = QLatin1String("10.0");
  }
  if (msvcVersionString.startsWith(QLatin1String("17.")))
    flavor = Abi::WindowsMsvc2022Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("16.")))
    flavor = Abi::WindowsMsvc2019Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("15.")))
    flavor = Abi::WindowsMsvc2017Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("14.")))
    flavor = Abi::WindowsMsvc2015Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("12.")))
    flavor = Abi::WindowsMsvc2013Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("11.")))
    flavor = Abi::WindowsMsvc2012Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("10.")))
    flavor = Abi::WindowsMsvc2010Flavor;
  else if (msvcVersionString.startsWith(QLatin1String("9.")))
    flavor = Abi::WindowsMsvc2008Flavor;
  else
    flavor = Abi::WindowsMsvc2005Flavor;
  const auto result = Abi(archForPlatform(platform), Abi::WindowsOS, flavor, Abi::PEFormat, wordWidthForPlatform(platform));
  if (!result.isValid())
    qWarning("Unable to completely determine the ABI of MSVC version %s (%s).", qPrintable(version), qPrintable(result.toString()));
  return result;
}

static auto generateDisplayName(const QString &name, MsvcToolChain::Type t, MsvcToolChain::Platform p) -> QString
{
  if (t == MsvcToolChain::WindowsSDK) {
    auto sdkName = name;
    sdkName += QString::fromLatin1(" (%1)").arg(platformName(p));
    return sdkName;
  }
  // Comes as "9.0" from the registry
  QString vcName = QLatin1String("Microsoft Visual C++ Compiler ");
  vcName += name;
  vcName += QString::fromLatin1(" (%1)").arg(platformName(p));
  return vcName;
}

static auto msvcCompilationDefine(const char *def) -> QByteArray
{
  const QByteArray macro(def);
  return "#if defined(" + macro + ")\n__PPOUT__(" + macro + ")\n#endif\n";
}

static auto msvcCompilationFile() -> QByteArray
{
  static const char *macros[] = {"_ATL_VER", "__ATOM__", "__AVX__", "__AVX2__", "_CHAR_UNSIGNED", "__CLR_VER", "_CMMN_INTRIN_FUNC", "_CONTROL_FLOW_GUARD", "__cplusplus", "__cplusplus_cli", "__cplusplus_winrt", "_CPPLIB_VER", "_CPPRTTI", "_CPPUNWIND", "_DEBUG", "_DLL", "_INTEGRAL_MAX_BITS", "__INTELLISENSE__", "_ISO_VOLATILE", "_KERNEL_MODE", "_M_AAMD64", "_M_ALPHA", "_M_AMD64", "_MANAGED", "_M_ARM", "_M_ARM64", "_M_ARM_ARMV7VE", "_M_ARM_FP", "_M_ARM_NT", "_M_ARMT", "_M_CEE", "_M_CEE_PURE", "_M_CEE_SAFE", "_MFC_VER", "_M_FP_EXCEPT", "_M_FP_FAST", "_M_FP_PRECISE", "_M_FP_STRICT", "_M_IA64", "_M_IX86", "_M_IX86_FP", "_M_MPPC", "_M_MRX000", "_M_PPC", "_MSC_BUILD", "_MSC_EXTENSIONS", "_MSC_FULL_VER", "_MSC_VER", "_MSVC_LANG", "__MSVC_RUNTIME_CHECKS", "_MT", "_M_THUMB", "_M_X64", "_NATIVE_WCHAR_T_DEFINED", "_OPENMP", "_PREFAST_", "__STDC__", "__STDC_HOSTED__", "__STDCPP_THREADS__", "_VC_NODEFAULTLIB", "_WCHAR_T_DEFINED", "_WIN32", "_WIN32_WCE", "_WIN64", "_WINRT_DLL", "_Wp64", nullptr};
  QByteArray file = "#define __PPOUT__(x) V##x=x\n\n";
  for (auto i = 0; macros[i] != nullptr; ++i)
    file += msvcCompilationDefine(macros[i]);
  file += "\nvoid main(){}\n\n";
  return file;
}

// Run MSVC 'cl' compiler to obtain #defines.
// This function must be thread-safe!
//
// Some notes regarding the used approach:
//
// It seems that there is no reliable way to get all the
// predefined macros for a cl invocation. The following two
// approaches are unfortunately limited since both lead to an
// incomplete list of actually predefined macros and come with
// other problems, too.
//
// 1) Maintain a list of predefined macros from the official
//    documentation (for MSVC2015, e.g. [1]). Feed cl with a
//    temporary file that queries the values of those macros.
//
//    Problems:
//     * Maintaining that list.
//     * The documentation is incomplete, we do not get all
//       predefined macros. E.g. the cl from MSVC2015, set up
//       with "vcvars.bat x86_arm", predefines among others
//       _M_ARMT, but that's not reflected in the
//       documentation.
//
// 2) Run cl with the undocumented options /B1 and /Bx, as
//    described in [2].
//
//    Note: With qmake from Qt >= 5.8 it's possible to print
//    the macros formatted as preprocessor code in an easy to
//    read/compare/diff way:
//
//      > cl /nologo /c /TC /B1 qmake NUL
//      > cl /nologo /c /TP /Bx qmake NUL
//
//    Problems:
//     * Using undocumented options.
//     * Resulting macros are incomplete.
//       For example, the nowadays default option /Zc:wchar_t
//       predefines _WCHAR_T_DEFINED, but this is not reflected
//       with this approach.
//
//       To work around this we would need extra cl invocations
//       to get the actual values of the missing macros
//       (approach 1).
//
// Currently we combine both approaches in this way:
//  * As base, maintain the list from the documentation and
//    update it once a new MSVC version is released.
//  * Enrich it with macros that we discover with approach 2
//    once a new MSVC version is released.
//  * Enrich it further with macros that are not covered with
//    the above points.
//
// TODO: Update the predefined macros for MSVC 2017 once the
//       page exists.
//
// [1] https://msdn.microsoft.com/en-us/library/b0084kay.aspx
// [2] http://stackoverflow.com/questions/3665537/how-to-find-out-cl-exes-built-in-macros
auto MsvcToolChain::msvcPredefinedMacros(const QStringList &cxxflags, const Environment &env) const -> Macros
{
  Macros predefinedMacros;

  QStringList toProcess;
  for (const auto &arg : cxxflags) {
    if (arg.startsWith("/D") || arg.startsWith("-D")) {
      const auto define = arg.mid(2);
      predefinedMacros.append(Macro::fromKeyValue(define));
    } else if (arg.startsWith("/U") || arg.startsWith("-U")) {
      predefinedMacros.append({arg.mid(2).toLocal8Bit(), MacroType::Undefine});
    } else {
      toProcess.append(arg);
    }
  }

  TempFileSaver saver(TemporaryDirectory::masterDirectoryPath() + "/envtestXXXXXX.cpp");
  saver.write(msvcCompilationFile());
  if (!saver.finalize()) {
    qWarning("%s: %s", Q_FUNC_INFO, qPrintable(saver.errorString()));
    return predefinedMacros;
  }
  QtcProcess cpp;
  cpp.setEnvironment(env);
  cpp.setWorkingDirectory(TemporaryDirectory::masterDirectoryFilePath());
  QStringList arguments;
  const auto binary = env.searchInPath(QLatin1String("cl.exe"));
  if (binary.isEmpty()) {
    qWarning("%s: The compiler binary cl.exe could not be found in the path.", Q_FUNC_INFO);
    return predefinedMacros;
  }

  if (language() == Constants::C_LANGUAGE_ID)
    arguments << QLatin1String("/TC");
  arguments << toProcess << QLatin1String("/EP") << saver.filePath().toUserOutput();
  cpp.setCommand({binary, arguments});
  cpp.runBlocking();
  if (cpp.result() != QtcProcess::FinishedWithSuccess)
    return predefinedMacros;

  const auto output = filtered(cpp.stdOut().split('\n'), [](const QString &s) { return s.startsWith('V'); });
  for (const auto &line : output)
    predefinedMacros.append(Macro::fromKeyValue(line.mid(1)));
  return predefinedMacros;
}

//
// We want to detect the language version based on the predefined macros.
// Unfortunately MSVC does not conform to standard when it comes to the predefined
// __cplusplus macro - it reports "199711L", even for newer language versions.
//
// However:
//   * For >= Visual Studio 2015 Update 3 predefines _MSVC_LANG which has the proper value
//     of __cplusplus.
//     See https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2017
//   * For >= Visual Studio 2017 Version 15.7 __cplusplus is correct once /Zc:__cplusplus
//     is provided on the command line. Then __cplusplus == _MSVC_LANG.
//     See https://blogs.msdn.microsoft.com/vcblog/2018/04/09/msvc-now-correctly-reports-__cplusplus
//
// We rely on _MSVC_LANG if possible, otherwise on some hard coded language versions
// depending on _MSC_VER.
//
// For _MSV_VER values, see https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2017.
//
auto MsvcToolChain::msvcLanguageVersion(const QStringList & /*cxxflags*/, const Id &language, const Macros &macros) const -> LanguageVersion
{
  using Utils::LanguageVersion;

  auto mscVer = -1;
  QByteArray msvcLang;
  for (const auto &macro : macros) {
    if (macro.key == "_MSVC_LANG")
      msvcLang = macro.value;
    if (macro.key == "_MSC_VER")
      mscVer = macro.value.toInt(nullptr);
  }
  QTC_CHECK(mscVer > 0);

  if (language == Constants::CXX_LANGUAGE_ID) {
    if (!msvcLang.isEmpty()) // >= Visual Studio 2015 Update 3
      return cxxLanguageVersion(msvcLang);
    if (mscVer >= 1800) // >= Visual Studio 2013 (12.0)
      return LanguageVersion::CXX14;
    if (mscVer >= 1600) // >= Visual Studio 2010 (10.0)
      return LanguageVersion::CXX11;
    return LanguageVersion::CXX98;
  } else if (language == Constants::C_LANGUAGE_ID) {
    if (mscVer >= 1910) // >= Visual Studio 2017 RTW (15.0)
      return LanguageVersion::C11;
    return LanguageVersion::C99;
  } else {
    QTC_CHECK(false && "Unexpected toolchain language, assuming latest C++ we support.");
    return LanguageVersion::LatestCxx;
  }
}

// Windows: Expand the delayed evaluation references returned by the
// SDK setup scripts: "PATH=!Path!;foo". Some values might expand
// to empty and should not be added
static auto winExpandDelayedEnvReferences(QString in, const Environment &env) -> QString
{
  const QChar exclamationMark = QLatin1Char('!');
  for (auto pos = 0; pos < in.size();) {
    // Replace "!REF!" by its value in process environment
    pos = in.indexOf(exclamationMark, pos);
    if (pos == -1)
      break;
    const int nextPos = in.indexOf(exclamationMark, pos + 1);
    if (nextPos == -1)
      break;
    const auto var = in.mid(pos + 1, nextPos - pos - 1);
    const auto replacement = env.expandedValueForKey(var.toUpper());
    in.replace(pos, nextPos + 1 - pos, replacement);
    pos += replacement.size();
  }
  return in;
}

auto MsvcToolChain::environmentModifications(QFutureInterface<GenerateEnvResult> &future, QString vcvarsBat, QString varsBatArg) -> void
{
  const auto inEnv = Environment::systemEnvironment();
  Environment outEnv;
  QMap<QString, QString> envPairs;
  EnvironmentItems diff;
  const auto error = generateEnvironmentSettings(inEnv, vcvarsBat, varsBatArg, envPairs);
  if (!error) {
    // Now loop through and process them
    for (auto envIter = envPairs.cbegin(), end = envPairs.cend(); envIter != end; ++envIter) {
      const auto expandedValue = winExpandDelayedEnvReferences(envIter.value(), inEnv);
      if (!expandedValue.isEmpty())
        outEnv.set(envIter.key(), expandedValue);
    }

    diff = inEnv.diff(outEnv, true);
    for (int i = diff.size() - 1; i >= 0; --i) {
      if (diff.at(i).name.startsWith(QLatin1Char('='))) {
        // Exclude "=C:", "=EXITCODE"
        diff.removeAt(i);
      }
    }
  }

  future.reportResult({error, diff});
}

auto MsvcToolChain::initEnvModWatcher(const QFuture<GenerateEnvResult> &future) -> void
{
  QObject::connect(&m_envModWatcher, &QFutureWatcher<GenerateEnvResult>::resultReadyAt, [&]() {
    const auto &result = m_envModWatcher.result();
    if (result.error) {
      const auto &errorMessage = *result.error;
      if (!errorMessage.isEmpty())
        TaskHub::addTask(CompileTask(Task::Error, errorMessage));
    } else {
      updateEnvironmentModifications(result.environmentItems);
    }
  });
  m_envModWatcher.setFuture(future);
}

auto MsvcToolChain::updateEnvironmentModifications(EnvironmentItems modifications) -> void
{
  EnvironmentItem::sort(&modifications);
  if (modifications != m_environmentModifications) {
    if (Log().isDebugEnabled()) {
      qCDebug(Log) << "Update environment for " << displayName();
      for (const auto &item : qAsConst(modifications))
        qCDebug(Log) << '\t' << item;
    }
    m_environmentModifications = modifications;
    rescanForCompiler();
    toolChainUpdated();
  } else {
    qCDebug(Log) << "No updates for " << displayName();
  }
}

auto MsvcToolChain::readEnvironmentSetting(const Environment &env) const -> Environment
{
  auto resultEnv = env;
  if (m_environmentModifications.isEmpty()) {
    m_envModWatcher.waitForFinished();
    if (m_envModWatcher.future().isFinished() && !m_envModWatcher.future().isCanceled()) {
      const auto &result = m_envModWatcher.result();
      if (result.error) {
        const auto &errorMessage = *result.error;
        if (!errorMessage.isEmpty())
          TaskHub::addTask(CompileTask(Task::Error, errorMessage));
      } else {
        resultEnv.modify(result.environmentItems);
      }
    }
  } else {
    resultEnv.modify(m_environmentModifications);
  }
  return resultEnv;
}

// --------------------------------------------------------------------------
// MsvcToolChain
// --------------------------------------------------------------------------

static auto addToAvailableMsvcToolchains(const MsvcToolChain *toolchain) -> void
{
  if (toolchain->typeId() != Constants::MSVC_TOOLCHAIN_TYPEID)
    return;

  if (!g_availableMsvcToolchains.contains(toolchain))
    g_availableMsvcToolchains.push_back(toolchain);
}

MsvcToolChain::MsvcToolChain(Id typeId) : ToolChain(typeId)
{
  setDisplayName("Microsoft Visual C++ Compiler");
  setTypeDisplayName(tr("MSVC"));
  addToAvailableMsvcToolchains(this);
  setTargetAbiKey(KEY_ROOT "SupportedAbi");
}

auto MsvcToolChain::inferWarningsForLevel(int warningLevel, WarningFlags &flags) -> void
{
  // reset all except unrelated flag
  flags = flags & WarningFlags::AsErrors;

  if (warningLevel >= 1)
    flags |= WarningFlags(WarningFlags::Default | WarningFlags::IgnoredQualifiers | WarningFlags::HiddenLocals | WarningFlags::UnknownPragma);
  if (warningLevel >= 2)
    flags |= WarningFlags::All;
  if (warningLevel >= 3) {
    flags |= WarningFlags(WarningFlags::Extra | WarningFlags::NonVirtualDestructor | WarningFlags::SignedComparison | WarningFlags::UnusedLocals | WarningFlags::Deprecated);
  }
  if (warningLevel >= 4)
    flags |= WarningFlags::UnusedParams;
}

MsvcToolChain::~MsvcToolChain()
{
  g_availableMsvcToolchains.removeOne(this);
}

auto MsvcToolChain::isValid() const -> bool
{
  if (m_vcvarsBat.isEmpty())
    return false;
  const QFileInfo fi(m_vcvarsBat);
  return fi.isFile() && fi.isExecutable();
}

auto MsvcToolChain::originalTargetTriple() const -> QString
{
  return targetAbi().wordWidth() == 64 ? QLatin1String("x86_64-pc-windows-msvc") : QLatin1String("i686-pc-windows-msvc");
}

auto MsvcToolChain::suggestedMkspecList() const -> QStringList
{
  // "win32-msvc" is the common MSVC mkspec introduced in Qt 5.8.1
  switch (targetAbi().osFlavor()) {
  case Abi::WindowsMsvc2005Flavor:
    return {"win32-msvc", "win32-msvc2005"};
  case Abi::WindowsMsvc2008Flavor:
    return {"win32-msvc", "win32-msvc2008"};
  case Abi::WindowsMsvc2010Flavor:
    return {"win32-msvc", "win32-msvc2010"};
  case Abi::WindowsMsvc2012Flavor:
    return {"win32-msvc", "win32-msvc2012", "win32-msvc2010"};
  case Abi::WindowsMsvc2013Flavor:
    return {"win32-msvc", "win32-msvc2013", "winphone-arm-msvc2013", "winphone-x86-msvc2013", "winrt-arm-msvc2013", "winrt-x86-msvc2013", "winrt-x64-msvc2013", "win32-msvc2012", "win32-msvc2010"};
  case Abi::WindowsMsvc2015Flavor:
    return {"win32-msvc", "win32-msvc2015", "winphone-arm-msvc2015", "winphone-x86-msvc2015", "winrt-arm-msvc2015", "winrt-x86-msvc2015", "winrt-x64-msvc2015"};
  case Abi::WindowsMsvc2017Flavor:
    return {"win32-msvc", "win32-msvc2017", "winrt-arm-msvc2017", "winrt-x86-msvc2017", "winrt-x64-msvc2017"};
  case Abi::WindowsMsvc2019Flavor:
    return {"win32-msvc", "win32-msvc2019", "win32-arm64-msvc", "winrt-arm-msvc2019", "winrt-x86-msvc2019", "winrt-x64-msvc2019"};
  case Abi::WindowsMsvc2022Flavor:
    return {"win32-msvc", "win32-msvc2022", "win32-arm64-msvc"};
  default:
    break;
  }
  return {};
}

auto MsvcToolChain::supportedAbis() const -> Abis
{
  const auto abi = targetAbi();
  Abis abis = {abi};
  switch (abi.osFlavor()) {
  case Abi::WindowsMsvc2022Flavor:
    abis << Abi(abi.architecture(), abi.os(), Abi::WindowsMsvc2019Flavor, abi.binaryFormat(), abi.wordWidth(), abi.param());
    Q_FALLTHROUGH();
  case Abi::WindowsMsvc2019Flavor:
    abis << Abi(abi.architecture(), abi.os(), Abi::WindowsMsvc2017Flavor, abi.binaryFormat(), abi.wordWidth(), abi.param());
    Q_FALLTHROUGH();
  case Abi::WindowsMsvc2017Flavor:
    abis << Abi(abi.architecture(), abi.os(), Abi::WindowsMsvc2015Flavor, abi.binaryFormat(), abi.wordWidth(), abi.param());
    break;
  default:
    break;
  }
  return abis;
}

auto MsvcToolChain::toMap() const -> QVariantMap
{
  auto data = ToolChain::toMap();
  data.insert(QLatin1String(varsBatKeyC), m_vcvarsBat);
  if (!m_varsBatArg.isEmpty())
    data.insert(QLatin1String(varsBatArgKeyC), m_varsBatArg);
  EnvironmentItem::sort(&m_environmentModifications);
  data.insert(QLatin1String(environModsKeyC), EnvironmentItem::toVariantList(m_environmentModifications));
  return data;
}

auto MsvcToolChain::fromMap(const QVariantMap &data) -> bool
{
  if (!ToolChain::fromMap(data)) {
    g_availableMsvcToolchains.removeOne(this);
    return false;
  }
  m_vcvarsBat = QDir::fromNativeSeparators(data.value(QLatin1String(varsBatKeyC)).toString());
  m_varsBatArg = data.value(QLatin1String(varsBatArgKeyC)).toString();

  m_environmentModifications = EnvironmentItem::itemsFromVariantList(data.value(QLatin1String(environModsKeyC)).toList());
  rescanForCompiler();

  initEnvModWatcher(runAsync(envModThreadPool(), &MsvcToolChain::environmentModifications, m_vcvarsBat, m_varsBatArg));

  const auto valid = !m_vcvarsBat.isEmpty() && targetAbi().isValid();
  if (!valid)
    g_availableMsvcToolchains.removeOne(this);

  return valid;
}

auto MsvcToolChain::createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget>
{
  return std::make_unique<MsvcToolChainConfigWidget>(this);
}

auto MsvcToolChain::hostPrefersToolchain() const -> bool
{
  return hostPrefersPlatform(platform());
}

static auto hasFlagEffectOnMacros(const QString &flag) -> bool
{
  if (flag.startsWith("-") || flag.startsWith("/")) {
    const auto f = flag.mid(1);
    if (f.startsWith("I"))
      return false; // Skip include paths
    if (f.startsWith("w", Qt::CaseInsensitive))
      return false; // Skip warning options
    if (f.startsWith("Y") || (f.startsWith("F") && f != "F"))
      return false; // Skip pch-related flags
  }
  return true;
}

auto MsvcToolChain::createMacroInspectionRunner() const -> MacroInspectionRunner
{
  auto env(m_lastEnvironment);
  addToEnvironment(env);
  auto macroCache = predefinedMacrosCache();
  const auto lang = language();

  // This runner must be thread-safe!
  return [this, env, macroCache, lang](const QStringList &cxxflags) {
    const auto filteredFlags = filtered(cxxflags, [](const QString &arg) {
      return hasFlagEffectOnMacros(arg);
    });

    const auto cachedMacros = macroCache->check(filteredFlags);
    if (cachedMacros)
      return cachedMacros.value();

    const auto macros = msvcPredefinedMacros(filteredFlags, env);

    const auto report = MacroInspectionReport{macros, msvcLanguageVersion(filteredFlags, lang, macros)};
    macroCache->insert(filteredFlags, report);

    return report;
  };
}

auto MsvcToolChain::languageExtensions(const QStringList &cxxflags) const -> LanguageExtensions
{
  using Utils::LanguageExtension;
  LanguageExtensions extensions(LanguageExtension::Microsoft);
  if (cxxflags.contains(QLatin1String("/openmp")))
    extensions |= LanguageExtension::OpenMP;

  // see http://msdn.microsoft.com/en-us/library/0k0w269d%28v=vs.71%29.aspx
  if (cxxflags.contains(QLatin1String("/Za")))
    extensions &= ~LanguageExtensions(LanguageExtension::Microsoft);

  return extensions;
}

auto MsvcToolChain::warningFlags(const QStringList &cflags) const -> WarningFlags
{
  auto flags = WarningFlags::NoWarnings;
  foreach(QString flag, cflags) {
    if (!flag.isEmpty() && flag[0] == QLatin1Char('-'))
      flag[0] = QLatin1Char('/');

    if (flag == QLatin1String("/WX")) {
      flags |= WarningFlags::AsErrors;
    } else if (flag == QLatin1String("/W0") || flag == QLatin1String("/w")) {
      inferWarningsForLevel(0, flags);
    } else if (flag == QLatin1String("/W1")) {
      inferWarningsForLevel(1, flags);
    } else if (flag == QLatin1String("/W2")) {
      inferWarningsForLevel(2, flags);
    } else if (flag == QLatin1String("/W3") || flag == QLatin1String("/W4") || flag == QLatin1String("/Wall")) {
      inferWarningsForLevel(3, flags);
    }

    WarningFlagAdder add(flag, flags);
    if (add.triggered())
      continue;
    // http://msdn.microsoft.com/en-us/library/ay4h0tc9.aspx
    add(4263, WarningFlags::OverloadedVirtual);
    // http://msdn.microsoft.com/en-us/library/ytxde1x7.aspx
    add(4230, WarningFlags::IgnoredQualifiers);
    // not exact match, http://msdn.microsoft.com/en-us/library/0hx5ckb0.aspx
    add(4258, WarningFlags::HiddenLocals);
    // http://msdn.microsoft.com/en-us/library/wzxffy8c.aspx
    add(4265, WarningFlags::NonVirtualDestructor);
    // http://msdn.microsoft.com/en-us/library/y92ktdf2%28v=vs.90%29.aspx
    add(4018, WarningFlags::SignedComparison);
    // http://msdn.microsoft.com/en-us/library/w099eeey%28v=vs.90%29.aspx
    add(4068, WarningFlags::UnknownPragma);
    // http://msdn.microsoft.com/en-us/library/26kb9fy0%28v=vs.80%29.aspx
    add(4100, WarningFlags::UnusedParams);
    // http://msdn.microsoft.com/en-us/library/c733d5h9%28v=vs.90%29.aspx
    add(4101, WarningFlags::UnusedLocals);
    // http://msdn.microsoft.com/en-us/library/xb1db44s%28v=vs.90%29.aspx
    add(4189, WarningFlags::UnusedLocals);
    // http://msdn.microsoft.com/en-us/library/ttcz0bys%28v=vs.90%29.aspx
    add(4996, WarningFlags::Deprecated);
  }
  return flags;
}

auto MsvcToolChain::includedFiles(const QStringList &flags, const QString &directoryPath) const -> QStringList
{
  return ToolChain::includedFiles("/FI", flags, directoryPath);
}

auto MsvcToolChain::createBuiltInHeaderPathsRunner(const Environment &env) const -> BuiltInHeaderPathsRunner
{
  auto fullEnv = env;
  addToEnvironment(fullEnv);

  return [this, fullEnv](const QStringList &, const QString &, const QString &) {
    QMutexLocker locker(&m_headerPathsMutex);
    const auto envList = fullEnv.toStringList();
    const auto it = m_headerPathsPerEnv.constFind(envList);
    if (it != m_headerPathsPerEnv.cend())
      return *it;
    return *m_headerPathsPerEnv.insert(envList, toBuiltInHeaderPaths(fullEnv.pathListValue("INCLUDE")));
  };
}

auto MsvcToolChain::addToEnvironment(Environment &env) const -> void
{
  // We cache the full environment (incoming + modifications by setup script).
  if (!m_resultEnvironment.size() || env != m_lastEnvironment) {
    qCDebug(Log) << "addToEnvironment: " << displayName();
    m_lastEnvironment = env;
    m_resultEnvironment = readEnvironmentSetting(env);
  }
  env = m_resultEnvironment;
}

static auto wrappedMakeCommand(const QString &command) -> QString
{
  const QString wrapperPath = QDir::currentPath() + "/msvc_make.bat";
  QFile wrapper(wrapperPath);
  if (!wrapper.open(QIODevice::WriteOnly))
    return command;
  QTextStream stream(&wrapper);
  stream << "chcp 65001\n";
  stream << "\"" << QDir::toNativeSeparators(command) << "\" %*";

  return wrapperPath;
}

auto MsvcToolChain::makeCommand(const Environment &environment) const -> FilePath
{
  const auto useJom = ProjectExplorerPlugin::projectExplorerSettings().useJom;
  const QString jom("jom.exe");
  const QString nmake("nmake.exe");
  FilePath tmp;

  FilePath command;
  if (useJom) {
    tmp = environment.searchInPath(jom, {Core::ICore::libexecPath(), Core::ICore::libexecPath("jom")});
    if (!tmp.isEmpty())
      command = tmp;
  }

  if (command.isEmpty()) {
    tmp = environment.searchInPath(nmake);
    if (!tmp.isEmpty())
      command = tmp;
  }

  if (command.isEmpty())
    command = FilePath::fromString(useJom ? jom : nmake);

  if (environment.hasKey("VSLANG"))
    return FilePath::fromString(wrappedMakeCommand(command.toString()));

  return command;
}

auto MsvcToolChain::rescanForCompiler() -> void
{
  auto env = Environment::systemEnvironment();
  addToEnvironment(env);

  setCompilerCommand(env.searchInPath(QLatin1String("cl.exe"), {}, [](const FilePath &name) {
    QDir dir(QDir::cleanPath(name.toFileInfo().absolutePath() + QStringLiteral("/..")));
    do {
      if (QFile::exists(dir.absoluteFilePath(QStringLiteral("vcvarsall.bat"))) || QFile::exists(dir.absolutePath() + "/Auxiliary/Build/vcvarsall.bat"))
        return true;
    } while (dir.cdUp() && !dir.isRoot());
    return false;
  }));
}

auto MsvcToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  return {new MsvcParser};
}

auto MsvcToolChain::setupVarsBat(const Abi &abi, const QString &varsBat, const QString &varsBatArg) -> void
{
  m_lastEnvironment = Environment::systemEnvironment();
  setTargetAbiNoSignal(abi);
  m_vcvarsBat = varsBat;
  m_varsBatArg = varsBatArg;

  if (!varsBat.isEmpty()) {
    initEnvModWatcher(runAsync(envModThreadPool(), &MsvcToolChain::environmentModifications, varsBat, varsBatArg));
  }
}

auto MsvcToolChain::resetVarsBat() -> void
{
  m_lastEnvironment = Environment::systemEnvironment();
  setTargetAbiNoSignal(Abi());
  m_vcvarsBat.clear();
  m_varsBatArg.clear();
}

auto MsvcToolChain::platform() const -> Platform
{
  const auto args = m_varsBatArg.split(' ');
  if (const auto entry = platformEntryFromName(args.value(0)))
    return entry->platform;
  return HostOsInfo::hostArchitecture() == HostOsInfo::HostArchitectureAMD64 ? amd64 : x86;
}

// --------------------------------------------------------------------------
// MsvcBasedToolChainConfigWidget: Creates a simple GUI without error label
// to display name and varsBat. Derived classes should add the error label and
// call setFromMsvcToolChain().
// --------------------------------------------------------------------------

MsvcBasedToolChainConfigWidget::MsvcBasedToolChainConfigWidget(ToolChain *tc) : ToolChainConfigWidget(tc), m_nameDisplayLabel(new QLabel(this)), m_varsBatDisplayLabel(new QLabel(this))
{
  m_nameDisplayLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_mainLayout->addRow(m_nameDisplayLabel);
  m_varsBatDisplayLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_mainLayout->addRow(tr("Initialization:"), m_varsBatDisplayLabel);
}

static auto msvcVarsToDisplay(const MsvcToolChain &tc) -> QString
{
  auto varsBatDisplay = QDir::toNativeSeparators(tc.varsBat());
  if (!tc.varsBatArg().isEmpty()) {
    varsBatDisplay += QLatin1Char(' ');
    varsBatDisplay += tc.varsBatArg();
  }
  return varsBatDisplay;
}

auto MsvcBasedToolChainConfigWidget::setFromMsvcToolChain() -> void
{
  const auto *tc = static_cast<const MsvcToolChain*>(toolChain());
  QTC_ASSERT(tc, return);
  m_nameDisplayLabel->setText(tc->displayName());
  m_varsBatDisplayLabel->setText(msvcVarsToDisplay(*tc));
}

// --------------------------------------------------------------------------
// MsvcToolChainConfigWidget
// --------------------------------------------------------------------------

MsvcToolChainConfigWidget::MsvcToolChainConfigWidget(ToolChain *tc) : MsvcBasedToolChainConfigWidget(tc), m_varsBatPathCombo(new QComboBox(this)), m_varsBatArchCombo(new QComboBox(this)), m_varsBatArgumentsEdit(new QLineEdit(this)), m_abiWidget(new AbiWidget)
{
  m_mainLayout->removeRow(m_mainLayout->rowCount() - 1);

  const auto hLayout = new QHBoxLayout();
  m_varsBatPathCombo->setObjectName("varsBatCombo");
  m_varsBatPathCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  m_varsBatPathCombo->setEditable(true);
  for (const auto tmpTc : qAsConst(g_availableMsvcToolchains)) {
    const auto nativeVcVars = QDir::toNativeSeparators(tmpTc->varsBat());
    if (!tmpTc->varsBat().isEmpty() && m_varsBatPathCombo->findText(nativeVcVars) == -1) {
      m_varsBatPathCombo->addItem(nativeVcVars);
    }
  }
  const auto isAmd64 = HostOsInfo::hostArchitecture() == HostOsInfo::HostArchitectureAMD64;
  // TODO: Add missing values to MsvcToolChain::Platform
  m_varsBatArchCombo->addItem(tr("<empty>"), isAmd64 ? MsvcToolChain::amd64 : MsvcToolChain::x86);
  m_varsBatArchCombo->addItem("x86", MsvcToolChain::x86);
  m_varsBatArchCombo->addItem("amd64", MsvcToolChain::amd64);
  m_varsBatArchCombo->addItem("arm", MsvcToolChain::arm);
  m_varsBatArchCombo->addItem("x86_amd64", MsvcToolChain::x86_amd64);
  m_varsBatArchCombo->addItem("x86_arm", MsvcToolChain::x86_arm);
  m_varsBatArchCombo->addItem("x86_arm64", MsvcToolChain::x86_arm64);
  m_varsBatArchCombo->addItem("amd64_x86", MsvcToolChain::amd64_x86);
  m_varsBatArchCombo->addItem("amd64_arm", MsvcToolChain::amd64_arm);
  m_varsBatArchCombo->addItem("amd64_arm64", MsvcToolChain::amd64_arm64);
  m_varsBatArchCombo->addItem("ia64", MsvcToolChain::ia64);
  m_varsBatArchCombo->addItem("x86_ia64", MsvcToolChain::x86_ia64);
  m_varsBatArgumentsEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  m_varsBatArgumentsEdit->setToolTip(tr("Additional arguments for the vcvarsall.bat call"));
  hLayout->addWidget(m_varsBatPathCombo);
  hLayout->addWidget(m_varsBatArchCombo);
  hLayout->addWidget(m_varsBatArgumentsEdit);
  m_mainLayout->addRow(tr("Initialization:"), hLayout);
  m_mainLayout->addRow(tr("&ABI:"), m_abiWidget);
  addErrorLabel();
  setFromMsvcToolChain();

  connect(m_varsBatPathCombo, &QComboBox::currentTextChanged, this, &MsvcToolChainConfigWidget::handleVcVarsChange);
  connect(m_varsBatArchCombo, &QComboBox::currentTextChanged, this, &MsvcToolChainConfigWidget::handleVcVarsArchChange);
  connect(m_varsBatArgumentsEdit, &QLineEdit::textChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_abiWidget, &AbiWidget::abiChanged, this, &ToolChainConfigWidget::dirty);
}

auto MsvcToolChainConfigWidget::applyImpl() -> void
{
  auto *tc = static_cast<MsvcToolChain*>(toolChain());
  QTC_ASSERT(tc, return);
  const auto vcVars = QDir::fromNativeSeparators(m_varsBatPathCombo->currentText());
  tc->setupVarsBat(m_abiWidget->currentAbi(), vcVars, vcVarsArguments());
  setFromMsvcToolChain();
}

auto MsvcToolChainConfigWidget::discardImpl() -> void
{
  setFromMsvcToolChain();
}

auto MsvcToolChainConfigWidget::isDirtyImpl() const -> bool
{
  const auto msvcToolChain = static_cast<MsvcToolChain*>(toolChain());

  return msvcToolChain->varsBat() != QDir::fromNativeSeparators(m_varsBatPathCombo->currentText()) || msvcToolChain->varsBatArg() != vcVarsArguments() || msvcToolChain->targetAbi() != m_abiWidget->currentAbi();
}

auto MsvcToolChainConfigWidget::makeReadOnlyImpl() -> void
{
  m_varsBatPathCombo->setEnabled(false);
  m_varsBatArchCombo->setEnabled(false);
  m_varsBatArgumentsEdit->setEnabled(false);
  m_abiWidget->setEnabled(false);
}

auto MsvcToolChainConfigWidget::setFromMsvcToolChain() -> void
{
  const auto *tc = static_cast<const MsvcToolChain*>(toolChain());
  QTC_ASSERT(tc, return);
  m_nameDisplayLabel->setText(tc->displayName());
  auto args = tc->varsBatArg();
  auto argList = args.split(' ');
  for (auto i = 0; i < argList.count(); ++i) {
    if (m_varsBatArchCombo->findText(argList.at(i).trimmed()) != -1) {
      const auto arch = argList.takeAt(i);
      m_varsBatArchCombo->setCurrentText(arch);
      args = argList.join(QLatin1Char(' '));
      break;
    }
  }
  m_varsBatPathCombo->setCurrentText(QDir::toNativeSeparators(tc->varsBat()));
  m_varsBatArgumentsEdit->setText(args);
  m_abiWidget->setAbis(tc->supportedAbis(), tc->targetAbi());
}

auto MsvcToolChainConfigWidget::updateAbis() -> void
{
  const auto normalizedVcVars = QDir::fromNativeSeparators(m_varsBatPathCombo->currentText());
  const auto *currentTc = static_cast<const MsvcToolChain*>(toolChain());
  QTC_ASSERT(currentTc, return);
  const auto platform = m_varsBatArchCombo->currentData().value<MsvcToolChain::Platform>();
  const auto arch = archForPlatform(platform);
  const auto wordWidth = wordWidthForPlatform(platform);

  // Search the selected vcVars bat file in already detected MSVC compilers.
  // For each variant of MSVC found, add its supported ABIs to the ABI widget so the user can
  // choose one appropriately.
  Abis supportedAbis;
  Abi targetAbi;
  for (const auto tc : qAsConst(g_availableMsvcToolchains)) {
    if (tc->varsBat() == normalizedVcVars && tc->targetAbi().wordWidth() == wordWidth && tc->targetAbi().architecture() == arch && tc->language() == currentTc->language()) {
      // We need to filter out duplicates as there might be multiple toolchains with
      // same abi (like x86, amd64_x86 for example).
      for (const auto &abi : tc->supportedAbis()) {
        if (!supportedAbis.contains(abi))
          supportedAbis.append(abi);
      }
      targetAbi = tc->targetAbi();
    }
  }

  // If we didn't find an exact match, try to find a fallback according to varsBat only.
  // This can happen when the toolchain does not support user-selected arch/wordWidth.
  if (!targetAbi.isValid()) {
    const auto tc = findOrDefault(g_availableMsvcToolchains, [normalizedVcVars](const MsvcToolChain *tc) {
      return tc->varsBat() == normalizedVcVars;
    });
    if (tc) {
      targetAbi = Abi(arch, tc->targetAbi().os(), tc->targetAbi().osFlavor(), tc->targetAbi().binaryFormat(), wordWidth);
    }
  }

  // Always set ABIs, even if none was found, to prevent stale data in the ABI widget.
  // In that case, a custom ABI will be selected according to targetAbi.
  m_abiWidget->setAbis(supportedAbis, targetAbi);

  emit dirty();
}

auto MsvcToolChainConfigWidget::handleVcVarsChange(const QString &) -> void
{
  updateAbis();
}

auto MsvcToolChainConfigWidget::handleVcVarsArchChange(const QString &) -> void
{
  // supportedAbi list in the widget only contains matching ABIs to whatever arch was selected.
  // We need to reupdate it from scratch with new arch parameters
  updateAbis();
}

auto MsvcToolChainConfigWidget::vcVarsArguments() const -> QString
{
  auto varsBatArg = m_varsBatArchCombo->currentText() == tr("<empty>") ? "" : m_varsBatArchCombo->currentText();
  if (!m_varsBatArgumentsEdit->text().isEmpty())
    varsBatArg += QLatin1Char(' ') + m_varsBatArgumentsEdit->text();
  return varsBatArg;
}

// --------------------------------------------------------------------------
// ClangClToolChainConfigWidget
// --------------------------------------------------------------------------

ClangClToolChainConfigWidget::ClangClToolChainConfigWidget(ToolChain *tc) : MsvcBasedToolChainConfigWidget(tc), m_varsBatDisplayCombo(new QComboBox(this))
{
  m_mainLayout->removeRow(m_mainLayout->rowCount() - 1);

  m_varsBatDisplayCombo->setObjectName("varsBatCombo");
  m_varsBatDisplayCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_mainLayout->addRow(tr("Initialization:"), m_varsBatDisplayCombo);

  if (tc->isAutoDetected()) {
    m_llvmDirLabel = new QLabel(this);
    m_llvmDirLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_mainLayout->addRow(tr("&Compiler path:"), m_llvmDirLabel);
  } else {
    const auto gnuVersionArgs = QStringList("--version");
    m_compilerCommand = new PathChooser(this);
    m_compilerCommand->setExpectedKind(PathChooser::ExistingCommand);
    m_compilerCommand->setCommandVersionArguments(gnuVersionArgs);
    m_compilerCommand->setHistoryCompleter("PE.Clang.Command.History");
    m_mainLayout->addRow(tr("&Compiler path:"), m_compilerCommand);
  }
  addErrorLabel();
  setFromClangClToolChain();

  if (m_compilerCommand) {
    connect(m_compilerCommand, &PathChooser::rawPathChanged, this, &ClangClToolChainConfigWidget::dirty);
  }
}

auto ClangClToolChainConfigWidget::setFromClangClToolChain() -> void
{
  const auto *currentTC = static_cast<const MsvcToolChain*>(toolChain());
  m_nameDisplayLabel->setText(currentTC->displayName());
  m_varsBatDisplayCombo->clear();
  m_varsBatDisplayCombo->addItem(msvcVarsToDisplay(*currentTC));
  for (const auto tc : qAsConst(g_availableMsvcToolchains)) {
    const auto varsToDisplay = msvcVarsToDisplay(*tc);
    if (m_varsBatDisplayCombo->findText(varsToDisplay) == -1)
      m_varsBatDisplayCombo->addItem(varsToDisplay);
  }

  const auto *clangClToolChain = static_cast<const ClangClToolChain*>(toolChain());
  if (clangClToolChain->isAutoDetected())
    m_llvmDirLabel->setText(clangClToolChain->clangPath().toUserOutput());
  else
    m_compilerCommand->setFilePath(clangClToolChain->clangPath());
}

static auto findMsvcToolChain(unsigned char wordWidth, Abi::OSFlavor flavor) -> const MsvcToolChain*
{
  return findOrDefault(g_availableMsvcToolchains, [wordWidth, flavor](const MsvcToolChain *tc) {
    const auto abi = tc->targetAbi();
    return abi.osFlavor() == flavor && wordWidth == abi.wordWidth();
  });
}

static auto findMsvcToolChain(const QString &displayedVarsBat) -> const MsvcToolChain*
{
  return findOrDefault(g_availableMsvcToolchains, [&displayedVarsBat](const MsvcToolChain *tc) {
    return msvcVarsToDisplay(*tc) == displayedVarsBat;
  });
}

static auto clangClVersion(const FilePath &clangClPath) -> QVersionNumber
{
  QString error;
  const auto dllversion = winGetDLLVersion(WinDLLFileVersion, clangClPath.toString(), &error);

  if (!dllversion.isEmpty())
    return QVersionNumber::fromString(dllversion);

  QtcProcess clangClProcess;
  clangClProcess.setCommand({clangClPath, {"--version"}});
  clangClProcess.runBlocking();
  if (clangClProcess.result() != QtcProcess::FinishedWithSuccess)
    return {};
  const auto match = QRegularExpression(QStringLiteral("clang version (\\d+(\\.\\d+)+)")).match(clangClProcess.stdOut());
  if (!match.hasMatch())
    return {};
  return QVersionNumber::fromString(match.captured(1));
}

static auto selectMsvcToolChain(const QString &displayedVarsBat, const FilePath &clangClPath, unsigned char wordWidth) -> const MsvcToolChain*
{
  const MsvcToolChain *toolChain = nullptr;
  if (!displayedVarsBat.isEmpty()) {
    toolChain = findMsvcToolChain(displayedVarsBat);
    if (toolChain)
      return toolChain;
  }

  QTC_CHECK(displayedVarsBat.isEmpty());
  const auto version = clangClVersion(clangClPath);
  if (version.majorVersion() >= 6) {
    toolChain = findMsvcToolChain(wordWidth, Abi::WindowsMsvc2022Flavor);
    if (!toolChain)
      toolChain = findMsvcToolChain(wordWidth, Abi::WindowsMsvc2019Flavor);
    if (!toolChain)
      toolChain = findMsvcToolChain(wordWidth, Abi::WindowsMsvc2017Flavor);
  }
  if (!toolChain) {
    toolChain = findMsvcToolChain(wordWidth, Abi::WindowsMsvc2015Flavor);
    if (!toolChain)
      toolChain = findMsvcToolChain(wordWidth, Abi::WindowsMsvc2013Flavor);
  }
  return toolChain;
}

static auto detectClangClToolChainInPath(const FilePath &clangClPath, const Toolchains &alreadyKnown, const QString &displayedVarsBat, bool isDefault = false) -> Toolchains
{
  Toolchains res;
  const unsigned char wordWidth = is64BitWindowsBinary(clangClPath) ? 64 : 32;
  const auto toolChain = selectMsvcToolChain(displayedVarsBat, clangClPath, wordWidth);

  if (!toolChain) {
    qWarning("Unable to find a suitable MSVC version for \"%s\".", qPrintable(clangClPath.toUserOutput()));
    return res;
  }

  const auto systemEnvironment = Environment::systemEnvironment();
  const auto targetAbi = toolChain->targetAbi();
  const auto name = QString("%1LLVM %2 bit based on %3").arg(QLatin1String(isDefault ? "Default " : "")).arg(wordWidth).arg(Abi::toString(targetAbi.osFlavor()).toUpper());
  for (auto language : {Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID}) {
    const auto tc = static_cast<ClangClToolChain*>(findOrDefault(alreadyKnown, [&](ToolChain *tc) -> bool {
      if (tc->typeId() != Constants::CLANG_CL_TOOLCHAIN_TYPEID)
        return false;
      if (tc->targetAbi() != targetAbi)
        return false;
      if (tc->language() != language)
        return false;
      return systemEnvironment.isSameExecutable(tc->compilerCommand().toString(), clangClPath.toString());
    }));
    if (tc) {
      res << tc;
    } else {
      const auto cltc = new ClangClToolChain;
      cltc->setClangPath(clangClPath);
      cltc->setDisplayName(name);
      cltc->setDetection(ToolChain::AutoDetection);
      cltc->setLanguage(language);
      cltc->setupVarsBat(toolChain->targetAbi(), toolChain->varsBat(), toolChain->varsBatArg());
      res << cltc;
    }
  }
  return res;
}

auto ClangClToolChainConfigWidget::applyImpl() -> void
{
  const auto clangClPath = m_compilerCommand->filePath();
  const auto clangClToolChain = static_cast<ClangClToolChain*>(toolChain());
  clangClToolChain->setClangPath(clangClPath);

  if (clangClPath.fileName() != "clang-cl.exe") {
    clangClToolChain->resetVarsBat();
    setFromClangClToolChain();
    return;
  }

  const auto displayedVarsBat = m_varsBatDisplayCombo->currentText();
  auto results = detectClangClToolChainInPath(clangClPath, {}, displayedVarsBat);

  if (results.isEmpty()) {
    clangClToolChain->resetVarsBat();
  } else {
    for (const ToolChain *toolchain : results) {
      if (toolchain->language() == clangClToolChain->language()) {
        const auto mstc = static_cast<const MsvcToolChain*>(toolchain);
        clangClToolChain->setupVarsBat(mstc->targetAbi(), mstc->varsBat(), mstc->varsBatArg());
        break;
      }
    }

    qDeleteAll(results);
  }
  setFromClangClToolChain();
}

auto ClangClToolChainConfigWidget::discardImpl() -> void
{
  setFromClangClToolChain();
}

auto ClangClToolChainConfigWidget::makeReadOnlyImpl() -> void
{
  m_varsBatDisplayCombo->setEnabled(false);
}

// --------------------------------------------------------------------------
// ClangClToolChain, piggy-backing on MSVC2015 and providing the compiler
// clang-cl.exe as a [to some extent] compatible drop-in replacement for cl.
// --------------------------------------------------------------------------

ClangClToolChain::ClangClToolChain() : MsvcToolChain(Constants::CLANG_CL_TOOLCHAIN_TYPEID)
{
  setDisplayName("clang-cl");
  setTypeDisplayName(QCoreApplication::translate("ProjectExplorer::ClangToolChainFactory", "Clang"));
}

auto ClangClToolChain::isValid() const -> bool
{
  const auto clang = clangPath();
  return MsvcToolChain::isValid() && clang.exists() && clang.fileName() == "clang-cl.exe";
}

auto ClangClToolChain::addToEnvironment(Environment &env) const -> void
{
  MsvcToolChain::addToEnvironment(env);
  env.prependOrSetPath(m_clangPath.parentDir()); // bin folder
}

auto ClangClToolChain::compilerCommand() const -> FilePath
{
  return m_clangPath;
}

auto ClangClToolChain::suggestedMkspecList() const -> QStringList
{
  const QString mkspec = "win32-clang-" + Abi::toString(targetAbi().osFlavor());
  return {mkspec, "win32-clang-msvc"};
}

auto ClangClToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  return {new ClangClParser};
}

static inline auto llvmDirKey() -> QString
{
  return QStringLiteral("ProjectExplorer.ClangClToolChain.LlvmDir");
}

auto ClangClToolChain::toMap() const -> QVariantMap
{
  auto result = MsvcToolChain::toMap();
  result.insert(llvmDirKey(), m_clangPath.toString());
  return result;
}

auto ClangClToolChain::fromMap(const QVariantMap &data) -> bool
{
  if (!MsvcToolChain::fromMap(data))
    return false;
  const auto clangPath = data.value(llvmDirKey()).toString();
  if (clangPath.isEmpty())
    return false;
  m_clangPath = FilePath::fromString(clangPath);

  return true;
}

auto ClangClToolChain::createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget>
{
  return std::make_unique<ClangClToolChainConfigWidget>(this);
}

auto ClangClToolChain::operator==(const ToolChain &other) const -> bool
{
  if (!MsvcToolChain::operator==(other))
    return false;

  const auto *clangClTc = static_cast<const ClangClToolChain*>(&other);
  return m_clangPath == clangClTc->m_clangPath;
}

auto ClangClToolChain::priority() const -> int
{
  return MsvcToolChain::priority() - 1;
}

auto ClangClToolChain::msvcPredefinedMacros(const QStringList &cxxflags, const Environment &env) const -> Macros
{
  if (!cxxflags.contains("--driver-mode=g++"))
    return MsvcToolChain::msvcPredefinedMacros(cxxflags, env);

  QtcProcess cpp;
  cpp.setEnvironment(env);
  cpp.setWorkingDirectory(TemporaryDirectory::masterDirectoryFilePath());

  auto arguments = cxxflags;
  arguments.append(gccPredefinedMacrosOptions(language()));
  arguments.append("-");
  cpp.setCommand({compilerCommand(), arguments});
  cpp.runBlocking();
  if (cpp.result() != QtcProcess::FinishedWithSuccess) {
    // Show the warning but still parse the output.
    QTC_CHECK(false && "clang-cl exited with non-zero code.");
  }

  return Macro::toMacros(cpp.allRawOutput());
}

auto ClangClToolChain::msvcLanguageVersion(const QStringList &cxxflags, const Id &language, const Macros &macros) const -> LanguageVersion
{
  if (cxxflags.contains("--driver-mode=g++"))
    return languageVersion(language, macros);
  return MsvcToolChain::msvcLanguageVersion(cxxflags, language, macros);
}

auto ClangClToolChain::createBuiltInHeaderPathsRunner(const Environment &env) const -> BuiltInHeaderPathsRunner
{
  {
    QMutexLocker locker(&m_headerPathsMutex);
    m_headerPathsPerEnv.clear();
  }

  return MsvcToolChain::createBuiltInHeaderPathsRunner(env);
}

// --------------------------------------------------------------------------
// MsvcToolChainFactory
// --------------------------------------------------------------------------

MsvcToolChainFactory::MsvcToolChainFactory()
{
  setDisplayName(MsvcToolChain::tr("MSVC"));
  setSupportedToolChainType(Constants::MSVC_TOOLCHAIN_TYPEID);
  setSupportedLanguages({Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID});
  setToolchainConstructor([] { return new MsvcToolChain(Constants::MSVC_TOOLCHAIN_TYPEID); });
}

auto MsvcToolChainFactory::vcVarsBatFor(const QString &basePath, MsvcToolChain::Platform platform, const QVersionNumber &v) -> QString
{
  QString result;
  if (const auto p = platformEntry(platform)) {
    result += basePath;
    // Starting with 15.0 (MSVC2017), the .bat are in one folder.
    if (v.majorVersion() <= 14)
      result += QLatin1String(p->prefix);
    result += QLatin1Char('/');
    result += QLatin1String(p->bat);
  }
  return result;
}

static auto findOrCreateToolchains(const ToolchainDetector &detector, const QString &name, const Abi &abi, const QString &varsBat, const QString &varsBatArg) -> Toolchains
{
  Toolchains res;
  for (auto language : {Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID}) {
    const auto tc = findOrDefault(detector.alreadyKnown, [&](ToolChain *tc) -> bool {
      if (tc->typeId() != Constants::MSVC_TOOLCHAIN_TYPEID)
        return false;
      if (tc->targetAbi() != abi)
        return false;
      if (tc->language() != language)
        return false;
      const auto mtc = static_cast<MsvcToolChain*>(tc);
      return mtc->varsBat() == varsBat && mtc->varsBatArg() == varsBatArg;
    });
    if (tc) {
      res << tc;
    } else {
      const auto mstc = new MsvcToolChain(Constants::MSVC_TOOLCHAIN_TYPEID);
      mstc->setupVarsBat(abi, varsBat, varsBatArg);
      mstc->setDisplayName(name);
      mstc->setLanguage(language);
      res << mstc;
    }
  }
  return res;
}

// Detect build tools introduced with MSVC2015
static auto detectCppBuildTools2015(Toolchains *list) -> void
{
  struct Entry {
    const char *postFix;
    const char *varsBatArg;
    Abi::Architecture architecture;
    Abi::BinaryFormat format;
    unsigned char wordSize;
  };

  const Entry entries[] = {{" (x86)", "x86", Abi::X86Architecture, Abi::PEFormat, 32}, {" (x64)", "amd64", Abi::X86Architecture, Abi::PEFormat, 64}, {" (x86_arm)", "x86_arm", Abi::ArmArchitecture, Abi::PEFormat, 32}, {" (x64_arm)", "amd64_arm", Abi::ArmArchitecture, Abi::PEFormat, 32}, {" (x86_arm64)", "x86_arm64", Abi::ArmArchitecture, Abi::PEFormat, 64}, {" (x64_arm64)", "amd64_arm64", Abi::ArmArchitecture, Abi::PEFormat, 64}};

  const QString name = "Microsoft Visual C++ Build Tools";
  const QString vcVarsBat = windowsProgramFilesDir() + '/' + name + "/vcbuildtools.bat";
  if (!QFileInfo(vcVarsBat).isFile())
    return;
  for (const auto &e : entries) {
    const Abi abi(e.architecture, Abi::WindowsOS, Abi::WindowsMsvc2015Flavor, e.format, e.wordSize);
    for (const auto language : {Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID}) {
      const auto tc = new MsvcToolChain(Constants::MSVC_TOOLCHAIN_TYPEID);
      tc->setupVarsBat(abi, vcVarsBat, QLatin1String(e.varsBatArg));
      tc->setDisplayName(name + QLatin1String(e.postFix));
      tc->setDetection(ToolChain::AutoDetection);
      tc->setLanguage(language);
      list->append(tc);
    }
  }
}

auto MsvcToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  if (!detector.device.isNull()) {
    // FIXME currently no support for msvc toolchains on a device
    return {};
  }

  Toolchains results;

  // 1) Installed SDKs preferred over standalone Visual studio
  const QSettings sdkRegistry(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows"), QSettings::NativeFormat);
  const auto defaultSdkPath = sdkRegistry.value(QLatin1String("CurrentInstallFolder")).toString();
  if (!defaultSdkPath.isEmpty()) {
    foreach(const QString &sdkKey, sdkRegistry.childGroups()) {
      const auto name = sdkRegistry.value(sdkKey + QLatin1String("/ProductName")).toString();
      const auto folder = sdkRegistry.value(sdkKey + QLatin1String("/InstallationFolder")).toString();
      if (folder.isEmpty())
        continue;

      QDir dir(folder);
      if (!dir.cd(QLatin1String("bin")))
        continue;
      QFileInfo fi(dir, QLatin1String("SetEnv.cmd"));
      if (!fi.exists())
        continue;

      QList<ToolChain*> tmp;
      const QVector<QPair<MsvcToolChain::Platform, QString>> platforms = {{MsvcToolChain::x86, "x86"}, {MsvcToolChain::amd64, "x64"}, {MsvcToolChain::ia64, "ia64"},};
      for (const auto &platform : platforms) {
        tmp.append(findOrCreateToolchains(detector, generateDisplayName(name, MsvcToolChain::WindowsSDK, platform.first), findAbiOfMsvc(MsvcToolChain::WindowsSDK, platform.first, sdkKey), fi.absoluteFilePath(), "/" + platform.second));
      }
      // Make sure the default is front.
      if (folder == defaultSdkPath)
        results = tmp + results;
      else
        results += tmp;
    } // foreach
  }

  // 2) Installed MSVCs
  // prioritized list.
  // x86_arm was put before amd64_arm as a workaround for auto detected windows phone
  // toolchains. As soon as windows phone builds support x64 cross builds, this change
  // can be reverted.
  const MsvcToolChain::Platform platforms[] = {MsvcToolChain::x86, MsvcToolChain::amd64_x86, MsvcToolChain::amd64, MsvcToolChain::x86_amd64, MsvcToolChain::arm, MsvcToolChain::x86_arm, MsvcToolChain::amd64_arm, MsvcToolChain::x86_arm64, MsvcToolChain::amd64_arm64, MsvcToolChain::ia64, MsvcToolChain::x86_ia64};

  foreach(const VisualStudioInstallation &i, detectVisualStudio()) {
    for (const auto platform : platforms) {
      const auto toolchainInstalled = QFileInfo(vcVarsBatFor(i.vcVarsPath, platform, i.version)).isFile();
      if (hostSupportsPlatform(platform) && toolchainInstalled) {
        results.append(findOrCreateToolchains(detector, generateDisplayName(i.vsName, MsvcToolChain::VS, platform), findAbiOfMsvc(MsvcToolChain::VS, platform, i.vsName), i.vcVarsAll, platformName(platform)));
      }
    }
  }

  detectCppBuildTools2015(&results);

  for (const auto tc : qAsConst(results))
    tc->setDetection(ToolChain::AutoDetection);

  return results;
}

ClangClToolChainFactory::ClangClToolChainFactory()
{
  setDisplayName(ClangClToolChain::tr("clang-cl"));
  setSupportedLanguages({Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID});
  setSupportedToolChainType(Constants::CLANG_CL_TOOLCHAIN_TYPEID);
  setToolchainConstructor([] { return new ClangClToolChain; });
}

auto ClangClToolChainFactory::canCreate() const -> bool
{
  return !g_availableMsvcToolchains.isEmpty();
}

auto ClangClToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  if (!detector.device.isNull()) {
    // FIXME currently no support for msvc toolchains on a device
    return {};
  }
  #ifdef Q_OS_WIN64
  const char registryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\LLVM\\LLVM";
  #else
    const char registryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\LLVM\\LLVM";
  #endif

  Toolchains results;
  auto known = detector.alreadyKnown;

  auto qtCreatorsClang = Core::ICore::clangExecutable(CLANG_BINDIR);
  if (!qtCreatorsClang.isEmpty()) {
    qtCreatorsClang = qtCreatorsClang.parentDir().pathAppended("clang-cl.exe");
    results.append(detectClangClToolChainInPath(qtCreatorsClang, detector.alreadyKnown, "", true));
    known.append(results);
  }

  const QSettings registry(QLatin1String(registryNode), QSettings::NativeFormat);
  if (registry.status() == QSettings::NoError) {
    const auto path = FilePath::fromUserInput(registry.value(QStringLiteral(".")).toString());
    const auto clangClPath = path / "bin/clang-cl.exe";
    if (!path.isEmpty()) {
      results.append(detectClangClToolChainInPath(clangClPath, known, ""));
      known.append(results);
    }
  }

  const auto systemEnvironment = Environment::systemEnvironment();
  const auto clangClPath = systemEnvironment.searchInPath("clang-cl");
  if (!clangClPath.isEmpty())
    results.append(detectClangClToolChainInPath(clangClPath, known, ""));

  return results;
}

auto MsvcToolChain::operator==(const ToolChain &other) const -> bool
{
  if (!ToolChain::operator==(other))
    return false;

  const auto *msvcTc = dynamic_cast<const MsvcToolChain*>(&other);
  return targetAbi() == msvcTc->targetAbi() && m_vcvarsBat == msvcTc->m_vcvarsBat && m_varsBatArg == msvcTc->m_varsBatArg;
}

auto MsvcToolChain::priority() const -> int
{
  return hostPrefersToolchain() ? PriorityHigh : PriorityNormal;
}

auto MsvcToolChain::cancelMsvcToolChainDetection() -> void
{
  envModThreadPool()->clear();
}

auto MsvcToolChain::generateEnvironmentSettings(const Environment &env, const QString &batchFile, const QString &batchArgs, QMap<QString, QString> &envPairs) -> optional<QString>
{
  const QString marker = "####################";
  // Create a temporary file name for the output. Use a temporary file here
  // as I don't know another way to do this in Qt...

  // Create a batch file to create and save the env settings
  TempFileSaver saver(TemporaryDirectory::masterDirectoryPath() + "/XXXXXX.bat");

  QByteArray call = "call ";
  call += ProcessArgs::quoteArg(batchFile).toLocal8Bit();
  if (!batchArgs.isEmpty()) {
    call += ' ';
    call += batchArgs.toLocal8Bit();
  }
  if (HostOsInfo::isWindowsHost())
    saver.write("chcp 65001\r\n");
  saver.write("set VSCMD_SKIP_SENDTELEMETRY=1\r\n");
  saver.write(call + "\r\n");
  saver.write("@echo " + marker.toLocal8Bit() + "\r\n");
  saver.write("set\r\n");
  saver.write("@echo " + marker.toLocal8Bit() + "\r\n");
  if (!saver.finalize()) {
    qWarning("%s: %s", Q_FUNC_INFO, qPrintable(saver.errorString()));
    return QString();
  }

  QtcProcess run;

  // As of WinSDK 7.1, there is logic preventing the path from being set
  // correctly if "ORIGINALPATH" is already set. That can cause problems
  // if Creator is launched within a session set up by setenv.cmd.
  auto runEnv = env;
  runEnv.unset(QLatin1String("ORIGINALPATH"));
  run.setEnvironment(runEnv);
  run.setTimeoutS(60);
  auto cmdPath = FilePath::fromUserInput(QString::fromLocal8Bit(qgetenv("COMSPEC")));
  if (cmdPath.isEmpty())
    cmdPath = env.searchInPath(QLatin1String("cmd.exe"));
  // Windows SDK setup scripts require command line switches for environment expansion.
  CommandLine cmd(cmdPath, {"/E:ON", "/V:ON", "/c", saver.filePath().toUserOutput()});
  qCDebug(Log) << "readEnvironmentSetting: " << call << cmd.toUserOutput() << " Env: " << runEnv.size();
  run.setCodec(QTextCodec::codecForName("UTF-8"));
  run.setCommand(cmd);
  run.runBlocking();

  if (run.result() != QtcProcess::FinishedWithSuccess) {
    const auto message = !run.stdErr().isEmpty() ? run.stdErr() : run.exitMessage();
    qWarning().noquote() << message;
    auto command = QDir::toNativeSeparators(batchFile);
    if (!batchArgs.isEmpty())
      command += ' ' + batchArgs;
    return QCoreApplication::translate("ProjectExplorer::Internal::MsvcToolChain", "Failed to retrieve MSVC Environment from \"%1\":\n" "%2").arg(command, message);
  }

  // The SDK/MSVC scripts do not return exit codes != 0. Check on stdout.
  const auto stdOut = run.stdOut();

  //
  // Now parse the file to get the environment settings
  const int start = stdOut.indexOf(marker);
  if (start == -1) {
    qWarning("Could not find start marker in stdout output.");
    return QString();
  }

  const int end = stdOut.indexOf(marker, start + 1);
  if (end == -1) {
    qWarning("Could not find end marker in stdout output.");
    return QString();
  }

  const auto output = stdOut.mid(start, end - start);

  foreach(const QString &line, output.split(QLatin1String("\n"))) {
    const int pos = line.indexOf('=');
    if (pos > 0) {
      const auto varName = line.mid(0, pos);
      const auto varValue = line.mid(pos + 1);
      envPairs.insert(varName, varValue);
    }
  }

  return nullopt;
}

auto MsvcToolChainFactory::canCreate() const -> bool
{
  return !g_availableMsvcToolchains.isEmpty();
}

MsvcToolChain::WarningFlagAdder::WarningFlagAdder(const QString &flag, WarningFlags &flags) : m_flags(flags)
{
  if (flag.startsWith(QLatin1String("-wd"))) {
    m_doesEnable = false;
  } else if (flag.startsWith(QLatin1String("-w"))) {
    m_doesEnable = true;
  } else {
    m_triggered = true;
    return;
  }
  auto ok = false;
  if (m_doesEnable)
    m_warningCode = flag.mid(2).toInt(&ok);
  else
    m_warningCode = flag.mid(3).toInt(&ok);
  if (!ok)
    m_triggered = true;
}

auto MsvcToolChain::WarningFlagAdder::operator()(int warningCode, WarningFlags flagsSet) -> void
{
  if (m_triggered)
    return;
  if (warningCode == m_warningCode) {
    m_triggered = true;
    if (m_doesEnable)
      m_flags |= flagsSet;
    else
      m_flags &= ~flagsSet;
  }
}

auto MsvcToolChain::WarningFlagAdder::triggered() const -> bool
{
  return m_triggered;
}

} // namespace Internal
} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Internal::MsvcToolChain::Platform)
