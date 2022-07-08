// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitinformation.hpp"

#include "abi.hpp"
#include "devicesupport/desktopdevice.hpp"
#include "devicesupport/devicemanager.hpp"
#include "devicesupport/devicemanagermodel.hpp"
#include "devicesupport/idevicefactory.hpp"
#include "projectexplorerconstants.hpp"
#include "kit.hpp"
#include "toolchain.hpp"
#include "toolchainmanager.hpp"

#include <docker/dockerconstants.h>

#include <ssh/sshconnection.h>

#include <utils/algorithm.hpp>
#include <utils/elidinglabel.hpp>
#include <utils/environment.hpp>
#include <utils/environmentdialog.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/macroexpander.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/variablechooser.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {

constexpr char KITINFORMATION_ID_V1[] = "PE.Profile.ToolChain";
constexpr char KITINFORMATION_ID_V2[] = "PE.Profile.ToolChains";
constexpr char KITINFORMATION_ID_V3[] = "PE.Profile.ToolChainsV3";

// --------------------------------------------------------------------------
// SysRootKitAspect:
// --------------------------------------------------------------------------

namespace Internal {

class SysRootKitAspectWidget : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::SysRootKitAspect)
public:
  SysRootKitAspectWidget(Kit *k, const KitAspect *ki) : KitAspectWidget(k, ki)
  {
    m_chooser = createSubWidget<PathChooser>();
    m_chooser->setExpectedKind(PathChooser::ExistingDirectory);
    m_chooser->setHistoryCompleter(QLatin1String("PE.SysRoot.History"));
    m_chooser->setFilePath(SysRootKitAspect::sysRoot(k));
    connect(m_chooser, &PathChooser::pathChanged, this, &SysRootKitAspectWidget::pathWasChanged);
  }

  ~SysRootKitAspectWidget() override { delete m_chooser; }

private:
  auto makeReadOnly() -> void override { m_chooser->setReadOnly(true); }

  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_chooser);
    builder.addItem(Layouting::Span(2, m_chooser));
  }

  auto refresh() -> void override
  {
    if (!m_ignoreChange)
      m_chooser->setFilePath(SysRootKitAspect::sysRoot(m_kit));
  }

  auto pathWasChanged() -> void
  {
    m_ignoreChange = true;
    SysRootKitAspect::setSysRoot(m_kit, m_chooser->filePath());
    m_ignoreChange = false;
  }

  PathChooser *m_chooser;
  bool m_ignoreChange = false;
};
} // namespace Internal

SysRootKitAspect::SysRootKitAspect()
{
  setObjectName(QLatin1String("SysRootInformation"));
  setId(id());
  setDisplayName(tr("Sysroot"));
  setDescription(tr("The root directory of the system image to use.<br>" "Leave empty when building for the desktop."));
  setPriority(31000);
}

auto SysRootKitAspect::validate(const Kit *k) const -> Tasks
{
  Tasks result;
  const auto dir = sysRoot(k);
  if (dir.isEmpty())
    return result;

  if (dir.startsWith("target:") || dir.startsWith("remote:"))
    return result;

  if (!dir.exists()) {
    result << BuildSystemTask(Task::Warning, tr("Sys Root \"%1\" does not exist in the file system.").arg(dir.toUserOutput()));
  } else if (!dir.isDir()) {
    result << BuildSystemTask(Task::Warning, tr("Sys Root \"%1\" is not a directory.").arg(dir.toUserOutput()));
  } else if (dir.dirEntries(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
    result << BuildSystemTask(Task::Warning, tr("Sys Root \"%1\" is empty.").arg(dir.toUserOutput()));
  }
  return result;
}

auto SysRootKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);

  return new Internal::SysRootKitAspectWidget(k, this);
}

auto SysRootKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  return {{tr("Sys Root"), sysRoot(k).toUserOutput()}};
}

auto SysRootKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  QTC_ASSERT(kit, return);

  expander->registerFileVariables("SysRoot", tr("Sys Root"), [kit] {
    return sysRoot(kit);
  });
}

auto SysRootKitAspect::id() -> Id
{
  return "PE.Profile.SysRoot";
}

auto SysRootKitAspect::sysRoot(const Kit *k) -> FilePath
{
  if (!k)
    return FilePath();

  if (!k->value(id()).toString().isEmpty())
    return FilePath::fromString(k->value(id()).toString());

  for (const auto tc : ToolChainKitAspect::toolChains(k)) {
    if (!tc->sysRoot().isEmpty())
      return FilePath::fromString(tc->sysRoot());
  }

  return FilePath();
}

auto SysRootKitAspect::setSysRoot(Kit *k, const FilePath &v) -> void
{
  if (!k)
    return;

  for (const auto tc : ToolChainKitAspect::toolChains(k)) {
    if (!tc->sysRoot().isEmpty()) {
      // It's the sysroot from toolchain, don't set it.
      if (tc->sysRoot() == v.toString())
        return;

      // We've changed the default toolchain sysroot, set it.
      break;
    }
  }
  k->setValue(id(), v.toString());
}

// --------------------------------------------------------------------------
// ToolChainKitAspect:
// --------------------------------------------------------------------------

namespace Internal {

class ToolChainKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::ToolChainKitAspect)
public:
  ToolChainKitAspectWidget(Kit *k, const KitAspect *ki) : KitAspectWidget(k, ki)
  {
    m_mainWidget = createSubWidget<QWidget>();
    m_mainWidget->setContentsMargins(0, 0, 0, 0);

    const auto layout = new QGridLayout(m_mainWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setColumnStretch(1, 2);

    auto languageList = ToolChainManager::allLanguages();
    sort(languageList, [](Id l1, Id l2) {
      return ToolChainManager::displayNameOfLanguageId(l1) < ToolChainManager::displayNameOfLanguageId(l2);
    });
    QTC_ASSERT(!languageList.isEmpty(), return);
    auto row = 0;
    for (auto l : qAsConst(languageList)) {
      layout->addWidget(new QLabel(ToolChainManager::displayNameOfLanguageId(l) + ':'), row, 0);
      auto cb = new QComboBox;
      cb->setSizePolicy(QSizePolicy::Ignored, cb->sizePolicy().verticalPolicy());
      cb->setToolTip(ki->description());

      m_languageComboboxMap.insert(l, cb);
      layout->addWidget(cb, row, 1);
      ++row;

      connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, l](int idx) { currentToolChainChanged(l, idx); });
    }

    refresh();

    m_manageButton = createManageButton(Constants::TOOLCHAIN_SETTINGS_PAGE_ID);
  }

  ~ToolChainKitAspectWidget() override
  {
    delete m_mainWidget;
    delete m_manageButton;
  }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_mainWidget);
    builder.addItem(m_mainWidget);
    builder.addItem(m_manageButton);
  }

  auto refresh() -> void override
  {
    m_ignoreChanges = true;
    foreach(Utils::Id l, m_languageComboboxMap.keys()) {
      const auto ltcList = ToolChainManager::toolchains(equal(&ToolChain::language, l));

      const auto cb = m_languageComboboxMap.value(l);
      cb->clear();
      cb->addItem(tr("<No compiler>"), QByteArray());

      foreach(ToolChain *tc, ltcList)
        cb->addItem(tc->displayName(), tc->id());

      cb->setEnabled(cb->count() > 1 && !m_isReadOnly);
      const auto index = indexOf(cb, ToolChainKitAspect::toolChain(m_kit, l));
      cb->setCurrentIndex(index);
    }
    m_ignoreChanges = false;
  }

  auto makeReadOnly() -> void override
  {
    m_isReadOnly = true;
    foreach(Utils::Id l, m_languageComboboxMap.keys()) {
      m_languageComboboxMap.value(l)->setEnabled(false);
    }
  }

  auto currentToolChainChanged(Id language, int idx) -> void
  {
    if (m_ignoreChanges || idx < 0)
      return;

    const auto id = m_languageComboboxMap.value(language)->itemData(idx).toByteArray();
    const auto tc = ToolChainManager::findToolChain(id);
    QTC_ASSERT(!tc || tc->language() == language, return);
    if (tc)
      ToolChainKitAspect::setToolChain(m_kit, tc);
    else
      ToolChainKitAspect::clearToolChain(m_kit, language);
  }

  auto indexOf(QComboBox *cb, const ToolChain *tc) -> int
  {
    const auto id = tc ? tc->id() : QByteArray();
    for (auto i = 0; i < cb->count(); ++i) {
      if (id == cb->itemData(i).toByteArray())
        return i;
    }
    return -1;
  }

  QWidget *m_mainWidget = nullptr;
  QWidget *m_manageButton = nullptr;
  QHash<Id, QComboBox*> m_languageComboboxMap;
  bool m_ignoreChanges = false;
  bool m_isReadOnly = false;
};
} // namespace Internal

ToolChainKitAspect::ToolChainKitAspect()
{
  setObjectName(QLatin1String("ToolChainInformation"));
  setId(id());
  setDisplayName(tr("Compiler"));
  setDescription(tr("The compiler to use for building.<br>" "Make sure the compiler will produce binaries compatible " "with the target device, Qt version and other libraries used."));
  setPriority(30000);

  connect(KitManager::instance(), &KitManager::kitsLoaded, this, &ToolChainKitAspect::kitsWereLoaded);
}

// language id -> tool chain id
static auto defaultToolChainIds() -> QMap<Id, QByteArray>
{
  QMap<Id, QByteArray> toolChains;
  const auto abi = Abi::hostAbi();
  const auto tcList = ToolChainManager::toolchains(equal(&ToolChain::targetAbi, abi));
  foreach(Utils::Id l, ToolChainManager::allLanguages()) {
    const auto tc = findOrDefault(tcList, equal(&ToolChain::language, l));
    toolChains.insert(l, tc ? tc->id() : QByteArray());
  }
  return toolChains;
}

static auto defaultToolChainValue() -> QVariant
{
  const auto toolChains = defaultToolChainIds();
  QVariantMap result;
  const auto end = toolChains.end();
  for (auto it = toolChains.begin(); it != end; ++it) {
    result.insert(it.key().toString(), it.value());
  }
  return result;
}

auto ToolChainKitAspect::validate(const Kit *k) const -> Tasks
{
  Tasks result;

  const auto tcList = toolChains(k);
  if (tcList.isEmpty()) {
    result << BuildSystemTask(Task::Warning, msgNoToolChainInTarget());
  } else {
    QSet<Abi> targetAbis;
    foreach(ToolChain *tc, tcList) {
      targetAbis.insert(tc->targetAbi());
      result << tc->validateKit(k);
    }
    if (targetAbis.count() != 1) {
      result << BuildSystemTask(Task::Error, tr("Compilers produce code for different ABIs: %1").arg(Utils::transform<QList>(targetAbis, &Abi::toString).join(", ")));
    }
  }
  return result;
}

auto ToolChainKitAspect::upgrade(Kit *k) -> void
{
  QTC_ASSERT(k, return);

  const Id oldIdV1 = KITINFORMATION_ID_V1;
  const Id oldIdV2 = KITINFORMATION_ID_V2;

  // upgrade <=4.1 to 4.2 (keep old settings around for now)
  {
    const auto oldValue = k->value(oldIdV1);
    const auto value = k->value(oldIdV2);
    if (value.isNull() && !oldValue.isNull()) {
      QVariantMap newValue;
      if (oldValue.type() == QVariant::Map) {
        // Used between 4.1 and 4.2:
        newValue = oldValue.toMap();
      } else {
        // Used up to 4.1:
        newValue.insert(languageId(Deprecated::Toolchain::Cxx), oldValue.toString());

        const auto typeId = DeviceTypeKitAspect::deviceTypeId(k);
        if (typeId == Constants::DESKTOP_DEVICE_TYPE) {
          // insert default C compiler which did not exist before
          newValue.insert(languageId(Deprecated::Toolchain::C), defaultToolChainIds().value(Id(Constants::C_LANGUAGE_ID)));
        }
      }
      k->setValue(oldIdV2, newValue);
      k->setSticky(oldIdV2, k->isSticky(oldIdV1));
    }
  }

  // upgrade 4.2 to 4.3 (keep old settings around for now)
  {
    const auto oldValue = k->value(oldIdV2);
    const auto value = k->value(id());
    if (value.isNull() && !oldValue.isNull()) {
      auto newValue = oldValue.toMap();
      auto it = newValue.find(languageId(Deprecated::Toolchain::C));
      if (it != newValue.end())
        newValue.insert(Id(Constants::C_LANGUAGE_ID).toString(), it.value());
      it = newValue.find(languageId(Deprecated::Toolchain::Cxx));
      if (it != newValue.end())
        newValue.insert(Id(Constants::CXX_LANGUAGE_ID).toString(), it.value());
      k->setValue(id(), newValue);
      k->setSticky(id(), k->isSticky(oldIdV2));
    }
  }

  // upgrade 4.3-temporary-master-state to 4.3:
  {
    const auto valueMap = k->value(id()).toMap();
    QVariantMap result;
    for (const auto &key : valueMap.keys()) {
      const int pos = key.lastIndexOf('.');
      if (pos >= 0)
        result.insert(key.mid(pos + 1), valueMap.value(key));
      else
        result.insert(key, valueMap.value(key));
    }
    k->setValue(id(), result);
  }
}

auto ToolChainKitAspect::fix(Kit *k) -> void
{
  QTC_ASSERT(ToolChainManager::isLoaded(), return);
  foreach(const Utils::Id& l, ToolChainManager::allLanguages()) {
    const auto tcId = toolChainId(k, l);
    if (!tcId.isEmpty() && !ToolChainManager::findToolChain(tcId)) {
      qWarning("Tool chain set up in kit \"%s\" for \"%s\" not found.", qPrintable(k->displayName()), qPrintable(ToolChainManager::displayNameOfLanguageId(l)));
      clearToolChain(k, l); // make sure to clear out no longer known tool chains
    }
  }
}

static auto findLanguage(const QString &ls) -> Id
{
  auto lsUpper = ls.toUpper();
  return findOrDefault(ToolChainManager::allLanguages(), [lsUpper](Id l) { return lsUpper == l.toString().toUpper(); });
}

auto ToolChainKitAspect::setup(Kit *k) -> void
{
  QTC_ASSERT(ToolChainManager::isLoaded(), return);
  QTC_ASSERT(k, return);

  auto value = k->value(id()).toMap();
  auto lockToolchains = k->isSdkProvided() && !value.isEmpty();
  if (value.empty())
    value = defaultToolChainValue().toMap();

  for (auto i = value.constBegin(); i != value.constEnd(); ++i) {
    auto l = findLanguage(i.key());

    if (!l.isValid()) {
      lockToolchains = false;
      continue;
    }

    const auto id = i.value().toByteArray();
    const auto tc = ToolChainManager::findToolChain(id);
    if (tc)
      continue;

    // ID is not found: Might be an ABI string...
    lockToolchains = false;
    const auto abi = QString::fromUtf8(id);
    const auto possibleTcs = ToolChainManager::toolchains([abi, l](const ToolChain *t) {
      return t->targetAbi().toString() == abi && t->language() == l;
    });
    ToolChain *bestTc = nullptr;
    for (const auto tc : possibleTcs) {
      if (!bestTc || tc->priority() > bestTc->priority())
        bestTc = tc;
    }
    if (bestTc)
      setToolChain(k, bestTc);
    else
      clearToolChain(k, l);
  }

  k->setSticky(id(), lockToolchains);
}

auto ToolChainKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::ToolChainKitAspectWidget(k, this);
}

auto ToolChainKitAspect::displayNamePostfix(const Kit *k) const -> QString
{
  const auto tc = cxxToolChain(k);
  return tc ? tc->displayName() : QString();
}

auto ToolChainKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  const auto tc = cxxToolChain(k);
  return {{tr("Compiler"), tc ? tc->displayName() : tr("None")}};
}

auto ToolChainKitAspect::addToBuildEnvironment(const Kit *k, Environment &env) const -> void
{
  const auto tc = cxxToolChain(k);
  if (tc)
    tc->addToEnvironment(env);
}

auto ToolChainKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  QTC_ASSERT(kit, return);

  // Compatibility with Qt Creator < 4.2:
  expander->registerVariable("Compiler:Name", tr("Compiler"), [kit] {
    const ToolChain *tc = cxxToolChain(kit);
    return tc ? tc->displayName() : tr("None");
  });

  expander->registerVariable("Compiler:Executable", tr("Path to the compiler executable"), [kit] {
    const ToolChain *tc = cxxToolChain(kit);
    return tc ? tc->compilerCommand().path() : QString();
  });

  // After 4.2
  expander->registerPrefix("Compiler:Name", tr("Compiler for different languages"), [kit](const QString &ls) {
    const ToolChain *tc = toolChain(kit, findLanguage(ls));
    return tc ? tc->displayName() : tr("None");
  });
  expander->registerPrefix("Compiler:Executable", tr("Compiler executable for different languages"), [kit](const QString &ls) {
    const ToolChain *tc = toolChain(kit, findLanguage(ls));
    return tc ? tc->compilerCommand().path() : QString();
  });
}

auto ToolChainKitAspect::createOutputParsers(const Kit *k) const -> QList<OutputLineParser*>
{
  for (const Id langId : {Constants::CXX_LANGUAGE_ID, Constants::C_LANGUAGE_ID}) {
    if (const ToolChain *const tc = toolChain(k, langId))
      return tc->createOutputParsers();
  }
  return {};
}

auto ToolChainKitAspect::availableFeatures(const Kit *k) const -> QSet<Id>
{
  QSet<Id> result;
  for (const auto tc : toolChains(k))
    result.insert(tc->typeId().withPrefix("ToolChain."));
  return result;
}

auto ToolChainKitAspect::id() -> Id
{
  return KITINFORMATION_ID_V3;
}

auto ToolChainKitAspect::toolChainId(const Kit *k, Id language) -> QByteArray
{
  QTC_ASSERT(ToolChainManager::isLoaded(), return nullptr);
  if (!k)
    return QByteArray();
  const auto value = k->value(id()).toMap();
  return value.value(language.toString(), QByteArray()).toByteArray();
}

auto ToolChainKitAspect::toolChain(const Kit *k, Id language) -> ToolChain*
{
  return ToolChainManager::findToolChain(toolChainId(k, language));
}

auto ToolChainKitAspect::cToolChain(const Kit *k) -> ToolChain*
{
  return ToolChainManager::findToolChain(toolChainId(k, Constants::C_LANGUAGE_ID));
}

auto ToolChainKitAspect::cxxToolChain(const Kit *k) -> ToolChain*
{
  return ToolChainManager::findToolChain(toolChainId(k, Constants::CXX_LANGUAGE_ID));
}

auto ToolChainKitAspect::toolChains(const Kit *k) -> QList<ToolChain*>
{
  QTC_ASSERT(k, return QList<ToolChain *>());

  const auto value = k->value(id()).toMap();
  const auto tcList = transform<QList>(ToolChainManager::allLanguages(), [&value](Id l) {
    return ToolChainManager::findToolChain(value.value(l.toString()).toByteArray());
  });
  return filtered(tcList, [](ToolChain *tc) { return tc; });
}

auto ToolChainKitAspect::setToolChain(Kit *k, ToolChain *tc) -> void
{
  QTC_ASSERT(tc, return);
  QTC_ASSERT(k, return);
  auto result = k->value(id()).toMap();
  result.insert(tc->language().toString(), tc->id());

  k->setValue(id(), result);
}

/**
 * @brief ToolChainKitAspect::setAllToolChainsToMatch
 *
 * Set up all toolchains to be similar to the one toolchain provided. Similar ideally means
 * that all toolchains use the "same" compiler from the same installation, but we will
 * settle for a toolchain with a matching API instead.
 *
 * @param k The kit to set up
 * @param tc The toolchain to match other languages for.
 */
auto ToolChainKitAspect::setAllToolChainsToMatch(Kit *k, ToolChain *tc) -> void
{
  QTC_ASSERT(tc, return);
  QTC_ASSERT(k, return);

  const auto allTcList = ToolChainManager::toolchains();
  QTC_ASSERT(allTcList.contains(tc), return);

  auto result = k->value(id()).toMap();
  result.insert(tc->language().toString(), tc->id());

  for (const auto l : ToolChainManager::allLanguages()) {
    if (l == tc->language())
      continue;

    const ToolChain *match = nullptr;
    const ToolChain *bestMatch = nullptr;
    for (const auto other : allTcList) {
      if (!other->isValid() || other->language() != l)
        continue;
      if (other->targetAbi() == tc->targetAbi())
        match = other;
      if (match == other && other->compilerCommand().parentDir() == tc->compilerCommand().parentDir()) {
        bestMatch = other;
        break;
      }
    }
    if (bestMatch)
      result.insert(l.toString(), bestMatch->id());
    else if (match)
      result.insert(l.toString(), match->id());
    else
      result.insert(l.toString(), QByteArray());
  }

  k->setValue(id(), result);
}

auto ToolChainKitAspect::clearToolChain(Kit *k, Id language) -> void
{
  QTC_ASSERT(language.isValid(), return);
  QTC_ASSERT(k, return);

  auto result = k->value(id()).toMap();
  result.insert(language.toString(), QByteArray());
  k->setValue(id(), result);
}

auto ToolChainKitAspect::targetAbi(const Kit *k) -> Abi
{
  auto tcList = toolChains(k);
  // Find the best possible ABI for all the tool chains...
  Abi cxxAbi;
  QHash<Abi, int> abiCount;
  foreach(ToolChain *tc, tcList) {
    auto ta = tc->targetAbi();
    if (tc->language() == Id(Constants::CXX_LANGUAGE_ID))
      cxxAbi = tc->targetAbi();
    abiCount[ta] = (abiCount.contains(ta) ? abiCount[ta] + 1 : 1);
  }
  QVector<Abi> candidates;
  auto count = -1;
  candidates.reserve(tcList.count());
  for (auto i = abiCount.begin(); i != abiCount.end(); ++i) {
    if (i.value() > count) {
      candidates.clear();
      candidates.append(i.key());
      count = i.value();
    } else if (i.value() == count) {
      candidates.append(i.key());
    }
  }

  // Found a good candidate:
  if (candidates.isEmpty())
    return Abi::hostAbi();
  if (candidates.contains(cxxAbi)) // Use Cxx compiler as a tie breaker
    return cxxAbi;
  return candidates.at(0); // Use basically a random Abi...
}

auto ToolChainKitAspect::msgNoToolChainInTarget() -> QString
{
  return tr("No compiler set in kit.");
}

auto ToolChainKitAspect::kitsWereLoaded() -> void
{
  foreach(Kit *k, KitManager::kits())
    fix(k);

  connect(ToolChainManager::instance(), &ToolChainManager::toolChainRemoved, this, &ToolChainKitAspect::toolChainRemoved);
  connect(ToolChainManager::instance(), &ToolChainManager::toolChainUpdated, this, &ToolChainKitAspect::toolChainUpdated);
}

auto ToolChainKitAspect::toolChainUpdated(ToolChain *tc) -> void
{
  for (const auto k : KitManager::kits()) {
    if (toolChain(k, tc->language()) == tc)
      notifyAboutUpdate(k);
  }
}

auto ToolChainKitAspect::toolChainRemoved(ToolChain *tc) -> void
{
  Q_UNUSED(tc)
  foreach(Kit *k, KitManager::kits())
    fix(k);
}

// --------------------------------------------------------------------------
// DeviceTypeKitAspect:
// --------------------------------------------------------------------------
namespace Internal {
class DeviceTypeKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::DeviceTypeKitAspect)
public:
  DeviceTypeKitAspectWidget(Kit *workingCopy, const KitAspect *ki) : KitAspectWidget(workingCopy, ki), m_comboBox(createSubWidget<QComboBox>())
  {
    for (const auto factory : IDeviceFactory::allDeviceFactories())
      m_comboBox->addItem(factory->displayName(), factory->deviceType().toSetting());
    m_comboBox->setToolTip(ki->description());
    refresh();
    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceTypeKitAspectWidget::currentTypeChanged);
  }

  ~DeviceTypeKitAspectWidget() override { delete m_comboBox; }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_comboBox);
    builder.addItem(m_comboBox);
  }

  auto makeReadOnly() -> void override { m_comboBox->setEnabled(false); }

  auto refresh() -> void override
  {
    const auto devType = DeviceTypeKitAspect::deviceTypeId(m_kit);
    if (!devType.isValid())
      m_comboBox->setCurrentIndex(-1);
    for (auto i = 0; i < m_comboBox->count(); ++i) {
      if (m_comboBox->itemData(i) == devType.toSetting()) {
        m_comboBox->setCurrentIndex(i);
        break;
      }
    }
  }

  auto currentTypeChanged(int idx) -> void
  {
    const auto type = idx < 0 ? Id() : Id::fromSetting(m_comboBox->itemData(idx));
    DeviceTypeKitAspect::setDeviceTypeId(m_kit, type);
  }

  QComboBox *m_comboBox;
};
} // namespace Internal

DeviceTypeKitAspect::DeviceTypeKitAspect()
{
  setObjectName(QLatin1String("DeviceTypeInformation"));
  setId(id());
  setDisplayName(tr("Device type"));
  setDescription(tr("The type of device to run applications on."));
  setPriority(33000);
  makeEssential();
}

auto DeviceTypeKitAspect::setup(Kit *k) -> void
{
  if (k && !k->hasValue(id()))
    k->setValue(id(), QByteArray(Constants::DESKTOP_DEVICE_TYPE));
}

auto DeviceTypeKitAspect::validate(const Kit *k) const -> Tasks
{
  Q_UNUSED(k)
  return {};
}

auto DeviceTypeKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::DeviceTypeKitAspectWidget(k, this);
}

auto DeviceTypeKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  QTC_ASSERT(k, return {});
  const auto type = deviceTypeId(k);
  auto typeDisplayName = tr("Unknown device type");
  if (type.isValid()) {
    if (const auto factory = IDeviceFactory::find(type))
      typeDisplayName = factory->displayName();
  }
  return {{tr("Device type"), typeDisplayName}};
}

auto DeviceTypeKitAspect::id() -> const Id
{
  return "PE.Profile.DeviceType";
}

auto DeviceTypeKitAspect::deviceTypeId(const Kit *k) -> const Id
{
  return k ? Id::fromSetting(k->value(id())) : Id();
}

auto DeviceTypeKitAspect::setDeviceTypeId(Kit *k, Id type) -> void
{
  QTC_ASSERT(k, return);
  k->setValue(id(), type.toSetting());
}

auto DeviceTypeKitAspect::supportedPlatforms(const Kit *k) const -> QSet<Id>
{
  return {deviceTypeId(k)};
}

auto DeviceTypeKitAspect::availableFeatures(const Kit *k) const -> QSet<Id>
{
  const auto id = deviceTypeId(k);
  if (id.isValid())
    return {id.withPrefix("DeviceType.")};
  return {};
}

// --------------------------------------------------------------------------
// DeviceKitAspect:
// --------------------------------------------------------------------------
namespace Internal {
class DeviceKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::DeviceKitAspect)
public:
  DeviceKitAspectWidget(Kit *workingCopy, const KitAspect *ki) : KitAspectWidget(workingCopy, ki), m_comboBox(createSubWidget<QComboBox>()), m_model(new DeviceManagerModel(DeviceManager::instance()))
  {
    m_comboBox->setSizePolicy(QSizePolicy::Preferred, m_comboBox->sizePolicy().verticalPolicy());
    m_comboBox->setModel(m_model);
    m_comboBox->setMinimumContentsLength(16); // Don't stretch too much for Kit Page
    m_manageButton = createManageButton(Constants::DEVICE_SETTINGS_PAGE_ID);
    refresh();
    m_comboBox->setToolTip(ki->description());

    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, &DeviceKitAspectWidget::modelAboutToReset);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DeviceKitAspectWidget::modelReset);
    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceKitAspectWidget::currentDeviceChanged);
  }

  ~DeviceKitAspectWidget() override
  {
    delete m_comboBox;
    delete m_model;
    delete m_manageButton;
  }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_comboBox);
    builder.addItem(m_comboBox);
    builder.addItem(m_manageButton);
  }

  auto makeReadOnly() -> void override { m_comboBox->setEnabled(false); }

  auto refresh() -> void override
  {
    m_model->setTypeFilter(DeviceTypeKitAspect::deviceTypeId(m_kit));
    m_comboBox->setCurrentIndex(m_model->indexOf(DeviceKitAspect::device(m_kit)));
  }

  auto modelAboutToReset() -> void
  {
    m_selectedId = m_model->deviceId(m_comboBox->currentIndex());
    m_ignoreChange = true;
  }

  auto modelReset() -> void
  {
    m_comboBox->setCurrentIndex(m_model->indexForId(m_selectedId));
    m_ignoreChange = false;
  }

  auto currentDeviceChanged() -> void
  {
    if (m_ignoreChange)
      return;
    DeviceKitAspect::setDeviceId(m_kit, m_model->deviceId(m_comboBox->currentIndex()));
  }

  bool m_ignoreChange = false;
  QComboBox *m_comboBox;
  QWidget *m_manageButton;
  DeviceManagerModel *m_model;
  Id m_selectedId;
};
} // namespace Internal

DeviceKitAspect::DeviceKitAspect()
{
  setObjectName(QLatin1String("DeviceInformation"));
  setId(id());
  setDisplayName(tr("Device"));
  setDescription(tr("The device to run the applications on."));
  setPriority(32000);

  connect(KitManager::instance(), &KitManager::kitsLoaded, this, &DeviceKitAspect::kitsWereLoaded);
}

auto DeviceKitAspect::defaultValue(const Kit *k) const -> QVariant
{
  const auto type = DeviceTypeKitAspect::deviceTypeId(k);
  // Use default device if that is compatible:
  auto dev = DeviceManager::instance()->defaultDevice(type);
  if (dev && dev->isCompatibleWith(k))
    return dev->id().toString();
  // Use any other device that is compatible:
  for (auto i = 0; i < DeviceManager::instance()->deviceCount(); ++i) {
    dev = DeviceManager::instance()->deviceAt(i);
    if (dev && dev->isCompatibleWith(k))
      return dev->id().toString();
  }
  // Fail: No device set up.
  return QString();
}

auto DeviceKitAspect::validate(const Kit *k) const -> Tasks
{
  const auto dev = device(k);
  Tasks result;
  if (dev.isNull())
    result.append(BuildSystemTask(Task::Warning, tr("No device set.")));
  else if (!dev->isCompatibleWith(k))
    result.append(BuildSystemTask(Task::Error, tr("Device is incompatible with this kit.")));

  if (dev)
    result.append(dev->validate());

  return result;
}

auto DeviceKitAspect::fix(Kit *k) -> void
{
  const auto dev = device(k);
  if (!dev.isNull() && !dev->isCompatibleWith(k)) {
    qWarning("Device is no longer compatible with kit \"%s\", removing it.", qPrintable(k->displayName()));
    setDeviceId(k, Id());
  }
}

auto DeviceKitAspect::setup(Kit *k) -> void
{
  QTC_ASSERT(DeviceManager::instance()->isLoaded(), return);
  const auto dev = device(k);
  if (!dev.isNull() && dev->isCompatibleWith(k))
    return;

  setDeviceId(k, Id::fromSetting(defaultValue(k)));
}

auto DeviceKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::DeviceKitAspectWidget(k, this);
}

auto DeviceKitAspect::displayNamePostfix(const Kit *k) const -> QString
{
  const auto dev = device(k);
  return dev.isNull() ? QString() : dev->displayName();
}

auto DeviceKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  const auto dev = device(k);
  return {{tr("Device"), dev.isNull() ? tr("Unconfigured") : dev->displayName()}};
}

auto DeviceKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  QTC_ASSERT(kit, return);
  expander->registerVariable("Device:HostAddress", tr("Host address"), [kit]() -> QString {
    const auto device = DeviceKitAspect::device(kit);
    return device ? device->sshParameters().host() : QString();
  });
  expander->registerVariable("Device:SshPort", tr("SSH port"), [kit]() -> QString {
    const auto device = DeviceKitAspect::device(kit);
    return device ? QString::number(device->sshParameters().port()) : QString();
  });
  expander->registerVariable("Device:UserName", tr("User name"), [kit]() -> QString {
    const auto device = DeviceKitAspect::device(kit);
    return device ? device->sshParameters().userName() : QString();
  });
  expander->registerVariable("Device:KeyFile", tr("Private key file"), [kit]() -> QString {
    const auto device = DeviceKitAspect::device(kit);
    return device ? device->sshParameters().privateKeyFile.toString() : QString();
  });
  expander->registerVariable("Device:Name", tr("Device name"), [kit]() -> QString {
    const auto device = DeviceKitAspect::device(kit);
    return device ? device->displayName() : QString();
  });
}

auto DeviceKitAspect::id() -> Id
{
  return "PE.Profile.Device";
}

auto DeviceKitAspect::device(const Kit *k) -> IDevice::ConstPtr
{
  QTC_ASSERT(DeviceManager::instance()->isLoaded(), return IDevice::ConstPtr());
  return DeviceManager::instance()->find(deviceId(k));
}

auto DeviceKitAspect::deviceId(const Kit *k) -> Id
{
  return k ? Id::fromSetting(k->value(id())) : Id();
}

auto DeviceKitAspect::setDevice(Kit *k, IDevice::ConstPtr dev) -> void
{
  setDeviceId(k, dev ? dev->id() : Id());
}

auto DeviceKitAspect::setDeviceId(Kit *k, Id id) -> void
{
  QTC_ASSERT(k, return);
  k->setValue(DeviceKitAspect::id(), id.toSetting());
}

auto DeviceKitAspect::kitsWereLoaded() -> void
{
  foreach(Kit *k, KitManager::kits())
    fix(k);

  const auto dm = DeviceManager::instance();
  connect(dm, &DeviceManager::deviceListReplaced, this, &DeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceAdded, this, &DeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceRemoved, this, &DeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceUpdated, this, &DeviceKitAspect::deviceUpdated);

  connect(KitManager::instance(), &KitManager::kitUpdated, this, &DeviceKitAspect::kitUpdated);
  connect(KitManager::instance(), &KitManager::unmanagedKitUpdated, this, &DeviceKitAspect::kitUpdated);
}

auto DeviceKitAspect::deviceUpdated(Id id) -> void
{
  foreach(Kit *k, KitManager::kits()) {
    if (deviceId(k) == id)
      notifyAboutUpdate(k);
  }
}

auto DeviceKitAspect::kitUpdated(Kit *k) -> void
{
  setup(k); // Set default device if necessary
}

auto DeviceKitAspect::devicesChanged() -> void
{
  foreach(Kit *k, KitManager::kits())
    setup(k); // Set default device if necessary
}

// --------------------------------------------------------------------------
// BuildDeviceKitAspect:
// --------------------------------------------------------------------------
namespace Internal {
class BuildDeviceKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::BuildDeviceKitAspect)
public:
  BuildDeviceKitAspectWidget(Kit *workingCopy, const KitAspect *ki) : KitAspectWidget(workingCopy, ki), m_comboBox(createSubWidget<QComboBox>()), m_model(new DeviceManagerModel(DeviceManager::instance()))
  {
    m_comboBox->setSizePolicy(QSizePolicy::Ignored, m_comboBox->sizePolicy().verticalPolicy());
    m_comboBox->setModel(m_model);
    m_manageButton = createManageButton(Constants::DEVICE_SETTINGS_PAGE_ID);
    refresh();
    m_comboBox->setToolTip(ki->description());

    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, &BuildDeviceKitAspectWidget::modelAboutToReset);
    connect(m_model, &QAbstractItemModel::modelReset, this, &BuildDeviceKitAspectWidget::modelReset);
    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BuildDeviceKitAspectWidget::currentDeviceChanged);
  }

  ~BuildDeviceKitAspectWidget() override
  {
    delete m_comboBox;
    delete m_model;
    delete m_manageButton;
  }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_comboBox);
    builder.addItem(m_comboBox);
    builder.addItem(m_manageButton);
  }

  auto makeReadOnly() -> void override { m_comboBox->setEnabled(false); }

  auto refresh() -> void override
  {
    QList<Id> blackList;
    const DeviceManager *dm = DeviceManager::instance();
    for (auto i = 0; i < dm->deviceCount(); ++i) {
      const auto device = dm->deviceAt(i);
      if (!(device->type() == Constants::DESKTOP_DEVICE_TYPE || device->type() == Docker::Constants::DOCKER_DEVICE_TYPE))
        blackList.append(device->id());
    }

    m_model->setFilter(blackList);
    m_comboBox->setCurrentIndex(m_model->indexOf(BuildDeviceKitAspect::device(m_kit)));
  }

  auto modelAboutToReset() -> void
  {
    m_selectedId = m_model->deviceId(m_comboBox->currentIndex());
    m_ignoreChange = true;
  }

  auto modelReset() -> void
  {
    m_comboBox->setCurrentIndex(m_model->indexForId(m_selectedId));
    m_ignoreChange = false;
  }

  auto currentDeviceChanged() -> void
  {
    if (m_ignoreChange)
      return;
    BuildDeviceKitAspect::setDeviceId(m_kit, m_model->deviceId(m_comboBox->currentIndex()));
  }

  bool m_ignoreChange = false;
  QComboBox *m_comboBox;
  QWidget *m_manageButton;
  DeviceManagerModel *m_model;
  Id m_selectedId;
};
} // namespace Internal

BuildDeviceKitAspect::BuildDeviceKitAspect()
{
  setObjectName("BuildDeviceInformation");
  setId(id());
  setDisplayName(tr("Build device"));
  setDescription(tr("The device used to build applications on."));
  setPriority(31900);

  connect(KitManager::instance(), &KitManager::kitsLoaded, this, &BuildDeviceKitAspect::kitsWereLoaded);
}

auto BuildDeviceKitAspect::setup(Kit *k) -> void
{
  QTC_ASSERT(DeviceManager::instance()->isLoaded(), return);
  auto dev = device(k);
  if (!dev.isNull())
    return;

  dev = defaultDevice();
  setDeviceId(k, dev ? dev->id() : Id());
}

auto BuildDeviceKitAspect::defaultDevice() -> IDevice::ConstPtr
{
  return DeviceManager::defaultDesktopDevice();
}

auto BuildDeviceKitAspect::validate(const Kit *k) const -> Tasks
{
  const auto dev = device(k);
  Tasks result;
  if (dev.isNull())
    result.append(BuildSystemTask(Task::Warning, tr("No build device set.")));

  return result;
}

auto BuildDeviceKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::BuildDeviceKitAspectWidget(k, this);
}

auto BuildDeviceKitAspect::displayNamePostfix(const Kit *k) const -> QString
{
  const auto dev = device(k);
  return dev.isNull() ? QString() : dev->displayName();
}

auto BuildDeviceKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  const auto dev = device(k);
  return {{tr("Build device"), dev.isNull() ? tr("Unconfigured") : dev->displayName()}};
}

auto BuildDeviceKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  QTC_ASSERT(kit, return);
  expander->registerVariable("BuildDevice:HostAddress", tr("Build host address"), [kit]() -> QString {
    const auto device = BuildDeviceKitAspect::device(kit);
    return device ? device->sshParameters().host() : QString();
  });
  expander->registerVariable("BuildDevice:SshPort", tr("Build SSH port"), [kit]() -> QString {
    const auto device = BuildDeviceKitAspect::device(kit);
    return device ? QString::number(device->sshParameters().port()) : QString();
  });
  expander->registerVariable("BuildDevice:UserName", tr("Build user name"), [kit]() -> QString {
    const auto device = BuildDeviceKitAspect::device(kit);
    return device ? device->sshParameters().userName() : QString();
  });
  expander->registerVariable("BuildDevice:KeyFile", tr("Build private key file"), [kit]() -> QString {
    const auto device = BuildDeviceKitAspect::device(kit);
    return device ? device->sshParameters().privateKeyFile.toString() : QString();
  });
  expander->registerVariable("BuildDevice:Name", tr("Build device name"), [kit]() -> QString {
    const auto device = BuildDeviceKitAspect::device(kit);
    return device ? device->displayName() : QString();
  });
}

auto BuildDeviceKitAspect::id() -> Id
{
  return "PE.Profile.BuildDevice";
}

auto BuildDeviceKitAspect::device(const Kit *k) -> IDevice::ConstPtr
{
  QTC_ASSERT(DeviceManager::instance()->isLoaded(), return IDevice::ConstPtr());
  auto dev = DeviceManager::instance()->find(deviceId(k));
  if (!dev)
    dev = defaultDevice();
  return dev;
}

auto BuildDeviceKitAspect::deviceId(const Kit *k) -> Id
{
  return k ? Id::fromSetting(k->value(id())) : Id();
}

auto BuildDeviceKitAspect::setDevice(Kit *k, IDevice::ConstPtr dev) -> void
{
  setDeviceId(k, dev ? dev->id() : Id());
}

auto BuildDeviceKitAspect::setDeviceId(Kit *k, Id id) -> void
{
  QTC_ASSERT(k, return);
  k->setValue(BuildDeviceKitAspect::id(), id.toSetting());
}

auto BuildDeviceKitAspect::kitsWereLoaded() -> void
{
  foreach(Kit *k, KitManager::kits())
    fix(k);

  const auto dm = DeviceManager::instance();
  connect(dm, &DeviceManager::deviceListReplaced, this, &BuildDeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceAdded, this, &BuildDeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceRemoved, this, &BuildDeviceKitAspect::devicesChanged);
  connect(dm, &DeviceManager::deviceUpdated, this, &BuildDeviceKitAspect::deviceUpdated);

  connect(KitManager::instance(), &KitManager::kitUpdated, this, &BuildDeviceKitAspect::kitUpdated);
  connect(KitManager::instance(), &KitManager::unmanagedKitUpdated, this, &BuildDeviceKitAspect::kitUpdated);
}

auto BuildDeviceKitAspect::deviceUpdated(Id id) -> void
{
  foreach(Kit *k, KitManager::kits()) {
    if (deviceId(k) == id)
      notifyAboutUpdate(k);
  }
}

auto BuildDeviceKitAspect::kitUpdated(Kit *k) -> void
{
  setup(k); // Set default device if necessary
}

auto BuildDeviceKitAspect::devicesChanged() -> void
{
  foreach(Kit *k, KitManager::kits())
    setup(k); // Set default device if necessary
}

// --------------------------------------------------------------------------
// EnvironmentKitAspect:
// --------------------------------------------------------------------------
namespace Internal {
class EnvironmentKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::EnvironmentKitAspect)
public:
  EnvironmentKitAspectWidget(Kit *workingCopy, const KitAspect *ki) : KitAspectWidget(workingCopy, ki), m_summaryLabel(createSubWidget<ElidingLabel>()), m_manageButton(createSubWidget<QPushButton>()), m_mainWidget(createSubWidget<QWidget>())
  {
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_summaryLabel);
    if (HostOsInfo::isWindowsHost())
      initMSVCOutputSwitch(layout);
    m_mainWidget->setLayout(layout);
    refresh();
    m_manageButton->setText(tr("Change..."));
    connect(m_manageButton, &QAbstractButton::clicked, this, &EnvironmentKitAspectWidget::editEnvironmentChanges);
  }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_mainWidget);
    builder.addItem(m_mainWidget);
    builder.addItem(m_manageButton);
  }

  auto makeReadOnly() -> void override { m_manageButton->setEnabled(false); }

  auto refresh() -> void override
  {
    const auto changes = currentEnvironment();
    const auto shortSummary = EnvironmentItem::toStringList(changes).join("; ");
    m_summaryLabel->setText(shortSummary.isEmpty() ? tr("No changes to apply.") : shortSummary);
  }

  auto editEnvironmentChanges() -> void
  {
    auto expander = m_kit->macroExpander();
    const EnvironmentDialog::Polisher polisher = [expander](QWidget *w) {
      VariableChooser::addSupportForChildWidgets(w, expander);
    };
    auto changes = EnvironmentDialog::getEnvironmentItems(m_summaryLabel, currentEnvironment(), QString(), polisher);
    if (!changes)
      return;

    if (HostOsInfo::isWindowsHost()) {
      const EnvironmentItem forceMSVCEnglishItem("VSLANG", "1033");
      if (m_vslangCheckbox->isChecked() && changes->indexOf(forceMSVCEnglishItem) < 0)
        changes->append(forceMSVCEnglishItem);
    }

    EnvironmentKitAspect::setEnvironmentChanges(m_kit, *changes);
  }

  auto currentEnvironment() const -> EnvironmentItems
  {
    auto changes = EnvironmentKitAspect::environmentChanges(m_kit);

    if (HostOsInfo::isWindowsHost()) {
      const EnvironmentItem forceMSVCEnglishItem("VSLANG", "1033");
      if (changes.indexOf(forceMSVCEnglishItem) >= 0) {
        m_vslangCheckbox->setCheckState(Qt::Checked);
        changes.removeAll(forceMSVCEnglishItem);
      }
    }

    sort(changes, [](const EnvironmentItem &lhs, const EnvironmentItem &rhs) { return QString::localeAwareCompare(lhs.name, rhs.name) < 0; });
    return changes;
  }

  auto initMSVCOutputSwitch(QVBoxLayout *layout) -> void
  {
    m_vslangCheckbox = new QCheckBox(tr("Force UTF-8 MSVC compiler output"));
    layout->addWidget(m_vslangCheckbox);
    m_vslangCheckbox->setToolTip(tr("Either switches MSVC to English or keeps the language and " "just forces UTF-8 output (may vary depending on the used MSVC " "compiler)."));
    connect(m_vslangCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
      auto changes = EnvironmentKitAspect::environmentChanges(m_kit);
      const EnvironmentItem forceMSVCEnglishItem("VSLANG", "1033");
      if (!checked && changes.indexOf(forceMSVCEnglishItem) >= 0)
        changes.removeAll(forceMSVCEnglishItem);
      if (checked && changes.indexOf(forceMSVCEnglishItem) < 0)
        changes.append(forceMSVCEnglishItem);
      EnvironmentKitAspect::setEnvironmentChanges(m_kit, changes);
    });
  }

  ElidingLabel *m_summaryLabel;
  QPushButton *m_manageButton;
  QCheckBox *m_vslangCheckbox;
  QWidget *m_mainWidget;
};
} // namespace Internal

EnvironmentKitAspect::EnvironmentKitAspect()
{
  setObjectName(QLatin1String("EnvironmentKitAspect"));
  setId(id());
  setDisplayName(tr("Environment"));
  setDescription(tr("Additional build environment settings when using this kit."));
  setPriority(29000);
}

auto EnvironmentKitAspect::validate(const Kit *k) const -> Tasks
{
  Tasks result;
  QTC_ASSERT(k, return result);

  const auto variant = k->value(id());
  if (!variant.isNull() && !variant.canConvert(QVariant::List))
    result << BuildSystemTask(Task::Error, tr("The environment setting value is invalid."));

  return result;
}

auto EnvironmentKitAspect::fix(Kit *k) -> void
{
  QTC_ASSERT(k, return);

  const auto variant = k->value(id());
  if (!variant.isNull() && !variant.canConvert(QVariant::List)) {
    qWarning("Kit \"%s\" has a wrong environment value set.", qPrintable(k->displayName()));
    setEnvironmentChanges(k, EnvironmentItems());
  }
}

auto EnvironmentKitAspect::addToBuildEnvironment(const Kit *k, Environment &env) const -> void
{
  const auto values = transform(EnvironmentItem::toStringList(environmentChanges(k)), [k](const QString &v) { return k->macroExpander()->expand(v); });
  env.modify(EnvironmentItem::fromStringList(values));
}

auto EnvironmentKitAspect::addToRunEnvironment(const Kit *k, Environment &env) const -> void
{
  addToBuildEnvironment(k, env);
}

auto EnvironmentKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new Internal::EnvironmentKitAspectWidget(k, this);
}

auto EnvironmentKitAspect::toUserOutput(const Kit *k) const -> ItemList
{
  return {qMakePair(tr("Environment"), EnvironmentItem::toStringList(environmentChanges(k)).join("<br>"))};
}

auto EnvironmentKitAspect::id() -> Id
{
  return "PE.Profile.Environment";
}

auto EnvironmentKitAspect::environmentChanges(const Kit *k) -> EnvironmentItems
{
  if (k)
    return EnvironmentItem::fromStringList(k->value(id()).toStringList());
  return EnvironmentItems();
}

auto EnvironmentKitAspect::setEnvironmentChanges(Kit *k, const EnvironmentItems &changes) -> void
{
  if (k)
    k->setValue(id(), EnvironmentItem::toStringList(changes));
}

} // namespace ProjectExplorer
