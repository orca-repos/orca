// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "../tools/orcacrashhandler/crashhandlersetup.hpp"

#include <app/app_version.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/optional.hpp>
#include <utils/qtcsettings.hpp>
#include <utils/singleton.hpp>
#include <utils/temporarydirectory.hpp>
#include <utils/terminalcommand.hpp>

#include <extensionsystem/iplugin.hpp>
#include <extensionsystem/pluginerroroverview.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <extensionsystem/pluginspec.hpp>

#include <qtsingleapplication.hpp>

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QScopeGuard>
#include <QStyle>
#include <QTextStream>
#include <QThreadPool>
#include <QTranslator>
#include <QVariant>
#include <QNetworkProxyFactory>
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextCodec>

#include <vector>

#ifdef ENABLE_QT_BREAKPAD
#include <qtsystemexceptionhandler.hpp>
#endif

#ifdef ENABLE_CRASHPAD
#define NOMINMAX
#include "client/crashpad_client.hpp"
#include "client/crash_report_database.hpp"
#include "client/settings.hpp"
#endif

#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

using namespace ExtensionSystem;

enum class indent {
  option = 4,
  description = 34
};

constexpr char core_plugin_name_c[] = "Core";
constexpr char fixed_options_c[] =
" [OPTION]... [FILE]...\n"
"Options:\n"
"    -help                         Display this help\n"
"    -version                      Display program version\n"
"    -client                       Attempt to connect to already running first instance\n"
"    -settingspath <path>          Override the default path where user settings are stored\n"
"    -installsettingspath <path>   Override the default path from where user-independent settings are read\n"
"    -temporarycleansettings, -tcs Use clean settings for debug or testing reasons\n"
"    -pid <pid>                    Attempt to connect to instance given by pid\n"
"    -block                        Block until editor is closed\n"
"    -pluginpath <path>            Add a custom search path for plugins\n";
constexpr char help_option1[] = "-h";
constexpr char help_option2[] = "-help";
constexpr char help_option3[] = "/h";
constexpr char help_option4[] = "--help";
constexpr char version_option[] = "-version";
constexpr char client_option[] = "-client";
constexpr char settings_option[] = "-settingspath";
constexpr char install_settings_option[] = "-installsettingspath";
constexpr char test_option[] = "-test";
constexpr char temporary_clean_settings1[] = "-temporarycleansettings";
constexpr char temporary_clean_settings2[] = "-tcs";
constexpr char pid_option[] = "-pid";
constexpr char block_option[] = "-block";
constexpr char pluginpath_option[] = "-pluginpath";
constexpr char user_library_path_option[] = "-user-library-path"; // hidden option for orca.sh

using plugin_spec_set = QVector<PluginSpec *>;

static auto toHtml(const QString &t) -> QString
{
  auto res = t;
  res.replace(QLatin1Char('&'), QLatin1String("&amp;"));
  res.replace(QLatin1Char('<'), QLatin1String("&lt;"));
  res.replace(QLatin1Char('>'), QLatin1String("&gt;"));
  res.insert(0, QLatin1String("<html><pre>"));
  res.append(QLatin1String("</pre></html>"));
  return res;
}

static auto displayHelpText(const QString &t) -> void
{
  if (Utils::HostOsInfo::isWindowsHost() && qApp)
    QMessageBox::information(nullptr, QLatin1String(Core::Constants::IDE_DISPLAY_NAME), toHtml(t));
  else
    qWarning("%s", qPrintable(t));
}

static auto displayError(const QString &t) -> void
{
  if (Utils::HostOsInfo::isWindowsHost() && qApp)
    QMessageBox::critical(nullptr, QLatin1String(Core::Constants::IDE_DISPLAY_NAME), t);
  else
    qCritical("%s", qPrintable(t));
}

static auto printVersion(const PluginSpec *coreplugin) -> void
{
  QString version;
  QTextStream str(&version);
  str << '\n' << Core::Constants::IDE_DISPLAY_NAME << ' ' << coreplugin->version() << " based on Qt " << qVersion() << "\n\n";
  PluginManager::formatPluginVersions(str);
  str << '\n' << coreplugin->copyright() << '\n';
  displayHelpText(version);
}

static auto printHelp(const QString &a0) -> void
{
  QString help;
  QTextStream str(&help);
  str << "Usage: " << a0 << fixed_options_c;
  PluginManager::formatOptions(str, static_cast<int>(indent::option), static_cast<int>(indent::description));
  PluginManager::formatPluginOptions(str, static_cast<int>(indent::option), static_cast<int>(indent::description));
  displayHelpText(help);
}

auto applicationDirPath(char *arg = nullptr) -> QString
{
  static QString dir;

  if (arg)
    dir = QFileInfo(QString::fromLocal8Bit(arg)).dir().absolutePath();

  if (QCoreApplication::instance())
    return QApplication::applicationDirPath();

  return dir;
}

static auto resourcePath() -> QString
{
  return QDir::cleanPath(applicationDirPath() + '/' + RELATIVE_DATA_PATH);
}

static auto msgCoreLoadFailure(const QString &why) -> QString
{
  return QCoreApplication::translate("Application", "Failed to load core: %1").arg(why);
}

static auto askMsgSendFailed() -> int
{
  return QMessageBox::question(nullptr, QApplication::translate("Application", "Could not send message"), QCoreApplication::translate("Application", "Unable to send command line arguments " "to the already running instance. It does not appear to " "be responding. Do you want to start a new instance of " "%1?").arg(Core::Constants::IDE_DISPLAY_NAME), QMessageBox::Yes | QMessageBox::No | QMessageBox::Retry, QMessageBox::Retry);
}

static auto getPluginPaths() -> QStringList
{
  QStringList rc(QDir::cleanPath(QApplication::applicationDirPath() + '/' + RELATIVE_PLUGIN_PATH));

  // Local plugin path: <localappdata>/plugins/<ideversion>
  //    where <localappdata> is e.g.
  //    "%LOCALAPPDATA%\QtProject\orca" on Windows Vista and later
  //    "$XDG_DATA_HOME/data/QtProject/orca" or "~/.local/share/data/QtProject/orca" on Linux
  //    "~/Library/Application Support/OrcaProject/Orca" on Mac
  auto plugin_path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

  if constexpr (Utils::HostOsInfo::isAnyUnixHost() && !Utils::HostOsInfo::isMacHost())
    plugin_path += QLatin1String("/data");

  plugin_path += QLatin1Char('/') + QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR) + QLatin1Char('/');
  plugin_path += QLatin1String(Utils::HostOsInfo::isMacHost() ? Core::Constants::IDE_DISPLAY_NAME : Core::Constants::IDE_ID);
  plugin_path += QLatin1String("/plugins/");

  // Orca X.Y.Z can load plugins from X.Y.(Z-1) etc, so add current and previous
  // patch versions
  const QString minor_version = QString::number(IDE_VERSION_MAJOR) + '.' + QString::number(IDE_VERSION_MINOR) + '.';
  const auto min_patch_version = qMin(IDE_VERSION_RELEASE, QVersionNumber::fromString(Core::Constants::IDE_VERSION_COMPAT).microVersion());

  for (auto patchVersion = IDE_VERSION_RELEASE; patchVersion >= min_patch_version; --patchVersion)
    rc.push_back(plugin_path + minor_version + QString::number(patchVersion));

  return rc;
}

static auto setupInstallSettings(QString &install_settingspath) -> void
{
  if (!install_settingspath.isEmpty() && !QFileInfo(install_settingspath).isDir()) {
    displayError(QString("-installsettingspath \"%0\" needs to be the path where a %1/%2.ini exist.").arg(install_settingspath, QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR), QLatin1String(Core::Constants::IDE_CASED_ID)));
    install_settingspath.clear();
  }

  // Check if the default install settings contain a setting for the actual install settings.
  // This can be an absolute path, or a path relative to applicationDirPath().
  // The result is interpreted like -settingspath, but for SystemScope
  static constexpr char k_install_settings_key[] = "Settings/InstallSettings";
  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, install_settingspath.isEmpty() ? resourcePath() : install_settingspath);

  if (const QSettings install_settings(QSettings::IniFormat, QSettings::UserScope, QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR), QLatin1String(Core::Constants::IDE_CASED_ID)); install_settings.contains(k_install_settings_key)) {
    auto install_settings_path = install_settings.value(k_install_settings_key).toString();
    if (QDir::isRelativePath(install_settings_path))
      install_settings_path = applicationDirPath() + '/' + install_settings_path;
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, install_settings_path);
  }
}

static auto createUserSettings() -> Utils::QtcSettings*
{
  return new Utils::QtcSettings(QSettings::IniFormat, QSettings::UserScope, QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR), QLatin1String(Core::Constants::IDE_CASED_ID));
}

static auto setHighDpiEnvironmentVariable() -> void
{
  if constexpr (Utils::HostOsInfo::isMacHost())
    return;

  const std::unique_ptr<QSettings> settings(createUserSettings());

  constexpr auto default_value = Utils::HostOsInfo::isWindowsHost();
  const auto enable_high_dpi_scaling = settings->value("Core/EnableHighDpiScaling", default_value).toBool();

  if (static constexpr char env_var_qt_device_pixel_ratio[] = "QT_DEVICE_PIXEL_RATIO"; enable_high_dpi_scaling && !qEnvironmentVariableIsSet(env_var_qt_device_pixel_ratio) // legacy in 5.6, but still functional
    && !qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR") && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR") && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #if QT_VERSION == QT_VERSION_CHECK(5, 14, 0)
        // work around QTBUG-80934
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::Round);
    #endif

    #endif
  } else {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /* AA_DisableHighDpiScaling is deprecated */
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
    #endif
  }
}

auto loadFonts() -> void
{
  const QDir dir(resourcePath() + "/fonts/");

  for (const auto fonts = dir.entryInfoList(QStringList("*.ttf"), QDir::Files); const auto &file_info : fonts)
    QFontDatabase::addApplicationFont(file_info.absoluteFilePath());
}

struct Options {
  QString settings_path;
  QString install_settings_path;
  QStringList custom_plugin_paths;
  // list of arguments that were handled and not passed to the application or plugin manager
  QStringList pre_app_arguments;
  // list of arguments to be passed to the application or plugin manager
  std::vector<char*> app_arguments;
  Utils::optional<QString> user_library_path;
  bool has_test_option = false;
  bool wants_clean_settings = false;
};

auto parseCommandLine(const int argc, char *argv[]) -> Options
{
  Options options;
  auto it = argv;
  const auto end = argv + argc;

  while (it != end) {
    const auto arg = QString::fromLocal8Bit(*it);
    const auto has_next = it + 1 != end;
    const auto next_arg = has_next ? QString::fromLocal8Bit(*(it + 1)) : QString();

    if (arg == settings_option && has_next) {
      ++it;
      options.settings_path = QDir::fromNativeSeparators(next_arg);
      options.pre_app_arguments << arg << next_arg;
    } else if (arg == install_settings_option && has_next) {
      ++it;
      options.install_settings_path = QDir::fromNativeSeparators(next_arg);
      options.pre_app_arguments << arg << next_arg;
    } else if (arg == pluginpath_option && has_next) {
      ++it;
      options.custom_plugin_paths += QDir::fromNativeSeparators(next_arg);
      options.pre_app_arguments << arg << next_arg;
    } else if (arg == user_library_path_option && has_next) {
      ++it;
      options.user_library_path = next_arg;
      options.pre_app_arguments << arg << next_arg;
    } else if (arg == temporary_clean_settings1 || arg == temporary_clean_settings2) {
      options.wants_clean_settings = true;
      options.pre_app_arguments << arg;
    } else {
      // arguments that are still passed on to the application
      if (arg == test_option)
        options.has_test_option = true;
      options.app_arguments.push_back(*it);
    }
    ++it;
  }

  return options;
}

class Restarter {
public:
  Restarter(const int argc, char *argv[])
  {
    Q_UNUSED(argc)
    m_executable = QString::fromLocal8Bit(argv[0]);
    m_working_path = QDir::currentPath();
  }

  auto setArguments(const QStringList &args) -> void { m_args = args; }
  auto executable() const -> QString { return m_executable; }
  auto arguments() const -> QStringList { return m_args; }
  auto workingPath() const -> QString { return m_working_path; }

  auto restartOrExit(const int exit_code) const -> int
  {
    return qApp->property("restart").toBool() ? restart(exit_code) : exit_code;
  }

  auto restart(const int exit_code) const -> int
  {
    QProcess::startDetached(m_executable, m_args, m_working_path);
    return exit_code;
  }

private:
  QString m_executable;
  QStringList m_args;
  QString m_working_path;
};

auto lastSessionArgument() -> QStringList
{
  // using insider information here is not particularly beautiful, anyhow
  const auto has_project_explorer = Utils::anyOf(PluginManager::plugins(), Utils::equal(&PluginSpec::name, QString("ProjectExplorer")));
  return has_project_explorer ? QStringList({"-lastsession"}) : QStringList();
}

#ifdef ENABLE_CRASHPAD
auto startCrashpad(const QString &libexec_path, bool crash_reporting_enabled) -> bool
{
  using namespace crashpad;

  // Cache directory that will store crashpad information and minidumps
  const auto database_path = QDir::cleanPath(libexec_path + "/crashpad_reports");
  auto handler_path = QDir::cleanPath(libexec_path + "/crashpad_handler");
  #ifdef Q_OS_WIN
  handler_path += ".exe";
  base::FilePath database(database_path.toStdWString());
  base::FilePath handler(handler_path.toStdWString());
  #elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    base::FilePath database(databasePath.toStdString());
    base::FilePath handler(handlerPath.toStdString());
  #endif

  std::unique_ptr<CrashReportDatabase> db = CrashReportDatabase::Initialize(database);
  if (db && db->GetSettings())
    db->GetSettings()->SetUploadsEnabled(crash_reporting_enabled);

  // URL used to submit minidumps to
  std::string url(CRASHPAD_BACKEND_URL);

  // Optional annotations passed via --annotations to the handler
  std::map<std::string, std::string> annotations;
  annotations["app-version"] = Core::Constants::IDE_VERSION_DISPLAY;
  annotations["qt-version"] = QT_VERSION_STR;

  // Optional arguments to pass to the handler
  std::vector<std::string> arguments;
  arguments.emplace_back("--no-rate-limit");

  CrashpadClient *client = new CrashpadClient();
  const bool success = client->StartHandler(handler, database, database, url, annotations, arguments, true, true);

  return success;
}
#endif

auto main(int argc, char **argv) -> int
{
  Restarter restarter(argc, argv);
  Utils::Environment::systemEnvironment(); // cache system environment before we do any changes

  // Manually determine various command line options
  // We can't use the regular way of the plugin manager,
  // because settings can change the way plugin manager behaves
  auto options = parseCommandLine(argc, argv);
  applicationDirPath(argv[0]);

  if (qEnvironmentVariableIsSet("QTC_DO_NOT_PROPAGATE_LD_PRELOAD")) {
    Utils::Environment::modifySystemEnvironment({{"LD_PRELOAD", "", Utils::EnvironmentItem::Unset}});
  }

  if (options.user_library_path) {
    if ((*options.user_library_path).isEmpty()) {
      Utils::Environment::modifySystemEnvironment({{"LD_LIBRARY_PATH", "", Utils::EnvironmentItem::Unset}});
    } else {
      Utils::Environment::modifySystemEnvironment({{"LD_LIBRARY_PATH", *options.user_library_path, Utils::EnvironmentItem::SetEnabled}});
    }
  }

  #if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!qEnvironmentVariableIsSet("QT_OPENGL"))
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
  #else
  qputenv("QSG_RHI_BACKEND", "opengl");
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
  #endif

  if (qEnvironmentVariableIsSet("ORCA_DISABLE_NATIVE_MENUBAR") || qgetenv("XDG_CURRENT_DESKTOP").startsWith("Unity")) {
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
  }

  if (Utils::HostOsInfo::isRunningUnderRosetta()) {
    // work around QTBUG-97085: QRegularExpression jitting is not reentrant under Rosetta
    qputenv("QT_ENABLE_REGEXP_JIT", "0");
  }

  #if defined(QTC_FORCE_XCB)
  if constexpr (Utils::HostOsInfo::isLinuxHost() && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
    // Enforce XCB on Linux/Gnome, if the user didn't override via QT_QPA_PLATFORM
    // This was previously done in Qt, but removed in Qt 6.3. We found that bad things can still happen,
    // like the Wayland session simply crashing when starting Orca.
    // TODO: Reconsider when Qt/Wayland is reliably working on the supported distributions
    const auto has_wayland_display = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
    const auto is_wayland_session_type = qgetenv("XDG_SESSION_TYPE") == "wayland";
    const auto current_desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    const auto session_desktop = qgetenv("XDG_SESSION_DESKTOP").toLower();
    const auto is_gnome = current_desktop.contains("gnome") || session_desktop.contains("gnome");
    if (const auto is_wayland = has_wayland_display || is_wayland_session_type; is_gnome && is_wayland) {
      qInfo() << "Warning: Ignoring WAYLAND_DISPLAY on Gnome." << "Use QT_QPA_PLATFORM=wayland to run on Wayland anyway.";
      qputenv("QT_QPA_PLATFORM", "xcb");
    }
  }
  #endif

  Utils::TemporaryDirectory::setMasterTemporaryDirectory(QDir::tempPath() + "/" + Core::Constants::IDE_CASED_ID + "-XXXXXX");

  #ifdef Q_OS_MACOS
    // increase the number of file that can be opened in Orca.
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);

    rl.rlim_cur = qMin((rlim_t)OPEN_MAX, rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
  #endif

  if (options.settings_path.isEmpty() && (options.has_test_option || options.wants_clean_settings)) {
    QScopedPointer<Utils::TemporaryDirectory> temporary_clean_settings_dir;
    temporary_clean_settings_dir.reset(new Utils::TemporaryDirectory("qtc-test-settings"));
    if (!temporary_clean_settings_dir->isValid())
      return 1;
    options.settings_path = temporary_clean_settings_dir->path().path();
  }

  if (!options.settings_path.isEmpty())
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, options.settings_path);

  // Must be done before any QSettings class is created
  QSettings::setDefaultFormat(QSettings::IniFormat);
  setupInstallSettings(options.install_settings_path);

  // plugin manager takes control of this settings object
  setHighDpiEnvironmentVariable();
  SharedTools::QtSingleApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  auto numberof_arguments = static_cast<int>(options.app_arguments.size());

  SharedTools::QtSingleApplication app((QLatin1String(Core::Constants::IDE_DISPLAY_NAME)), numberof_arguments, options.app_arguments.data());
  QCoreApplication::setApplicationName(Core::Constants::IDE_CASED_ID);
  QCoreApplication::setApplicationVersion(QLatin1String(Core::Constants::IDE_VERSION_LONG));
  QCoreApplication::setOrganizationName(QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR));
  QGuiApplication::setApplicationDisplayName(Core::Constants::IDE_DISPLAY_NAME);

  auto cleanup = qScopeGuard([] { Utils::Singleton::deleteAll(); });
  const auto plugin_arguments = SharedTools::QtSingleApplication::arguments();

  /*Initialize global settings and resetup install settings with QApplication::applicationDirPath */
  setupInstallSettings(options.install_settings_path);

  auto settings = createUserSettings();
  auto global_settings = new Utils::QtcSettings(QSettings::IniFormat, QSettings::SystemScope, QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR), QLatin1String(Core::Constants::IDE_CASED_ID));

  Utils::TerminalCommand::setSettings(settings);
  loadFonts();

  if (Utils::HostOsInfo::isWindowsHost() && !qFuzzyCompare(qApp->devicePixelRatio(), 1.0) && QApplication::style()->objectName().startsWith(QLatin1String("windows"), Qt::CaseInsensitive)) {
    QApplication::setStyle(QLatin1String("fusion"));
  }
  const auto thread_count = QThreadPool::globalInstance()->maxThreadCount();
  QThreadPool::globalInstance()->setMaxThreadCount(qMax(4, 2 * thread_count));

  const QString libexec_path = QCoreApplication::applicationDirPath() + '/' + RELATIVE_LIBEXEC_PATH;

  #ifdef ENABLE_QT_BREAKPAD
    QtSystemExceptionHandler systemExceptionHandler(libexecPath);
  #else

  // Display a backtrace once a serious signal is delivered (Linux only).
  CrashHandlerSetup setup_crash_handler(Core::Constants::IDE_DISPLAY_NAME, CrashHandlerSetup::EnableRestart, libexec_path);
  #endif

  #ifdef ENABLE_CRASHPAD
    bool crashReportingEnabled = settings->value("CrashReportingEnabled", false).toBool();
    startCrashpad(libexecPath, crashReportingEnabled);
  #endif

  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
    app.setAttribute(Qt::AA_DisableWindowContextHelpButton);
  #endif

  PluginManager plugin_manager;
  PluginManager::setPluginIID(QLatin1String("org.orca-repos.orca.plugin"));
  PluginManager::setGlobalSettings(global_settings);
  PluginManager::setSettings(settings);

  QTranslator translator;
  QTranslator qt_translator;

  auto ui_languages = QLocale::system().uiLanguages();

  if (auto override_language = settings->value(QLatin1String("General/OverrideLanguage")).toString(); !override_language.isEmpty())
    ui_languages.prepend(override_language);

  const QString &creator_tr_path = resourcePath() + "/translations";

  for (auto locale : qAsConst(ui_languages)) {
    locale = QLocale(locale).name();
    if (translator.load("orca_" + locale, creator_tr_path)) {
      const auto &qt_tr_path = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
      // Binary installer puts Qt tr files into creatorTrPath
      if (const QString &qt_tr_file = QLatin1String("qt_") + locale; qt_translator.load(qt_tr_file, qt_tr_path) || qt_translator.load(qt_tr_file, creator_tr_path)) {
        SharedTools::QtSingleApplication::installTranslator(&translator);
        SharedTools::QtSingleApplication::installTranslator(&qt_translator);
        app.setProperty("qtc_locale", locale);
        break;
      }
      Q_UNUSED(translator.load(QString())); // unload()
    } else if (locale == QLatin1String("C") /* overrideLanguage == "English" */) {
      // use built-in
      break;
    } else if (locale.startsWith(QLatin1String("en")) /* "English" is built-in */) {
      // use built-in
      break;
    }
  }

  if (auto override_codec_for_locale = settings->value("General/OverrideCodecForLocale").toByteArray(); !override_codec_for_locale.isEmpty())
    QTextCodec::setCodecForLocale(QTextCodec::codecForName(override_codec_for_locale));

  SharedTools::QtSingleApplication::setDesktopFileName("org.qt-project.orca.desktop");

  // Make sure we honor the system's proxy settings
  QNetworkProxyFactory::setUseSystemConfiguration(true);

  // Load
  const auto plugin_paths = getPluginPaths() + options.custom_plugin_paths;

  PluginManager::setPluginPaths(plugin_paths);
  QMap<QString, QString> found_app_options;

  if (plugin_arguments.size() > 1) {
    QMap<QString, bool> app_options;
    app_options.insert(QLatin1String(help_option1), false);
    app_options.insert(QLatin1String(help_option2), false);
    app_options.insert(QLatin1String(help_option3), false);
    app_options.insert(QLatin1String(help_option4), false);
    app_options.insert(QLatin1String(version_option), false);
    app_options.insert(QLatin1String(client_option), false);
    app_options.insert(QLatin1String(pid_option), true);
    app_options.insert(QLatin1String(block_option), false);
    QString error_message;
    if (!PluginManager::parseOptions(plugin_arguments, app_options, &found_app_options, &error_message)) {
      displayError(error_message);
      printHelp(QFileInfo(SharedTools::QtSingleApplication::applicationFilePath()).baseName());
      return -1;
    }
  }

  restarter.setArguments(options.pre_app_arguments + PluginManager::argumentsForRestart() + lastSessionArgument());

  // if settingspath is not provided we need to pass on the settings in use
  const auto settingspath = options.pre_app_arguments.contains(QLatin1String(settings_option)) ? QString() : options.settings_path;
  const PluginManager::ProcessData process_data = {restarter.executable(), options.pre_app_arguments + PluginManager::argumentsForRestart(), restarter.workingPath(), settingspath};
  PluginManager::setCreatorProcessData(process_data);
  const auto plugins = PluginManager::plugins();
  PluginSpec *coreplugin = nullptr;

  for (auto spec : plugins) {
    if (spec->name() == QLatin1String(core_plugin_name_c)) {
      coreplugin = spec;
      break;
    }
  }

  if (!coreplugin) {
    auto native_paths = QDir::toNativeSeparators(plugin_paths.join(QLatin1Char(',')));
    const auto reason = QCoreApplication::translate("Application", "Could not find Core plugin in %1").arg(native_paths);
    displayError(msgCoreLoadFailure(reason));
    return 1;
  }

  if (!coreplugin->isEffectivelyEnabled()) {
    const auto reason = QCoreApplication::translate("Application", "Core plugin is disabled.");
    displayError(msgCoreLoadFailure(reason));
    return 1;
  }

  if (coreplugin->hasError()) {
    displayError(msgCoreLoadFailure(coreplugin->errorString()));
    return 1;
  }

  if (found_app_options.contains(QLatin1String(version_option))) {
    printVersion(coreplugin);
    return 0;
  }

  if (found_app_options.contains(QLatin1String(help_option1)) || found_app_options.contains(QLatin1String(help_option2)) || found_app_options.contains(QLatin1String(help_option3)) || found_app_options.contains(QLatin1String(help_option4))) {
    printHelp(QFileInfo(SharedTools::QtSingleApplication::applicationFilePath()).baseName());
    return 0;
  }

  qint64 pid = -1;

  if (found_app_options.contains(QLatin1String(pid_option))) {
    auto pid_string = found_app_options.value(QLatin1String(pid_option));
    bool pid_ok;
    qint64 tmp_pid = pid_string.toInt(&pid_ok);
    if (pid_ok)
      pid = tmp_pid;
  }

  if (auto is_block = found_app_options.contains(QLatin1String(block_option)); app.isRunning() && (pid != -1 || is_block || found_app_options.contains(QLatin1String(client_option)))) {
    app.setBlock(is_block);
    if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
      return 0;

    // Message could not be send, maybe it was in the process of quitting
    if (app.isRunning(pid)) {
      // Nah app is still running, ask the user
      auto button = askMsgSendFailed();
      while (button == QMessageBox::Retry) {
        if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
          return 0;
        if (!app.isRunning(pid)) // App quit while we were trying so start a new creator
          button = QMessageBox::Yes;
        else
          button = askMsgSendFailed();
      }
      if (button == QMessageBox::No)
        return -1;
    }
  }

  PluginManager::checkForProblematicPlugins();
  PluginManager::loadPlugins();

  if (coreplugin->hasError()) {
    displayError(msgCoreLoadFailure(coreplugin->errorString()));
    return 1;
  }

  // Set up remote arguments.
  QObject::connect(&app, &SharedTools::QtSingleApplication::messageReceived, &plugin_manager, &PluginManager::remoteArguments);
  QObject::connect(&app, SIGNAL(fileOpenRequest(QString)), coreplugin->plugin(), SLOT(fileOpenRequest(QString)));

  // shutdown plugin manager on the exit
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &plugin_manager, &PluginManager::shutdown);

  #ifdef Q_OS_LINUX
  class MemoryTrimmer : public QObject {
  public:
    MemoryTrimmer()
    {
      m_trimTimer.setSingleShot(true);
      m_trimTimer.setInterval(60000);
      // glibc may not actually free memory in free().
      connect(&m_trimTimer, &QTimer::timeout, this, [] { malloc_trim(0); });
    }

    auto eventFilter(QObject *, QEvent *e) -> bool override
    {
      if ((e->type() == QEvent::MouseButtonPress || e->type() == QEvent::KeyPress) && !m_trimTimer.isActive()) {
        m_trimTimer.start();
      }
      return false;
    }

    QTimer m_trimTimer;
  };
  MemoryTrimmer trimmer;
  app.installEventFilter(&trimmer);
  #endif

  return restarter.restartOrExit(SharedTools::QtSingleApplication::exec());
}
