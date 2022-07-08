// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "toolchainmanager.hpp"

#include "abi.hpp"
#include "kitinformation.hpp"
#include "msvctoolchain.hpp"
#include "toolchain.hpp"
#include "toolchainsettingsaccessor.hpp"

#include <core/icore.hpp>

#include <utils/fileutils.hpp>
#include <utils/persistentsettings.hpp>
#include <utils/qtcassert.hpp>
#include <utils/algorithm.hpp>

#include <QSettings>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------------
// ToolChainManagerPrivate
// --------------------------------------------------------------------------

struct LanguageDisplayPair {
  Id id;
  QString displayName;
};

class ToolChainManagerPrivate {
public:
  ~ToolChainManagerPrivate();

  std::unique_ptr<ToolChainSettingsAccessor> m_accessor;

  Toolchains m_toolChains;       // prioritized List
  BadToolchains m_badToolchains; // to be skipped when auto-detecting
  QVector<LanguageDisplayPair> m_languages;
  ToolchainDetectionSettings m_detectionSettings;
  bool m_loaded = false;
};

ToolChainManagerPrivate::~ToolChainManagerPrivate()
{
  qDeleteAll(m_toolChains);
  m_toolChains.clear();
}

static ToolChainManager *m_instance = nullptr;
static ToolChainManagerPrivate *d = nullptr;

} // namespace Internal

using namespace Internal;

constexpr char DETECT_X64_AS_X32_KEY[] = "ProjectExplorer/Toolchains/DetectX64AsX32";

static auto badToolchainsKey() -> QString { return {"BadToolChains"}; }

// --------------------------------------------------------------------------
// ToolChainManager
// --------------------------------------------------------------------------

ToolChainManager::ToolChainManager(QObject *parent) : QObject(parent)
{
  Q_ASSERT(!m_instance);
  m_instance = this;

  d = new ToolChainManagerPrivate;

  connect(Core::ICore::instance(), &Core::ICore::saveSettingsRequested, this, &ToolChainManager::saveToolChains);
  connect(this, &ToolChainManager::toolChainAdded, this, &ToolChainManager::toolChainsChanged);
  connect(this, &ToolChainManager::toolChainRemoved, this, &ToolChainManager::toolChainsChanged);
  connect(this, &ToolChainManager::toolChainUpdated, this, &ToolChainManager::toolChainsChanged);

  const QSettings *const s = Core::ICore::settings();
  d->m_detectionSettings.detectX64AsX32 = s->value(DETECT_X64_AS_X32_KEY, ToolchainDetectionSettings().detectX64AsX32).toBool();
  d->m_badToolchains = BadToolchains::fromVariant(s->value(badToolchainsKey()));
}

ToolChainManager::~ToolChainManager()
{
  m_instance = nullptr;
  delete d;
  d = nullptr;
}

auto ToolChainManager::instance() -> ToolChainManager*
{
  return m_instance;
}

auto ToolChainManager::restoreToolChains() -> void
{
  QTC_ASSERT(!d->m_accessor, return);
  d->m_accessor = std::make_unique<ToolChainSettingsAccessor>();

  for (const auto tc : d->m_accessor->restoreToolChains(Core::ICore::dialogParent()))
    registerToolChain(tc);

  d->m_loaded = true;
  emit m_instance->toolChainsLoaded();
}

auto ToolChainManager::saveToolChains() -> void
{
  QTC_ASSERT(d->m_accessor, return);

  d->m_accessor->saveToolChains(d->m_toolChains, Core::ICore::dialogParent());
  const auto s = Core::ICore::settings();
  s->setValueWithDefault(DETECT_X64_AS_X32_KEY, d->m_detectionSettings.detectX64AsX32, ToolchainDetectionSettings().detectX64AsX32);
  s->setValue(badToolchainsKey(), d->m_badToolchains.toVariant());
}

auto ToolChainManager::toolchains() -> const Toolchains&
{
  return d->m_toolChains;
}

auto ToolChainManager::toolchains(const ToolChain::Predicate &predicate) -> Toolchains
{
  QTC_ASSERT(predicate, return {});
  return filtered(d->m_toolChains, predicate);
}

auto ToolChainManager::toolChain(const ToolChain::Predicate &predicate) -> ToolChain*
{
  return findOrDefault(d->m_toolChains, predicate);
}

auto ToolChainManager::findToolChains(const Abi &abi) -> Toolchains
{
  Toolchains result;
  for (const auto tc : qAsConst(d->m_toolChains)) {
    const auto isCompatible = anyOf(tc->supportedAbis(), [abi](const Abi &supportedAbi) {
      return supportedAbi.isCompatibleWith(abi);
    });

    if (isCompatible)
      result.append(tc);
  }
  return result;
}

auto ToolChainManager::findToolChain(const QByteArray &id) -> ToolChain*
{
  if (id.isEmpty())
    return nullptr;

  auto tc = findOrDefault(d->m_toolChains, equal(&ToolChain::id, id));

  // Compatibility with versions 3.5 and earlier:
  if (!tc) {
    const int pos = id.indexOf(':');
    if (pos < 0)
      return tc;

    const auto shortId = id.mid(pos + 1);

    tc = findOrDefault(d->m_toolChains, equal(&ToolChain::id, shortId));
  }
  return tc;
}

auto ToolChainManager::isLoaded() -> bool
{
  return d->m_loaded;
}

auto ToolChainManager::notifyAboutUpdate(ToolChain *tc) -> void
{
  if (!tc || !d->m_toolChains.contains(tc))
    return;
  emit m_instance->toolChainUpdated(tc);
}

auto ToolChainManager::registerToolChain(ToolChain *tc) -> bool
{
  QTC_ASSERT(tc, return false);
  QTC_ASSERT(isLanguageSupported(tc->language()), qDebug() << qPrintable("language \"" + tc->language().toString() + "\" unknown while registering \"" + tc->compilerCommand().toString() + "\""); return false);
  QTC_ASSERT(d->m_accessor, return false);

  if (d->m_toolChains.contains(tc))
    return true;
  foreach(ToolChain *current, d->m_toolChains) {
    if (*tc == *current && !tc->isAutoDetected())
      return false;
    QTC_ASSERT(current->id() != tc->id(), return false);
  }

  d->m_toolChains.append(tc);
  emit m_instance->toolChainAdded(tc);
  return true;
}

auto ToolChainManager::deregisterToolChain(ToolChain *tc) -> void
{
  if (!tc || !d->m_toolChains.contains(tc))
    return;
  d->m_toolChains.removeOne(tc);
  emit m_instance->toolChainRemoved(tc);
  delete tc;
}

auto ToolChainManager::allLanguages() -> QList<Id>
{
  return Utils::transform<QList>(d->m_languages, &LanguageDisplayPair::id);
}

auto ToolChainManager::registerLanguage(const Id &language, const QString &displayName) -> bool
{
  QTC_ASSERT(language.isValid(), return false);
  QTC_ASSERT(!isLanguageSupported(language), return false);
  QTC_ASSERT(!displayName.isEmpty(), return false);
  d->m_languages.push_back({language, displayName});
  return true;
}

auto ToolChainManager::displayNameOfLanguageId(const Id &id) -> QString
{
  QTC_ASSERT(id.isValid(), return tr("None"));
  auto entry = findOrDefault(d->m_languages, equal(&LanguageDisplayPair::id, id));
  QTC_ASSERT(entry.id.isValid(), return tr("None"));
  return entry.displayName;
}

auto ToolChainManager::isLanguageSupported(const Id &id) -> bool
{
  return contains(d->m_languages, equal(&LanguageDisplayPair::id, id));
}

auto ToolChainManager::aboutToShutdown() -> void
{
  if (HostOsInfo::isWindowsHost())
    MsvcToolChain::cancelMsvcToolChainDetection();
}

auto ToolChainManager::detectionSettings() -> ToolchainDetectionSettings
{
  return d->m_detectionSettings;
}

auto ToolChainManager::setDetectionSettings(const ToolchainDetectionSettings &settings) -> void
{
  d->m_detectionSettings = settings;
}

auto ToolChainManager::resetBadToolchains() -> void
{
  d->m_badToolchains.toolchains.clear();
}

auto ToolChainManager::isBadToolchain(const FilePath &toolchain) -> bool
{
  return d->m_badToolchains.isBadToolchain(toolchain);
}

auto ToolChainManager::addBadToolchain(const FilePath &toolchain) -> void
{
  d->m_badToolchains.toolchains << toolchain;
}

} // namespace ProjectExplorer
