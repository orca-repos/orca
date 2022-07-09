// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmaketool.hpp"

#include "cmaketoolmanager.hpp"

#include <core/helpmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <memory>

using namespace Utils;

namespace CMakeProjectManager {

constexpr char CMAKE_INFORMATION_ID[] = "Id";
constexpr char CMAKE_INFORMATION_COMMAND[] = "Binary";
constexpr char CMAKE_INFORMATION_DISPLAYNAME[] = "DisplayName";
constexpr char CMAKE_INFORMATION_AUTORUN[] = "AutoRun";
constexpr char CMAKE_INFORMATION_QCH_FILE_PATH[] = "QchFile";
// obsolete since Qt Creator 5. Kept for backward compatibility
constexpr char CMAKE_INFORMATION_AUTO_CREATE_BUILD_DIRECTORY[] = "AutoCreateBuildDirectory";
constexpr char CMAKE_INFORMATION_AUTODETECTED[] = "AutoDetected";
constexpr char CMAKE_INFORMATION_DETECTIONSOURCE[] = "DetectionSource";
constexpr char CMAKE_INFORMATION_READERTYPE[] = "ReaderType";

auto CMakeTool::Generator::matches(const QString &n, const QString &ex) const -> bool
{
  return n == name && (ex.isEmpty() || extraGenerators.contains(ex));
}

namespace Internal {

constexpr char READER_TYPE_FILEAPI[] = "fileapi";

static auto readerTypeFromString(const QString &input) -> Utils::optional<CMakeTool::ReaderType>
{
  // Do not try to be clever here, just use whatever is in the string!
  if (input == READER_TYPE_FILEAPI)
    return CMakeTool::FileApi;
  return {};
}

static auto readerTypeToString(const CMakeTool::ReaderType &type) -> QString
{
  switch (type) {
  case CMakeTool::FileApi:
    return QString(READER_TYPE_FILEAPI);
  default:
    return QString();
  }
}

// --------------------------------------------------------------------
// CMakeIntrospectionData:
// --------------------------------------------------------------------

class FileApi {
public:
  QString kind;
  std::pair<int, int> version;
};

class IntrospectionData {
public:
  bool m_didAttemptToRun = false;
  bool m_didRun = true;

  QList<CMakeTool::Generator> m_generators;
  QMap<QString, QStringList> m_functionArgs;
  QVector<FileApi> m_fileApis;
  QStringList m_variables;
  QStringList m_functions;
  CMakeTool::Version m_version;
};

} // namespace Internal

///////////////////////////
// CMakeTool
///////////////////////////
CMakeTool::CMakeTool(Detection d, const Id &id) : m_id(id), m_isAutoDetected(d == AutoDetection), m_introspection(std::make_unique<Internal::IntrospectionData>())
{
  QTC_ASSERT(m_id.isValid(), m_id = Id::fromString(QUuid::createUuid().toString()));
}

CMakeTool::CMakeTool(const QVariantMap &map, bool fromSdk) : CMakeTool(fromSdk ? CMakeTool::AutoDetection : CMakeTool::ManualDetection, Id::fromSetting(map.value(CMAKE_INFORMATION_ID)))
{
  m_displayName = map.value(CMAKE_INFORMATION_DISPLAYNAME).toString();
  m_isAutoRun = map.value(CMAKE_INFORMATION_AUTORUN, true).toBool();
  m_autoCreateBuildDirectory = map.value(CMAKE_INFORMATION_AUTO_CREATE_BUILD_DIRECTORY, false).toBool();
  m_readerType = Internal::readerTypeFromString(map.value(CMAKE_INFORMATION_READERTYPE).toString());

  //loading a CMakeTool from SDK is always autodetection
  if (!fromSdk)
    m_isAutoDetected = map.value(CMAKE_INFORMATION_AUTODETECTED, false).toBool();
  m_detectionSource = map.value(CMAKE_INFORMATION_DETECTIONSOURCE).toString();

  setFilePath(FilePath::fromString(map.value(CMAKE_INFORMATION_COMMAND).toString()));

  m_qchFilePath = FilePath::fromVariant(map.value(CMAKE_INFORMATION_QCH_FILE_PATH));

  if (m_qchFilePath.isEmpty())
    m_qchFilePath = searchQchFile(m_executable);
}

CMakeTool::~CMakeTool() = default;

auto CMakeTool::createId() -> Id
{
  return Id::fromString(QUuid::createUuid().toString());
}

auto CMakeTool::setFilePath(const FilePath &executable) -> void
{
  if (m_executable == executable)
    return;

  m_introspection = std::make_unique<Internal::IntrospectionData>();

  m_executable = executable;
  CMakeToolManager::notifyAboutUpdate(this);
}

auto CMakeTool::filePath() const -> FilePath
{
  return m_executable;
}

auto CMakeTool::setAutorun(bool autoRun) -> void
{
  if (m_isAutoRun == autoRun)
    return;

  m_isAutoRun = autoRun;
  CMakeToolManager::notifyAboutUpdate(this);
}

auto CMakeTool::isValid() const -> bool
{
  if (!m_id.isValid() || !m_introspection)
    return false;

  if (!m_introspection->m_didAttemptToRun)
    readInformation();

  return m_introspection->m_didRun && !m_introspection->m_fileApis.isEmpty();
}

auto CMakeTool::runCMake(QtcProcess &cmake, const QStringList &args, int timeoutS) const -> void
{
  cmake.setTimeoutS(timeoutS);
  cmake.setDisableUnixTerminal();
  auto env = Environment::systemEnvironment();
  env.setupEnglishOutput();
  cmake.setEnvironment(env);
  cmake.setTimeOutMessageBoxEnabled(false);
  cmake.setCommand({cmakeExecutable(), args});
  cmake.runBlocking();
}

auto CMakeTool::toMap() const -> QVariantMap
{
  QVariantMap data;
  data.insert(CMAKE_INFORMATION_DISPLAYNAME, m_displayName);
  data.insert(CMAKE_INFORMATION_ID, m_id.toSetting());
  data.insert(CMAKE_INFORMATION_COMMAND, m_executable.toString());
  data.insert(CMAKE_INFORMATION_QCH_FILE_PATH, m_qchFilePath.toString());
  data.insert(CMAKE_INFORMATION_AUTORUN, m_isAutoRun);
  data.insert(CMAKE_INFORMATION_AUTO_CREATE_BUILD_DIRECTORY, m_autoCreateBuildDirectory);
  if (m_readerType)
    data.insert(CMAKE_INFORMATION_READERTYPE, Internal::readerTypeToString(m_readerType.value()));
  data.insert(CMAKE_INFORMATION_AUTODETECTED, m_isAutoDetected);
  data.insert(CMAKE_INFORMATION_DETECTIONSOURCE, m_detectionSource);
  return data;
}

auto CMakeTool::cmakeExecutable() const -> FilePath
{
  return cmakeExecutable(m_executable);
}

auto CMakeTool::setQchFilePath(const FilePath &path) -> void
{
  m_qchFilePath = path;
}

auto CMakeTool::qchFilePath() const -> FilePath
{
  return m_qchFilePath;
}

auto CMakeTool::cmakeExecutable(const FilePath &path) -> FilePath
{
  if (path.osType() == OsTypeMac) {
    const auto executableString = path.toString();
    const int appIndex = executableString.lastIndexOf(".app");
    const auto appCutIndex = appIndex + 4;
    const auto endsWithApp = appIndex >= 0 && appCutIndex >= executableString.size();
    const auto containsApp = appIndex >= 0 && !endsWithApp && executableString.at(appCutIndex) == '/';
    if (endsWithApp || containsApp) {
      const auto toTest = FilePath::fromString(executableString.left(appCutIndex)).pathAppended("Contents/bin/cmake");
      if (toTest.exists())
        return toTest.canonicalPath();
    }
  }

  const auto resolvedPath = path.canonicalPath();
  // Evil hack to make snap-packages of CMake work. See QTCREATORBUG-23376
  if (path.osType() == OsTypeLinux && resolvedPath.fileName() == "snap")
    return path;

  return resolvedPath;
}

auto CMakeTool::isAutoRun() const -> bool
{
  return m_isAutoRun;
}

auto CMakeTool::supportedGenerators() const -> QList<CMakeTool::Generator>
{
  return isValid() ? m_introspection->m_generators : QList<CMakeTool::Generator>();
}

auto CMakeTool::keywords() -> TextEditor::Keywords
{
  if (!isValid())
    return {};

  if (m_introspection->m_functions.isEmpty() && m_introspection->m_didRun) {
    QtcProcess proc;
    runCMake(proc, {"--help-command-list"}, 5);
    if (proc.result() == QtcProcess::FinishedWithSuccess)
      m_introspection->m_functions = proc.stdOut().split('\n');

    runCMake(proc, {"--help-commands"}, 5);
    if (proc.result() == QtcProcess::FinishedWithSuccess)
      parseFunctionDetailsOutput(proc.stdOut());

    runCMake(proc, {"--help-property-list"}, 5);
    if (proc.result() == QtcProcess::FinishedWithSuccess)
      m_introspection->m_variables = parseVariableOutput(proc.stdOut());

    runCMake(proc, {"--help-variable-list"}, 5);
    if (proc.result() == QtcProcess::FinishedWithSuccess) {
      m_introspection->m_variables.append(parseVariableOutput(proc.stdOut()));
      m_introspection->m_variables = Utils::filteredUnique(m_introspection->m_variables);
      Utils::sort(m_introspection->m_variables);
    }
  }

  return TextEditor::Keywords(m_introspection->m_variables, m_introspection->m_functions, m_introspection->m_functionArgs);
}

auto CMakeTool::hasFileApi() const -> bool
{
  return isValid() ? !m_introspection->m_fileApis.isEmpty() : false;
}

auto CMakeTool::version() const -> CMakeTool::Version
{
  return m_introspection ? m_introspection->m_version : CMakeTool::Version();
}

auto CMakeTool::versionDisplay() const -> QString
{
  if (!m_introspection)
    return CMakeToolManager::tr("Version not parseable");

  const auto &version = m_introspection->m_version;
  if (version.fullVersion.isEmpty())
    return QString::fromUtf8(version.fullVersion);

  return QString("%1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch);
}

auto CMakeTool::isAutoDetected() const -> bool
{
  return m_isAutoDetected;
}

auto CMakeTool::displayName() const -> QString
{
  return m_displayName;
}

auto CMakeTool::setDisplayName(const QString &displayName) -> void
{
  m_displayName = displayName;
  CMakeToolManager::notifyAboutUpdate(this);
}

auto CMakeTool::setPathMapper(const CMakeTool::PathMapper &pathMapper) -> void
{
  m_pathMapper = pathMapper;
}

auto CMakeTool::pathMapper() const -> CMakeTool::PathMapper
{
  if (m_pathMapper)
    return m_pathMapper;
  return [](const FilePath &fn) { return fn; };
}

auto CMakeTool::readerType() const -> Utils::optional<CMakeTool::ReaderType>
{
  if (m_readerType)
    return m_readerType; // Allow overriding the auto-detected value via .user files

  // Find best possible reader type:
  if (hasFileApi())
    return FileApi;
  return {};
}

auto CMakeTool::searchQchFile(const FilePath &executable) -> FilePath
{
  if (executable.isEmpty() || executable.needsDevice()) // do not register docs from devices
    return {};

  auto prefixDir = executable.parentDir().parentDir();
  QDir docDir{prefixDir.pathAppended("doc/cmake").toString()};
  if (!docDir.exists())
    docDir.setPath(prefixDir.pathAppended("share/doc/cmake").toString());
  if (!docDir.exists())
    return {};

  const auto files = docDir.entryList(QStringList("*.qch"));
  for (const auto &docFile : files) {
    if (docFile.startsWith("cmake", Qt::CaseInsensitive)) {
      return FilePath::fromString(docDir.absoluteFilePath(docFile));
    }
  }

  return {};
}

auto CMakeTool::documentationUrl(const Version &version, bool online) -> QString
{
  if (online) {
    QString helpVersion = "latest";
    if (!(version.major == 0 && version.minor == 0))
      helpVersion = QString("v%1.%2").arg(version.major).arg(version.minor);

    return QString("https://cmake.org/cmake/help/%1").arg(helpVersion);
  }

  return QString("qthelp://org.cmake.%1.%2.%3/doc").arg(version.major).arg(version.minor).arg(version.patch);
}

auto CMakeTool::openCMakeHelpUrl(const CMakeTool *tool, const QString &linkUrl) -> void
{
  auto online = true;
  Version version;
  if (tool && tool->isValid()) {
    online = tool->qchFilePath().isEmpty();
    version = tool->version();
  }

  Core::HelpManager::showHelpUrl(linkUrl.arg(documentationUrl(version, online)));
}

auto CMakeTool::readInformation() const -> void
{
  QTC_ASSERT(m_introspection, return);
  if (!m_introspection->m_didRun && m_introspection->m_didAttemptToRun)
    return;

  m_introspection->m_didAttemptToRun = true;

  fetchFromCapabilities();
}

static auto parseDefinition(const QString &definition) -> QStringList
{
  QStringList result;
  QString word;
  auto ignoreWord = false;
  QVector<QChar> braceStack;

  foreach(const QChar &c, definition) {
    if (c == '[' || c == '<' || c == '(') {
      braceStack.append(c);
      ignoreWord = false;
    } else if (c == ']' || c == '>' || c == ')') {
      if (braceStack.isEmpty() || braceStack.takeLast() == '<')
        ignoreWord = true;
    }

    if (c == ' ' || c == '[' || c == '<' || c == '(' || c == ']' || c == '>' || c == ')') {
      if (!ignoreWord && !word.isEmpty()) {
        if (result.isEmpty() || Utils::allOf(word, [](const QChar &c) { return c.isUpper() || c == '_'; }))
          result.append(word);
      }
      word.clear();
      ignoreWord = false;
    } else {
      word.append(c);
    }
  }
  return result;
}

auto CMakeTool::parseFunctionDetailsOutput(const QString &output) -> void
{
  const auto functionSet = Utils::toSet(m_introspection->m_functions);

  auto expectDefinition = false;
  QString currentDefinition;

  const auto lines = output.split('\n');
  for (auto i = 0; i < lines.count(); ++i) {
    const auto line = lines.at(i);

    if (line == "::") {
      expectDefinition = true;
      continue;
    }

    if (expectDefinition) {
      if (!line.startsWith(' ') && !line.isEmpty()) {
        expectDefinition = false;
        auto words = parseDefinition(currentDefinition);
        if (!words.isEmpty()) {
          const auto command = words.takeFirst();
          if (functionSet.contains(command)) {
            auto tmp = words + m_introspection->m_functionArgs[command];
            Utils::sort(tmp);
            m_introspection->m_functionArgs[command] = Utils::filteredUnique(tmp);
          }
        }
        if (!words.isEmpty() && functionSet.contains(words.at(0)))
          m_introspection->m_functionArgs[words.at(0)];
        currentDefinition.clear();
      } else {
        currentDefinition.append(line.trimmed() + ' ');
      }
    }
  }
}

auto CMakeTool::parseVariableOutput(const QString &output) -> QStringList
{
  const auto variableList = output.split('\n');
  QStringList result;
  foreach(const QString &v, variableList) {
    if (v.startsWith("CMAKE_COMPILER_IS_GNU<LANG>")) {
      // This key takes a compiler name :-/
      result << "CMAKE_COMPILER_IS_GNUCC" << "CMAKE_COMPILER_IS_GNUCXX";
    } else if (v.contains("<CONFIG>")) {
      const auto tmp = QString(v).replace("<CONFIG>", "%1");
      result << tmp.arg("DEBUG") << tmp.arg("RELEASE") << tmp.arg("MINSIZEREL") << tmp.arg("RELWITHDEBINFO");
    } else if (v.contains("<LANG>")) {
      const auto tmp = QString(v).replace("<LANG>", "%1");
      result << tmp.arg("C") << tmp.arg("CXX");
    } else if (!v.contains('<') && !v.contains('[')) {
      result << v;
    }
  }
  return result;
}

auto CMakeTool::fetchFromCapabilities() const -> void
{
  QtcProcess cmake;
  runCMake(cmake, {"-E", "capabilities"});

  if (cmake.result() == QtcProcess::FinishedWithSuccess) {
    m_introspection->m_didRun = true;
    parseFromCapabilities(cmake.stdOut());
  } else {
    m_introspection->m_didRun = false;
  }
}

static auto getVersion(const QVariantMap &obj, const QString value) -> int
{
  bool ok;
  auto result = obj.value(value).toInt(&ok);
  if (!ok)
    return -1;
  return result;
}

auto CMakeTool::parseFromCapabilities(const QString &input) const -> void
{
  auto doc = QJsonDocument::fromJson(input.toUtf8());
  if (!doc.isObject())
    return;

  const auto data = doc.object().toVariantMap();
  const auto generatorList = data.value("generators").toList();
  for (const auto &v : generatorList) {
    const auto gen = v.toMap();
    m_introspection->m_generators.append(Generator(gen.value("name").toString(), gen.value("extraGenerators").toStringList(), gen.value("platformSupport").toBool(), gen.value("toolsetSupport").toBool()));
  }

  {
    const auto fileApis = data.value("fileApi").toMap();
    const auto requests = fileApis.value("requests").toList();
    for (const auto &r : requests) {
      const auto object = r.toMap();
      const auto kind = object.value("kind").toString();
      const auto versionList = object.value("version").toList();
      auto highestVersion = std::make_pair(-1, -1);
      for (const auto &v : versionList) {
        const auto versionObject = v.toMap();
        const auto version = std::make_pair(getVersion(versionObject, "major"), getVersion(versionObject, "minor"));
        if (version.first > highestVersion.first || (version.first == highestVersion.first && version.second > highestVersion.second))
          highestVersion = version;
      }
      if (!kind.isNull() && highestVersion.first != -1 && highestVersion.second != -1)
        m_introspection->m_fileApis.append({kind, highestVersion});
    }
  }

  const auto versionInfo = data.value("version").toMap();
  m_introspection->m_version.major = versionInfo.value("major").toInt();
  m_introspection->m_version.minor = versionInfo.value("minor").toInt();
  m_introspection->m_version.patch = versionInfo.value("patch").toInt();
  m_introspection->m_version.fullVersion = versionInfo.value("string").toByteArray();

  // Fix up fileapi support for cmake 3.14:
  if (m_introspection->m_version.major == 3 && m_introspection->m_version.minor == 14) {
    m_introspection->m_fileApis.append({QString("codemodel"), std::make_pair(2, 0)});
    m_introspection->m_fileApis.append({QString("cache"), std::make_pair(2, 0)});
    m_introspection->m_fileApis.append({QString("cmakefiles"), std::make_pair(1, 0)});
  }
}

} // namespace CMakeProjectManager
