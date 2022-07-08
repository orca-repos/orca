// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kit.hpp"

#include "devicesupport/idevicefactory.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "ioutputparser.hpp"
#include "osparser.hpp"
#include "projectexplorerconstants.hpp"

#include <utils/algorithm.hpp>
#include <utils/displayname.hpp>
#include <utils/filepath.hpp>
#include <utils/icon.hpp>
#include <utils/macroexpander.hpp>
#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/utilsicons.hpp>

#include <QIcon>
#include <QTextStream>
#include <QUuid>

#include <numeric>

using namespace Core;
using namespace Utils;

constexpr char ID_KEY[] = "PE.Profile.Id";
constexpr char DISPLAYNAME_KEY[] = "PE.Profile.Name";
constexpr char FILESYSTEMFRIENDLYNAME_KEY[] = "PE.Profile.FileSystemFriendlyName";
constexpr char AUTODETECTED_KEY[] = "PE.Profile.AutoDetected";
constexpr char AUTODETECTIONSOURCE_KEY[] = "PE.Profile.AutoDetectionSource";
constexpr char SDK_PROVIDED_KEY[] = "PE.Profile.SDK";
constexpr char DATA_KEY[] = "PE.Profile.Data";
constexpr char ICON_KEY[] = "PE.Profile.Icon";
constexpr char DEVICE_TYPE_FOR_ICON_KEY[] = "PE.Profile.DeviceTypeForIcon";
constexpr char MUTABLE_INFO_KEY[] = "PE.Profile.MutableInfo";
constexpr char STICKY_INFO_KEY[] = "PE.Profile.StickyInfo";
constexpr char IRRELEVANT_ASPECTS_KEY[] = "PE.Kit.IrrelevantAspects";

namespace ProjectExplorer {
namespace Internal {

// -------------------------------------------------------------------------
// KitPrivate
// -------------------------------------------------------------------------

class KitPrivate {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Kit)
public:
  KitPrivate(Id id, Kit *kit) : m_id(id)
  {
    if (!id.isValid())
      m_id = Id::fromString(QUuid::createUuid().toString());

    m_unexpandedDisplayName.setDefaultValue(QCoreApplication::translate("ProjectExplorer::Kit", "Unnamed"));

    m_macroExpander.setDisplayName(tr("Kit"));
    m_macroExpander.setAccumulating(true);
    m_macroExpander.registerVariable("Kit:Id", tr("Kit ID"), [kit] { return kit->id().toString(); });
    m_macroExpander.registerVariable("Kit:FileSystemName", tr("Kit filesystem-friendly name"), [kit] { return kit->fileSystemFriendlyName(); });
    for (const auto aspect : KitManager::kitAspects())
      aspect->addToMacroExpander(kit, &m_macroExpander);

    // TODO: Remove the "Current" variants in ~4.16
    m_macroExpander.registerVariable("CurrentKit:Name", tr("The name of the currently active kit."), [kit] { return kit->displayName(); }, false);
    m_macroExpander.registerVariable("Kit:Name", tr("The name of the kit."), [kit] { return kit->displayName(); });

    m_macroExpander.registerVariable("CurrentKit:FileSystemName", tr("The name of the currently active kit in a filesystem-friendly version."), [kit] { return kit->fileSystemFriendlyName(); }, false);
    m_macroExpander.registerVariable("Kit:FileSystemName", tr("The name of the kit in a filesystem-friendly version."), [kit] { return kit->fileSystemFriendlyName(); });

    m_macroExpander.registerVariable("CurrentKit:Id", tr("The ID of the currently active kit."), [kit] { return kit->id().toString(); }, false);
    m_macroExpander.registerVariable("Kit:Id", tr("The ID of the kit."), [kit] { return kit->id().toString(); });
  }

  DisplayName m_unexpandedDisplayName;
  QString m_fileSystemFriendlyName;
  QString m_autoDetectionSource;
  Id m_id;
  int m_nestedBlockingLevel = 0;
  bool m_autodetected = false;
  bool m_sdkProvided = false;
  bool m_hasError = false;
  bool m_hasWarning = false;
  bool m_hasValidityInfo = false;
  bool m_mustNotify = false;
  QIcon m_cachedIcon;
  FilePath m_iconPath;
  Id m_deviceTypeForIcon;
  QHash<Id, QVariant> m_data;
  QSet<Id> m_sticky;
  QSet<Id> m_mutable;
  optional<QSet<Id>> m_irrelevantAspects;
  MacroExpander m_macroExpander;
};

} // namespace Internal

// -------------------------------------------------------------------------
// Kit:
// -------------------------------------------------------------------------

auto Kit::defaultPredicate() -> Predicate
{
  return [](const Kit *k) { return k->isValid(); };
}

Kit::Kit(Id id) : d(std::make_unique<Internal::KitPrivate>(id, this)) {}

Kit::Kit(const QVariantMap &data) : d(std::make_unique<Internal::KitPrivate>(Id(), this))
{
  d->m_id = Id::fromSetting(data.value(QLatin1String(ID_KEY)));

  d->m_autodetected = data.value(QLatin1String(AUTODETECTED_KEY)).toBool();
  d->m_autoDetectionSource = data.value(QLatin1String(AUTODETECTIONSOURCE_KEY)).toString();

  // if we don't have that setting assume that autodetected implies sdk
  const auto value = data.value(QLatin1String(SDK_PROVIDED_KEY));
  if (value.isValid())
    d->m_sdkProvided = value.toBool();
  else
    d->m_sdkProvided = d->m_autodetected;

  d->m_unexpandedDisplayName.fromMap(data, DISPLAYNAME_KEY);
  d->m_fileSystemFriendlyName = data.value(QLatin1String(FILESYSTEMFRIENDLYNAME_KEY)).toString();
  d->m_iconPath = FilePath::fromString(data.value(QLatin1String(ICON_KEY), d->m_iconPath.toString()).toString());
  d->m_deviceTypeForIcon = Id::fromSetting(data.value(DEVICE_TYPE_FOR_ICON_KEY));
  const auto it = data.constFind(IRRELEVANT_ASPECTS_KEY);
  if (it != data.constEnd())
    d->m_irrelevantAspects = transform<QSet<Id>>(it.value().toList(), &Id::fromSetting);

  const auto extra = data.value(QLatin1String(DATA_KEY)).toMap();
  d->m_data.clear(); // remove default values
  const auto cend = extra.constEnd();
  for (auto it = extra.constBegin(); it != cend; ++it)
    d->m_data.insert(Id::fromString(it.key()), it.value());

  auto mutableInfoList = data.value(QLatin1String(MUTABLE_INFO_KEY)).toStringList();
  foreach(const QString &mutableInfo, mutableInfoList)
    d->m_mutable.insert(Id::fromString(mutableInfo));

  auto stickyInfoList = data.value(QLatin1String(STICKY_INFO_KEY)).toStringList();
  foreach(const QString &stickyInfo, stickyInfoList)
    d->m_sticky.insert(Id::fromString(stickyInfo));
}

Kit::~Kit() = default;

auto Kit::blockNotification() -> void
{
  ++d->m_nestedBlockingLevel;
}

auto Kit::unblockNotification() -> void
{
  --d->m_nestedBlockingLevel;
  if (d->m_nestedBlockingLevel > 0)
    return;
  if (d->m_mustNotify)
    kitUpdated();
}

auto Kit::copyKitCommon(Kit *target, const Kit *source) -> void
{
  target->d->m_data = source->d->m_data;
  target->d->m_iconPath = source->d->m_iconPath;
  target->d->m_deviceTypeForIcon = source->d->m_deviceTypeForIcon;
  target->d->m_cachedIcon = source->d->m_cachedIcon;
  target->d->m_sticky = source->d->m_sticky;
  target->d->m_mutable = source->d->m_mutable;
  target->d->m_irrelevantAspects = source->d->m_irrelevantAspects;
  target->d->m_hasValidityInfo = false;
}

auto Kit::clone(bool keepName) const -> Kit*
{
  const auto k = new Kit;
  copyKitCommon(k, this);
  if (keepName)
    k->d->m_unexpandedDisplayName = d->m_unexpandedDisplayName;
  else
    k->d->m_unexpandedDisplayName.setValue(newKitName(KitManager::kits()));
  k->d->m_autodetected = false;
  // Do not clone m_fileSystemFriendlyName, needs to be unique
  k->d->m_hasError = d->m_hasError; // TODO: Is this intentionally not done for copyFrom()?
  return k;
}

auto Kit::copyFrom(const Kit *k) -> void
{
  copyKitCommon(this, k);
  d->m_autodetected = k->d->m_autodetected;
  d->m_autoDetectionSource = k->d->m_autoDetectionSource;
  d->m_unexpandedDisplayName = k->d->m_unexpandedDisplayName;
  d->m_fileSystemFriendlyName = k->d->m_fileSystemFriendlyName;
}

auto Kit::isValid() const -> bool
{
  if (!d->m_id.isValid())
    return false;

  if (!d->m_hasValidityInfo)
    validate();

  return !d->m_hasError;
}

auto Kit::hasWarning() const -> bool
{
  if (!d->m_hasValidityInfo)
    validate();

  return d->m_hasWarning;
}

auto Kit::validate() const -> Tasks
{
  Tasks result;
  for (const auto aspect : KitManager::kitAspects())
    result.append(aspect->validate(this));

  d->m_hasError = containsType(result, Task::TaskType::Error);
  d->m_hasWarning = containsType(result, Task::TaskType::Warning);

  sort(result);
  d->m_hasValidityInfo = true;
  return result;
}

auto Kit::fix() -> void
{
  KitGuard g(this);
  for (const auto aspect : KitManager::kitAspects())
    aspect->fix(this);
}

auto Kit::setup() -> void
{
  KitGuard g(this);
  const auto aspects = KitManager::kitAspects();
  for (const auto aspect : aspects)
    aspect->setup(this);
}

auto Kit::upgrade() -> void
{
  KitGuard g(this);
  // Process the KitAspects in reverse order: They may only be based on other information
  // lower in the stack.
  for (const auto aspect : KitManager::kitAspects())
    aspect->upgrade(this);
}

auto Kit::unexpandedDisplayName() const -> QString
{
  return d->m_unexpandedDisplayName.value();
}

auto Kit::displayName() const -> QString
{
  return d->m_macroExpander.expand(unexpandedDisplayName());
}

auto Kit::setUnexpandedDisplayName(const QString &name) -> void
{
  if (d->m_unexpandedDisplayName.setValue(name))
    kitUpdated();
}

auto Kit::setCustomFileSystemFriendlyName(const QString &fileSystemFriendlyName) -> void
{
  d->m_fileSystemFriendlyName = fileSystemFriendlyName;
}

auto Kit::customFileSystemFriendlyName() const -> QString
{
  return d->m_fileSystemFriendlyName;
}

auto Kit::fileSystemFriendlyName() const -> QString
{
  auto name = customFileSystemFriendlyName();
  if (name.isEmpty())
    name = FileUtils::qmakeFriendlyName(displayName());
  foreach(Kit *i, KitManager::kits()) {
    if (i == this)
      continue;
    if (name == FileUtils::qmakeFriendlyName(i->displayName())) {
      // append part of the kit id: That should be unique enough;-)
      // Leading { will be turned into _ which should be fine.
      name = FileUtils::qmakeFriendlyName(name + QLatin1Char('_') + (id().toString().left(7)));
      break;
    }
  }
  return name;
}

auto Kit::isAutoDetected() const -> bool
{
  return d->m_autodetected;
}

auto Kit::autoDetectionSource() const -> QString
{
  return d->m_autoDetectionSource;
}

auto Kit::isSdkProvided() const -> bool
{
  return d->m_sdkProvided;
}

auto Kit::id() const -> Id
{
  return d->m_id;
}

auto Kit::weight() const -> int
{
  const auto &aspects = KitManager::kitAspects();
  return std::accumulate(aspects.begin(), aspects.end(), 0, [this](int sum, const KitAspect *aspect) {
    return sum + aspect->weight(this);
  });
}

static auto iconForDeviceType(Id deviceType) -> QIcon
{
  const IDeviceFactory *factory = findOrDefault(IDeviceFactory::allDeviceFactories(), [&deviceType](const IDeviceFactory *factory) {
    return factory->deviceType() == deviceType;
  });
  return factory ? factory->icon() : QIcon();
}

auto Kit::icon() const -> QIcon
{
  if (!d->m_cachedIcon.isNull())
    return d->m_cachedIcon;

  if (!d->m_deviceTypeForIcon.isValid() && !d->m_iconPath.isEmpty() && d->m_iconPath.exists()) {
    d->m_cachedIcon = QIcon(d->m_iconPath.toString());
    return d->m_cachedIcon;
  }

  const auto deviceType = d->m_deviceTypeForIcon.isValid() ? d->m_deviceTypeForIcon : DeviceTypeKitAspect::deviceTypeId(this);
  const auto deviceTypeIcon = iconForDeviceType(deviceType);
  if (!deviceTypeIcon.isNull()) {
    d->m_cachedIcon = deviceTypeIcon;
    return d->m_cachedIcon;
  }

  d->m_cachedIcon = iconForDeviceType(Constants::DESKTOP_DEVICE_TYPE);
  return d->m_cachedIcon;
}

auto Kit::displayIcon() const -> QIcon
{
  auto result = icon();
  if (hasWarning()) {
    static const auto warningIcon(Icons::WARNING.icon());
    result = warningIcon;
  }
  if (!isValid()) {
    static const auto errorIcon(Icons::CRITICAL.icon());
    result = errorIcon;
  }
  return result;
}

auto Kit::iconPath() const -> FilePath
{
  return d->m_iconPath;
}

auto Kit::setIconPath(const FilePath &path) -> void
{
  if (d->m_iconPath == path)
    return;
  d->m_deviceTypeForIcon = Id();
  d->m_iconPath = path;
  kitUpdated();
}

auto Kit::setDeviceTypeForIcon(Id deviceType) -> void
{
  if (d->m_deviceTypeForIcon == deviceType)
    return;
  d->m_iconPath.clear();
  d->m_deviceTypeForIcon = deviceType;
  kitUpdated();
}

auto Kit::allKeys() const -> QList<Id>
{
  return d->m_data.keys();
}

auto Kit::value(Id key, const QVariant &unset) const -> QVariant
{
  return d->m_data.value(key, unset);
}

auto Kit::hasValue(Id key) const -> bool
{
  return d->m_data.contains(key);
}

auto Kit::setValue(Id key, const QVariant &value) -> void
{
  if (d->m_data.value(key) == value)
    return;
  d->m_data.insert(key, value);
  kitUpdated();
}

/// \internal
auto Kit::setValueSilently(Id key, const QVariant &value) -> void
{
  if (d->m_data.value(key) == value)
    return;
  d->m_data.insert(key, value);
}

/// \internal
auto Kit::removeKeySilently(Id key) -> void
{
  if (!d->m_data.contains(key))
    return;
  d->m_data.remove(key);
  d->m_sticky.remove(key);
  d->m_mutable.remove(key);
}

auto Kit::removeKey(Id key) -> void
{
  if (!d->m_data.contains(key))
    return;
  d->m_data.remove(key);
  d->m_sticky.remove(key);
  d->m_mutable.remove(key);
  kitUpdated();
}

auto Kit::isSticky(Id id) const -> bool
{
  return d->m_sticky.contains(id);
}

auto Kit::isDataEqual(const Kit *other) const -> bool
{
  return d->m_data == other->d->m_data;
}

auto Kit::isEqual(const Kit *other) const -> bool
{
  return isDataEqual(other) && d->m_iconPath == other->d->m_iconPath && d->m_deviceTypeForIcon == other->d->m_deviceTypeForIcon && d->m_unexpandedDisplayName == other->d->m_unexpandedDisplayName && d->m_fileSystemFriendlyName == other->d->m_fileSystemFriendlyName && d->m_irrelevantAspects == other->d->m_irrelevantAspects && d->m_mutable == other->d->m_mutable;
}

auto Kit::toMap() const -> QVariantMap
{
  using IdVariantConstIt = QHash<Id, QVariant>::ConstIterator;

  QVariantMap data;
  d->m_unexpandedDisplayName.toMap(data, DISPLAYNAME_KEY);
  data.insert(QLatin1String(ID_KEY), QString::fromLatin1(d->m_id.name()));
  data.insert(QLatin1String(AUTODETECTED_KEY), d->m_autodetected);
  if (!d->m_fileSystemFriendlyName.isEmpty())
    data.insert(QLatin1String(FILESYSTEMFRIENDLYNAME_KEY), d->m_fileSystemFriendlyName);
  data.insert(QLatin1String(AUTODETECTIONSOURCE_KEY), d->m_autoDetectionSource);
  data.insert(QLatin1String(SDK_PROVIDED_KEY), d->m_sdkProvided);
  data.insert(QLatin1String(ICON_KEY), d->m_iconPath.toString());
  data.insert(DEVICE_TYPE_FOR_ICON_KEY, d->m_deviceTypeForIcon.toSetting());

  QStringList mutableInfo;
  foreach(Id id, d->m_mutable)
    mutableInfo << id.toString();
  data.insert(QLatin1String(MUTABLE_INFO_KEY), mutableInfo);

  QStringList stickyInfo;
  foreach(Id id, d->m_sticky)
    stickyInfo << id.toString();
  data.insert(QLatin1String(STICKY_INFO_KEY), stickyInfo);

  if (d->m_irrelevantAspects) {
    data.insert(IRRELEVANT_ASPECTS_KEY, transform<QVariantList>(d->m_irrelevantAspects.value(), &Id::toSetting));
  }

  QVariantMap extra;

  const auto cend = d->m_data.constEnd();
  for (auto it = d->m_data.constBegin(); it != cend; ++it)
    extra.insert(QString::fromLatin1(it.key().name().constData()), it.value());
  data.insert(QLatin1String(DATA_KEY), extra);

  return data;
}

auto Kit::addToBuildEnvironment(Environment &env) const -> void
{
  for (const auto aspect : KitManager::kitAspects())
    aspect->addToBuildEnvironment(this, env);
}

auto Kit::addToRunEnvironment(Environment &env) const -> void
{
  for (const auto aspect : KitManager::kitAspects())
    aspect->addToRunEnvironment(this, env);
}

auto Kit::buildEnvironment() const -> Environment
{
  auto env = Environment::systemEnvironment(); // FIXME: Use build device
  addToBuildEnvironment(env);
  return env;
}

auto Kit::runEnvironment() const -> Environment
{
  auto env = Environment::systemEnvironment(); // FIXME: Use run device
  addToRunEnvironment(env);
  return env;
}

auto Kit::createOutputParsers() const -> QList<OutputLineParser*>
{
  QList<OutputLineParser*> parsers{new OsParser};
  for (const auto aspect : KitManager::kitAspects())
    parsers << aspect->createOutputParsers(this);
  return parsers;
}

auto Kit::toHtml(const Tasks &additional, const QString &extraText) const -> QString
{
  QString result;
  QTextStream str(&result);
  str << "<html><body>";
  str << "<h3>" << displayName() << "</h3>";

  if (!extraText.isEmpty())
    str << "<p>" << extraText << "</p>";

  if (!isValid() || hasWarning() || !additional.isEmpty())
    str << "<p>" << ProjectExplorer::toHtml(additional + validate()) << "</p>";

  str << "<table>";
  for (const auto aspect : KitManager::kitAspects()) {
    const auto list = aspect->toUserOutput(this);
    for (const auto &j : list) {
      auto contents = j.second;
      if (contents.count() > 256) {
        int pos = contents.lastIndexOf("<br>", 256);
        if (pos < 0) // no linebreak, so cut early.
          pos = 80;
        contents = contents.mid(0, pos);
        contents += "&lt;...&gt;";
      }
      str << "<tr><td><b>" << j.first << ":</b></td><td>" << contents << "</td></tr>";
    }
  }
  str << "</table></body></html>";
  return result;
}

auto Kit::setAutoDetected(bool detected) -> void
{
  if (d->m_autodetected == detected)
    return;
  d->m_autodetected = detected;
  kitUpdated();
}

auto Kit::setAutoDetectionSource(const QString &autoDetectionSource) -> void
{
  if (d->m_autoDetectionSource == autoDetectionSource)
    return;
  d->m_autoDetectionSource = autoDetectionSource;
  kitUpdated();
}

auto Kit::setSdkProvided(bool sdkProvided) -> void
{
  if (d->m_sdkProvided == sdkProvided)
    return;
  d->m_sdkProvided = sdkProvided;
  kitUpdated();
}

auto Kit::makeSticky() -> void
{
  for (const auto aspect : KitManager::kitAspects()) {
    if (hasValue(aspect->id()))
      setSticky(aspect->id(), true);
  }
}

auto Kit::setSticky(Id id, bool b) -> void
{
  if (d->m_sticky.contains(id) == b)
    return;

  if (b)
    d->m_sticky.insert(id);
  else
    d->m_sticky.remove(id);
  kitUpdated();
}

auto Kit::makeUnSticky() -> void
{
  if (d->m_sticky.isEmpty())
    return;
  d->m_sticky.clear();
  kitUpdated();
}

auto Kit::setMutable(Id id, bool b) -> void
{
  if (d->m_mutable.contains(id) == b)
    return;

  if (b)
    d->m_mutable.insert(id);
  else
    d->m_mutable.remove(id);
  kitUpdated();
}

auto Kit::isMutable(Id id) const -> bool
{
  return d->m_mutable.contains(id);
}

auto Kit::setIrrelevantAspects(const QSet<Id> &irrelevant) -> void
{
  d->m_irrelevantAspects = irrelevant;
}

auto Kit::irrelevantAspects() const -> QSet<Id>
{
  return d->m_irrelevantAspects.value_or(KitManager::irrelevantAspects());
}

auto Kit::supportedPlatforms() const -> QSet<Id>
{
  QSet<Id> platforms;
  for (const KitAspect *aspect : KitManager::kitAspects()) {
    const auto ip = aspect->supportedPlatforms(this);
    if (ip.isEmpty())
      continue;
    if (platforms.isEmpty())
      platforms = ip;
    else
      platforms.intersect(ip);
  }
  return platforms;
}

auto Kit::availableFeatures() const -> QSet<Id>
{
  QSet<Id> features;
  for (const KitAspect *aspect : KitManager::kitAspects())
    features |= aspect->availableFeatures(this);
  return features;
}

auto Kit::hasFeatures(const QSet<Id> &features) const -> bool
{
  return availableFeatures().contains(features);
}

auto Kit::macroExpander() const -> MacroExpander*
{
  return &d->m_macroExpander;
}

auto Kit::newKitName(const QList<Kit*> &allKits) const -> QString
{
  return newKitName(unexpandedDisplayName(), allKits);
}

auto Kit::newKitName(const QString &name, const QList<Kit*> &allKits) -> QString
{
  const auto baseName = name.isEmpty() ? QCoreApplication::translate("ProjectExplorer::Kit", "Unnamed") : QCoreApplication::translate("ProjectExplorer::Kit", "Clone of %1").arg(name);
  return makeUniquelyNumbered(baseName, transform(allKits, &Kit::unexpandedDisplayName));
}

auto Kit::kitUpdated() -> void
{
  if (d->m_nestedBlockingLevel > 0) {
    d->m_mustNotify = true;
    return;
  }
  d->m_hasValidityInfo = false;
  d->m_cachedIcon = QIcon();
  KitManager::notifyAboutUpdate(this);
  d->m_mustNotify = false;
}

static auto replacementKey() -> Id { return "IsReplacementKit"; }

auto Kit::makeReplacementKit() -> void
{
  setValueSilently(replacementKey(), true);
}

auto Kit::isReplacementKit() const -> bool
{
  return value(replacementKey()).toBool();
}

} // namespace ProjectExplorer
