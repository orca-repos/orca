// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "gcctoolchain.hpp"

#include "abiwidget.hpp"
#include "clangparser.hpp"
#include "gccparser.hpp"
#include "linuxiccparser.hpp"
#include "projectmacro.hpp"
#include "toolchainconfigwidget.hpp"
#include "toolchainmanager.hpp"

#include <core/icore.hpp>
#include <core/messagemanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTimer>

#include <memory>

namespace {
static Q_LOGGING_CATEGORY(gccLog, "qtc.projectexplorer.toolchain.gcc", QtWarningMsg);
} // namespace

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class TargetTripleWidget;

class GccToolChainConfigWidget : public ToolChainConfigWidget {
  Q_OBJECT

public:
  explicit GccToolChainConfigWidget(GccToolChain *tc);

protected:
  auto handleCompilerCommandChange() -> void;
  auto handlePlatformCodeGenFlagsChange() -> void;
  auto handlePlatformLinkerFlagsChange() -> void;
  auto applyImpl() -> void override;
  auto discardImpl() -> void override { setFromToolchain(); }
  auto isDirtyImpl() const -> bool override;
  auto makeReadOnlyImpl() -> void override;
  auto setFromToolchain() -> void;

  AbiWidget *m_abiWidget;

private:
  PathChooser *m_compilerCommand;
  QLineEdit *m_platformCodeGenFlagsLineEdit;
  QLineEdit *m_platformLinkerFlagsLineEdit;
  TargetTripleWidget *const m_targetTripleWidget;
  bool m_isReadOnly = false;
  Macros m_macros;
};

class ClangToolChainConfigWidget : public GccToolChainConfigWidget {
  Q_OBJECT

public:
  explicit ClangToolChainConfigWidget(ClangToolChain *tc);

private:
  auto applyImpl() -> void override;
  auto discardImpl() -> void override { setFromClangToolchain(); }
  auto isDirtyImpl() const -> bool override;
  auto makeReadOnlyImpl() -> void override;
  auto setFromClangToolchain() -> void;
  auto updateParentToolChainComboBox() -> void;

  QList<QMetaObject::Connection> m_parentToolChainConnections;
  QComboBox *m_parentToolchainCombo = nullptr;
};

} // namespace Internal

using namespace Internal;

// --------------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------------

static constexpr char compilerPlatformCodeGenFlagsKeyC[] = "ProjectExplorer.GccToolChain.PlatformCodeGenFlags";
static constexpr char compilerPlatformLinkerFlagsKeyC[] = "ProjectExplorer.GccToolChain.PlatformLinkerFlags";
static constexpr char targetAbiKeyC[] = "ProjectExplorer.GccToolChain.TargetAbi";
static constexpr char originalTargetTripleKeyC[] = "ProjectExplorer.GccToolChain.OriginalTargetTriple";
static constexpr char supportedAbisKeyC[] = "ProjectExplorer.GccToolChain.SupportedAbis";
static constexpr char parentToolChainIdKeyC[] = "ProjectExplorer.ClangToolChain.ParentToolChainId";
static constexpr char binaryRegexp[] = "(?:^|-|\\b)(?:gcc|g\\+\\+|clang(?:\\+\\+)?)(?:-([\\d.]+))?$";

static auto runGcc(const FilePath &gcc, const QStringList &arguments, const Environment &env) -> QByteArray
{
  if (!gcc.isExecutableFile())
    return QByteArray();

  QtcProcess cpp;
  auto environment(env);
  environment.setupEnglishOutput();

  cpp.setEnvironment(environment);
  cpp.setTimeoutS(10);
  cpp.setCommand({gcc, arguments});
  cpp.runBlocking();
  if (cpp.result() != QtcProcess::FinishedWithSuccess || cpp.exitCode() != 0) {
    Core::MessageManager::writeFlashing({"Compiler feature detection failure!", cpp.exitMessage(), QString::fromUtf8(cpp.allRawOutput())});
    return QByteArray();
  }

  return cpp.allOutput().toUtf8();
}

static auto gccPredefinedMacros(const FilePath &gcc, const QStringList &args, const Environment &env) -> Macros
{
  auto arguments = args;
  arguments << "-";

  auto predefinedMacros = Macro::toMacros(runGcc(gcc, arguments, env));
  // Sanity check in case we get an error message instead of real output:
  QTC_CHECK(predefinedMacros.isEmpty() || predefinedMacros.front().type == ProjectExplorer::MacroType::Define);
  if (HostOsInfo::isMacHost()) {
    // Turn off flag indicating Apple's blocks support
    const Macro blocksDefine("__BLOCKS__", "1");
    const Macro blocksUndefine("__BLOCKS__", MacroType::Undefine);
    const int idx = predefinedMacros.indexOf(blocksDefine);
    if (idx != -1)
      predefinedMacros[idx] = blocksUndefine;

    // Define __strong and __weak (used for Apple's GC extension of C) to be empty
    predefinedMacros.append({"__strong"});
    predefinedMacros.append({"__weak"});
  }
  return predefinedMacros;
}

auto GccToolChain::gccHeaderPaths(const FilePath &gcc, const QStringList &arguments, const Environment &env) -> HeaderPaths
{
  HeaderPaths builtInHeaderPaths;
  QByteArray line;
  auto data = runGcc(gcc, arguments, env);
  QBuffer cpp(&data);
  cpp.open(QIODevice::ReadOnly);
  while (cpp.canReadLine()) {
    line = cpp.readLine();
    if (line.startsWith("#include"))
      break;
  }

  if (!line.isEmpty() && line.startsWith("#include")) {
    auto kind = HeaderPathType::User;
    while (cpp.canReadLine()) {
      line = cpp.readLine();
      if (line.startsWith("#include")) {
        kind = HeaderPathType::BuiltIn;
      } else if (! line.isEmpty() && QChar(line.at(0)).isSpace()) {
        auto thisHeaderKind = kind;

        line = line.trimmed();

        const int index = line.indexOf(" (framework directory)");
        if (index != -1) {
          line.truncate(index);
          thisHeaderKind = HeaderPathType::Framework;
        }

        const auto headerPath = QFileInfo(QFile::decodeName(line)).canonicalFilePath();
        builtInHeaderPaths.append({headerPath, thisHeaderKind});
      } else if (line.startsWith("End of search list.")) {
        break;
      } else {
        qWarning("%s: Ignoring line: %s", __FUNCTION__, line.constData());
      }
    }
  }
  return builtInHeaderPaths;
}

static auto guessGccAbi(const QString &m, const Macros &macros) -> Abis
{
  Abis abiList;

  const auto guessed = Abi::abiFromTargetTriplet(m);
  if (guessed.isNull())
    return abiList;

  const auto arch = guessed.architecture();
  const auto os = guessed.os();
  auto flavor = guessed.osFlavor();
  const auto format = guessed.binaryFormat();
  int width = guessed.wordWidth();

  const auto sizeOfMacro = findOrDefault(macros, [](const Macro &m) { return m.key == "__SIZEOF_SIZE_T__"; });
  if (sizeOfMacro.isValid() && sizeOfMacro.type == MacroType::Define)
    width = sizeOfMacro.value.toInt() * 8;
  const auto &mscVerMacro = findOrDefault(macros, [](const Macro &m) { return m.key == "_MSC_VER"; });
  if (mscVerMacro.type == MacroType::Define) {
    const auto msvcVersion = mscVerMacro.value.toInt();
    flavor = Abi::flavorForMsvcVersion(msvcVersion);
  }

  if (os == Abi::DarwinOS) {
    // Apple does PPC and x86!
    abiList << Abi(arch, os, flavor, format, width);
    abiList << Abi(arch, os, flavor, format, width == 64 ? 32 : 64);
  } else if (arch == Abi::X86Architecture && (width == 0 || width == 64)) {
    abiList << Abi(arch, os, flavor, format, 64);
    if (width != 64 || (!m.contains("mingw") && ToolChainManager::detectionSettings().detectX64AsX32)) {
      abiList << Abi(arch, os, flavor, format, 32);
    }
  } else {
    abiList << Abi(arch, os, flavor, format, width);
  }
  return abiList;
}

static auto guessGccAbi(const FilePath &path, const Environment &env, const Macros &macros, const QStringList &extraArgs = {}) -> GccToolChain::DetectedAbisResult
{
  if (path.isEmpty())
    return GccToolChain::DetectedAbisResult();

  auto arguments = extraArgs;
  arguments << "-dumpmachine";
  const auto machine = QString::fromLocal8Bit(runGcc(path, arguments, env)).trimmed();
  if (machine.isEmpty()) {
    // ICC does not implement the -dumpmachine option on macOS.
    if (HostOsInfo::isMacHost() && (path.fileName() == "icc" || path.fileName() == "icpc"))
      return GccToolChain::DetectedAbisResult({Abi::hostAbi()});
    return GccToolChain::DetectedAbisResult(); // no need to continue if running failed once...
  }
  return GccToolChain::DetectedAbisResult(guessGccAbi(machine, macros), machine);
}

static auto gccVersion(const FilePath &path, const Environment &env, const QStringList &extraArgs) -> QString
{
  auto arguments = extraArgs;
  arguments << "-dumpversion";
  return QString::fromLocal8Bit(runGcc(path, arguments, env)).trimmed();
}

static auto gccInstallDir(const FilePath &compiler, const Environment &env, const QStringList &extraArgs = {}) -> FilePath
{
  auto arguments = extraArgs;
  arguments << "-print-search-dirs";
  auto output = QString::fromLocal8Bit(runGcc(compiler, arguments, env)).trimmed();
  // Expected output looks like this:
  //   install: /usr/lib/gcc/x86_64-linux-gnu/7/
  //   ...
  // Note that clang also supports "-print-search-dirs". However, the
  // install dir is not part of the output (tested with clang-8/clang-9).

  const QString prefix = "install: ";
  const auto line = QTextStream(&output).readLine();
  if (!line.startsWith(prefix))
    return {};
  return compiler.withNewPath(QDir::cleanPath(line.mid(prefix.size())));
}

// --------------------------------------------------------------------------
// GccToolChain
// --------------------------------------------------------------------------

GccToolChain::GccToolChain(Id typeId) : ToolChain(typeId)
{
  setTypeDisplayName(tr("GCC"));
  setTargetAbiKey(targetAbiKeyC);
  setCompilerCommandKey("ProjectExplorer.GccToolChain.Path");
}

auto GccToolChain::setSupportedAbis(const Abis &abis) -> void
{
  if (m_supportedAbis == abis)
    return;

  m_supportedAbis = abis;
  toolChainUpdated();
}

auto GccToolChain::setOriginalTargetTriple(const QString &targetTriple) -> void
{
  if (m_originalTargetTriple == targetTriple)
    return;

  m_originalTargetTriple = targetTriple;
  toolChainUpdated();
}

auto GccToolChain::setInstallDir(const FilePath &installDir) -> void
{
  if (m_installDir == installDir)
    return;

  m_installDir = installDir;
  toolChainUpdated();
}

auto GccToolChain::defaultDisplayName() const -> QString
{
  auto type = typeDisplayName();
  const QRegularExpression regexp(binaryRegexp);
  const auto match = regexp.match(compilerCommand().fileName());
  if (match.lastCapturedIndex() >= 1)
    type += ' ' + match.captured(1);
  const auto abi = targetAbi();
  if (abi.architecture() == Abi::UnknownArchitecture || abi.wordWidth() == 0)
    return type;
  return tr("%1 (%2, %3 %4 at %5)").arg(type, ToolChainManager::displayNameOfLanguageId(language()), Abi::toString(abi.architecture()), Abi::toString(abi.wordWidth()), compilerCommand().toUserOutput());
}

auto GccToolChain::defaultLanguageExtensions() const -> LanguageExtensions
{
  return LanguageExtension::Gnu;
}

auto GccToolChain::originalTargetTriple() const -> QString
{
  if (m_originalTargetTriple.isEmpty())
    m_originalTargetTriple = detectSupportedAbis().originalTargetTriple;
  return m_originalTargetTriple;
}

auto GccToolChain::version() const -> QString
{
  if (m_version.isEmpty())
    m_version = detectVersion();
  return m_version;
}

auto GccToolChain::installDir() const -> FilePath
{
  if (m_installDir.isEmpty())
    m_installDir = detectInstallDir();
  return m_installDir;
}

auto GccToolChain::supportedAbis() const -> Abis
{
  return m_supportedAbis;
}

static auto isNetworkCompiler(const QString &dirPath) -> bool
{
  return dirPath.contains("icecc") || dirPath.contains("distcc");
}

static auto findLocalCompiler(const FilePath &compilerPath, const Environment &env) -> FilePath
{
  // Find the "real" compiler if icecc, distcc or similar are in use. Ignore ccache, since that
  // is local already.

  // Get the path to the compiler, ignoring direct calls to icecc and distcc as we cannot
  // do anything about those.
  if (!isNetworkCompiler(compilerPath.parentDir().toString()))
    return compilerPath;

  // Filter out network compilers
  const auto pathComponents = filtered(env.path(), [](const FilePath &dirPath) {
    return !isNetworkCompiler(dirPath.toString());
  });

  // This effectively searches the PATH twice, once via pathComponents and once via PATH itself:
  // searchInPath filters duplicates, so that will not hurt.
  const auto path = env.searchInPath(compilerPath.fileName(), pathComponents);

  return path.isEmpty() ? compilerPath : path;
}

// For querying operations such as -dM
static auto filteredFlags(const QStringList &allFlags, bool considerSysroot) -> QStringList
{
  QStringList filtered;
  for (auto i = 0; i < allFlags.size(); ++i) {
    const auto &a = allFlags.at(i);
    if (a.startsWith("--gcc-toolchain=")) {
      filtered << a;
    } else if (a == "-arch") {
      if (++i < allFlags.length() && !filtered.contains(a))
        filtered << a << allFlags.at(i);
    } else if ((considerSysroot && (a == "--sysroot" || a == "-isysroot")) || a == "-D" || a == "-U" || a == "-gcc-toolchain" || a == "-target" || a == "-mllvm" || a == "-isystem") {
      if (++i < allFlags.length())
        filtered << a << allFlags.at(i);
    } else if (a.startsWith("-m") || a == "-Os" || a == "-O0" || a == "-O1" || a == "-O2" || a == "-O3" || a == "-ffinite-math-only" || a == "-fshort-double" || a == "-fshort-wchar" || a == "-fsignaling-nans" || a == "-fno-inline" || a == "-fno-exceptions" || a == "-fstack-protector" || a == "-fstack-protector-all" || a == "-fsanitize=address" || a == "-fno-rtti" || a.startsWith("-std=") || a.startsWith("-stdlib=") || a.startsWith("-specs=") || a == "-ansi" || a == "-undef" || a.startsWith("-D") || a.startsWith("-U") || a == "-fopenmp" || a == "-Wno-deprecated" || a == "-fPIC" || a == "-fpic" || a == "-fPIE" || a == "-fpie" || a.startsWith("-stdlib=") || a.startsWith("-B") || a.startsWith("--target=") || (a.startsWith("-isystem") && a.length() > 8) || a == "-nostdinc" || a == "-nostdinc++") {
      filtered << a;
    }
  }
  return filtered;
}

auto GccToolChain::createMacroInspectionRunner() const -> MacroInspectionRunner
{
  // Using a clean environment breaks ccache/distcc/etc.
  auto env = Environment::systemEnvironment();
  addToEnvironment(env);
  const auto platformCodeGenFlags = m_platformCodeGenFlags;
  auto reinterpretOptions = m_optionsReinterpreter;
  QTC_CHECK(reinterpretOptions);
  auto macroCache = predefinedMacrosCache();
  auto lang = language();

  /*
   * Asks compiler for set of predefined macros
   * flags are the compiler flags collected from project settings
   * returns the list of defines, one per line, e.g. "#define __GXX_WEAK__ 1"
   * Note: changing compiler flags sometimes changes macros set, e.g. -fopenmp
   * adds _OPENMP macro, for full list of macro search by word "when" on this page:
   * http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
   *
   * This runner must be thread-safe!
   */
  return [env, compilerCommand = compilerCommand(), platformCodeGenFlags, reinterpretOptions, macroCache, lang](const QStringList &flags) {
    const auto allFlags = platformCodeGenFlags + flags; // add only cxxflags is empty?
    auto arguments = gccPredefinedMacrosOptions(lang) + filteredFlags(allFlags, true);
    arguments = reinterpretOptions(arguments);
    const auto cachedMacros = macroCache->check(arguments);
    if (cachedMacros)
      return cachedMacros.value();

    const auto macros = gccPredefinedMacros(findLocalCompiler(compilerCommand, env), arguments, env);

    const auto report = MacroInspectionReport{macros, languageVersion(lang, macros)};
    macroCache->insert(arguments, report);

    qCDebug(gccLog) << "MacroInspectionReport for code model:";
    qCDebug(gccLog) << "Language version:" << static_cast<int>(report.languageVersion);
    for (const auto &m : macros) {
      qCDebug(gccLog) << compilerCommand.toUserOutput() << (lang == Constants::CXX_LANGUAGE_ID ? ": C++ [" : ": C [") << arguments.join(", ") << "]" << QString::fromUtf8(m.toByteArray());
    }

    return report;
  };
}

/**
 * @brief Parses gcc flags -std=*, -fopenmp, -fms-extensions.
 * @see http://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html
 */
auto GccToolChain::languageExtensions(const QStringList &cxxflags) const -> LanguageExtensions
{
  auto extensions = defaultLanguageExtensions();

  const auto allCxxflags = m_platformCodeGenFlags + cxxflags; // add only cxxflags is empty?
  foreach(const QString &flag, allCxxflags) {
    if (flag.startsWith("-std=")) {
      const auto std = flag.mid(5).toLatin1();
      if (std.startsWith("gnu"))
        extensions |= LanguageExtension::Gnu;
      else
        extensions &= ~LanguageExtensions(LanguageExtension::Gnu);
    } else if (flag == "-fopenmp") {
      extensions |= LanguageExtension::OpenMP;
    } else if (flag == "-fms-extensions") {
      extensions |= LanguageExtension::Microsoft;
    }
  }

  return extensions;
}

auto GccToolChain::warningFlags(const QStringList &cflags) const -> WarningFlags
{
  // based on 'LC_ALL="en" gcc -Q --help=warnings | grep enabled'
  auto flags(WarningFlags::Deprecated | WarningFlags::IgnoredQualifiers | WarningFlags::SignedComparison | WarningFlags::UninitializedVars);
  const auto groupWall(WarningFlags::All | WarningFlags::UnknownPragma | WarningFlags::UnusedFunctions | WarningFlags::UnusedLocals | WarningFlags::UnusedResult | WarningFlags::UnusedValue | WarningFlags::SignedComparison | WarningFlags::UninitializedVars);
  const auto groupWextra(WarningFlags::Extra | WarningFlags::IgnoredQualifiers | WarningFlags::UnusedParams);

  foreach(const QString &flag, cflags) {
    if (flag == "--all-warnings")
      flags |= groupWall;
    else if (flag == "--extra-warnings")
      flags |= groupWextra;

    WarningFlagAdder add(flag, flags);
    if (add.triggered())
      continue;

    // supported by clang too
    add("error", WarningFlags::AsErrors);
    add("all", groupWall);
    add("extra", groupWextra);
    add("deprecated", WarningFlags::Deprecated);
    add("effc++", WarningFlags::EffectiveCxx);
    add("ignored-qualifiers", WarningFlags::IgnoredQualifiers);
    add("non-virtual-dtor", WarningFlags::NonVirtualDestructor);
    add("overloaded-virtual", WarningFlags::OverloadedVirtual);
    add("shadow", WarningFlags::HiddenLocals);
    add("sign-compare", WarningFlags::SignedComparison);
    add("unknown-pragmas", WarningFlags::UnknownPragma);
    add("unused", WarningFlags::UnusedFunctions | WarningFlags::UnusedLocals | WarningFlags::UnusedParams | WarningFlags::UnusedResult | WarningFlags::UnusedValue);
    add("unused-function", WarningFlags::UnusedFunctions);
    add("unused-variable", WarningFlags::UnusedLocals);
    add("unused-parameter", WarningFlags::UnusedParams);
    add("unused-result", WarningFlags::UnusedResult);
    add("unused-value", WarningFlags::UnusedValue);
    add("uninitialized", WarningFlags::UninitializedVars);
  }
  return flags;
}

auto GccToolChain::includedFiles(const QStringList &flags, const QString &directoryPath) const -> QStringList
{
  return ToolChain::includedFiles("-include", flags, directoryPath);
}

auto GccToolChain::gccPrepareArguments(const QStringList &flags, const QString &sysRoot, const QStringList &platformCodeGenFlags, Id languageId, OptionsReinterpreter reinterpretOptions) -> QStringList
{
  QStringList arguments;
  const auto hasKitSysroot = !sysRoot.isEmpty();
  if (hasKitSysroot)
    arguments.append(QString::fromLatin1("--sysroot=%1").arg(sysRoot));

  QStringList allFlags;
  allFlags << platformCodeGenFlags << flags;
  arguments += filteredFlags(allFlags, !hasKitSysroot);
  arguments << languageOption(languageId) << "-E" << "-v" << "-";
  arguments = reinterpretOptions(arguments);

  return arguments;
}

// NOTE: extraHeaderPathsFunction must NOT capture this or it's members!!!
auto GccToolChain::initExtraHeaderPathsFunction(ExtraHeaderPathsFunction &&extraHeaderPathsFunction) const -> void
{
  m_extraHeaderPathsFunction = std::move(extraHeaderPathsFunction);
}

auto GccToolChain::builtInHeaderPaths(const Environment &env, const FilePath &compilerCommand, const QStringList &platformCodeGenFlags, OptionsReinterpreter reinterpretOptions, HeaderPathsCache headerCache, Id languageId, ExtraHeaderPathsFunction extraHeaderPathsFunction, const QStringList &flags, const QString &sysRoot, const QString &originalTargetTriple) -> HeaderPaths
{
  auto arguments = gccPrepareArguments(flags, sysRoot, platformCodeGenFlags, languageId, reinterpretOptions);

  // Must be clang case only.
  if (!originalTargetTriple.isEmpty())
    arguments << "-target" << originalTargetTriple;

  const auto cachedPaths = headerCache->check(qMakePair(env, arguments));
  if (cachedPaths)
    return cachedPaths.value();

  auto paths = gccHeaderPaths(findLocalCompiler(compilerCommand, env), arguments, env);
  extraHeaderPathsFunction(paths);
  headerCache->insert(qMakePair(env, arguments), paths);

  qCDebug(gccLog) << "Reporting header paths to code model:";
  for (const auto &hp : qAsConst(paths)) {
    qCDebug(gccLog) << compilerCommand.toUserOutput() << (languageId == Constants::CXX_LANGUAGE_ID ? ": C++ [" : ": C [") << arguments.join(", ") << "]" << hp.path;
  }

  return paths;
}

auto GccToolChain::createBuiltInHeaderPathsRunner(const Environment &env) const -> BuiltInHeaderPathsRunner
{
  // Using a clean environment breaks ccache/distcc/etc.
  auto fullEnv = env;
  addToEnvironment(fullEnv);

  // This runner must be thread-safe!
  return [fullEnv, compilerCommand = compilerCommand(), platformCodeGenFlags = m_platformCodeGenFlags, reinterpretOptions = m_optionsReinterpreter, headerCache = headerPathsCache(), languageId = language(), extraHeaderPathsFunction = m_extraHeaderPathsFunction](const QStringList &flags, const QString &sysRoot, const QString &) {
    return builtInHeaderPaths(fullEnv, compilerCommand, platformCodeGenFlags, reinterpretOptions, headerCache, languageId, extraHeaderPathsFunction, flags, sysRoot,
                              /*originalTargetTriple=*/""); // Must be empty for gcc.
  };
}

auto GccToolChain::addCommandPathToEnvironment(const FilePath &command, Environment &env) -> void
{
  env.prependOrSetPath(command.parentDir());
}

auto GccToolChain::addToEnvironment(Environment &env) const -> void
{
  // On Windows gcc invokes cc1plus which is in libexec directory.
  // cc1plus depends on libwinpthread-1.dll which is in bin, so bin must be in the PATH.
  if (compilerCommand().osType() == OsTypeWindows)
    addCommandPathToEnvironment(compilerCommand(), env);
}

auto GccToolChain::suggestedMkspecList() const -> QStringList
{
  const auto abi = targetAbi();
  const auto host = Abi::hostAbi();

  // Cross compile: Leave the mkspec alone!
  if (abi.architecture() != host.architecture() || abi.os() != host.os() || abi.osFlavor() != host.osFlavor()) // Note: This can fail:-(
    return {};

  if (abi.os() == Abi::DarwinOS) {
    const auto v = version();
    // prefer versioned g++ on macOS. This is required to enable building for older macOS versions
    if (v.startsWith("4.0") && compilerCommand().endsWith("-4.0"))
      return {"macx-g++40"};
    if (v.startsWith("4.2") && compilerCommand().endsWith("-4.2"))
      return {"macx-g++42"};
    return {"macx-g++"};
  }

  if (abi.os() == Abi::LinuxOS) {
    if (abi.osFlavor() != Abi::GenericFlavor)
      return {}; // most likely not a desktop, so leave the mkspec alone.
    if (abi.wordWidth() == host.wordWidth()) {
      // no need to explicitly set the word width, but provide that mkspec anyway to make sure
      // that the correct compiler is picked if a mkspec with a wordwidth is given.
      return {"linux-g++", "linux-g++-" + QString::number(targetAbi().wordWidth())};
    }
    return {"linux-g++-" + QString::number(targetAbi().wordWidth())};
  }

  if (abi.os() == Abi::BsdOS && abi.osFlavor() == Abi::FreeBsdFlavor)
    return {"freebsd-g++"};

  return {};
}

auto GccToolChain::makeCommand(const Environment &environment) const -> FilePath
{
  const auto tmp = environment.searchInPath("make");
  return tmp.isEmpty() ? "make" : tmp;
}

auto GccToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  return GccParser::gccParserSuite();
}

auto GccToolChain::resetToolChain(const FilePath &path) -> void
{
  const auto resetDisplayName = (displayName() == defaultDisplayName());

  setCompilerCommand(path);

  const auto currentAbi = targetAbi();
  const auto detectedAbis = detectSupportedAbis();
  m_supportedAbis = detectedAbis.supportedAbis;
  m_originalTargetTriple = detectedAbis.originalTargetTriple;
  m_installDir = installDir();

  if (m_supportedAbis.isEmpty())
    setTargetAbiNoSignal(Abi());
  else if (!m_supportedAbis.contains(currentAbi))
    setTargetAbiNoSignal(m_supportedAbis.at(0));

  if (resetDisplayName)
    setDisplayName(defaultDisplayName()); // calls toolChainUpdated()!
  else
    toolChainUpdated();
}

auto GccToolChain::setPlatformCodeGenFlags(const QStringList &flags) -> void
{
  if (flags != m_platformCodeGenFlags) {
    m_platformCodeGenFlags = flags;
    toolChainUpdated();
  }
}

auto GccToolChain::extraCodeModelFlags() const -> QStringList
{
  return platformCodeGenFlags();
}

/*!
    Code gen flags that have to be passed to the compiler.
 */
auto GccToolChain::platformCodeGenFlags() const -> QStringList
{
  return m_platformCodeGenFlags;
}

auto GccToolChain::setPlatformLinkerFlags(const QStringList &flags) -> void
{
  if (flags != m_platformLinkerFlags) {
    m_platformLinkerFlags = flags;
    toolChainUpdated();
  }
}

/*!
    Flags that have to be passed to the linker.

    For example: \c{-arch armv7}
 */
auto GccToolChain::platformLinkerFlags() const -> QStringList
{
  return m_platformLinkerFlags;
}

auto GccToolChain::toMap() const -> QVariantMap
{
  auto data = ToolChain::toMap();
  data.insert(compilerPlatformCodeGenFlagsKeyC, m_platformCodeGenFlags);
  data.insert(compilerPlatformLinkerFlagsKeyC, m_platformLinkerFlags);
  data.insert(originalTargetTripleKeyC, m_originalTargetTriple);
  data.insert(supportedAbisKeyC, Utils::transform<QStringList>(m_supportedAbis, &Abi::toString));
  return data;
}

auto GccToolChain::fromMap(const QVariantMap &data) -> bool
{
  if (!ToolChain::fromMap(data))
    return false;

  m_platformCodeGenFlags = data.value(compilerPlatformCodeGenFlagsKeyC).toStringList();
  m_platformLinkerFlags = data.value(compilerPlatformLinkerFlagsKeyC).toStringList();
  m_originalTargetTriple = data.value(originalTargetTripleKeyC).toString();
  const auto abiList = data.value(supportedAbisKeyC).toStringList();
  m_supportedAbis.clear();
  for (const auto &a : abiList)
    m_supportedAbis.append(Abi::fromString(a));

  const auto targetAbiString = data.value(targetAbiKeyC).toString();
  if (targetAbiString.isEmpty())
    resetToolChain(compilerCommand());

  return true;
}

auto GccToolChain::operator ==(const ToolChain &other) const -> bool
{
  if (!ToolChain::operator ==(other))
    return false;

  const auto gccTc = static_cast<const GccToolChain*>(&other);
  return compilerCommand() == gccTc->compilerCommand() && targetAbi() == gccTc->targetAbi() && m_platformCodeGenFlags == gccTc->m_platformCodeGenFlags && m_platformLinkerFlags == gccTc->m_platformLinkerFlags;
}

auto GccToolChain::createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget>
{
  return std::make_unique<GccToolChainConfigWidget>(this);
}

auto GccToolChain::updateSupportedAbis() const -> void
{
  if (m_supportedAbis.isEmpty()) {
    const auto detected = detectSupportedAbis();
    m_supportedAbis = detected.supportedAbis;
    m_originalTargetTriple = detected.originalTargetTriple;
  }
}

auto GccToolChain::setOptionsReinterpreter(const OptionsReinterpreter &optionsReinterpreter) -> void
{
  m_optionsReinterpreter = optionsReinterpreter;
}

auto GccToolChain::detectSupportedAbis() const -> DetectedAbisResult
{
  auto env = Environment::systemEnvironment();
  addToEnvironment(env);
  const auto macros = createMacroInspectionRunner()({}).macros;
  return guessGccAbi(findLocalCompiler(compilerCommand(), env), env, macros, platformCodeGenFlags());
}

auto GccToolChain::detectVersion() const -> QString
{
  auto env = Environment::systemEnvironment();
  addToEnvironment(env);
  return gccVersion(findLocalCompiler(compilerCommand(), env), env, filteredFlags(platformCodeGenFlags(), true));
}

auto GccToolChain::detectInstallDir() const -> FilePath
{
  auto env = Environment::systemEnvironment();
  addToEnvironment(env);
  return gccInstallDir(findLocalCompiler(compilerCommand(), env), env, filteredFlags(platformCodeGenFlags(), true));
}

// --------------------------------------------------------------------------
// GccToolChainFactory
// --------------------------------------------------------------------------

static auto gnuSearchPathsFromRegistry() -> FilePaths
{
  if (!HostOsInfo::isWindowsHost())
    return {};

  // Registry token for the "GNU Tools for ARM Embedded Processors".
  static const char kRegistryToken[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\" "Windows\\CurrentVersion\\Uninstall\\";

  FilePaths searchPaths;

  QSettings registry(kRegistryToken, QSettings::NativeFormat);
  const auto productGroups = registry.childGroups();
  for (const auto &productKey : productGroups) {
    if (!productKey.startsWith("GNU Tools for ARM Embedded Processors"))
      continue;
    registry.beginGroup(productKey);
    auto uninstallFilePath = registry.value("UninstallString").toString();
    if (uninstallFilePath.startsWith(QLatin1Char('"')))
      uninstallFilePath.remove(0, 1);
    if (uninstallFilePath.endsWith(QLatin1Char('"')))
      uninstallFilePath.remove(uninstallFilePath.size() - 1, 1);
    registry.endGroup();

    const auto toolkitRootPath = QFileInfo(uninstallFilePath).path();
    const QString toolchainPath = toolkitRootPath + QLatin1String("/bin");
    searchPaths.push_back(FilePath::fromString(toolchainPath));
  }

  return searchPaths;
}

static auto atmelSearchPathsFromRegistry() -> FilePaths
{
  if (!HostOsInfo::isWindowsHost())
    return {};

  // Registry token for the "Atmel" toolchains, e.g. provided by installed
  // "Atmel Studio" IDE.
  static const char kRegistryToken[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Atmel\\";

  FilePaths searchPaths;
  QSettings registry(kRegistryToken, QSettings::NativeFormat);

  // This code enumerate the installed toolchains provided
  // by the Atmel Studio v6.x.
  const auto toolchainGroups = registry.childGroups();
  for (const auto &toolchainKey : toolchainGroups) {
    if (!toolchainKey.endsWith("GCC"))
      continue;
    registry.beginGroup(toolchainKey);
    const auto entries = registry.childGroups();
    for (const auto &entryKey : entries) {
      registry.beginGroup(entryKey);
      const auto installDir = registry.value("Native/InstallDir").toString();
      const auto version = registry.value("Native/Version").toString();
      registry.endGroup();

      QString toolchainPath = installDir + QLatin1String("/Atmel Toolchain/") + toolchainKey + QLatin1String("/Native/") + version;
      if (toolchainKey.startsWith("ARM"))
        toolchainPath += QLatin1String("/arm-gnu-toolchain");
      else if (toolchainKey.startsWith("AVR32"))
        toolchainPath += QLatin1String("/avr32-gnu-toolchain");
      else if (toolchainKey.startsWith("AVR8"))
        toolchainPath += QLatin1String("/avr8-gnu-toolchain");
      else
        break;

      toolchainPath += QLatin1String("/bin");

      const auto path = FilePath::fromString(toolchainPath);
      if (path.exists()) {
        searchPaths.push_back(FilePath::fromString(toolchainPath));
        break;
      }
    }
    registry.endGroup();
  }

  // This code enumerate the installed toolchains provided
  // by the Atmel Studio v7.
  registry.beginGroup("AtmelStudio");
  const auto productVersions = registry.childGroups();
  for (const auto &productVersionKey : productVersions) {
    registry.beginGroup(productVersionKey);
    const auto installDir = registry.value("InstallDir").toString();
    registry.endGroup();

    const QStringList knownToolchainSubdirs = {"/toolchain/arm/arm-gnu-toolchain/bin/", "/toolchain/avr8/avr8-gnu-toolchain/bin/", "/toolchain/avr32/avr32-gnu-toolchain/bin/",};

    for (const auto &subdir : knownToolchainSubdirs) {
      const QString toolchainPath = installDir + subdir;
      const auto path = FilePath::fromString(toolchainPath);
      if (!path.exists())
        continue;
      searchPaths.push_back(path);
    }
  }
  registry.endGroup();

  return searchPaths;
}

static auto renesasRl78SearchPathsFromRegistry() -> FilePaths
{
  if (!HostOsInfo::isWindowsHost())
    return {};

  // Registry token for the "Renesas RL78" toolchain.
  static const char kRegistryToken[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\" "Windows\\CurrentVersion\\Uninstall";

  FilePaths searchPaths;

  QSettings registry(QLatin1String(kRegistryToken), QSettings::NativeFormat);
  const auto productGroups = registry.childGroups();
  for (const auto &productKey : productGroups) {
    if (!productKey.startsWith("GCC for Renesas RL78"))
      continue;
    registry.beginGroup(productKey);
    const auto installLocation = registry.value("InstallLocation").toString();
    registry.endGroup();
    if (installLocation.isEmpty())
      continue;

    const auto toolchainPath = FilePath::fromUserInput(installLocation).pathAppended("rl78-elf/rl78-elf/bin");
    if (!toolchainPath.exists())
      continue;
    searchPaths.push_back(toolchainPath);
  }

  return searchPaths;
}

GccToolChainFactory::GccToolChainFactory()
{
  setDisplayName(GccToolChain::tr("GCC"));
  setSupportedToolChainType(Constants::GCC_TOOLCHAIN_TYPEID);
  setSupportedLanguages({Constants::C_LANGUAGE_ID, Constants::CXX_LANGUAGE_ID});
  setToolchainConstructor([] { return new GccToolChain(Constants::GCC_TOOLCHAIN_TYPEID); });
  setUserCreatable(true);
}

auto GccToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  // GCC is almost never what you want on macOS, but it is by default found in /usr/bin
  if (HostOsInfo::isMacHost() && (!detector.device || detector.device->type() == Constants::DESKTOP_DEVICE_TYPE)) {
    return {};
  }
  Toolchains tcs;
  static const auto tcChecker = [](const ToolChain *tc) {
    return tc->targetAbi().osFlavor() != Abi::WindowsMSysFlavor && tc->compilerCommand().fileName() != "c89-gcc" && tc->compilerCommand().fileName() != "c99-gcc";
  };
  tcs.append(autoDetectToolchains("g++", DetectVariants::Yes, Constants::CXX_LANGUAGE_ID, Constants::GCC_TOOLCHAIN_TYPEID, detector, tcChecker));
  tcs.append(autoDetectToolchains("gcc", DetectVariants::Yes, Constants::C_LANGUAGE_ID, Constants::GCC_TOOLCHAIN_TYPEID, detector, tcChecker));
  return tcs;
}

auto GccToolChainFactory::detectForImport(const ToolChainDescription &tcd) const -> Toolchains
{
  const auto fileName = tcd.compilerPath.completeBaseName();
  const auto resolvedSymlinksFileName = tcd.compilerPath.resolveSymlinks().completeBaseName();

  const auto isCCompiler = tcd.language == Constants::C_LANGUAGE_ID && (fileName.startsWith("gcc") || fileName.endsWith("gcc") || (fileName == "cc" && !resolvedSymlinksFileName.contains("clang")));

  const auto isCxxCompiler = tcd.language == Constants::CXX_LANGUAGE_ID && (fileName.startsWith("g++") || fileName.endsWith("g++") || (fileName == "c++" && !resolvedSymlinksFileName.contains("clang")));

  if (isCCompiler || isCxxCompiler) {
    return autoDetectToolChain(tcd, [](const ToolChain *tc) {
      return tc->targetAbi().osFlavor() != Abi::WindowsMSysFlavor;
    });
  }
  return {};
}

static auto findCompilerCandidates(const ToolchainDetector &detector, const QString &compilerName, bool detectVariants) -> FilePaths
{
  const auto device = detector.device;
  const QFileInfo fi(compilerName);
  if (device.isNull() && fi.isAbsolute() && fi.isFile())
    return {FilePath::fromString(compilerName)};

  QStringList nameFilters(compilerName);
  if (detectVariants) {
    nameFilters << compilerName + "-[1-9]*"       // "clang-8", "gcc-5"
      << ("*-" + compilerName)                    // "avr-gcc", "avr32-gcc"
      << ("*-" + compilerName + "-[1-9]*")        // "avr-gcc-4.8.1", "avr32-gcc-4.4.7"
      << ("*-*-*-" + compilerName)                // "arm-none-eabi-gcc"
      << ("*-*-*-" + compilerName + "-[1-9]*")    // "arm-none-eabi-gcc-9.1.0"
      << ("*-*-*-*-" + compilerName)              // "x86_64-pc-linux-gnu-gcc"
      << ("*-*-*-*-" + compilerName + "-[1-9]*"); // "x86_64-pc-linux-gnu-gcc-7.4.1"
  }
  nameFilters = transform(nameFilters, [os = device ? device->osType() : HostOsInfo::hostOs()](const QString &baseName) {
    return OsSpecificAspects::withExecutableSuffix(os, baseName);
  });

  FilePaths compilerPaths;

  if (!device.isNull()) {
    // FIXME: Merge with block below
    auto searchPaths = detector.searchPaths;
    if (searchPaths.isEmpty())
      searchPaths = device->systemEnvironment().path();
    for (const auto &deviceDir : qAsConst(searchPaths)) {
      static const QRegularExpression regexp(binaryRegexp);
      const auto callBack = [&compilerPaths, compilerName](const FilePath &candidate) {
        if (candidate.fileName() == compilerName)
          compilerPaths << candidate;
        else if (regexp.match(candidate.path()).hasMatch())
          compilerPaths << candidate;
        return true;
      };
      const auto globalDir = device->mapToGlobalPath(deviceDir);
      device->iterateDirectory(globalDir, callBack, {nameFilters, QDir::Files | QDir::Executable});
    }
  } else {
    // The normal, local host case.
    auto searchPaths = detector.searchPaths;
    if (searchPaths.isEmpty()) {
      searchPaths = Environment::systemEnvironment().path();
      searchPaths << gnuSearchPathsFromRegistry();
      searchPaths << atmelSearchPathsFromRegistry();
      searchPaths << renesasRl78SearchPathsFromRegistry();
      if (HostOsInfo::isAnyUnixHost()) {
        FilePath ccachePath = "/usr/lib/ccache/bin";
        if (!ccachePath.exists())
          ccachePath = "/usr/lib/ccache";
        if (ccachePath.exists() && !searchPaths.contains(ccachePath))
          searchPaths << ccachePath;
      }
    }
    for (const auto &dir : qAsConst(searchPaths)) {
      static const QRegularExpression regexp(binaryRegexp);
      QDir binDir(dir.toString());
      const auto fileNames = binDir.entryList(nameFilters, QDir::Files | QDir::Executable);
      for (const auto &fileName : fileNames) {
        if (fileName != compilerName && !regexp.match(QFileInfo(fileName).completeBaseName()).hasMatch()) {
          continue;
        }
        compilerPaths << FilePath::fromString(binDir.filePath(fileName));
      }
    }
  }

  return compilerPaths;
}

auto GccToolChainFactory::autoDetectToolchains(const QString &compilerName, DetectVariants detectVariants, const Id language, const Id requiredTypeId, const ToolchainDetector &detector, const ToolchainChecker &checker) const -> Toolchains
{
  const auto compilerPaths = findCompilerCandidates(detector, compilerName, detectVariants == DetectVariants::Yes);
  auto existingCandidates = filtered(detector.alreadyKnown, [language](const ToolChain *tc) { return tc->language() == language; });
  Toolchains result;
  for (const auto &compilerPath : qAsConst(compilerPaths)) {
    auto alreadyExists = false;
    for (const auto existingTc : existingCandidates) {
      // We have a match if the existing toolchain ultimately refers to the same file
      // as the candidate path, either directly or via a hard or soft link.
      // Exceptions:
      //   - clang++ is often a soft link to clang, but behaves differently.
      //   - ccache and icecc also create soft links that must not be followed here.
      auto existingTcMatches = false;
      const auto existingCommand = existingTc->compilerCommand();
      if ((requiredTypeId == Constants::CLANG_TOOLCHAIN_TYPEID && ((language == Constants::CXX_LANGUAGE_ID && !existingCommand.fileName().contains("clang++")) || (language == Constants::C_LANGUAGE_ID && !existingCommand.baseName().endsWith("clang")))) || compilerPath.toString().contains("icecc") || compilerPath.toString().contains("ccache")) {
        existingTcMatches = existingCommand == compilerPath;
      } else {
        existingTcMatches = Environment::systemEnvironment().isSameExecutable(existingCommand.toString(), compilerPath.toString()) || (HostOsInfo::isWindowsHost() && existingCommand.toFileInfo().size() == compilerPath.toFileInfo().size());
      }
      if (existingTcMatches) {
        if (existingTc->typeId() == requiredTypeId && (!checker || checker(existingTc)) && !result.contains(existingTc)) {
          result << existingTc;
        }
        alreadyExists = true;
      }
    }
    if (!alreadyExists) {
      const auto newToolchains = autoDetectToolChain({compilerPath, language}, checker);
      result << newToolchains;
      existingCandidates << newToolchains;
    }
  }

  return result;
}

auto GccToolChainFactory::autoDetectToolChain(const ToolChainDescription &tcd, const ToolchainChecker &checker) const -> Toolchains
{
  Toolchains result;

  auto systemEnvironment = tcd.compilerPath.deviceEnvironment();
  GccToolChain::addCommandPathToEnvironment(tcd.compilerPath, systemEnvironment);
  const auto localCompilerPath = findLocalCompiler(tcd.compilerPath, systemEnvironment);
  if (ToolChainManager::isBadToolchain(localCompilerPath))
    return result;
  const auto macros = gccPredefinedMacros(localCompilerPath, gccPredefinedMacrosOptions(tcd.language), systemEnvironment);
  if (macros.isEmpty()) {
    ToolChainManager::addBadToolchain(localCompilerPath);
    return result;
  }
  const auto detectedAbis = guessGccAbi(localCompilerPath, systemEnvironment, macros);
  for (const auto &abi : detectedAbis.supportedAbis) {
    std::unique_ptr<GccToolChain> tc(dynamic_cast<GccToolChain*>(create()));
    if (!tc)
      return result;

    tc->setLanguage(tcd.language);
    tc->setDetection(ToolChain::AutoDetection);
    tc->predefinedMacrosCache()->insert(QStringList(), ToolChain::MacroInspectionReport{macros, ToolChain::languageVersion(tcd.language, macros)});
    tc->setCompilerCommand(tcd.compilerPath);
    tc->setSupportedAbis(detectedAbis.supportedAbis);
    tc->setTargetAbi(abi);
    tc->setOriginalTargetTriple(detectedAbis.originalTargetTriple);
    tc->setDisplayName(tc->defaultDisplayName()); // reset displayname
    if (!checker || checker(tc.get()))
      result.append(tc.release());
  }
  return result;
}

// --------------------------------------------------------------------------
// GccToolChainConfigWidget
// --------------------------------------------------------------------------

namespace Internal {
class TargetTripleWidget : public QWidget {
  Q_OBJECT public:
  TargetTripleWidget(const ToolChain *toolchain)
  {
    const auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tripleLineEdit.setEnabled(false);
    m_overrideCheckBox.setText(tr("Override for code model"));
    m_overrideCheckBox.setToolTip(tr("Check this button in the rare case that the code model\n" "fails because clang does not understand the target architecture."));
    layout->addWidget(&m_tripleLineEdit, 1);
    layout->addWidget(&m_overrideCheckBox);
    layout->addStretch(1);

    connect(&m_tripleLineEdit, &QLineEdit::textEdited, this, &TargetTripleWidget::valueChanged);
    connect(&m_overrideCheckBox, &QCheckBox::toggled, &m_tripleLineEdit, &QLineEdit::setEnabled);

    m_tripleLineEdit.setText(toolchain->effectiveCodeModelTargetTriple());
    m_overrideCheckBox.setChecked(!toolchain->explicitCodeModelTargetTriple().isEmpty());
  }

  auto explicitCodeModelTargetTriple() const -> QString
  {
    if (m_overrideCheckBox.isChecked())
      return m_tripleLineEdit.text();
    return {};
  }

signals:
  auto valueChanged() -> void;

private:
  QLineEdit m_tripleLineEdit;
  QCheckBox m_overrideCheckBox;
};
}

GccToolChainConfigWidget::GccToolChainConfigWidget(GccToolChain *tc) : ToolChainConfigWidget(tc), m_abiWidget(new AbiWidget), m_compilerCommand(new PathChooser), m_targetTripleWidget(new TargetTripleWidget(tc))
{
  Q_ASSERT(tc);

  const auto gnuVersionArgs = QStringList("--version");
  m_compilerCommand->setExpectedKind(PathChooser::ExistingCommand);
  m_compilerCommand->setCommandVersionArguments(gnuVersionArgs);
  m_compilerCommand->setHistoryCompleter("PE.Gcc.Command.History");
  m_mainLayout->addRow(tr("&Compiler path:"), m_compilerCommand);
  m_platformCodeGenFlagsLineEdit = new QLineEdit(this);
  m_platformCodeGenFlagsLineEdit->setText(ProcessArgs::joinArgs(tc->platformCodeGenFlags()));
  m_mainLayout->addRow(tr("Platform codegen flags:"), m_platformCodeGenFlagsLineEdit);
  m_platformLinkerFlagsLineEdit = new QLineEdit(this);
  m_platformLinkerFlagsLineEdit->setText(ProcessArgs::joinArgs(tc->platformLinkerFlags()));
  m_mainLayout->addRow(tr("Platform linker flags:"), m_platformLinkerFlagsLineEdit);
  m_mainLayout->addRow(tr("&ABI:"), m_abiWidget);
  m_mainLayout->addRow(tr("Target triple:"), m_targetTripleWidget);

  m_abiWidget->setEnabled(false);
  addErrorLabel();

  setFromToolchain();

  connect(m_compilerCommand, &PathChooser::rawPathChanged, this, &GccToolChainConfigWidget::handleCompilerCommandChange);
  connect(m_platformCodeGenFlagsLineEdit, &QLineEdit::editingFinished, this, &GccToolChainConfigWidget::handlePlatformCodeGenFlagsChange);
  connect(m_platformLinkerFlagsLineEdit, &QLineEdit::editingFinished, this, &GccToolChainConfigWidget::handlePlatformLinkerFlagsChange);
  connect(m_abiWidget, &AbiWidget::abiChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_targetTripleWidget, &TargetTripleWidget::valueChanged, this, &ToolChainConfigWidget::dirty);
}

auto GccToolChainConfigWidget::applyImpl() -> void
{
  if (toolChain()->isAutoDetected())
    return;

  const auto tc = static_cast<GccToolChain*>(toolChain());
  Q_ASSERT(tc);
  const auto displayName = tc->displayName();
  tc->setCompilerCommand(m_compilerCommand->filePath());
  if (m_abiWidget) {
    tc->setSupportedAbis(m_abiWidget->supportedAbis());
    tc->setTargetAbi(m_abiWidget->currentAbi());
  }
  tc->setInstallDir(tc->detectInstallDir());
  tc->setOriginalTargetTriple(tc->detectSupportedAbis().originalTargetTriple);
  tc->setExplicitCodeModelTargetTriple(m_targetTripleWidget->explicitCodeModelTargetTriple());
  tc->setDisplayName(displayName); // reset display name
  tc->setPlatformCodeGenFlags(splitString(m_platformCodeGenFlagsLineEdit->text()));
  tc->setPlatformLinkerFlags(splitString(m_platformLinkerFlagsLineEdit->text()));

  if (m_macros.isEmpty())
    return;

  tc->predefinedMacrosCache()->insert(tc->platformCodeGenFlags(), ToolChain::MacroInspectionReport{m_macros, ToolChain::languageVersion(tc->language(), m_macros)});
}

auto GccToolChainConfigWidget::setFromToolchain() -> void
{
  // subwidgets are not yet connected!
  QSignalBlocker blocker(this);
  const auto tc = static_cast<GccToolChain*>(toolChain());
  m_compilerCommand->setFilePath(tc->compilerCommand());
  m_platformCodeGenFlagsLineEdit->setText(ProcessArgs::joinArgs(tc->platformCodeGenFlags()));
  m_platformLinkerFlagsLineEdit->setText(ProcessArgs::joinArgs(tc->platformLinkerFlags()));
  if (m_abiWidget) {
    m_abiWidget->setAbis(tc->supportedAbis(), tc->targetAbi());
    if (!m_isReadOnly && !m_compilerCommand->filePath().toString().isEmpty())
      m_abiWidget->setEnabled(true);
  }
}

auto GccToolChainConfigWidget::isDirtyImpl() const -> bool
{
  const auto tc = static_cast<GccToolChain*>(toolChain());
  Q_ASSERT(tc);
  return m_compilerCommand->filePath() != tc->compilerCommand() || m_platformCodeGenFlagsLineEdit->text() != ProcessArgs::joinArgs(tc->platformCodeGenFlags()) || m_platformLinkerFlagsLineEdit->text() != ProcessArgs::joinArgs(tc->platformLinkerFlags()) || m_targetTripleWidget->explicitCodeModelTargetTriple() != tc->explicitCodeModelTargetTriple() || (m_abiWidget && m_abiWidget->currentAbi() != tc->targetAbi());
}

auto GccToolChainConfigWidget::makeReadOnlyImpl() -> void
{
  m_compilerCommand->setReadOnly(true);
  if (m_abiWidget)
    m_abiWidget->setEnabled(false);
  m_platformCodeGenFlagsLineEdit->setEnabled(false);
  m_platformLinkerFlagsLineEdit->setEnabled(false);
  m_targetTripleWidget->setEnabled(false);
  m_isReadOnly = true;
}

auto GccToolChainConfigWidget::handleCompilerCommandChange() -> void
{
  if (!m_abiWidget)
    return;

  auto haveCompiler = false;
  const auto currentAbi = m_abiWidget->currentAbi();
  const auto customAbi = m_abiWidget->isCustomAbi() && m_abiWidget->isEnabled();
  const auto path = m_compilerCommand->filePath();
  Abis abiList;

  if (!path.isEmpty()) {
    const auto fi(path.toFileInfo());
    haveCompiler = fi.isExecutable() && fi.isFile();
  }
  if (haveCompiler) {
    auto env = path.deviceEnvironment();
    GccToolChain::addCommandPathToEnvironment(path, env);
    const auto args = gccPredefinedMacrosOptions(Constants::CXX_LANGUAGE_ID) + splitString(m_platformCodeGenFlagsLineEdit->text());
    const auto localCompilerPath = findLocalCompiler(path, env);
    m_macros = gccPredefinedMacros(localCompilerPath, args, env);
    abiList = guessGccAbi(localCompilerPath, env, m_macros, splitString(m_platformCodeGenFlagsLineEdit->text())).supportedAbis;
  }
  m_abiWidget->setEnabled(haveCompiler);

  // Find a good ABI for the new compiler:
  Abi newAbi;
  if (customAbi || abiList.contains(currentAbi))
    newAbi = currentAbi;

  m_abiWidget->setAbis(abiList, newAbi);
  emit dirty();
}

auto GccToolChainConfigWidget::handlePlatformCodeGenFlagsChange() -> void
{
  const auto str1 = m_platformCodeGenFlagsLineEdit->text();
  const auto str2 = ProcessArgs::joinArgs(splitString(str1));
  if (str1 != str2)
    m_platformCodeGenFlagsLineEdit->setText(str2);
  else
    handleCompilerCommandChange();
}

auto GccToolChainConfigWidget::handlePlatformLinkerFlagsChange() -> void
{
  const auto str1 = m_platformLinkerFlagsLineEdit->text();
  const auto str2 = ProcessArgs::joinArgs(splitString(str1));
  if (str1 != str2)
    m_platformLinkerFlagsLineEdit->setText(str2);
  else emit dirty();
}

// --------------------------------------------------------------------------
// ClangToolChain
// --------------------------------------------------------------------------

static auto mingwToolChains() -> const Toolchains
{
  return ToolChainManager::toolchains([](const ToolChain *tc) -> bool {
    return tc->typeId() == Constants::MINGW_TOOLCHAIN_TYPEID;
  });
}

static auto mingwToolChainFromId(const QByteArray &id) -> const MingwToolChain*
{
  if (id.isEmpty())
    return nullptr;

  for (const ToolChain *tc : mingwToolChains()) {
    if (tc->id() == id)
      return static_cast<const MingwToolChain*>(tc);
  }

  return nullptr;
}

auto ClangToolChain::syncAutodetectedWithParentToolchains() -> void
{
  if (!HostOsInfo::isWindowsHost() || typeId() != Constants::CLANG_TOOLCHAIN_TYPEID || !isAutoDetected()) {
    return;
  }

  QObject::disconnect(m_thisToolchainRemovedConnection);
  QObject::disconnect(m_mingwToolchainAddedConnection);

  if (!ToolChainManager::isLoaded()) {
    QObject::connect(ToolChainManager::instance(), &ToolChainManager::toolChainsLoaded, [id = id()] {
      if (const auto tc = ToolChainManager::findToolChain(id)) {
        if (tc->typeId() == Constants::CLANG_TOOLCHAIN_TYPEID)
          static_cast<ClangToolChain*>(tc)->syncAutodetectedWithParentToolchains();
      }
    });
    return;
  }

  if (!mingwToolChainFromId(m_parentToolChainId)) {
    const auto mingwTCs = mingwToolChains();
    m_parentToolChainId = mingwTCs.isEmpty() ? QByteArray() : mingwTCs.front()->id();
  }

  // Subscribe only autodetected toolchains.
  const auto tcManager = ToolChainManager::instance();
  m_mingwToolchainAddedConnection = QObject::connect(tcManager, &ToolChainManager::toolChainAdded, [this](ToolChain *tc) {
    if (tc->typeId() == Constants::MINGW_TOOLCHAIN_TYPEID && !mingwToolChainFromId(m_parentToolChainId)) {
      m_parentToolChainId = tc->id();
    }
  });
  m_thisToolchainRemovedConnection = QObject::connect(tcManager, &ToolChainManager::toolChainRemoved, [this](ToolChain *tc) {
    if (tc == this) {
      QObject::disconnect(m_thisToolchainRemovedConnection);
      QObject::disconnect(m_mingwToolchainAddedConnection);
    } else if (m_parentToolChainId == tc->id()) {
      const auto mingwTCs = mingwToolChains();
      m_parentToolChainId = mingwTCs.isEmpty() ? QByteArray() : mingwTCs.front()->id();
    }
  });
}

ClangToolChain::ClangToolChain() : ClangToolChain(Constants::CLANG_TOOLCHAIN_TYPEID) {}

ClangToolChain::ClangToolChain(Id typeId) : GccToolChain(typeId)
{
  setTypeDisplayName(tr("Clang"));
  syncAutodetectedWithParentToolchains();
}

ClangToolChain::~ClangToolChain()
{
  QObject::disconnect(m_thisToolchainRemovedConnection);
  QObject::disconnect(m_mingwToolchainAddedConnection);
}

static auto mingwAwareMakeCommand(const Environment &environment) -> FilePath
{
  const auto makes = HostOsInfo::isWindowsHost() ? QStringList({"mingw32-make.exe", "make.exe"}) : QStringList({"make"});

  FilePath tmp;
  for (const auto &make : makes) {
    tmp = environment.searchInPath(make);
    if (!tmp.isEmpty())
      return tmp;
  }
  return FilePath::fromString(makes.first());
}

auto ClangToolChain::makeCommand(const Environment &environment) const -> FilePath
{
  return mingwAwareMakeCommand(environment);
}

/**
 * @brief Similar to \a GccToolchain::languageExtensions, but recognizes
 * "-fborland-extensions".
 */
auto ClangToolChain::languageExtensions(const QStringList &cxxflags) const -> LanguageExtensions
{
  auto extensions = GccToolChain::languageExtensions(cxxflags);
  if (cxxflags.contains("-fborland-extensions"))
    extensions |= LanguageExtension::Borland;
  return extensions;
}

auto ClangToolChain::warningFlags(const QStringList &cflags) const -> WarningFlags
{
  auto flags = GccToolChain::warningFlags(cflags);
  foreach(const QString &flag, cflags) {
    if (flag == "-Wdocumentation")
      flags |= WarningFlags::Documentation;
    if (flag == "-Wno-documentation")
      flags &= ~WarningFlags::Documentation;
  }
  return flags;
}

auto ClangToolChain::suggestedMkspecList() const -> QStringList
{
  if (const ToolChain *const parentTc = ToolChainManager::findToolChain(m_parentToolChainId))
    return parentTc->suggestedMkspecList();
  const auto abi = targetAbi();
  if (abi.os() == Abi::DarwinOS)
    return {"macx-clang", "macx-clang-32", "unsupported/macx-clang", "macx-ios-clang"};
  if (abi.os() == Abi::LinuxOS)
    return {"linux-clang", "unsupported/linux-clang"};
  if (abi.os() == Abi::WindowsOS)
    return {"win32-clang-g++"};
  if (abi.architecture() == Abi::AsmJsArchitecture && abi.binaryFormat() == Abi::EmscriptenFormat)
    return {"wasm-emscripten"};
  return {}; // Note: Not supported by Qt yet, so default to the mkspec the Qt was build with
}

auto ClangToolChain::addToEnvironment(Environment &env) const -> void
{
  GccToolChain::addToEnvironment(env);

  const auto sysroot = sysRoot();
  if (!sysroot.isEmpty())
    env.prependOrSetPath(FilePath::fromString(sysroot) / "bin");

  // Clang takes PWD as basis for debug info, if set.
  // When running Qt Creator from a shell, PWD is initially set to an "arbitrary" value.
  // Since the tools are not called through a shell, PWD is never changed to the actual cwd,
  // so we better make sure PWD is empty to begin with
  env.unset("PWD");
}

auto ClangToolChain::originalTargetTriple() const -> QString
{
  const auto parentTC = mingwToolChainFromId(m_parentToolChainId);
  if (parentTC)
    return parentTC->originalTargetTriple();

  return GccToolChain::originalTargetTriple();
}

auto ClangToolChain::sysRoot() const -> QString
{
  const auto parentTC = mingwToolChainFromId(m_parentToolChainId);
  if (!parentTC)
    return QString();

  const auto mingwCompiler = parentTC->compilerCommand();
  return mingwCompiler.parentDir().parentDir().toString();
}

auto ClangToolChain::createBuiltInHeaderPathsRunner(const Environment &env) const -> BuiltInHeaderPathsRunner
{
  // Using a clean environment breaks ccache/distcc/etc.
  auto fullEnv = env;
  addToEnvironment(fullEnv);

  // This runner must be thread-safe!
  return [fullEnv, compilerCommand = compilerCommand(), platformCodeGenFlags = m_platformCodeGenFlags, reinterpretOptions = m_optionsReinterpreter, headerCache = headerPathsCache(), languageId = language(), extraHeaderPathsFunction = m_extraHeaderPathsFunction](const QStringList &flags, const QString &sysRoot, const QString &target) {
    return builtInHeaderPaths(fullEnv, compilerCommand, platformCodeGenFlags, reinterpretOptions, headerCache, languageId, extraHeaderPathsFunction, flags, sysRoot, target);
  };
}

auto ClangToolChain::createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget>
{
  return std::make_unique<ClangToolChainConfigWidget>(this);
}

auto ClangToolChain::toMap() const -> QVariantMap
{
  auto data = GccToolChain::toMap();
  data.insert(parentToolChainIdKeyC, m_parentToolChainId);
  return data;
}

auto ClangToolChain::fromMap(const QVariantMap &data) -> bool
{
  if (!GccToolChain::fromMap(data))
    return false;

  m_parentToolChainId = data.value(parentToolChainIdKeyC).toByteArray();
  syncAutodetectedWithParentToolchains();
  return true;
}

auto ClangToolChain::defaultLanguageExtensions() const -> LanguageExtensions
{
  return LanguageExtension::Gnu;
}

auto ClangToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  return ClangParser::clangParserSuite();
}

// --------------------------------------------------------------------------
// ClangToolChainFactory
// --------------------------------------------------------------------------

ClangToolChainFactory::ClangToolChainFactory()
{
  setDisplayName(ClangToolChain::tr("Clang"));
  setSupportedToolChainType(Constants::CLANG_TOOLCHAIN_TYPEID);
  setSupportedLanguages({Constants::CXX_LANGUAGE_ID, Constants::C_LANGUAGE_ID});
  setToolchainConstructor([] { return new ClangToolChain; });
}

auto ClangToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  Toolchains tcs;
  auto known = detector.alreadyKnown;

  tcs.append(autoDetectToolchains("clang++", DetectVariants::Yes, Constants::CXX_LANGUAGE_ID, Constants::CLANG_TOOLCHAIN_TYPEID, detector));
  tcs.append(autoDetectToolchains("clang", DetectVariants::Yes, Constants::C_LANGUAGE_ID, Constants::CLANG_TOOLCHAIN_TYPEID, detector));
  known.append(tcs);

  const auto compilerPath = Core::ICore::clangExecutable(CLANG_BINDIR);
  if (!compilerPath.isEmpty()) {
    const auto clang = compilerPath.parentDir().pathAppended("clang").withExecutableSuffix();
    tcs.append(autoDetectToolchains(clang.toString(), DetectVariants::No, Constants::C_LANGUAGE_ID, Constants::CLANG_TOOLCHAIN_TYPEID, ToolchainDetector(known, detector.device, detector.searchPaths)));
  }

  return tcs;
}

auto ClangToolChainFactory::detectForImport(const ToolChainDescription &tcd) const -> Toolchains
{
  const auto fileName = tcd.compilerPath.completeBaseName();
  const auto resolvedSymlinksFileName = tcd.compilerPath.resolveSymlinks().completeBaseName();

  const auto isCCompiler = tcd.language == Constants::C_LANGUAGE_ID && ((fileName.startsWith("clang") && !fileName.startsWith("clang++")) || (fileName == "cc" && resolvedSymlinksFileName.contains("clang")));

  const auto isCxxCompiler = tcd.language == Constants::CXX_LANGUAGE_ID && (fileName.startsWith("clang++") || (fileName == "c++" && resolvedSymlinksFileName.contains("clang")));

  if (isCCompiler || isCxxCompiler) {
    return autoDetectToolChain(tcd);
  }
  return {};
}

ClangToolChainConfigWidget::ClangToolChainConfigWidget(ClangToolChain *tc) : GccToolChainConfigWidget(tc)
{
  if (!HostOsInfo::isWindowsHost() || tc->typeId() != Constants::CLANG_TOOLCHAIN_TYPEID)
    return;

  // Remove m_abiWidget row because the parent toolchain abi is going to be used.
  m_mainLayout->removeRow(m_mainLayout->rowCount() - 3); // FIXME: Do something sane instead.
  m_abiWidget = nullptr;

  m_parentToolchainCombo = new QComboBox(this);
  m_mainLayout->insertRow(m_mainLayout->rowCount() - 1, tr("Parent toolchain:"), m_parentToolchainCombo);

  const auto tcManager = ToolChainManager::instance();
  m_parentToolChainConnections.append(connect(tcManager, &ToolChainManager::toolChainUpdated, this, [this](ToolChain *tc) {
    if (tc->typeId() == Constants::MINGW_TOOLCHAIN_TYPEID)
      updateParentToolChainComboBox();
  }));
  m_parentToolChainConnections.append(connect(tcManager, &ToolChainManager::toolChainAdded, this, [this](ToolChain *tc) {
    if (tc->typeId() == Constants::MINGW_TOOLCHAIN_TYPEID)
      updateParentToolChainComboBox();
  }));
  m_parentToolChainConnections.append(connect(tcManager, &ToolChainManager::toolChainRemoved, this, [this](ToolChain *tc) {
    if (tc->id() == toolChain()->id()) {
      for (auto &connection : m_parentToolChainConnections)
        disconnect(connection);
      return;
    }
    if (tc->typeId() == Constants::MINGW_TOOLCHAIN_TYPEID)
      updateParentToolChainComboBox();
  }));

  setFromClangToolchain();
}

auto ClangToolChainConfigWidget::updateParentToolChainComboBox() -> void
{
  const auto *tc = static_cast<ClangToolChain*>(toolChain());
  auto parentId = m_parentToolchainCombo->currentData().toByteArray();
  if (tc->isAutoDetected() || m_parentToolchainCombo->count() == 0)
    parentId = tc->m_parentToolChainId;

  const auto parentTC = mingwToolChainFromId(parentId);

  m_parentToolchainCombo->clear();
  m_parentToolchainCombo->addItem(parentTC ? parentTC->displayName() : QString(), parentTC ? parentId : QByteArray());

  if (tc->isAutoDetected())
    return;

  for (const ToolChain *mingwTC : mingwToolChains()) {
    if (mingwTC->id() == parentId)
      continue;
    if (mingwTC->language() != tc->language())
      continue;
    m_parentToolchainCombo->addItem(mingwTC->displayName(), mingwTC->id());
  }
}

auto ClangToolChainConfigWidget::setFromClangToolchain() -> void
{
  setFromToolchain();

  if (m_parentToolchainCombo)
    updateParentToolChainComboBox();
}

auto ClangToolChainConfigWidget::applyImpl() -> void
{
  GccToolChainConfigWidget::applyImpl();
  if (!m_parentToolchainCombo)
    return;

  auto *tc = static_cast<ClangToolChain*>(toolChain());
  tc->m_parentToolChainId.clear();

  const auto parentId = m_parentToolchainCombo->currentData().toByteArray();
  if (!parentId.isEmpty()) {
    for (const ToolChain *mingwTC : mingwToolChains()) {
      if (parentId == mingwTC->id()) {
        tc->m_parentToolChainId = mingwTC->id();
        tc->setTargetAbi(mingwTC->targetAbi());
        tc->setSupportedAbis(mingwTC->supportedAbis());
        break;
      }
    }
  }
}

auto ClangToolChainConfigWidget::isDirtyImpl() const -> bool
{
  if (GccToolChainConfigWidget::isDirtyImpl())
    return true;

  if (!m_parentToolchainCombo)
    return false;

  const auto tc = static_cast<ClangToolChain*>(toolChain());
  Q_ASSERT(tc);
  const auto parentTC = mingwToolChainFromId(tc->m_parentToolChainId);
  const auto parentId = parentTC ? parentTC->id() : QByteArray();
  return parentId != m_parentToolchainCombo->currentData();
}

auto ClangToolChainConfigWidget::makeReadOnlyImpl() -> void
{
  GccToolChainConfigWidget::makeReadOnlyImpl();
  if (m_parentToolchainCombo)
    m_parentToolchainCombo->setEnabled(false);
}

// --------------------------------------------------------------------------
// MingwToolChain
// --------------------------------------------------------------------------

MingwToolChain::MingwToolChain() : GccToolChain(Constants::MINGW_TOOLCHAIN_TYPEID)
{
  setTypeDisplayName(tr("MinGW"));
}

auto MingwToolChain::suggestedMkspecList() const -> QStringList
{
  if (HostOsInfo::isWindowsHost())
    return {"win32-g++"};
  if (HostOsInfo::isLinuxHost()) {
    if (version().startsWith("4.6."))
      return {"win32-g++-4.6-cross", "unsupported/win32-g++-4.6-cross"};
    return {"win32-g++-cross", "unsupported/win32-g++-cross"};
  }
  return {};
}

auto MingwToolChain::makeCommand(const Environment &environment) const -> FilePath
{
  return mingwAwareMakeCommand(environment);
}

// --------------------------------------------------------------------------
// MingwToolChainFactory
// --------------------------------------------------------------------------

MingwToolChainFactory::MingwToolChainFactory()
{
  setDisplayName(MingwToolChain::tr("MinGW"));
  setSupportedToolChainType(Constants::MINGW_TOOLCHAIN_TYPEID);
  setSupportedLanguages({Constants::CXX_LANGUAGE_ID, Constants::C_LANGUAGE_ID});
  setToolchainConstructor([] { return new MingwToolChain; });
}

auto MingwToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  static const auto tcChecker = [](const ToolChain *tc) {
    return tc->targetAbi().osFlavor() == Abi::WindowsMSysFlavor;
  };
  auto result = autoDetectToolchains("g++", DetectVariants::Yes, Constants::CXX_LANGUAGE_ID, Constants::MINGW_TOOLCHAIN_TYPEID, detector, tcChecker);
  result += autoDetectToolchains("gcc", DetectVariants::Yes, Constants::C_LANGUAGE_ID, Constants::MINGW_TOOLCHAIN_TYPEID, detector, tcChecker);
  return result;
}

auto MingwToolChainFactory::detectForImport(const ToolChainDescription &tcd) const -> Toolchains
{
  const auto fileName = tcd.compilerPath.completeBaseName();
  if ((tcd.language == Constants::C_LANGUAGE_ID && (fileName.startsWith("gcc") || fileName.endsWith("gcc"))) || (tcd.language == Constants::CXX_LANGUAGE_ID && (fileName.startsWith("g++") || fileName.endsWith("g++")))) {
    return autoDetectToolChain(tcd, [](const ToolChain *tc) {
      return tc->targetAbi().osFlavor() == Abi::WindowsMSysFlavor;
    });
  }

  return {};
}

// --------------------------------------------------------------------------
// LinuxIccToolChain
// --------------------------------------------------------------------------

LinuxIccToolChain::LinuxIccToolChain() : GccToolChain(Constants::LINUXICC_TOOLCHAIN_TYPEID)
{
  setTypeDisplayName(tr("ICC"));
}

/**
 * Similar to \a GccToolchain::languageExtensions, but uses "-openmp" instead of
 * "-fopenmp" and "-fms-dialect[=ver]" instead of "-fms-extensions".
 * @see UNIX manual for "icc"
 */
auto LinuxIccToolChain::languageExtensions(const QStringList &cxxflags) const -> LanguageExtensions
{
  auto copy = cxxflags;
  copy.removeAll("-fopenmp");
  copy.removeAll("-fms-extensions");

  auto extensions = GccToolChain::languageExtensions(cxxflags);
  if (cxxflags.contains("-openmp"))
    extensions |= LanguageExtension::OpenMP;
  if (cxxflags.contains("-fms-dialect") || cxxflags.contains("-fms-dialect=8") || cxxflags.contains("-fms-dialect=9") || cxxflags.contains("-fms-dialect=10"))
    extensions |= LanguageExtension::Microsoft;
  return extensions;
}

auto LinuxIccToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  return LinuxIccParser::iccParserSuite();
}

auto LinuxIccToolChain::suggestedMkspecList() const -> QStringList
{
  return {QString("linux-icc-%1").arg(targetAbi().wordWidth())};
}

// --------------------------------------------------------------------------
// LinuxIccToolChainFactory
// --------------------------------------------------------------------------

LinuxIccToolChainFactory::LinuxIccToolChainFactory()
{
  setDisplayName(LinuxIccToolChain::tr("ICC"));
  setSupportedToolChainType(Constants::LINUXICC_TOOLCHAIN_TYPEID);
  setSupportedLanguages({Constants::CXX_LANGUAGE_ID, Constants::C_LANGUAGE_ID});
  setToolchainConstructor([] { return new LinuxIccToolChain; });
}

auto LinuxIccToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  auto result = autoDetectToolchains("icpc", DetectVariants::No, Constants::CXX_LANGUAGE_ID, Constants::LINUXICC_TOOLCHAIN_TYPEID, detector);
  result += autoDetectToolchains("icc", DetectVariants::Yes, Constants::C_LANGUAGE_ID, Constants::LINUXICC_TOOLCHAIN_TYPEID, detector);
  return result;
}

auto LinuxIccToolChainFactory::detectForImport(const ToolChainDescription &tcd) const -> Toolchains
{
  const auto fileName = tcd.compilerPath.completeBaseName();
  if ((tcd.language == Constants::CXX_LANGUAGE_ID && fileName.startsWith("icpc")) || (tcd.language == Constants::C_LANGUAGE_ID && fileName.startsWith("icc"))) {
    return autoDetectToolChain(tcd);
  }
  return {};
}

GccToolChain::WarningFlagAdder::WarningFlagAdder(const QString &flag, WarningFlags &flags) : m_flags(flags)
{
  if (!flag.startsWith("-W")) {
    m_triggered = true;
    return;
  }

  m_doesEnable = !flag.startsWith("-Wno-");
  if (m_doesEnable)
    m_flagUtf8 = flag.mid(2).toUtf8();
  else
    m_flagUtf8 = flag.mid(5).toUtf8();
}

auto GccToolChain::WarningFlagAdder::operator ()(const char name[], WarningFlags flagsSet) -> void
{
  if (m_triggered)
    return;
  if (0 == strcmp(m_flagUtf8.data(), name)) {
    m_triggered = true;
    if (m_doesEnable)
      m_flags |= flagsSet;
    else
      m_flags &= ~flagsSet;
  }
}

auto GccToolChain::WarningFlagAdder::triggered() const -> bool
{
  return m_triggered;
}

} // namespace ProjectExplorer

// Unit tests:

#ifdef WITH_TESTS
#include "projectexplorer.hpp"

#include <QTest>
#include <QUrl>

namespace ProjectExplorer {
void ProjectExplorerPlugin::testGccAbiGuessing_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<QByteArray>("macros");
  QTest::addColumn<QStringList>("abiList");

  QTest::newRow("invalid input") << QString::fromLatin1("Some text") << QByteArray("") << (QStringList());
  QTest::newRow("empty input") << QString::fromLatin1("") << QByteArray("") << (QStringList());
  QTest::newRow("empty input (with macros)") << QString::fromLatin1("") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n#define __Something\n") << (QStringList());
  QTest::newRow("broken input -- 64bit") << QString::fromLatin1("arm-none-foo-gnueabi") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n#define __Something\n") << QStringList({"arm-baremetal-generic-elf-64bit"});
  QTest::newRow("broken input -- 32bit") << QString::fromLatin1("arm-none-foo-gnueabi") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n#define __Something\n") << QStringList({"arm-baremetal-generic-elf-32bit"});
  QTest::newRow("totally broken input -- 32bit") << QString::fromLatin1("foo-bar-foo") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n#define __Something\n") << QStringList();

  QTest::newRow("Linux 1 (32bit intel)") << QString::fromLatin1("i686-linux-gnu") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-linux-generic-elf-32bit"});
  QTest::newRow("Linux 2 (32bit intel)") << QString::fromLatin1("i486-linux-gnu") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-linux-generic-elf-32bit"});
  QTest::newRow("Linux 3 (64bit intel)") << QString::fromLatin1("x86_64-linux-gnu") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList("x86-linux-generic-elf-64bit");
  QTest::newRow("Linux 3 (64bit intel -- non 64bit)") << QString::fromLatin1("x86_64-linux-gnu") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-linux-generic-elf-32bit"});
  QTest::newRow("Linux 4 (32bit mips)") << QString::fromLatin1("mipsel-linux-uclibc") << QByteArray("#define __SIZEOF_SIZE_T__ 4") << QStringList({"mips-linux-generic-elf-32bit"});
  QTest::newRow("Linux 5 (QTCREATORBUG-4690)") // from QTCREATORBUG-4690
    << QString::fromLatin1("x86_64-redhat-linux6E") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList("x86-linux-generic-elf-64bit");
  QTest::newRow("Linux 6 (QTCREATORBUG-4690)") // from QTCREATORBUG-4690
    << QString::fromLatin1("x86_64-redhat-linux") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList("x86-linux-generic-elf-64bit");
  QTest::newRow("Linux 7 (arm)") << QString::fromLatin1("armv5tl-montavista-linux-gnueabi") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"arm-linux-generic-elf-32bit"});
  QTest::newRow("Linux 8 (arm)") << QString::fromLatin1("arm-angstrom-linux-gnueabi") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"arm-linux-generic-elf-32bit"});
  QTest::newRow("Linux 9 (ppc)") << QString::fromLatin1("powerpc-nsg-linux") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"ppc-linux-generic-elf-32bit"});
  QTest::newRow("Linux 10 (ppc 64bit)") << QString::fromLatin1("powerpc64-suse-linux") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList({"ppc-linux-generic-elf-64bit"});
  QTest::newRow("Linux 11 (64bit mips)") << QString::fromLatin1("mips64el-linux-uclibc") << QByteArray("#define __SIZEOF_SIZE_T__ 8") << QStringList({"mips-linux-generic-elf-64bit"});

  QTest::newRow("Mingw 1 (32bit)") << QString::fromLatin1("i686-w64-mingw32") << QByteArray("#define __SIZEOF_SIZE_T__ 4\r\n") << QStringList({"x86-windows-msys-pe-32bit"});
  QTest::newRow("Mingw 2 (64bit)") << QString::fromLatin1("i686-w64-mingw32") << QByteArray("#define __SIZEOF_SIZE_T__ 8\r\n") << QStringList({"x86-windows-msys-pe-64bit"});
  QTest::newRow("Mingw 3 (32 bit)") << QString::fromLatin1("mingw32") << QByteArray("#define __SIZEOF_SIZE_T__ 4\r\n") << QStringList({"x86-windows-msys-pe-32bit"});
  QTest::newRow("Cross Mingw 1 (64bit)") << QString::fromLatin1("amd64-mingw32msvc") << QByteArray("#define __SIZEOF_SIZE_T__ 8\r\n") << QStringList({"x86-windows-msys-pe-64bit"});
  QTest::newRow("Cross Mingw 2 (32bit)") << QString::fromLatin1("i586-mingw32msvc") << QByteArray("#define __SIZEOF_SIZE_T__ 4\r\n") << QStringList({"x86-windows-msys-pe-32bit"});
  QTest::newRow("Clang 1: windows") << QString::fromLatin1("x86_64-pc-win32") << QByteArray("#define __SIZEOF_SIZE_T__ 8\r\n") << QStringList("x86-windows-msys-pe-64bit");
  QTest::newRow("Clang 1: linux") << QString::fromLatin1("x86_64-unknown-linux-gnu") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList("x86-linux-generic-elf-64bit");
  QTest::newRow("Mac 1") << QString::fromLatin1("i686-apple-darwin10") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList({"x86-darwin-generic-mach_o-64bit", "x86-darwin-generic-mach_o-32bit"});
  QTest::newRow("Mac 2") << QString::fromLatin1("powerpc-apple-darwin10") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList({"ppc-darwin-generic-mach_o-64bit", "ppc-darwin-generic-mach_o-32bit"});
  QTest::newRow("Mac 3") << QString::fromLatin1("i686-apple-darwin9") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-darwin-generic-mach_o-32bit", "x86-darwin-generic-mach_o-64bit"});
  QTest::newRow("Mac IOS") << QString::fromLatin1("arm-apple-darwin9") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"arm-darwin-generic-mach_o-32bit", "arm-darwin-generic-mach_o-64bit"});
  QTest::newRow("Intel 1") << QString::fromLatin1("86_64 x86_64 GNU/Linux") << QByteArray("#define __SIZEOF_SIZE_T__ 8\n") << QStringList("x86-linux-generic-elf-64bit");
  QTest::newRow("FreeBSD 1") << QString::fromLatin1("i386-portbld-freebsd9.0") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-bsd-freebsd-elf-32bit"});
  QTest::newRow("FreeBSD 2") << QString::fromLatin1("i386-undermydesk-freebsd") << QByteArray("#define __SIZEOF_SIZE_T__ 4\n") << QStringList({"x86-bsd-freebsd-elf-32bit"});
}

void ProjectExplorerPlugin::testGccAbiGuessing()
{
  QFETCH(QString, input);
  QFETCH(QByteArray, macros);
  QFETCH(QStringList, abiList);

  const Abis al = guessGccAbi(input, ProjectExplorer::Macro::toMacros(macros));
  QCOMPARE(al.count(), abiList.count());
  for (int i = 0; i < al.count(); ++i)
    QCOMPARE(al.at(i).toString(), abiList.at(i));
}

} // namespace ProjectExplorer

#endif

#include <gcctoolchain.moc>
