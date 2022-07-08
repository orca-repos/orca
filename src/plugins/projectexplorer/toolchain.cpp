// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "toolchain.hpp"

#include "abi.hpp"
#include "headerpath.hpp"
#include "projectexplorerconstants.hpp"
#include "toolchainmanager.hpp"
#include "task.hpp"

#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <utility>

using namespace Utils;

static constexpr char ID_KEY[] = "ProjectExplorer.ToolChain.Id";
static constexpr char DISPLAY_NAME_KEY[] = "ProjectExplorer.ToolChain.DisplayName";
static constexpr char AUTODETECT_KEY[] = "ProjectExplorer.ToolChain.Autodetect";
static constexpr char DETECTION_SOURCE_KEY[] = "ProjectExplorer.ToolChain.DetectionSource";
static constexpr char LANGUAGE_KEY_V1[] = "ProjectExplorer.ToolChain.Language"; // For QtCreator <= 4.2
static constexpr char LANGUAGE_KEY_V2[] = "ProjectExplorer.ToolChain.LanguageV2"; // For QtCreator > 4.2
constexpr char CODE_MODEL_TRIPLE_KEY[] = "ExplicitCodeModelTargetTriple";

namespace ProjectExplorer {
namespace Internal {

static QList<ToolChainFactory *> g_toolChainFactories;

// --------------------------------------------------------------------------
// ToolChainPrivate
// --------------------------------------------------------------------------

class ToolChainPrivate {
public:
  using Detection = ToolChain::Detection;

  explicit ToolChainPrivate(Id typeId) : m_id(QUuid::createUuid().toByteArray()), m_typeId(typeId), m_predefinedMacrosCache(new ToolChain::MacrosCache::element_type()), m_headerPathsCache(new ToolChain::HeaderPathsCache::element_type())
  {
    QTC_ASSERT(m_typeId.isValid(), return);
    QTC_ASSERT(!m_typeId.toString().contains(QLatin1Char(':')), return);
  }

  QByteArray m_id;
  FilePath m_compilerCommand;
  QString m_compilerCommandKey;
  Abi m_targetAbi;
  QString m_targetAbiKey;
  QSet<Id> m_supportedLanguages;
  mutable QString m_displayName;
  QString m_typeDisplayName;
  Id m_typeId;
  Id m_language;
  Detection m_detection = ToolChain::UninitializedDetection;
  QString m_detectionSource;
  QString m_explicitCodeModelTargetTriple;

  ToolChain::MacrosCache m_predefinedMacrosCache;
  ToolChain::HeaderPathsCache m_headerPathsCache;
};

// Deprecated used from QtCreator <= 4.2

auto fromLanguageV1(int language) -> Id
{
  switch (language) {
  case Deprecated::Toolchain::C:
    return Id(Constants::C_LANGUAGE_ID);
  case Deprecated::Toolchain::Cxx:
    return Id(Constants::CXX_LANGUAGE_ID);
  case Deprecated::Toolchain::None: default:
    return Id();
  }
}

} // namespace Internal

namespace Deprecated {namespace Toolchain {
  auto languageId(Language l) -> QString
  {
    switch (l) {
    case None:
      return QStringLiteral("None");
    case C:
      return QStringLiteral("C");
    case Cxx:
      return QStringLiteral("Cxx");
    };
    return QString();
  }
  } // namespace Toolchain
}   // namespace Deprecated

/*!
    \class ProjectExplorer::ToolChain
    \brief The ToolChain class represents a tool chain.
    \sa ProjectExplorer::ToolChainManager
*/

// --------------------------------------------------------------------------

ToolChain::ToolChain(Id typeId) : d(std::make_unique<Internal::ToolChainPrivate>(typeId)) {}

auto ToolChain::setLanguage(Id language) -> void
{
  QTC_ASSERT(!d->m_language.isValid() || isAutoDetected(), return);
  QTC_ASSERT(language.isValid(), return);
  QTC_ASSERT(ToolChainManager::isLanguageSupported(language), return);

  d->m_language = language;
}

ToolChain::~ToolChain() = default;

auto ToolChain::displayName() const -> QString
{
  if (d->m_displayName.isEmpty())
    return typeDisplayName();
  return d->m_displayName;
}

auto ToolChain::setDisplayName(const QString &name) -> void
{
  if (d->m_displayName == name)
    return;

  d->m_displayName = name;
  toolChainUpdated();
}

auto ToolChain::isAutoDetected() const -> bool
{
  return detection() == AutoDetection || detection() == AutoDetectionFromSdk;
}

auto ToolChain::detection() const -> Detection
{
  return d->m_detection;
}

auto ToolChain::detectionSource() const -> QString
{
  return d->m_detectionSource;
}

auto ToolChain::id() const -> QByteArray
{
  return d->m_id;
}

auto ToolChain::suggestedMkspecList() const -> QStringList
{
  return {};
}

auto ToolChain::typeId() const -> Id
{
  return d->m_typeId;
}

auto ToolChain::supportedAbis() const -> Abis
{
  return {targetAbi()};
}

auto ToolChain::isValid() const -> bool
{
  if (compilerCommand().isEmpty())
    return false;
  return compilerCommand().isExecutableFile();
}

auto ToolChain::includedFiles(const QStringList &flags, const QString &directory) const -> QStringList
{
  Q_UNUSED(flags)
  Q_UNUSED(directory)
  return {};
}

auto ToolChain::language() const -> Id
{
  return d->m_language;
}

auto ToolChain::operator ==(const ToolChain &tc) const -> bool
{
  if (this == &tc)
    return true;

  // We ignore displayname
  return typeId() == tc.typeId() && isAutoDetected() == tc.isAutoDetected() && language() == tc.language();
}

auto ToolChain::clone() const -> ToolChain*
{
  for (const auto f : qAsConst(Internal::g_toolChainFactories)) {
    if (f->supportedToolChainType() == d->m_typeId) {
      const auto tc = f->create();
      QTC_ASSERT(tc, return nullptr);
      tc->fromMap(toMap());
      // New ID for the clone. It's different.
      tc->d->m_id = QUuid::createUuid().toByteArray();
      return tc;
    }
  }
  QTC_CHECK(false);
  return nullptr;
}

/*!
    Used by the tool chain manager to save user-generated tool chains.

    Make sure to call this function when deriving.
*/

auto ToolChain::toMap() const -> QVariantMap
{
  QVariantMap result;
  const QString idToSave = d->m_typeId.toString() + QLatin1Char(':') + QString::fromUtf8(id());
  result.insert(QLatin1String(ID_KEY), idToSave);
  result.insert(QLatin1String(DISPLAY_NAME_KEY), displayName());
  result.insert(QLatin1String(AUTODETECT_KEY), isAutoDetected());
  result.insert(QLatin1String(DETECTION_SOURCE_KEY), d->m_detectionSource);
  result.insert(CODE_MODEL_TRIPLE_KEY, d->m_explicitCodeModelTargetTriple);
  // <Compatibility with QtC 4.2>
  auto oldLanguageId = -1;
  if (language() == Constants::C_LANGUAGE_ID)
    oldLanguageId = 1;
  else if (language() == Constants::CXX_LANGUAGE_ID)
    oldLanguageId = 2;
  if (oldLanguageId >= 0)
    result.insert(LANGUAGE_KEY_V1, oldLanguageId);
  // </Compatibility>
  result.insert(QLatin1String(LANGUAGE_KEY_V2), language().toSetting());
  if (!d->m_targetAbiKey.isEmpty())
    result.insert(d->m_targetAbiKey, d->m_targetAbi.toString());
  if (!d->m_compilerCommandKey.isEmpty())
    result.insert(d->m_compilerCommandKey, d->m_compilerCommand.toVariant());
  return result;
}

auto ToolChain::toolChainUpdated() -> void
{
  d->m_predefinedMacrosCache->invalidate();
  d->m_headerPathsCache->invalidate();

  ToolChainManager::notifyAboutUpdate(this);
}

auto ToolChain::setDetection(Detection de) -> void
{
  d->m_detection = de;
}

auto ToolChain::setDetectionSource(const QString &source) -> void
{
  d->m_detectionSource = source;
}

auto ToolChain::typeDisplayName() const -> QString
{
  return d->m_typeDisplayName;
}

auto ToolChain::targetAbi() const -> Abi
{
  return d->m_targetAbi;
}

auto ToolChain::setTargetAbi(const Abi &abi) -> void
{
  if (abi == d->m_targetAbi)
    return;

  d->m_targetAbi = abi;
  toolChainUpdated();
}

auto ToolChain::setTargetAbiNoSignal(const Abi &abi) -> void
{
  d->m_targetAbi = abi;
}

auto ToolChain::setTargetAbiKey(const QString &abiKey) -> void
{
  d->m_targetAbiKey = abiKey;
}

auto ToolChain::compilerCommand() const -> FilePath
{
  return d->m_compilerCommand;
}

auto ToolChain::setCompilerCommand(const FilePath &command) -> void
{
  if (command == d->m_compilerCommand)
    return;
  d->m_compilerCommand = command;
  toolChainUpdated();
}

auto ToolChain::setCompilerCommandKey(const QString &commandKey) -> void
{
  d->m_compilerCommandKey = commandKey;
}

auto ToolChain::setTypeDisplayName(const QString &typeName) -> void
{
  d->m_typeDisplayName = typeName;
}

/*!
    Used by the tool chain manager to load user-generated tool chains.

    Make sure to call this function when deriving.
*/

auto ToolChain::fromMap(const QVariantMap &data) -> bool
{
  d->m_displayName = data.value(QLatin1String(DISPLAY_NAME_KEY)).toString();

  // make sure we have new style ids:
  const auto id = data.value(QLatin1String(ID_KEY)).toString();
  const int pos = id.indexOf(QLatin1Char(':'));
  QTC_ASSERT(pos > 0, return false);
  d->m_typeId = Id::fromString(id.left(pos));
  d->m_id = id.mid(pos + 1).toUtf8();

  const auto autoDetect = data.value(QLatin1String(AUTODETECT_KEY), false).toBool();
  d->m_detection = autoDetect ? AutoDetection : ManualDetection;
  d->m_detectionSource = data.value(DETECTION_SOURCE_KEY).toString();

  d->m_explicitCodeModelTargetTriple = data.value(CODE_MODEL_TRIPLE_KEY).toString();

  if (data.contains(LANGUAGE_KEY_V2)) {
    // remove hack to trim language id in 4.4: This is to fix up broken language
    // ids that happened in 4.3 master branch
    const auto langId = data.value(QLatin1String(LANGUAGE_KEY_V2)).toString();
    const int pos = langId.lastIndexOf('.');
    if (pos >= 0)
      d->m_language = Id::fromString(langId.mid(pos + 1));
    else
      d->m_language = Id::fromString(langId);
  } else if (data.contains(LANGUAGE_KEY_V1)) {
    // Import from old settings
    d->m_language = Internal::fromLanguageV1(data.value(QLatin1String(LANGUAGE_KEY_V1)).toInt());
  }

  if (!d->m_language.isValid())
    d->m_language = Id(Constants::CXX_LANGUAGE_ID);

  if (!d->m_targetAbiKey.isEmpty())
    d->m_targetAbi = Abi::fromString(data.value(d->m_targetAbiKey).toString());

  d->m_compilerCommand = FilePath::fromVariant(data.value(d->m_compilerCommandKey));

  return true;
}

auto ToolChain::headerPathsCache() const -> const HeaderPathsCache&
{
  return d->m_headerPathsCache;
}

auto ToolChain::predefinedMacrosCache() const -> const MacrosCache&
{
  return d->m_predefinedMacrosCache;
}

static auto toLanguageVersionAsLong(QByteArray dateAsByteArray) -> long
{
  if (dateAsByteArray.endsWith('L'))
    dateAsByteArray.chop(1); // Strip 'L'.

  auto success = false;
  const int result = dateAsByteArray.toLong(&success);
  QTC_CHECK(success);

  return result;
}

auto ToolChain::cxxLanguageVersion(const QByteArray &cplusplusMacroValue) -> LanguageVersion
{
  using Utils::LanguageVersion;
  const auto version = toLanguageVersionAsLong(cplusplusMacroValue);

  if (version > 201703L)
    return LanguageVersion::LatestCxx;
  if (version > 201402L)
    return LanguageVersion::CXX17;
  if (version > 201103L)
    return LanguageVersion::CXX14;
  if (version == 201103L)
    return LanguageVersion::CXX11;

  return LanguageVersion::CXX03;
}

auto ToolChain::languageVersion(const Id &language, const Macros &macros) -> LanguageVersion
{
  using Utils::LanguageVersion;

  if (language == Constants::CXX_LANGUAGE_ID) {
    for (const auto &macro : macros) {
      if (macro.key == "__cplusplus") // Check for the C++ identifying macro
        return cxxLanguageVersion(macro.value);
    }

    QTC_CHECK(false && "__cplusplus is not predefined, assuming latest C++ we support.");
    return LanguageVersion::LatestCxx;
  } else if (language == Constants::C_LANGUAGE_ID) {
    for (const auto &macro : macros) {
      if (macro.key == "__STDC_VERSION__") {
        const auto version = toLanguageVersionAsLong(macro.value);

        if (version > 201710L)
          return LanguageVersion::LatestC;
        if (version > 201112L)
          return LanguageVersion::C18;
        if (version > 199901L)
          return LanguageVersion::C11;
        if (version > 199409L)
          return LanguageVersion::C99;

        return LanguageVersion::C89;
      }
    }

    // The __STDC_VERSION__ macro was introduced after C89.
    // We haven't seen it, so it must be C89.
    return LanguageVersion::C89;
  } else {
    QTC_CHECK(false && "Unexpected toolchain language, assuming latest C++ we support.");
    return LanguageVersion::LatestCxx;
  }
}

auto ToolChain::includedFiles(const QString &option, const QStringList &flags, const QString &directoryPath) -> QStringList
{
  QStringList result;

  for (auto i = 0; i < flags.size(); ++i) {
    if (flags[i] == option && i + 1 < flags.size()) {
      auto includeFile = flags[++i];
      if (!QFileInfo(includeFile).isAbsolute())
        includeFile = directoryPath + "/" + includeFile;
      result.append(QDir::cleanPath(includeFile));
    }
  }

  return result;
}

/*!
    Used by the tool chain kit information to validate the kit.
*/

auto ToolChain::validateKit(const Kit *) const -> Tasks
{
  return {};
}

auto ToolChain::sysRoot() const -> QString
{
  return QString();
}

auto ToolChain::explicitCodeModelTargetTriple() const -> QString
{
  return d->m_explicitCodeModelTargetTriple;
}

auto ToolChain::effectiveCodeModelTargetTriple() const -> QString
{
  const auto overridden = explicitCodeModelTargetTriple();
  if (!overridden.isEmpty())
    return overridden;
  return originalTargetTriple();
}

auto ToolChain::setExplicitCodeModelTargetTriple(const QString &triple) -> void
{
  d->m_explicitCodeModelTargetTriple = triple;
}

/*!
    \class ProjectExplorer::ToolChainFactory
    \brief The ToolChainFactory class creates tool chains from settings or
    autodetects them.
*/

/*!
    \fn QString ProjectExplorer::ToolChainFactory::displayName() const = 0
    Contains the name used to display the name of the tool chain that will be
    created.
*/

/*!
    \fn QStringList ProjectExplorer::ToolChain::clangParserFlags(const QStringList &cxxflags) const = 0
    Converts tool chain specific flags to list flags that tune the libclang
    parser.
*/

/*!
    \fn bool ProjectExplorer::ToolChainFactory::canRestore(const QVariantMap &data)
    Used by the tool chain manager to restore user-generated tool chains.
*/

ToolChainFactory::ToolChainFactory()
{
  Internal::g_toolChainFactories.append(this);
}

ToolChainFactory::~ToolChainFactory()
{
  Internal::g_toolChainFactories.removeOne(this);
}

auto ToolChainFactory::allToolChainFactories() -> const QList<ToolChainFactory*>
{
  return Internal::g_toolChainFactories;
}

auto ToolChainFactory::autoDetect(const ToolchainDetector &detector) const -> Toolchains
{
  Q_UNUSED(detector)
  return {};
}

auto ToolChainFactory::detectForImport(const ToolChainDescription &tcd) const -> Toolchains
{
  Q_UNUSED(tcd)
  return {};
}

auto ToolChainFactory::canCreate() const -> bool
{
  return m_userCreatable;
}

auto ToolChainFactory::create() const -> ToolChain*
{
  return m_toolchainConstructor ? m_toolchainConstructor() : nullptr;
}

auto ToolChainFactory::restore(const QVariantMap &data) -> ToolChain*
{
  if (!m_toolchainConstructor)
    return nullptr;

  const auto tc = m_toolchainConstructor();
  QTC_ASSERT(tc, return nullptr);

  if (tc->fromMap(data))
    return tc;

  delete tc;
  return nullptr;
}

static auto rawIdData(const QVariantMap &data) -> QPair<QString, QString>
{
  const auto raw = data.value(QLatin1String(ID_KEY)).toString();
  const int pos = raw.indexOf(QLatin1Char(':'));
  QTC_ASSERT(pos > 0, return qMakePair(QString::fromLatin1("unknown"), QString::fromLatin1("unknown")));
  return qMakePair(raw.mid(0, pos), raw.mid(pos + 1));
}

auto ToolChainFactory::idFromMap(const QVariantMap &data) -> QByteArray
{
  return rawIdData(data).second.toUtf8();
}

auto ToolChainFactory::typeIdFromMap(const QVariantMap &data) -> Id
{
  return Id::fromString(rawIdData(data).first);
}

auto ToolChainFactory::autoDetectionToMap(QVariantMap &data, bool detected) -> void
{
  data.insert(QLatin1String(AUTODETECT_KEY), detected);
}

auto ToolChainFactory::createToolChain(Id toolChainType) -> ToolChain*
{
  for (const auto factory : qAsConst(Internal::g_toolChainFactories)) {
    if (factory->m_supportedToolChainType == toolChainType) {
      if (const auto tc = factory->create()) {
        tc->d->m_typeId = toolChainType;
        return tc;
      }
    }
  }
  return nullptr;
}

auto ToolChainFactory::supportedLanguages() const -> QList<Id>
{
  return m_supportsAllLanguages ? ToolChainManager::allLanguages() : m_supportedLanguages;
}

auto ToolChainFactory::supportedToolChainType() const -> Id
{
  return m_supportedToolChainType;
}

auto ToolChainFactory::setSupportedToolChainType(const Id &supportedToolChain) -> void
{
  m_supportedToolChainType = supportedToolChain;
}

auto ToolChainFactory::setSupportedLanguages(const QList<Id> &supportedLanguages) -> void
{
  m_supportedLanguages = supportedLanguages;
}

auto ToolChainFactory::setSupportsAllLanguages(bool supportsAllLanguages) -> void
{
  m_supportsAllLanguages = supportsAllLanguages;
}

auto ToolChainFactory::setToolchainConstructor(const std::function<ToolChain *()> &toolchainContructor) -> void
{
  m_toolchainConstructor = toolchainContructor;
}

auto ToolChainFactory::setUserCreatable(bool userCreatable) -> void
{
  m_userCreatable = userCreatable;
}

ToolchainDetector::ToolchainDetector(const Toolchains &alreadyKnown, const IDevice::ConstPtr &device, const FilePaths &searchPaths) : alreadyKnown(alreadyKnown), device(device), searchPaths(searchPaths) {}

BadToolchain::BadToolchain(const FilePath &filePath) : BadToolchain(filePath, filePath.symLinkTarget(), filePath.lastModified()) {}

BadToolchain::BadToolchain(const FilePath &filePath, const FilePath &symlinkTarget, const QDateTime &timestamp) : filePath(filePath), symlinkTarget(symlinkTarget), timestamp(timestamp) {}

static auto badToolchainFilePathKey() -> QString { return {"FilePath"}; }
static auto badToolchainSymlinkTargetKey() -> QString { return {"TargetFilePath"}; }
static auto badToolchainTimestampKey() -> QString { return {"Timestamp"}; }

auto BadToolchain::toMap() const -> QVariantMap
{
  return {std::make_pair(badToolchainFilePathKey(), filePath.toVariant()), std::make_pair(badToolchainSymlinkTargetKey(), symlinkTarget.toVariant()), std::make_pair(badToolchainTimestampKey(), timestamp.toMSecsSinceEpoch()),};
}

auto BadToolchain::fromMap(const QVariantMap &map) -> BadToolchain
{
  return {FilePath::fromVariant(map.value(badToolchainFilePathKey())), FilePath::fromVariant(map.value(badToolchainSymlinkTargetKey())), QDateTime::fromMSecsSinceEpoch(map.value(badToolchainTimestampKey()).toLongLong())};
}

BadToolchains::BadToolchains(const QList<BadToolchain> &toolchains) : toolchains(filtered(toolchains, [](const BadToolchain &badTc) {
  return badTc.filePath.lastModified() == badTc.timestamp && badTc.filePath.symLinkTarget() == badTc.symlinkTarget;
})) {}

auto BadToolchains::isBadToolchain(const FilePath &toolchain) const -> bool
{
  return contains(toolchains, [toolchain](const BadToolchain &badTc) {
    return badTc.filePath == toolchain.absoluteFilePath() || badTc.symlinkTarget == toolchain.absoluteFilePath();
  });
}

auto BadToolchains::toVariant() const -> QVariant
{
  return Utils::transform<QVariantList>(toolchains, &BadToolchain::toMap);
}

auto BadToolchains::fromVariant(const QVariant &v) -> BadToolchains
{
  return Utils::transform<QList<BadToolchain>>(v.toList(), [](const QVariant &e) { return BadToolchain::fromMap(e.toMap()); });
}

} // namespace ProjectExplorer
