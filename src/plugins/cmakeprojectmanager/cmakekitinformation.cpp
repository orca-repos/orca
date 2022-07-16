// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakekitinformation.hpp"

#include "cmakeprojectconstants.hpp"
#include "cmakeprojectplugin.hpp"
#include "cmakespecificsettings.hpp"
#include "cmaketool.hpp"
#include "cmaketoolmanager.hpp"

#include <app/app_version.hpp>

#include <core/core-interface.hpp>

#include <constants/ios/iosconstants.hpp>

#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectexplorersettings.hpp>
#include <projectexplorer/task.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/devicesupport/idevice.hpp>

#include <qtsupport/baseqtversion.hpp>
#include <qtsupport/qtkitinformation.hpp>

#include <utils/algorithm.hpp>
#include <utils/commandline.hpp>
#include <utils/elidinglabel.hpp>
#include <utils/environment.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcassert.hpp>
#include <utils/variablechooser.hpp>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {

static auto isIos(const Kit *k) -> bool
{
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);
  return deviceType == Ios::Constants::IOS_DEVICE_TYPE || deviceType == Ios::Constants::IOS_SIMULATOR_TYPE;
}

static auto defaultCMakeToolId() -> Id
{
  auto defaultTool = CMakeToolManager::defaultCMakeTool();
  return defaultTool ? defaultTool->id() : Id();
}

const char TOOL_ID[] = "CMakeProjectManager.CMakeKitInformation";

class CMakeKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeKitAspect)
public:
  CMakeKitAspectWidget(Kit *kit, const KitAspect *ki) : KitAspectWidget(kit, ki), m_comboBox(createSubWidget<QComboBox>()), m_manageButton(createManageButton(Constants::CMAKE_SETTINGS_PAGE_ID))
  {
    m_comboBox->setSizePolicy(QSizePolicy::Ignored, m_comboBox->sizePolicy().verticalPolicy());
    m_comboBox->setEnabled(false);
    m_comboBox->setToolTip(ki->description());

    foreach(CMakeTool *tool, CMakeToolManager::cmakeTools())
      cmakeToolAdded(tool->id());

    updateComboBox();
    refresh();
    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CMakeKitAspectWidget::currentCMakeToolChanged);

    auto cmakeMgr = CMakeToolManager::instance();
    connect(cmakeMgr, &CMakeToolManager::cmakeAdded, this, &CMakeKitAspectWidget::cmakeToolAdded);
    connect(cmakeMgr, &CMakeToolManager::cmakeRemoved, this, &CMakeKitAspectWidget::cmakeToolRemoved);
    connect(cmakeMgr, &CMakeToolManager::cmakeUpdated, this, &CMakeKitAspectWidget::cmakeToolUpdated);
  }

  ~CMakeKitAspectWidget() override
  {
    delete m_comboBox;
    delete m_manageButton;
  }

private:
  // KitAspectWidget interface
  auto makeReadOnly() -> void override { m_comboBox->setEnabled(false); }

  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_comboBox);
    builder.addItem(m_comboBox);
    builder.addItem(m_manageButton);
  }

  auto refresh() -> void override
  {
    auto tool = CMakeKitAspect::cmakeTool(m_kit);
    m_comboBox->setCurrentIndex(tool ? indexOf(tool->id()) : -1);
  }

  auto indexOf(Id id) -> int
  {
    for (auto i = 0; i < m_comboBox->count(); ++i) {
      if (id == Id::fromSetting(m_comboBox->itemData(i)))
        return i;
    }
    return -1;
  }

  auto updateComboBox() -> void
  {
    // remove unavailable cmake tool:
    auto pos = indexOf(Id());
    if (pos >= 0)
      m_comboBox->removeItem(pos);

    if (m_comboBox->count() == 0) {
      m_comboBox->addItem(tr("<No CMake Tool available>"), Id().toSetting());
      m_comboBox->setEnabled(false);
    } else {
      m_comboBox->setEnabled(true);
    }
  }

  auto cmakeToolAdded(Id id) -> void
  {
    const CMakeTool *tool = CMakeToolManager::findById(id);
    QTC_ASSERT(tool, return);

    m_comboBox->addItem(tool->displayName(), tool->id().toSetting());
    updateComboBox();
    refresh();
  }

  auto cmakeToolUpdated(Id id) -> void
  {
    const auto pos = indexOf(id);
    QTC_ASSERT(pos >= 0, return);

    const CMakeTool *tool = CMakeToolManager::findById(id);
    QTC_ASSERT(tool, return);

    m_comboBox->setItemText(pos, tool->displayName());
  }

  auto cmakeToolRemoved(Id id) -> void
  {
    const auto pos = indexOf(id);
    QTC_ASSERT(pos >= 0, return);

    // do not handle the current index changed signal
    m_removingItem = true;
    m_comboBox->removeItem(pos);
    m_removingItem = false;

    // update the checkbox and set the current index
    updateComboBox();
    refresh();
  }

  auto currentCMakeToolChanged(int index) -> void
  {
    if (m_removingItem)
      return;

    const auto id = Id::fromSetting(m_comboBox->itemData(index));
    CMakeKitAspect::setCMakeTool(m_kit, id);
  }

  bool m_removingItem = false;
  QComboBox *m_comboBox;
  QWidget *m_manageButton;
};

CMakeKitAspect::CMakeKitAspect()
{
  setObjectName(QLatin1String("CMakeKitAspect"));
  setId(TOOL_ID);
  setDisplayName(tr("CMake Tool"));
  setDescription(tr("The CMake Tool to use when building a project with CMake.<br>" "This setting is ignored when using other build systems."));
  setPriority(20000);

  //make sure the default value is set if a selected CMake is removed
  connect(CMakeToolManager::instance(), &CMakeToolManager::cmakeRemoved, [this] {
    for (auto k : KitManager::kits())
      fix(k);
  });

  //make sure the default value is set if a new default CMake is set
  connect(CMakeToolManager::instance(), &CMakeToolManager::defaultCMakeChanged, [this] {
    for (auto k : KitManager::kits())
      fix(k);
  });
}

auto CMakeKitAspect::id() -> Id
{
  return TOOL_ID;
}

auto CMakeKitAspect::cmakeToolId(const Kit *k) -> Id
{
  if (!k)
    return {};
  return Id::fromSetting(k->value(TOOL_ID));
}

auto CMakeKitAspect::cmakeTool(const Kit *k) -> CMakeTool*
{
  return CMakeToolManager::findById(cmakeToolId(k));
}

auto CMakeKitAspect::setCMakeTool(Kit *k, const Id id) -> void
{
  const auto toSet = id.isValid() ? id : defaultCMakeToolId();
  QTC_ASSERT(!id.isValid() || CMakeToolManager::findById(toSet), return);
  if (k)
    k->setValue(TOOL_ID, toSet.toSetting());
}

auto CMakeKitAspect::validate(const Kit *k) const -> Tasks
{
  Tasks result;
  auto tool = CMakeKitAspect::cmakeTool(k);
  if (tool) {
    auto version = tool->version();
    if (version.major < 3 || (version.major == 3 && version.minor < 14)) {
      result << BuildSystemTask(Task::Warning, msgUnsupportedVersion(version.fullVersion));
    }
  }
  return result;
}

auto CMakeKitAspect::setup(Kit *k) -> void
{
  auto tool = CMakeKitAspect::cmakeTool(k);
  if (tool)
    return;

  // Look for a suitable auto-detected one:
  const auto kitSource = k->autoDetectionSource();
  for (auto tool : CMakeToolManager::cmakeTools()) {
    const auto toolSource = tool->detectionSource();
    if (!toolSource.isEmpty() && toolSource == kitSource) {
      setCMakeTool(k, tool->id());
      return;
    }
  }

  setCMakeTool(k, defaultCMakeToolId());
}

auto CMakeKitAspect::fix(Kit *k) -> void
{
  setup(k);
}

auto CMakeKitAspect::toUserOutput(const Kit *k) const -> KitAspect::ItemList
{
  const CMakeTool *const tool = cmakeTool(k);
  return {{tr("CMake"), tool ? tool->displayName() : tr("Unconfigured")}};
}

auto CMakeKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  QTC_ASSERT(k, return nullptr);
  return new CMakeKitAspectWidget(k, this);
}

auto CMakeKitAspect::addToMacroExpander(Kit *k, MacroExpander *expander) const -> void
{
  QTC_ASSERT(k, return);
  expander->registerFileVariables("CMake:Executable", tr("Path to the cmake executable"), [k] {
    auto tool = CMakeKitAspect::cmakeTool(k);
    return tool ? tool->cmakeExecutable() : FilePath();
  });
}

auto CMakeKitAspect::availableFeatures(const Kit *k) const -> QSet<Id>
{
  if (cmakeTool(k))
    return {CMakeProjectManager::Constants::CMAKE_FEATURE_ID};
  return {};
}

auto CMakeKitAspect::msgUnsupportedVersion(const QByteArray &versionString) -> QString
{
  return tr("CMake version %1 is unsupported. Update to " "version 3.14 (with file-api) or later.").arg(QString::fromUtf8(versionString));
}

// --------------------------------------------------------------------
// CMakeGeneratorKitAspect:
// --------------------------------------------------------------------

const char GENERATOR_ID[] = "CMake.GeneratorKitInformation";

const char GENERATOR_KEY[] = "Generator";
const char EXTRA_GENERATOR_KEY[] = "ExtraGenerator";
const char PLATFORM_KEY[] = "Platform";
const char TOOLSET_KEY[] = "Toolset";

class CMakeGeneratorKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeGeneratorKitAspect)
public:
  CMakeGeneratorKitAspectWidget(Kit *kit, const KitAspect *ki) : KitAspectWidget(kit, ki), m_label(createSubWidget<ElidingLabel>()), m_changeButton(createSubWidget<QPushButton>())
  {
    const CMakeTool *tool = CMakeKitAspect::cmakeTool(kit);
    connect(this, &KitAspectWidget::labelLinkActivated, this, [=](const QString &) {
      CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake-generators.7.html");
    });

    m_label->setToolTip(ki->description());
    m_changeButton->setText(tr("Change..."));
    refresh();
    connect(m_changeButton, &QPushButton::clicked, this, &CMakeGeneratorKitAspectWidget::changeGenerator);
  }

  ~CMakeGeneratorKitAspectWidget() override
  {
    delete m_label;
    delete m_changeButton;
  }

private:
  // KitAspectWidget interface
  auto makeReadOnly() -> void override { m_changeButton->setEnabled(false); }

  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_label);
    builder.addItem(m_label);
    builder.addItem(m_changeButton);
  }

  auto refresh() -> void override
  {
    if (m_ignoreChange)
      return;

    const auto tool = CMakeKitAspect::cmakeTool(m_kit);
    if (tool != m_currentTool)
      m_currentTool = tool;

    m_changeButton->setEnabled(m_currentTool);
    const auto generator = CMakeGeneratorKitAspect::generator(kit());
    const auto extraGenerator = CMakeGeneratorKitAspect::extraGenerator(kit());
    const auto platform = CMakeGeneratorKitAspect::platform(kit());
    const auto toolset = CMakeGeneratorKitAspect::toolset(kit());

    QStringList messageLabel;
    if (!extraGenerator.isEmpty())
      messageLabel << extraGenerator << " - ";

    messageLabel << generator;

    if (!platform.isEmpty())
      messageLabel << ", " << tr("Platform") << ": " << platform;
    if (!toolset.isEmpty())
      messageLabel << ", " << tr("Toolset") << ": " << toolset;

    m_label->setText(messageLabel.join(""));
  }

  auto changeGenerator() -> void
  {
    QPointer<QDialog> changeDialog = new QDialog(m_changeButton);

    // Disable help button in titlebar on windows:
    auto flags = changeDialog->windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    changeDialog->setWindowFlags(flags);

    changeDialog->setWindowTitle(tr("CMake Generator"));

    auto layout = new QGridLayout(changeDialog);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto cmakeLabel = new QLabel;
    cmakeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto generatorCombo = new QComboBox;
    auto extraGeneratorCombo = new QComboBox;
    auto platformEdit = new QLineEdit;
    auto toolsetEdit = new QLineEdit;

    auto row = 0;
    layout->addWidget(new QLabel(QLatin1String("Executable:")));
    layout->addWidget(cmakeLabel, row, 1);

    ++row;
    layout->addWidget(new QLabel(tr("Generator:")), row, 0);
    layout->addWidget(generatorCombo, row, 1);

    ++row;
    layout->addWidget(new QLabel(tr("Extra generator:")), row, 0);
    layout->addWidget(extraGeneratorCombo, row, 1);

    ++row;
    layout->addWidget(new QLabel(tr("Platform:")), row, 0);
    layout->addWidget(platformEdit, row, 1);

    ++row;
    layout->addWidget(new QLabel(tr("Toolset:")), row, 0);
    layout->addWidget(toolsetEdit, row, 1);

    ++row;
    auto bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(bb, row, 0, 1, 2);

    connect(bb, &QDialogButtonBox::accepted, changeDialog.data(), &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, changeDialog.data(), &QDialog::reject);

    cmakeLabel->setText(m_currentTool->cmakeExecutable().toUserOutput());

    auto generatorList = m_currentTool->supportedGenerators();
    Utils::sort(generatorList, &CMakeTool::Generator::name);

    for (auto it = generatorList.constBegin(); it != generatorList.constEnd(); ++it)
      generatorCombo->addItem(it->name);

    auto updateDialog = [&generatorList, generatorCombo, extraGeneratorCombo, platformEdit, toolsetEdit](const QString &name) {
      const auto it = std::find_if(generatorList.constBegin(), generatorList.constEnd(), [name](const CMakeTool::Generator &g) { return g.name == name; });
      QTC_ASSERT(it != generatorList.constEnd(), return);
      generatorCombo->setCurrentText(name);

      extraGeneratorCombo->clear();
      extraGeneratorCombo->addItem(tr("<none>"), QString());
      for (const auto &eg : qAsConst(it->extraGenerators))
        extraGeneratorCombo->addItem(eg, eg);
      extraGeneratorCombo->setEnabled(extraGeneratorCombo->count() > 1);

      platformEdit->setEnabled(it->supportsPlatform);
      toolsetEdit->setEnabled(it->supportsToolset);
    };

    updateDialog(CMakeGeneratorKitAspect::generator(kit()));

    generatorCombo->setCurrentText(CMakeGeneratorKitAspect::generator(kit()));
    extraGeneratorCombo->setCurrentText(CMakeGeneratorKitAspect::extraGenerator(kit()));
    platformEdit->setText(platformEdit->isEnabled() ? CMakeGeneratorKitAspect::platform(kit()) : QString());
    toolsetEdit->setText(toolsetEdit->isEnabled() ? CMakeGeneratorKitAspect::toolset(kit()) : QString());

    connect(generatorCombo, &QComboBox::currentTextChanged, updateDialog);

    if (changeDialog->exec() == QDialog::Accepted) {
      if (!changeDialog)
        return;

      CMakeGeneratorKitAspect::set(kit(), generatorCombo->currentText(), extraGeneratorCombo->currentData().toString(), platformEdit->isEnabled() ? platformEdit->text() : QString(), toolsetEdit->isEnabled() ? toolsetEdit->text() : QString());

      refresh();
    }
  }

  bool m_ignoreChange = false;
  ElidingLabel *m_label;
  QPushButton *m_changeButton;
  CMakeTool *m_currentTool = nullptr;
};

namespace {

class GeneratorInfo {
public:
  GeneratorInfo() = default;
  GeneratorInfo(const QString &generator_, const QString &extraGenerator_ = QString(), const QString &platform_ = QString(), const QString &toolset_ = QString()) : generator(generator_), extraGenerator(extraGenerator_), platform(platform_), toolset(toolset_) {}

  auto toVariant() const -> QVariant
  {
    QVariantMap result;
    result.insert(GENERATOR_KEY, generator);
    result.insert(EXTRA_GENERATOR_KEY, extraGenerator);
    result.insert(PLATFORM_KEY, platform);
    result.insert(TOOLSET_KEY, toolset);
    return result;
  }

  auto fromVariant(const QVariant &v) -> void
  {
    const auto value = v.toMap();

    generator = value.value(GENERATOR_KEY).toString();
    extraGenerator = value.value(EXTRA_GENERATOR_KEY).toString();
    platform = value.value(PLATFORM_KEY).toString();
    toolset = value.value(TOOLSET_KEY).toString();
  }

  QString generator;
  QString extraGenerator;
  QString platform;
  QString toolset;
};

} // namespace

static auto generatorInfo(const Kit *k) -> GeneratorInfo
{
  GeneratorInfo info;
  if (!k)
    return info;

  info.fromVariant(k->value(GENERATOR_ID));
  return info;
}

static auto setGeneratorInfo(Kit *k, const GeneratorInfo &info) -> void
{
  if (!k)
    return;
  k->setValue(GENERATOR_ID, info.toVariant());
}

CMakeGeneratorKitAspect::CMakeGeneratorKitAspect()
{
  setObjectName(QLatin1String("CMakeGeneratorKitAspect"));
  setId(GENERATOR_ID);
  setDisplayName(tr("CMake <a href=\"generator\">generator</a>"));
  setDescription(tr("CMake generator defines how a project is built when using CMake.<br>" "This setting is ignored when using other build systems."));
  setPriority(19000);
}

auto CMakeGeneratorKitAspect::generator(const Kit *k) -> QString
{
  return generatorInfo(k).generator;
}

auto CMakeGeneratorKitAspect::extraGenerator(const Kit *k) -> QString
{
  return generatorInfo(k).extraGenerator;
}

auto CMakeGeneratorKitAspect::platform(const Kit *k) -> QString
{
  return generatorInfo(k).platform;
}

auto CMakeGeneratorKitAspect::toolset(const Kit *k) -> QString
{
  return generatorInfo(k).toolset;
}

auto CMakeGeneratorKitAspect::setGenerator(Kit *k, const QString &generator) -> void
{
  auto info = generatorInfo(k);
  info.generator = generator;
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::setExtraGenerator(Kit *k, const QString &extraGenerator) -> void
{
  auto info = generatorInfo(k);
  info.extraGenerator = extraGenerator;
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::setPlatform(Kit *k, const QString &platform) -> void
{
  auto info = generatorInfo(k);
  info.platform = platform;
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::setToolset(Kit *k, const QString &toolset) -> void
{
  auto info = generatorInfo(k);
  info.toolset = toolset;
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::set(Kit *k, const QString &generator, const QString &extraGenerator, const QString &platform, const QString &toolset) -> void
{
  GeneratorInfo info(generator, extraGenerator, platform, toolset);
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::generatorArguments(const Kit *k) -> QStringList
{
  QStringList result;
  auto info = generatorInfo(k);
  if (info.generator.isEmpty())
    return result;

  if (info.extraGenerator.isEmpty()) {
    result.append("-G" + info.generator);
  } else {
    result.append("-G" + info.extraGenerator + " - " + info.generator);
  }

  if (!info.platform.isEmpty())
    result.append("-A" + info.platform);

  if (!info.toolset.isEmpty())
    result.append("-T" + info.toolset);

  return result;
}

auto CMakeGeneratorKitAspect::generatorCMakeConfig(const ProjectExplorer::Kit *k) -> CMakeConfig
{
  CMakeConfig config;

  auto info = generatorInfo(k);
  if (info.generator.isEmpty())
    return config;

  config << CMakeConfigItem("CMAKE_GENERATOR", info.generator.toUtf8());

  if (!info.extraGenerator.isEmpty())
    config << CMakeConfigItem("CMAKE_EXTRA_GENERATOR", info.extraGenerator.toUtf8());

  if (!info.platform.isEmpty())
    config << CMakeConfigItem("CMAKE_GENERATOR_PLATFORM", info.platform.toUtf8());

  if (!info.toolset.isEmpty())
    config << CMakeConfigItem("CMAKE_GENERATOR_TOOLSET", info.toolset.toUtf8());

  return config;
}

auto CMakeGeneratorKitAspect::isMultiConfigGenerator(const Kit *k) -> bool
{
  const auto generator = CMakeGeneratorKitAspect::generator(k);
  return generator.indexOf("Visual Studio") != -1 || generator == "Xcode" || generator == "Ninja Multi-Config";
}

auto CMakeGeneratorKitAspect::defaultValue(const Kit *k) const -> QVariant
{
  QTC_ASSERT(k, return QVariant());

  auto tool = CMakeKitAspect::cmakeTool(k);
  if (!tool)
    return QVariant();

  if (isIos(k))
    return GeneratorInfo("Xcode").toVariant();

  const auto known = tool->supportedGenerators();
  auto it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
    return g.matches("Ninja");
  });
  if (it != known.constEnd()) {
    const auto hasNinja = [k]() {
      auto settings = Internal::CMakeProjectPlugin::projectTypeSpecificSettings();

      if (settings->ninjaPath.filePath().isEmpty()) {
        auto env = k->buildEnvironment();
        return !env.searchInPath("ninja").isEmpty();
      }
      return true;
    }();

    if (hasNinja)
      return GeneratorInfo("Ninja").toVariant();
  }

  if (tool->filePath().osType() == OsTypeWindows) {
    // *sigh* Windows with its zoo of incompatible stuff again...
    auto tc = ToolChainKitAspect::cxxToolChain(k);
    if (tc && tc->typeId() == ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID) {
      it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
        return g.matches("MinGW Makefiles");
      });
    } else {
      it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
        return g.matches("NMake Makefiles") || g.matches("NMake Makefiles JOM");
      });
      if (ProjectExplorerPlugin::projectExplorerSettings().useJom) {
        it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
          return g.matches("NMake Makefiles JOM");
        });
      }

      if (it == known.constEnd()) {
        it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
          return g.matches("NMake Makefiles");
        });
      }
    }
  } else {
    // Unix-oid OSes:
    it = std::find_if(known.constBegin(), known.constEnd(), [](const CMakeTool::Generator &g) {
      return g.matches("Unix Makefiles");
    });
  }
  if (it == known.constEnd())
    it = known.constBegin(); // Fallback to the first generator...
  if (it == known.constEnd())
    return QVariant();

  return GeneratorInfo(it->name).toVariant();
}

auto CMakeGeneratorKitAspect::validate(const Kit *k) const -> Tasks
{
  auto tool = CMakeKitAspect::cmakeTool(k);
  if (!tool)
    return {};

  Tasks result;
  const auto addWarning = [&result](const QString &desc) {
    result << BuildSystemTask(Task::Warning, desc);
  };

  if (!tool->isValid()) {
    addWarning(tr("CMake Tool is unconfigured, CMake generator will be ignored."));
  } else {
    const auto info = generatorInfo(k);
    auto known = tool->supportedGenerators();
    auto it = std::find_if(known.constBegin(), known.constEnd(), [info](const CMakeTool::Generator &g) {
      return g.matches(info.generator, info.extraGenerator);
    });
    if (it == known.constEnd()) {
      addWarning(tr("CMake Tool does not support the configured generator."));
    } else {
      if (!it->supportsPlatform && !info.platform.isEmpty())
        addWarning(tr("Platform is not supported by the selected CMake generator."));
      if (!it->supportsToolset && !info.toolset.isEmpty())
        addWarning(tr("Toolset is not supported by the selected CMake generator."));
    }
    if (!tool->hasFileApi()) {
      addWarning(tr("The selected CMake binary does not support file-api. " "%1 will not be able to parse CMake projects.").arg(Orca::Plugin::Core::IDE_DISPLAY_NAME));
    }
  }

  return result;
}

auto CMakeGeneratorKitAspect::setup(Kit *k) -> void
{
  if (!k || k->hasValue(id()))
    return;
  GeneratorInfo info;
  info.fromVariant(defaultValue(k));
  setGeneratorInfo(k, info);
}

auto CMakeGeneratorKitAspect::fix(Kit *k) -> void
{
  const CMakeTool *tool = CMakeKitAspect::cmakeTool(k);
  const auto info = generatorInfo(k);

  if (!tool)
    return;
  auto known = tool->supportedGenerators();
  auto it = std::find_if(known.constBegin(), known.constEnd(), [info](const CMakeTool::Generator &g) {
    return g.matches(info.generator, info.extraGenerator);
  });
  if (it == known.constEnd()) {
    GeneratorInfo dv;
    dv.fromVariant(defaultValue(k));
    setGeneratorInfo(k, dv);
  } else {
    const GeneratorInfo dv(isIos(k) ? QString("Xcode") : info.generator, info.extraGenerator, it->supportsPlatform ? info.platform : QString(), it->supportsToolset ? info.toolset : QString());
    setGeneratorInfo(k, dv);
  }
}

auto CMakeGeneratorKitAspect::upgrade(Kit *k) -> void
{
  QTC_ASSERT(k, return);

  const auto value = k->value(GENERATOR_ID);
  if (value.type() != QVariant::Map) {
    GeneratorInfo info;
    const auto fullName = value.toString();
    const int pos = fullName.indexOf(" - ");
    if (pos >= 0) {
      info.generator = fullName.mid(pos + 3);
      info.extraGenerator = fullName.mid(0, pos);
    } else {
      info.generator = fullName;
    }
    setGeneratorInfo(k, info);
  }
}

auto CMakeGeneratorKitAspect::toUserOutput(const Kit *k) const -> KitAspect::ItemList
{
  const auto info = generatorInfo(k);
  QString message;
  if (info.generator.isEmpty()) {
    message = tr("<Use Default Generator>");
  } else {
    message = tr("Generator: %1<br>Extra generator: %2").arg(info.generator).arg(info.extraGenerator);
    if (!info.platform.isEmpty())
      message += "<br/>" + tr("Platform: %1").arg(info.platform);
    if (!info.toolset.isEmpty())
      message += "<br/>" + tr("Toolset: %1").arg(info.toolset);
  }
  return {{tr("CMake Generator"), message}};
}

auto CMakeGeneratorKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  return new CMakeGeneratorKitAspectWidget(k, this);
}

auto CMakeGeneratorKitAspect::addToBuildEnvironment(const Kit *k, Environment &env) const -> void
{
  auto info = generatorInfo(k);
  if (info.generator == "NMake Makefiles JOM") {
    if (env.searchInPath("jom.exe").exists())
      return;
    env.appendOrSetPath(Orca::Plugin::Core::ICore::libexecPath());
    env.appendOrSetPath(Orca::Plugin::Core::ICore::libexecPath("jom"));
  }
}

// --------------------------------------------------------------------
// CMakeConfigurationKitAspect:
// --------------------------------------------------------------------

const char CONFIGURATION_ID[] = "CMake.ConfigurationKitInformation";
const char ADDITIONAL_CONFIGURATION_ID[] = "CMake.AdditionalConfigurationParameters";

const char CMAKE_C_TOOLCHAIN_KEY[] = "CMAKE_C_COMPILER";
const char CMAKE_CXX_TOOLCHAIN_KEY[] = "CMAKE_CXX_COMPILER";
const char CMAKE_QMAKE_KEY[] = "QT_QMAKE_EXECUTABLE";
const char CMAKE_PREFIX_PATH_KEY[] = "CMAKE_PREFIX_PATH";

class CMakeConfigurationKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeConfigurationKitAspect)
public:
  CMakeConfigurationKitAspectWidget(Kit *kit, const KitAspect *ki) : KitAspectWidget(kit, ki), m_summaryLabel(createSubWidget<ElidingLabel>()), m_manageButton(createSubWidget<QPushButton>())
  {
    refresh();
    m_manageButton->setText(tr("Change..."));
    connect(m_manageButton, &QAbstractButton::clicked, this, &CMakeConfigurationKitAspectWidget::editConfigurationChanges);
  }

private:
  // KitAspectWidget interface
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_summaryLabel);
    builder.addItem(m_summaryLabel);
    builder.addItem(m_manageButton);
  }

  auto makeReadOnly() -> void override
  {
    m_manageButton->setEnabled(false);
    if (m_dialog)
      m_dialog->reject();
  }

  auto refresh() -> void override
  {
    const auto current = CMakeConfigurationKitAspect::toArgumentsList(kit());
    const auto additionalText = CMakeConfigurationKitAspect::additionalConfiguration(kit());
    const auto labelText = additionalText.isEmpty() ? current.join(' ') : current.join(' ') + " " + additionalText;

    m_summaryLabel->setText(labelText);

    if (m_editor)
      m_editor->setPlainText(current.join('\n'));

    if (m_additionalEditor)
      m_additionalEditor->setText(additionalText);
  }

  auto editConfigurationChanges() -> void
  {
    if (m_dialog) {
      m_dialog->activateWindow();
      m_dialog->raise();
      return;
    }

    QTC_ASSERT(!m_editor, return);

    const CMakeTool *tool = CMakeKitAspect::cmakeTool(kit());

    m_dialog = new QDialog(m_summaryLabel->window());
    m_dialog->setWindowTitle(tr("Edit CMake Configuration"));
    auto layout = new QVBoxLayout(m_dialog);
    m_editor = new QPlainTextEdit;
    auto editorLabel = new QLabel(m_dialog);
    editorLabel->setText(tr("Enter one CMake <a href=\"variable\">variable</a> per line.<br/>" "To set a variable, use -D&lt;variable&gt;:&lt;type&gt;=&lt;value&gt;.<br/>" "&lt;type&gt; can have one of the following values: FILEPATH, PATH, " "BOOL, INTERNAL, or STRING."));
    connect(editorLabel, &QLabel::linkActivated, this, [=](const QString &) {
      CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake-variables.7.html");
    });
    m_editor->setMinimumSize(800, 200);

    auto chooser = new VariableChooser(m_dialog);
    chooser->addSupportedWidget(m_editor);
    chooser->addMacroExpanderProvider([this]() { return kit()->macroExpander(); });

    m_additionalEditor = new QLineEdit;
    auto additionalLabel = new QLabel(m_dialog);
    additionalLabel->setText(tr("Additional CMake <a href=\"options\">options</a>:"));
    connect(additionalLabel, &QLabel::linkActivated, this, [=](const QString &) {
      CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake.1.html#options");
    });

    auto additionalChooser = new VariableChooser(m_dialog);
    additionalChooser->addSupportedWidget(m_additionalEditor);
    additionalChooser->addMacroExpanderProvider([this]() { return kit()->macroExpander(); });

    auto additionalLayout = new QHBoxLayout();
    additionalLayout->addWidget(additionalLabel);
    additionalLayout->addWidget(m_additionalEditor);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Reset | QDialogButtonBox::Cancel);

    layout->addWidget(m_editor);
    layout->addWidget(editorLabel);
    layout->addLayout(additionalLayout);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, m_dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, m_dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::clicked, m_dialog, [buttons, this](QAbstractButton *button) {
      if (button != buttons->button(QDialogButtonBox::Reset))
        return;
      KitGuard guard(kit());
      CMakeConfigurationKitAspect::setConfiguration(kit(), CMakeConfigurationKitAspect::defaultConfiguration(kit()));
      CMakeConfigurationKitAspect::setAdditionalConfiguration(kit(), QString());
    });
    connect(m_dialog, &QDialog::accepted, this, &CMakeConfigurationKitAspectWidget::acceptChangesDialog);
    connect(m_dialog, &QDialog::rejected, this, &CMakeConfigurationKitAspectWidget::closeChangesDialog);
    connect(buttons->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &CMakeConfigurationKitAspectWidget::applyChanges);

    refresh();
    m_dialog->show();
  }

  auto applyChanges() -> void
  {
    QTC_ASSERT(m_editor, return);
    KitGuard guard(kit());

    QStringList unknownOptions;
    const auto config = CMakeConfig::fromArguments(m_editor->toPlainText().split('\n'), unknownOptions);
    CMakeConfigurationKitAspect::setConfiguration(kit(), config);

    auto additionalConfiguration = m_additionalEditor->text();
    if (!unknownOptions.isEmpty()) {
      if (!additionalConfiguration.isEmpty())
        additionalConfiguration += " ";
      additionalConfiguration += ProcessArgs::joinArgs(unknownOptions);
    }
    CMakeConfigurationKitAspect::setAdditionalConfiguration(kit(), additionalConfiguration);
  }

  auto closeChangesDialog() -> void
  {
    m_dialog->deleteLater();
    m_dialog = nullptr;
    m_editor = nullptr;
    m_additionalEditor = nullptr;
  }

  auto acceptChangesDialog() -> void
  {
    applyChanges();
    closeChangesDialog();
  }

  QLabel *m_summaryLabel;
  QPushButton *m_manageButton;
  QDialog *m_dialog = nullptr;
  QPlainTextEdit *m_editor = nullptr;
  QLineEdit *m_additionalEditor = nullptr;
};

CMakeConfigurationKitAspect::CMakeConfigurationKitAspect()
{
  setObjectName(QLatin1String("CMakeConfigurationKitAspect"));
  setId(CONFIGURATION_ID);
  setDisplayName(tr("CMake Configuration"));
  setDescription(tr("Default configuration passed to CMake when setting up a project."));
  setPriority(18000);
}

auto CMakeConfigurationKitAspect::configuration(const Kit *k) -> CMakeConfig
{
  if (!k)
    return CMakeConfig();
  const auto tmp = k->value(CONFIGURATION_ID).toStringList();
  return Utils::transform(tmp, &CMakeConfigItem::fromString);
}

auto CMakeConfigurationKitAspect::setConfiguration(Kit *k, const CMakeConfig &config) -> void
{
  if (!k)
    return;
  const auto tmp = Utils::transform(config.toList(), [](const CMakeConfigItem &i) { return i.toString(); });
  k->setValue(CONFIGURATION_ID, tmp);
}

auto CMakeConfigurationKitAspect::additionalConfiguration(const ProjectExplorer::Kit *k) -> QString
{
  if (!k)
    return QString();
  return k->value(ADDITIONAL_CONFIGURATION_ID).toString();
}

auto CMakeConfigurationKitAspect::setAdditionalConfiguration(ProjectExplorer::Kit *k, const QString &config) -> void
{
  if (!k)
    return;
  k->setValue(ADDITIONAL_CONFIGURATION_ID, config);
}

auto CMakeConfigurationKitAspect::toStringList(const Kit *k) -> QStringList
{
  auto current = Utils::transform(CMakeConfigurationKitAspect::configuration(k).toList(), [](const CMakeConfigItem &i) { return i.toString(); });
  current = Utils::filtered(current, [](const QString &s) { return !s.isEmpty(); });
  return current;
}

auto CMakeConfigurationKitAspect::fromStringList(Kit *k, const QStringList &in) -> void
{
  CMakeConfig result;
  for (const auto &s : in) {
    const auto item = CMakeConfigItem::fromString(s);
    if (!item.key.isEmpty())
      result << item;
  }
  setConfiguration(k, result);
}

auto CMakeConfigurationKitAspect::toArgumentsList(const Kit *k) -> QStringList
{
  auto current = Utils::transform(CMakeConfigurationKitAspect::configuration(k).toList(), [](const CMakeConfigItem &i) {
    return i.toArgument(nullptr);
  });
  current = Utils::filtered(current, [](const QString &s) { return s != "-D" || s != "-U"; });
  return current;
}

auto CMakeConfigurationKitAspect::defaultConfiguration(const Kit *k) -> CMakeConfig
{
  Q_UNUSED(k)
  CMakeConfig config;
  // Qt4:
  config << CMakeConfigItem(CMAKE_QMAKE_KEY, CMakeConfigItem::FILEPATH, "%{Qt:qmakeExecutable}");
  // Qt5:
  config << CMakeConfigItem(CMAKE_PREFIX_PATH_KEY, CMakeConfigItem::PATH, "%{Qt:QT_INSTALL_PREFIX}");

  config << CMakeConfigItem(CMAKE_C_TOOLCHAIN_KEY, CMakeConfigItem::FILEPATH, "%{Compiler:Executable:C}");
  config << CMakeConfigItem(CMAKE_CXX_TOOLCHAIN_KEY, CMakeConfigItem::FILEPATH, "%{Compiler:Executable:Cxx}");

  return config;
}

auto CMakeConfigurationKitAspect::defaultValue(const Kit *k) const -> QVariant
{
  // FIXME: Convert preload scripts
  auto config = defaultConfiguration(k);
  const auto tmp = Utils::transform(config.toList(), [](const CMakeConfigItem &i) { return i.toString(); });
  return tmp;
}

auto CMakeConfigurationKitAspect::validate(const Kit *k) const -> Tasks
{
  QTC_ASSERT(k, return Tasks());

  const QtSupport::QtVersion *const version = QtSupport::QtKitAspect::qtVersion(k);
  const ToolChain *const tcC = ToolChainKitAspect::cToolChain(k);
  const ToolChain *const tcCxx = ToolChainKitAspect::cxxToolChain(k);
  const auto config = configuration(k);

  const auto isQt4 = version && version->qtVersion() < QtSupport::QtVersionNumber(5, 0, 0);
  FilePath qmakePath;        // This is relative to the cmake used for building.
  QStringList qtInstallDirs; // This is relativ to the cmake used for building.
  FilePath tcCPath;
  FilePath tcCxxPath;
  for (const auto &i : config) {
    // Do not use expand(QByteArray) as we cannot be sure the input is latin1
    const auto expandedValue = FilePath::fromString(k->macroExpander()->expand(QString::fromUtf8(i.value)));
    if (i.key == CMAKE_QMAKE_KEY)
      qmakePath = expandedValue;
    else if (i.key == CMAKE_C_TOOLCHAIN_KEY)
      tcCPath = expandedValue;
    else if (i.key == CMAKE_CXX_TOOLCHAIN_KEY)
      tcCxxPath = expandedValue;
    else if (i.key == CMAKE_PREFIX_PATH_KEY)
      qtInstallDirs = CMakeConfigItem::cmakeSplitValue(expandedValue.path());
  }

  Tasks result;
  const auto addWarning = [&result](const QString &desc) {
    result << BuildSystemTask(Task::Warning, desc);
  };

  // Validate Qt:
  if (qmakePath.isEmpty()) {
    if (version && version->isValid() && isQt4) {
      addWarning(tr("CMake configuration has no path to qmake binary set, " "even though the kit has a valid Qt version."));
    }
  } else {
    if (!version || !version->isValid()) {
      addWarning(tr("CMake configuration has a path to a qmake binary set, " "even though the kit has no valid Qt version."));
    } else if (qmakePath != version->qmakeFilePath() && isQt4) {
      addWarning(tr("CMake configuration has a path to a qmake binary set " "that does not match the qmake binary path " "configured in the Qt version."));
    }
  }
  if (version && !qtInstallDirs.contains(version->prefix().path()) && !isQt4) {
    if (version->isValid()) {
      addWarning(tr("CMake configuration has no CMAKE_PREFIX_PATH set " "that points to the kit Qt version."));
    }
  }

  // Validate Toolchains:
  if (tcCPath.isEmpty()) {
    if (tcC && tcC->isValid()) {
      addWarning(tr("CMake configuration has no path to a C compiler set, " "even though the kit has a valid tool chain."));
    }
  } else {
    if (!tcC || !tcC->isValid()) {
      addWarning(tr("CMake configuration has a path to a C compiler set, " "even though the kit has no valid tool chain."));
    } else if (tcCPath != tcC->compilerCommand()) {
      addWarning(tr("CMake configuration has a path to a C compiler set " "that does not match the compiler path " "configured in the tool chain of the kit."));
    }
  }

  if (tcCxxPath.isEmpty()) {
    if (tcCxx && tcCxx->isValid()) {
      addWarning(tr("CMake configuration has no path to a C++ compiler set, " "even though the kit has a valid tool chain."));
    }
  } else {
    if (!tcCxx || !tcCxx->isValid()) {
      addWarning(tr("CMake configuration has a path to a C++ compiler set, " "even though the kit has no valid tool chain."));
    } else if (tcCxxPath != tcCxx->compilerCommand()) {
      addWarning(tr("CMake configuration has a path to a C++ compiler set " "that does not match the compiler path " "configured in the tool chain of the kit."));
    }
  }

  return result;
}

auto CMakeConfigurationKitAspect::setup(Kit *k) -> void
{
  if (k && !k->hasValue(CONFIGURATION_ID))
    k->setValue(CONFIGURATION_ID, defaultValue(k));
}

auto CMakeConfigurationKitAspect::fix(Kit *k) -> void
{
  Q_UNUSED(k)
}

auto CMakeConfigurationKitAspect::toUserOutput(const Kit *k) const -> KitAspect::ItemList
{
  return {{tr("CMake Configuration"), toStringList(k).join("<br>")}};
}

auto CMakeConfigurationKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  if (!k)
    return nullptr;
  return new CMakeConfigurationKitAspectWidget(k, this);
}

} // namespace CMakeProjectManager
