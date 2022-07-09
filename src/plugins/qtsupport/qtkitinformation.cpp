// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtkitinformation.hpp"

#include "qtsupportconstants.hpp"
#include "qtversionmanager.hpp"
#include "qtparser.hpp"
#include "qttestparser.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/task.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/buildablehelperlibrary.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcassert.hpp>

#include <QComboBox>

using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {
namespace Internal {

class QtKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(QtSupport::QtKitAspectWidget)

public:
  QtKitAspectWidget(Kit *k, const KitAspect *ki) : KitAspectWidget(k, ki)
  {
    m_combo = createSubWidget<QComboBox>();
    m_combo->setSizePolicy(QSizePolicy::Ignored, m_combo->sizePolicy().verticalPolicy());
    m_combo->addItem(tr("None"), -1);

    const auto versionIds = transform(QtVersionManager::versions(), &QtVersion::uniqueId);
    versionsChanged(versionIds, QList<int>(), QList<int>());

    m_manageButton = createManageButton(Constants::QTVERSION_SETTINGS_PAGE_ID);

    refresh();
    m_combo->setToolTip(ki->description());

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtKitAspectWidget::currentWasChanged);

    connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &QtKitAspectWidget::versionsChanged);
  }

  ~QtKitAspectWidget() final
  {
    delete m_combo;
    delete m_manageButton;
  }

private:
  auto makeReadOnly() -> void final { m_combo->setEnabled(false); }

  auto addToLayout(LayoutBuilder &builder) -> void
  {
    addMutableAction(m_combo);
    builder.addItem(m_combo);
    builder.addItem(m_manageButton);
  }

  auto refresh() -> void final
  {
    m_combo->setCurrentIndex(findQtVersion(QtKitAspect::qtVersionId(m_kit)));
  }

private:
  static auto itemNameFor(const QtVersion *v) -> QString
  {
    QTC_ASSERT(v, return QString());
    auto name = v->displayName();
    if (!v->isValid())
      name = tr("%1 (invalid)").arg(v->displayName());
    return name;
  }

  auto versionsChanged(const QList<int> &added, const QList<int> &removed, const QList<int> &changed) -> void
  {
    for (const auto id : added) {
      const auto v = QtVersionManager::version(id);
      QTC_CHECK(v);
      QTC_CHECK(findQtVersion(id) < 0);
      m_combo->addItem(itemNameFor(v), id);
    }
    for (const auto id : removed) {
      const auto pos = findQtVersion(id);
      if (pos >= 0) // We do not include invalid Qt versions, so do not try to remove those.
        m_combo->removeItem(pos);
    }
    for (const auto id : changed) {
      const auto v = QtVersionManager::version(id);
      const auto pos = findQtVersion(id);
      QTC_CHECK(pos >= 0);
      m_combo->setItemText(pos, itemNameFor(v));
    }
  }

  auto currentWasChanged(int idx) -> void
  {
    QtKitAspect::setQtVersionId(m_kit, m_combo->itemData(idx).toInt());
  }

  auto findQtVersion(const int id) const -> int
  {
    for (auto i = 0; i < m_combo->count(); ++i) {
      if (id == m_combo->itemData(i).toInt())
        return i;
    }
    return -1;
  }

  QComboBox *m_combo;
  QWidget *m_manageButton;
};
} // namespace Internal

QtKitAspect::QtKitAspect()
{
  setObjectName(QLatin1String("QtKitAspect"));
  setId(id());
  setDisplayName(tr("Qt version"));
  setDescription(tr("The Qt library to use for all projects using this kit.<br>" "A Qt version is required for qmake-based projects " "and optional when using other build systems."));
  setPriority(26000);

  connect(KitManager::instance(), &KitManager::kitsLoaded, this, &QtKitAspect::kitsWereLoaded);
}

auto QtKitAspect::setup(Kit *k) -> void
{
  if (!k || k->hasValue(id()))
    return;
  const auto tcAbi = ToolChainKitAspect::targetAbi(k);
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);

  const auto matches = QtVersionManager::versions([&tcAbi, &deviceType](const QtVersion *qt) {
    return qt->targetDeviceTypes().contains(deviceType) && contains(qt->qtAbis(), [&tcAbi](const Abi &qtAbi) {
      return qtAbi.isCompatibleWith(tcAbi);
    });
  });
  if (matches.empty())
    return;

  // An MSVC 2015 toolchain is compatible with an MSVC 2017 Qt, but we prefer an
  // MSVC 2015 Qt if we find one.
  const auto exactMatches = filtered(matches, [&tcAbi](const QtVersion *qt) {
    return qt->qtAbis().contains(tcAbi);
  });
  const auto &candidates = !exactMatches.empty() ? exactMatches : matches;

  const auto qtFromPath = QtVersionManager::version(equal(&QtVersion::detectionSource, QString("PATH")));
  if (qtFromPath && candidates.contains(qtFromPath))
    k->setValue(id(), qtFromPath->uniqueId());
  else
    k->setValue(id(), candidates.first()->uniqueId());
}

auto QtKitAspect::validate(const Kit *k) const -> Tasks
{
  QTC_ASSERT(QtVersionManager::isLoaded(), return {});
  const auto version = qtVersion(k);
  if (!version)
    return {};

  return version->validateKit(k);
}

auto QtKitAspect::fix(Kit *k) -> void
{
  QTC_ASSERT(QtVersionManager::isLoaded(), return);
  auto version = qtVersion(k);
  if (!version) {
    if (qtVersionId(k) >= 0) {
      qWarning("Qt version is no longer known, removing from kit \"%s\".", qPrintable(k->displayName()));
      setQtVersionId(k, -1);
    }
    return;
  }

  // Set a matching toolchain if we don't have one.
  if (ToolChainKitAspect::cxxToolChain(k))
    return;

  const auto spec = version->mkspec();
  auto possibleTcs = ToolChainManager::toolchains([version](const ToolChain *t) {
    if (!t->isValid() || t->language() != ProjectExplorer::Constants::CXX_LANGUAGE_ID)
      return false;
    return anyOf(version->qtAbis(), [t](const Abi &qtAbi) {
      return t->supportedAbis().contains(qtAbi) && t->targetAbi().wordWidth() == qtAbi.wordWidth() && t->targetAbi().architecture() == qtAbi.architecture();
    });
  });
  if (!possibleTcs.isEmpty()) {
    // Prefer exact matches.
    // TODO: We should probably prefer the compiler with the highest version number instead,
    //       but this information is currently not exposed by the ToolChain class.
    sort(possibleTcs, [version](const ToolChain *tc1, const ToolChain *tc2) {
      const auto &qtAbis = version->qtAbis();
      const auto tc1ExactMatch = qtAbis.contains(tc1->targetAbi());
      const auto tc2ExactMatch = qtAbis.contains(tc2->targetAbi());
      if (tc1ExactMatch && !tc2ExactMatch)
        return true;
      if (!tc1ExactMatch && tc2ExactMatch)
        return false;
      return tc1->priority() > tc2->priority();
    });

    const auto goodTcs = filtered(possibleTcs, [&spec](const ToolChain *t) {
      return t->suggestedMkspecList().contains(spec);
    });
    // Hack to prefer a tool chain from PATH (e.g. autodetected) over other matches.
    // This improves the situation a bit if a cross-compilation tool chain has the
    // same ABI as the host.
    const auto systemEnvironment = Environment::systemEnvironment();
    auto bestTc = findOrDefault(goodTcs, [&systemEnvironment](const ToolChain *t) {
      return systemEnvironment.path().contains(t->compilerCommand().parentDir());
    });
    if (!bestTc) {
      bestTc = goodTcs.isEmpty() ? possibleTcs.first() : goodTcs.first();
    }
    if (bestTc)
      ToolChainKitAspect::setAllToolChainsToMatch(k, bestTc);
  }
}

auto QtKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::QtKitAspectWidget(k, this);
}

auto QtKitAspect::displayNamePostfix(const Kit *k) const -> QString
{
  const auto version = qtVersion(k);
  return version ? version->displayName() : QString();
}

auto QtKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  const auto version = qtVersion(k);
  return {{tr("Qt version"), version ? version->displayName() : tr("None")}};
}

auto QtKitAspect::addToBuildEnvironment(const Kit *k, Environment &env) const -> void
{
  const auto version = qtVersion(k);
  if (version)
    version->addToEnvironment(k, env);
}

auto QtKitAspect::createOutputParsers(const Kit *k) const -> QList<OutputLineParser*>
{
  if (qtVersion(k))
    return {new Internal::QtTestParser, new QtParser};
  return {};
}

class QtMacroSubProvider {
public:
  QtMacroSubProvider(Kit *kit) : expander(QtVersion::createMacroExpander([kit] { return QtKitAspect::qtVersion(kit); })) {}

  auto operator()() const -> MacroExpander*
  {
    return expander.get();
  }

  std::shared_ptr<MacroExpander> expander;
};

auto QtKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  QTC_ASSERT(kit, return);
  expander->registerSubProvider(QtMacroSubProvider(kit));

  expander->registerVariable("Qt:Name", tr("Name of Qt Version"), [kit]() -> QString {
    const auto version = qtVersion(kit);
    return version ? version->displayName() : tr("unknown");
  });
  expander->registerVariable("Qt:qmakeExecutable", tr("Path to the qmake executable"), [kit]() -> QString {
    const auto version = qtVersion(kit);
    return version ? version->qmakeFilePath().path() : QString();
  });
}

auto QtKitAspect::id() -> Id
{
  return "QtSupport.QtInformation";
}

auto QtKitAspect::qtVersionId(const Kit *k) -> int
{
  if (!k)
    return -1;

  auto id = -1;
  const auto data = k->value(QtKitAspect::id(), -1);
  if (data.type() == QVariant::Int) {
    bool ok;
    id = data.toInt(&ok);
    if (!ok)
      id = -1;
  } else {
    auto source = data.toString();
    const auto v = QtVersionManager::version([source](const QtVersion *v) { return v->detectionSource() == source; });
    if (v)
      id = v->uniqueId();
  }
  return id;
}

auto QtKitAspect::setQtVersionId(Kit *k, const int id) -> void
{
  QTC_ASSERT(k, return);
  k->setValue(QtKitAspect::id(), id);
}

auto QtKitAspect::qtVersion(const Kit *k) -> QtVersion*
{
  return QtVersionManager::version(qtVersionId(k));
}

auto QtKitAspect::setQtVersion(Kit *k, const QtVersion *v) -> void
{
  if (!v)
    setQtVersionId(k, -1);
  else
    setQtVersionId(k, v->uniqueId());
}

/*!
 * Helper function that prepends the directory containing the C++ toolchain and Qt
 * binaries to PATH. This is used to in build configurations targeting broken build
 * systems to provide hints about which binaries to use.
 */

auto QtKitAspect::addHostBinariesToPath(const Kit *k, Environment &env) -> void
{
  if (const ToolChain *tc = ToolChainKitAspect::cxxToolChain(k))
    env.prependOrSetPath(tc->compilerCommand().parentDir());

  if (const QtVersion *qt = qtVersion(k))
    env.prependOrSetPath(qt->hostBinPath());
}

auto QtKitAspect::qtVersionsChanged(const QList<int> &addedIds, const QList<int> &removedIds, const QList<int> &changedIds) -> void
{
  Q_UNUSED(addedIds)
  Q_UNUSED(removedIds)
  for (const auto k : KitManager::kits()) {
    if (changedIds.contains(qtVersionId(k))) {
      k->validate(); // Qt version may have become (in)valid
      notifyAboutUpdate(k);
    }
  }
}

auto QtKitAspect::kitsWereLoaded() -> void
{
  for (const auto k : KitManager::kits())
    fix(k);

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &QtKitAspect::qtVersionsChanged);
}

auto QtKitAspect::platformPredicate(Id platform) -> Kit::Predicate
{
  return [platform](const Kit *kit) -> bool {
    const auto version = qtVersion(kit);
    return version && version->targetDeviceTypes().contains(platform);
  };
}

auto QtKitAspect::qtVersionPredicate(const QSet<Id> &required, const QtVersionNumber &min, const QtVersionNumber &max) -> Kit::Predicate
{
  return [required, min, max](const Kit *kit) -> bool {
    const auto version = qtVersion(kit);
    if (!version)
      return false;
    const auto current = version->qtVersion();
    if (min.majorVersion > -1 && current < min)
      return false;
    if (max.majorVersion > -1 && current > max)
      return false;
    return version->features().contains(required);
  };
}

auto QtKitAspect::supportedPlatforms(const Kit *k) const -> QSet<Id>
{
  const auto version = qtVersion(k);
  return version ? version->targetDeviceTypes() : QSet<Id>();
}

auto QtKitAspect::availableFeatures(const Kit *k) const -> QSet<Id>
{
  const auto version = qtVersion(k);
  return version ? version->features() : QSet<Id>();
}

auto QtKitAspect::weight(const Kit *k) const -> int
{
  const QtVersion *const qt = qtVersion(k);
  if (!qt)
    return 0;
  if (!qt->targetDeviceTypes().contains(DeviceTypeKitAspect::deviceTypeId(k)))
    return 0;
  const auto tcAbi = ToolChainKitAspect::targetAbi(k);
  if (qt->qtAbis().contains(tcAbi))
    return 2;
  return contains(qt->qtAbis(), [&tcAbi](const Abi &qtAbi) {
    return qtAbi.isCompatibleWith(tcAbi);
  }) ? 1 : 0;
}

auto SuppliesQtQuickImportPath::id() -> Id
{
  return Constants::FLAGS_SUPPLIES_QTQUICK_IMPORT_PATH;
}

auto KitQmlImportPath::id() -> Id
{
  return Constants::KIT_QML_IMPORT_PATH;
}

auto KitHasMergedHeaderPathsWithQmlImportPaths::id() -> Id
{
  return Constants::KIT_HAS_MERGED_HEADER_PATHS_WITH_QML_IMPORT_PATHS;
}

} // namespace QtSupport
