// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitmanager.hpp"

#include "abi.hpp"
#include "devicesupport/idevicefactory.hpp"
#include "kit.hpp"
#include "kitfeatureprovider.hpp"
#include "kitinformation.hpp"
#include "kitmanagerconfigwidget.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "task.hpp"
#include "toolchainmanager.hpp"

#include <core/icore.hpp>

#include <constants/android/androidconstants.hpp>
#include <constants/baremetal/baremetalconstants.hpp>
#include <constants/qnx/qnxconstants.hpp>
#include <constants/remotelinux/remotelinux_constants.hpp>

#include <utils/environment.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/persistentsettings.hpp>
#include <utils/pointeralgorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QAction>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStyle>

using namespace Core;
using namespace Utils;
using namespace ProjectExplorer::Internal;

namespace ProjectExplorer {

class KitList {
public:
  Id defaultKit;
  std::vector<std::unique_ptr<Kit>> kits;
};

static auto restoreKitsHelper(const FilePath &fileName) -> KitList;

namespace Internal {

constexpr char KIT_DATA_KEY[] = "Profile.";
constexpr char KIT_COUNT_KEY[] = "Profile.Count";
constexpr char KIT_FILE_VERSION_KEY[] = "Version";
constexpr char KIT_DEFAULT_KEY[] = "Profile.Default";
constexpr char KIT_IRRELEVANT_ASPECTS_KEY[] = "Kit.IrrelevantAspects";
constexpr char KIT_FILENAME[] = "profiles.xml";

static auto settingsFileName() -> FilePath
{
  return ICore::userResourcePath(KIT_FILENAME);
}

// --------------------------------------------------------------------------
// KitManagerPrivate:
// --------------------------------------------------------------------------

class KitManagerPrivate {
public:
  Kit *m_defaultKit = nullptr;
  bool m_initialized = false;
  std::vector<std::unique_ptr<Kit>> m_kitList;
  std::unique_ptr<PersistentSettingsWriter> m_writer;
  QSet<Id> m_irrelevantAspects;

  auto addKitAspect(KitAspect *ki) -> void
  {
    QTC_ASSERT(!m_aspectList.contains(ki), return);
    m_aspectList.append(ki);
    m_aspectListIsSorted = false;
  }

  auto removeKitAspect(KitAspect *ki) -> void
  {
    const int removed = m_aspectList.removeAll(ki);
    QTC_CHECK(removed == 1);
  }

  auto kitAspects() -> const QList<KitAspect*>
  {
    if (!m_aspectListIsSorted) {
      sort(m_aspectList, [](const KitAspect *a, const KitAspect *b) {
        return a->priority() > b->priority();
      });
      m_aspectListIsSorted = true;
    }
    return m_aspectList;
  }

  auto setBinaryForKit(const FilePath &fp) -> void { m_binaryForKit = fp; }
  auto binaryForKit() const -> FilePath { return m_binaryForKit; }

private:
  // Sorted by priority, in descending order...
  QList<KitAspect*> m_aspectList;
  // ... if this here is set:
  bool m_aspectListIsSorted = true;

  FilePath m_binaryForKit;
};

} // namespace Internal

// --------------------------------------------------------------------------
// KitManager:
// --------------------------------------------------------------------------

static KitManagerPrivate *d = nullptr;
static KitManager *m_instance = nullptr;

auto KitManager::instance() -> KitManager*
{
  if (!m_instance)
    m_instance = new KitManager;
  return m_instance;
}

KitManager::KitManager()
{
  d = new KitManagerPrivate;
  QTC_CHECK(!m_instance);
  m_instance = this;

  connect(ICore::instance(), &ICore::saveSettingsRequested, this, &KitManager::saveKits);

  connect(this, &KitManager::kitAdded, this, &KitManager::kitsChanged);
  connect(this, &KitManager::kitRemoved, this, &KitManager::kitsChanged);
  connect(this, &KitManager::kitUpdated, this, &KitManager::kitsChanged);
}

auto KitManager::destroy() -> void
{
  delete d;
  d = nullptr;
  delete m_instance;
  m_instance = nullptr;
}

auto KitManager::restoreKits() -> void
{
  QTC_ASSERT(!d->m_initialized, return);

  std::vector<std::unique_ptr<Kit>> resultList;

  // read all kits from user file
  Id defaultUserKit;
  std::vector<std::unique_ptr<Kit>> kitsToCheck;
  {
    auto userKits = restoreKitsHelper(settingsFileName());
    defaultUserKit = userKits.defaultKit;

    for (auto &k : userKits.kits) {
      if (k->isSdkProvided()) {
        kitsToCheck.emplace_back(std::move(k));
      } else {
        completeKit(k.get()); // Store manual kits
        resultList.emplace_back(std::move(k));
      }
    }
  }

  // read all kits from SDK
  {
    auto system = restoreKitsHelper(ICore::installerResourcePath(KIT_FILENAME));

    // SDK kits need to get updated with the user-provided extra settings:
    for (auto &current : system.kits) {
      // make sure we mark these as autodetected and run additional setup logic
      current->setAutoDetected(true);
      current->setSdkProvided(true);
      current->makeSticky();

      // Process:
      auto toStore = std::move(current);
      toStore->upgrade();
      toStore->setup(); // Make sure all kitinformation are properly set up before merging them
      // with the information from the user settings file

      // Check whether we had this kit stored and prefer the stored one:
      const auto i = std::find_if(std::begin(kitsToCheck), std::end(kitsToCheck), equal(&Kit::id, toStore->id()));
      if (i != std::end(kitsToCheck)) {
        auto ptr = i->get();

        // Overwrite settings that the SDK sets to those values:
        for (const KitAspect *aspect : kitAspects()) {
          // Copy sticky settings over:
          ptr->setSticky(aspect->id(), toStore->isSticky(aspect->id()));
          if (ptr->isSticky(aspect->id()))
            ptr->setValue(aspect->id(), toStore->value(aspect->id()));
        }
        toStore = std::move(*i);
        kitsToCheck.erase(i);
      }
      completeKit(toStore.get()); // Store manual kits
      resultList.emplace_back(std::move(toStore));
    }
  }

  // Delete all loaded autodetected kits that were not rediscovered:
  kitsToCheck.clear();

  // Remove replacement kits for which the original kit has turned up again.
  for (auto it = resultList.begin(); it != resultList.end();) {
    const auto &k = *it;
    if (k->isReplacementKit() && contains(resultList, [&k](const std::unique_ptr<Kit> &other) {
      return other->id() == k->id() && other != k;
    })) {
      it = resultList.erase(it);
    } else {
      ++it;
    }
  }

  static const auto kitMatchesAbiList = [](const Kit *kit, const Abis &abis) {
    const auto toolchains = ToolChainKitAspect::toolChains(kit);
    for (const ToolChain *const tc : toolchains) {
      const auto tcAbi = tc->targetAbi();
      for (const auto &abi : abis) {
        if (tcAbi.os() == abi.os() && tcAbi.architecture() == abi.architecture() && (tcAbi.os() != Abi::LinuxOS || tcAbi.osFlavor() == abi.osFlavor())) {
          return true;
        }
      }
    }
    return false;
  };

  const auto abisOfBinary = d->binaryForKit().isEmpty() ? Abis() : Abi::abisOfBinary(d->binaryForKit());
  const auto kitMatchesAbiOfBinary = [&abisOfBinary](const Kit *kit) {
    return kitMatchesAbiList(kit, abisOfBinary);
  };
  const auto haveKitForBinary = abisOfBinary.isEmpty() || contains(resultList, [&kitMatchesAbiOfBinary](const std::unique_ptr<Kit> &kit) {
    return kitMatchesAbiOfBinary(kit.get());
  });
  Kit *kitForBinary = nullptr;

  if (resultList.empty() || !haveKitForBinary) {
    // No kits exist yet, so let's try to autoconfigure some from the toolchains we know.
    QHash<Abi, QHash<Id, ToolChain*>> uniqueToolchains;

    // On Linux systems, we usually detect a plethora of same-ish toolchains. The following
    // algorithm gives precedence to icecc and ccache and otherwise simply chooses the one with
    // the shortest path. This should also take care of ensuring matching C/C++ pairs.
    // TODO: This should not need to be done here. Instead, it should be a convenience
    // operation on some lower level, e.g. in the toolchain class(es).
    // Also, we shouldn't detect so many doublets in the first place.
    for (const auto tc : ToolChainManager::toolchains()) {
      auto &bestTc = uniqueToolchains[tc->targetAbi()][tc->language()];
      if (!bestTc) {
        bestTc = tc;
        continue;
      }
      const auto bestFilePath = bestTc->compilerCommand().toString();
      const auto currentFilePath = tc->compilerCommand().toString();
      if (bestFilePath.contains("icecc"))
        continue;
      if (currentFilePath.contains("icecc")) {
        bestTc = tc;
        continue;
      }

      if (bestFilePath.contains("ccache"))
        continue;
      if (currentFilePath.contains("ccache")) {
        bestTc = tc;
        continue;
      }
      if (bestFilePath.length() > currentFilePath.length())
        bestTc = tc;
    }

    static const auto isHostKit = [](const Kit *kit) {
      return kitMatchesAbiList(kit, {Abi::hostAbi()});
    };

    static const auto deviceTypeForKit = [](const Kit *kit) {
      if (isHostKit(kit))
        return Constants::DESKTOP_DEVICE_TYPE;
      const auto toolchains = ToolChainKitAspect::toolChains(kit);
      for (const ToolChain *const tc : toolchains) {
        const auto tcAbi = tc->targetAbi();
        switch (tcAbi.os()) {
        case Abi::BareMetalOS:
          return BareMetal::Constants::BareMetalOsType;
        case Abi::BsdOS:
        case Abi::DarwinOS:
        case Abi::UnixOS:
          return RemoteLinux::Constants::GenericLinuxOsType;
        case Abi::LinuxOS:
          if (tcAbi.osFlavor() == Abi::AndroidLinuxFlavor)
            return Android::Constants::ANDROID_DEVICE_TYPE;
          return RemoteLinux::Constants::GenericLinuxOsType;
        case Abi::QnxOS:
          return Qnx::Constants::QNX_QNX_OS_TYPE;
        case Abi::VxWorks:
          return "VxWorks.Device.Type";
        default:
          break;
        }
      }
      return Constants::DESKTOP_DEVICE_TYPE;
    };

    // Create temporary kits for all toolchains found.
    decltype(resultList) tempList;
    for (auto it = uniqueToolchains.cbegin(); it != uniqueToolchains.cend(); ++it) {
      auto kit = std::make_unique<Kit>();
      kit->setSdkProvided(false);
      kit->setAutoDetected(false); // TODO: Why false? What does autodetected mean here?
      for (const auto tc : it.value())
        ToolChainKitAspect::setToolChain(kit.get(), tc);
      if (contains(resultList, [&kit](const std::unique_ptr<Kit> &existingKit) {
        return ToolChainKitAspect::toolChains(kit.get()) == ToolChainKitAspect::toolChains(existingKit.get());
      })) {
        continue;
      }
      if (isHostKit(kit.get()))
        kit->setUnexpandedDisplayName(tr("Desktop (%1)").arg(it.key().toString()));
      else
        kit->setUnexpandedDisplayName(it.key().toString());
      DeviceTypeKitAspect::setDeviceTypeId(kit.get(), deviceTypeForKit(kit.get()));
      kit->setup();
      tempList.emplace_back(std::move(kit));
    }

    // Now make the "best" temporary kits permanent. The logic is as follows:
    //     - If the user has requested a kit for a given binary and one or more kits
    //       with a matching ABI exist, then we randomly choose exactly one among those with
    //       the highest weight.
    //     - If the user has not requested a kit for a given binary or no such kit could
    //       be created, we choose all kits with the highest weight. If none of these
    //       is a host kit, then we also add the host kit with the highest weight.
    Utils::sort(tempList, [](const std::unique_ptr<Kit> &k1, const std::unique_ptr<Kit> &k2) {
      return k1->weight() > k2->weight();
    });
    if (!abisOfBinary.isEmpty()) {
      for (auto it = tempList.begin(); it != tempList.end(); ++it) {
        if (kitMatchesAbiOfBinary(it->get())) {
          kitForBinary = it->get();
          resultList.emplace_back(std::move(*it));
          tempList.erase(it);
          break;
        }
      }
    }
    QList<Kit*> hostKits;
    if (!kitForBinary && !tempList.empty()) {
      const auto maxWeight = tempList.front()->weight();
      for (auto it = tempList.begin(); it != tempList.end(); it = tempList.erase(it)) {
        if ((*it)->weight() < maxWeight)
          break;
        if (isHostKit(it->get()))
          hostKits << it->get();
        resultList.emplace_back(std::move(*it));
      }
      if (!contains(resultList, [](const std::unique_ptr<Kit> &kit) {
        return isHostKit(kit.get());
      })) {
        QTC_ASSERT(hostKits.isEmpty(), hostKits.clear());
        for (auto &kit : tempList) {
          if (isHostKit(kit.get())) {
            hostKits << kit.get();
            resultList.emplace_back(std::move(kit));
            break;
          }
        }
      }
    }

    if (hostKits.size() == 1)
      hostKits.first()->setUnexpandedDisplayName(tr("Desktop"));
  }

  auto k = kitForBinary;
  if (!k)
    k = findOrDefault(resultList, equal(&Kit::id, defaultUserKit));
  if (!k)
    k = findOrDefault(resultList, &Kit::isValid);
  std::swap(resultList, d->m_kitList);
  setDefaultKit(k);

  d->m_writer = std::make_unique<PersistentSettingsWriter>(settingsFileName(), "QtCreatorProfiles");
  d->m_initialized = true;
  emit m_instance->kitsLoaded();
  emit m_instance->kitsChanged();
}

KitManager::~KitManager() {}

auto KitManager::saveKits() -> void
{
  QTC_ASSERT(d, return);
  if (!d->m_writer) // ignore save requests while we are not initialized.
    return;

  QVariantMap data;
  data.insert(QLatin1String(KIT_FILE_VERSION_KEY), 1);

  auto count = 0;
  foreach(Kit *k, kits()) {
    auto tmp = k->toMap();
    if (tmp.isEmpty())
      continue;
    data.insert(QString::fromLatin1(KIT_DATA_KEY) + QString::number(count), tmp);
    ++count;
  }
  data.insert(QLatin1String(KIT_COUNT_KEY), count);
  data.insert(QLatin1String(KIT_DEFAULT_KEY), d->m_defaultKit ? QString::fromLatin1(d->m_defaultKit->id().name()) : QString());
  data.insert(KIT_IRRELEVANT_ASPECTS_KEY, transform<QVariantList>(d->m_irrelevantAspects, &Id::toSetting));
  d->m_writer->save(data, ICore::dialogParent());
}

auto KitManager::isLoaded() -> bool
{
  return d->m_initialized;
}

auto KitManager::registerKitAspect(KitAspect *ki) -> void
{
  instance();
  QTC_ASSERT(d, return);
  d->addKitAspect(ki);

  // Adding this aspect to possibly already existing kits is currently not
  // needed here as kits are only created after all aspects are created
  // in *Plugin::initialize().
  // Make sure we notice when this assumption breaks:
  QTC_CHECK(d->m_kitList.empty());
}

auto KitManager::deregisterKitAspect(KitAspect *ki) -> void
{
  // Happens regularly for the aspects from the ProjectExplorerPlugin as these
  // are destroyed after the manual call to KitManager::destroy() there, but as
  // this here is just for sanity reasons that the KitManager does not access
  // a destroyed aspect, a destroyed KitManager is not a problem.
  if (d)
    d->removeKitAspect(ki);
}

auto KitManager::setBinaryForKit(const FilePath &binary) -> void
{
  QTC_ASSERT(d, return);
  d->setBinaryForKit(binary);
}

auto KitManager::sortKits(const QList<Kit*> &kits) -> QList<Kit*>
{
  // This method was added to delay the sorting of kits as long as possible.
  // Since the displayName can contain variables it can be costly (e.g. involve
  // calling executables to find version information, etc.) to call that
  // method!
  // Avoid lots of potentially expensive calls to Kit::displayName():
  auto sortList = transform(kits, [](Kit *k) {
    return qMakePair(k->displayName(), k);
  });
  Utils::sort(sortList, [](const QPair<QString, Kit*> &a, const QPair<QString, Kit*> &b) -> bool {
    if (a.first == b.first)
      return a.second < b.second;
    return a.first < b.first;
  });
  return Utils::transform(sortList, &QPair<QString, Kit*>::second);
}

static auto restoreKitsHelper(const FilePath &fileName) -> KitList
{
  KitList result;

  if (!fileName.exists())
    return result;

  PersistentSettingsReader reader;
  if (!reader.load(fileName)) {
    qWarning("Warning: Failed to read \"%s\", cannot restore kits!", qPrintable(fileName.toUserOutput()));
    return result;
  }
  const auto data = reader.restoreValues();

  // Check version:
  const auto version = data.value(QLatin1String(KIT_FILE_VERSION_KEY), 0).toInt();
  if (version < 1) {
    qWarning("Warning: Kit file version %d not supported, cannot restore kits!", version);
    return result;
  }

  const auto count = data.value(QLatin1String(KIT_COUNT_KEY), 0).toInt();
  for (auto i = 0; i < count; ++i) {
    const QString key = QString::fromLatin1(KIT_DATA_KEY) + QString::number(i);
    if (!data.contains(key))
      break;

    const auto stMap = data.value(key).toMap();

    auto k = std::make_unique<Kit>(stMap);
    if (k->id().isValid()) {
      result.kits.emplace_back(std::move(k));
    } else {
      qWarning("Warning: Unable to restore kits stored in %s at position %d.", qPrintable(fileName.toUserOutput()), i);
    }
  }
  const auto id = Id::fromSetting(data.value(QLatin1String(KIT_DEFAULT_KEY)));
  if (!id.isValid())
    return result;

  if (contains(result.kits, [id](const std::unique_ptr<Kit> &k) { return k->id() == id; }))
    result.defaultKit = id;
  const auto it = data.constFind(KIT_IRRELEVANT_ASPECTS_KEY);
  if (it != data.constEnd())
    d->m_irrelevantAspects = transform<QSet<Id>>(it.value().toList(), &Id::fromSetting);

  return result;
}

auto KitManager::kits() -> const QList<Kit*>
{
  return Utils::toRawPointer<QList>(d->m_kitList);
}

auto KitManager::kit(Id id) -> Kit*
{
  if (!id.isValid())
    return nullptr;

  return findOrDefault(d->m_kitList, equal(&Kit::id, id));
}

auto KitManager::kit(const Kit::Predicate &predicate) -> Kit*
{
  return findOrDefault(kits(), predicate);
}

auto KitManager::defaultKit() -> Kit*
{
  return d->m_defaultKit;
}

auto KitManager::kitAspects() -> const QList<KitAspect*>
{
  return d->kitAspects();
}

auto KitManager::irrelevantAspects() -> const QSet<Id>
{
  return d->m_irrelevantAspects;
}

auto KitManager::setIrrelevantAspects(const QSet<Id> &aspects) -> void
{
  d->m_irrelevantAspects = aspects;
}

auto KitManager::notifyAboutUpdate(Kit *k) -> void
{
  if (!k || !isLoaded())
    return;

  if (contains(d->m_kitList, k)) emit m_instance->kitUpdated(k);
  else emit m_instance->unmanagedKitUpdated(k);
}

auto KitManager::registerKit(const std::function<void (Kit *)> &init, Id id) -> Kit*
{
  QTC_ASSERT(isLoaded(), return nullptr);

  auto k = std::make_unique<Kit>(id);
  QTC_ASSERT(k->id().isValid(), return nullptr);

  const auto kptr = k.get();
  if (init)
    init(kptr);

  // make sure we have all the information in our kits:
  completeKit(kptr);

  d->m_kitList.emplace_back(std::move(k));

  if (!d->m_defaultKit || (!d->m_defaultKit->isValid() && kptr->isValid()))
    setDefaultKit(kptr);

  emit m_instance->kitAdded(kptr);
  return kptr;
}

auto KitManager::deregisterKit(Kit *k) -> void
{
  if (!k || !contains(d->m_kitList, k))
    return;
  auto taken = take(d->m_kitList, k);
  if (defaultKit() == k) {
    const auto newDefault = findOrDefault(kits(), [](Kit *k) { return k->isValid(); });
    setDefaultKit(newDefault);
  }
  emit m_instance->kitRemoved(k);
}

auto KitManager::setDefaultKit(Kit *k) -> void
{
  if (defaultKit() == k)
    return;
  if (k && !contains(d->m_kitList, k))
    return;
  d->m_defaultKit = k;
  emit m_instance->defaultkitChanged();
}

auto KitManager::completeKit(Kit *k) -> void
{
  QTC_ASSERT(k, return);
  KitGuard g(k);
  for (const auto ki : d->kitAspects()) {
    ki->upgrade(k);
    if (!k->hasValue(ki->id()))
      ki->setup(k);
    else
      ki->fix(k);
  }
}

// --------------------------------------------------------------------
// KitAspect:
// --------------------------------------------------------------------

KitAspect::KitAspect()
{
  KitManager::registerKitAspect(this);
}

KitAspect::~KitAspect()
{
  KitManager::deregisterKitAspect(this);
}

auto KitAspect::weight(const Kit *k) const -> int
{
  return k->value(id()).isValid() ? 1 : 0;
}

auto KitAspect::addToBuildEnvironment(const Kit *k, Environment &env) const -> void
{
  Q_UNUSED(k)
  Q_UNUSED(env)
}

auto KitAspect::addToRunEnvironment(const Kit *k, Environment &env) const -> void
{
  Q_UNUSED(k)
  Q_UNUSED(env)
}

auto KitAspect::createOutputParsers(const Kit *k) const -> QList<OutputLineParser*>
{
  Q_UNUSED(k)
  return {};
}

auto KitAspect::displayNamePostfix(const Kit *k) const -> QString
{
  Q_UNUSED(k)
  return QString();
}

auto KitAspect::supportedPlatforms(const Kit *k) const -> QSet<Id>
{
  Q_UNUSED(k)
  return QSet<Id>();
}

auto KitAspect::availableFeatures(const Kit *k) const -> QSet<Id>
{
  Q_UNUSED(k)
  return QSet<Id>();
}

auto KitAspect::addToMacroExpander(Kit *k, MacroExpander *expander) const -> void
{
  Q_UNUSED(k)
  Q_UNUSED(expander)
}

auto KitAspect::notifyAboutUpdate(Kit *k) -> void
{
  if (k)
    k->kitUpdated();
}

KitAspectWidget::KitAspectWidget(Kit *kit, const KitAspect *ki) : m_kit(kit), m_kitInformation(ki)
{
  const auto id = ki->id();
  m_mutableAction = new QAction(tr("Mark as Mutable"));
  m_mutableAction->setCheckable(true);
  m_mutableAction->setChecked(m_kit->isMutable(id));
  m_mutableAction->setEnabled(!m_kit->isSticky(id));
  connect(m_mutableAction, &QAction::toggled, this, [this, id] {
    m_kit->setMutable(id, m_mutableAction->isChecked());
  });
}

KitAspectWidget::~KitAspectWidget()
{
  delete m_mutableAction;
}

auto KitAspectWidget::addToLayoutWithLabel(QWidget *parent) -> void
{
  QTC_ASSERT(parent, return);
  const auto label = createSubWidget<QLabel>(m_kitInformation->displayName() + ':');
  label->setToolTip(m_kitInformation->description());
  connect(label, &QLabel::linkActivated, this, [this](const QString &link) {
    emit labelLinkActivated(link);
  });

  LayoutExtender builder(parent->layout());
  builder.finishRow();
  builder.addItem(label);
  addToLayout(builder);
}

auto KitAspectWidget::addMutableAction(QWidget *child) -> void
{
  QTC_ASSERT(child, return);
  child->addAction(m_mutableAction);
  child->setContextMenuPolicy(Qt::ActionsContextMenu);
}

auto KitAspectWidget::createManageButton(Id pageId) -> QWidget*
{
  const auto button = createSubWidget<QPushButton>(msgManage());
  connect(button, &QPushButton::clicked, this, [pageId] {
    ICore::showOptionsDialog(pageId);
  });
  return button;
}

auto KitAspectWidget::msgManage() -> QString
{
  return tr("Manage...");
}

// --------------------------------------------------------------------
// KitFeatureProvider:
// --------------------------------------------------------------------

// This FeatureProvider maps the platforms onto the device types.

auto KitFeatureProvider::availableFeatures(Id id) const -> QSet<Id>
{
  QSet<Id> features;
  for (const Kit *k : KitManager::kits()) {
    if (k->supportedPlatforms().contains(id))
      features.unite(k->availableFeatures());
  }
  return features;
}

auto KitFeatureProvider::availablePlatforms() const -> QSet<Id>
{
  QSet<Id> platforms;
  for (const Kit *k : KitManager::kits())
    platforms.unite(k->supportedPlatforms());
  return platforms;
}

auto KitFeatureProvider::displayNameForPlatform(Id id) const -> QString
{
  if (const auto f = IDeviceFactory::find(id)) {
    auto dn = f->displayName();
    const auto deviceStr = QStringLiteral("device");
    if (dn.endsWith(deviceStr, Qt::CaseInsensitive))
      dn = dn.remove(deviceStr, Qt::CaseInsensitive).trimmed();
    QTC_CHECK(!dn.isEmpty());
    return dn;
  }
  return QString();
}

} // namespace ProjectExplorer
