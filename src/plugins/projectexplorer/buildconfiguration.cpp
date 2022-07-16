// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildconfiguration.hpp"

#include "buildaspects.hpp"
#include "buildinfo.hpp"
#include "buildsteplist.hpp"
#include "buildstepspage.hpp"
#include "buildsystem.hpp"
#include "customparser.hpp"
#include "environmentwidget.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "namedwidget.hpp"
#include "projectexplorerconstants.hpp"
#include "projectexplorer.hpp"
#include "project.hpp"
#include "projecttree.hpp"
#include "session.hpp"
#include "target.hpp"
#include "toolchain.hpp"

#include <core/core-interface.hpp>
#include <core/core-document-interface.hpp>

#include <utils/algorithm.hpp>
#include <utils/detailswidget.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/mimetypes/mimetype.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/qtcassert.hpp>
#include <utils/variablechooser.hpp>

#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QLoggingCategory>
#include <QVBoxLayout>

using namespace Utils;

constexpr char BUILD_STEP_LIST_COUNT[] = "ProjectExplorer.BuildConfiguration.BuildStepListCount";
constexpr char BUILD_STEP_LIST_PREFIX[] = "ProjectExplorer.BuildConfiguration.BuildStepList.";
constexpr char CLEAR_SYSTEM_ENVIRONMENT_KEY[] = "ProjectExplorer.BuildConfiguration.ClearSystemEnvironment";
constexpr char USER_ENVIRONMENT_CHANGES_KEY[] = "ProjectExplorer.BuildConfiguration.UserEnvironmentChanges";
constexpr char CUSTOM_PARSERS_KEY[] = "ProjectExplorer.BuildConfiguration.CustomParsers";
constexpr char PARSE_STD_OUT_KEY[] = "ProjectExplorer.BuildConfiguration.ParseStandardOutput";

Q_LOGGING_CATEGORY(bcLog, "qtc.buildconfig", QtWarningMsg)

namespace ProjectExplorer {
namespace Internal {

class BuildEnvironmentWidget : public NamedWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::BuildEnvironmentWidget)

public:
  explicit BuildEnvironmentWidget(BuildConfiguration *bc) : NamedWidget(tr("Build Environment"))
  {
    const auto clearBox = new QCheckBox(tr("Clear system environment"), this);
    clearBox->setChecked(!bc->useSystemEnvironment());

    auto envWidget = new EnvironmentWidget(this, EnvironmentWidget::TypeLocal, clearBox);
    envWidget->setBaseEnvironment(bc->baseEnvironment());
    envWidget->setBaseEnvironmentText(bc->baseEnvironmentText());
    envWidget->setUserChanges(bc->userEnvironmentChanges());

    connect(envWidget, &EnvironmentWidget::userChangesChanged, this, [bc, envWidget] {
      bc->setUserEnvironmentChanges(envWidget->userChanges());
    });

    connect(clearBox, &QAbstractButton::toggled, this, [bc, envWidget](bool checked) {
      bc->setUseSystemEnvironment(!checked);
      envWidget->setBaseEnvironment(bc->baseEnvironment());
      envWidget->setBaseEnvironmentText(bc->baseEnvironmentText());
    });

    connect(bc, &BuildConfiguration::environmentChanged, this, [bc, envWidget] {
      envWidget->setBaseEnvironment(bc->baseEnvironment());
      envWidget->setBaseEnvironmentText(bc->baseEnvironmentText());
    });

    const auto vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(clearBox);
    vbox->addWidget(envWidget);
  }
};

class CustomParsersBuildWidget : public NamedWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::CustomParsersBuildWidget)

public:
  CustomParsersBuildWidget(BuildConfiguration *bc) : NamedWidget(tr("Custom Output Parsers"))
  {
    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto pasteStdOutCB = new QCheckBox(tr("Parse standard output during build"), this);
    pasteStdOutCB->setToolTip(tr("Makes output parsers look for diagnostics " "on stdout rather than stderr."));
    pasteStdOutCB->setChecked(bc->parseStdOut());
    layout->addWidget(pasteStdOutCB);

    connect(pasteStdOutCB, &QCheckBox::clicked, bc, &BuildConfiguration::setParseStdOut);
    const auto selectionWidget = new CustomParsersSelectionWidget(this);
    layout->addWidget(selectionWidget);

    connect(selectionWidget, &CustomParsersSelectionWidget::selectionChanged, [selectionWidget, bc] {
      bc->setCustomParsers(selectionWidget->selectedParsers());
    });
    selectionWidget->setSelectedParsers(bc->customParsers());
  }
};

class BuildConfigurationPrivate {
public:
  BuildConfigurationPrivate(BuildConfiguration *bc) : m_buildSteps(bc, Constants::BUILDSTEPS_BUILD), m_cleanSteps(bc, Constants::BUILDSTEPS_CLEAN) {}

  bool m_clearSystemEnvironment = false;
  EnvironmentItems m_userEnvironmentChanges;
  BuildStepList m_buildSteps;
  BuildStepList m_cleanSteps;
  BuildDirectoryAspect *m_buildDirectoryAspect = nullptr;
  StringAspect *m_tooltipAspect = nullptr;
  FilePath m_lastEmittedBuildDirectory;
  mutable Environment m_cachedEnvironment;
  QString m_configWidgetDisplayName;
  bool m_configWidgetHasFrame = false;
  QList<Id> m_initialBuildSteps;
  QList<Id> m_initialCleanSteps;
  MacroExpander m_macroExpander;
  bool m_parseStdOut = false;
  QList<Id> m_customParsers;

  // FIXME: Remove.
  BuildConfiguration::BuildType m_initialBuildType = BuildConfiguration::Unknown;
  std::function<void(const BuildInfo &)> m_initializer;
};

} // Internal

BuildConfiguration::BuildConfiguration(Target *target, Id id) : ProjectConfiguration(target, id), d(new Internal::BuildConfigurationPrivate(this))
{
  QTC_CHECK(target && target == this->target());

  const auto expander = macroExpander();
  expander->setDisplayName(tr("Build Settings"));
  expander->setAccumulating(true);
  expander->registerSubProvider([target] { return target->macroExpander(); });

  expander->registerVariable("buildDir", tr("Build directory"), [this] { return buildDirectory().toUserOutput(); });

  // TODO: Remove "Current" variants in ~4.16.
  expander->registerVariable(Constants::VAR_CURRENTBUILD_NAME, tr("Name of current build"), [this] { return displayName(); }, false);

  expander->registerVariable("BuildConfig:Name", tr("Name of the build configuration"), [this] { return displayName(); });

  expander->registerPrefix(Constants::VAR_CURRENTBUILD_ENV, tr("Variables in the current build environment"), [this](const QString &var) { return environment().expandedValueForKey(var); }, false);
  expander->registerPrefix("BuildConfig:Env", tr("Variables in the build configuration's environment"), [this](const QString &var) { return environment().expandedValueForKey(var); });

  updateCacheAndEmitEnvironmentChanged();
  connect(Orca::Plugin::Core::ICore::instance(), &Orca::Plugin::Core::ICore::systemEnvironmentChanged, this, &BuildConfiguration::updateCacheAndEmitEnvironmentChanged);
  connect(target, &Target::kitChanged, this, &BuildConfiguration::updateCacheAndEmitEnvironmentChanged);
  connect(this, &BuildConfiguration::environmentChanged, this, &BuildConfiguration::emitBuildDirectoryChanged);
  connect(target->project(), &Project::environmentChanged, this, &BuildConfiguration::updateCacheAndEmitEnvironmentChanged);
  // Many macroexpanders are based on the current project, so they may change the environment:
  connect(ProjectTree::instance(), &ProjectTree::currentProjectChanged, this, &BuildConfiguration::updateCacheAndEmitEnvironmentChanged);

  d->m_buildDirectoryAspect = addAspect<BuildDirectoryAspect>(this);
  d->m_buildDirectoryAspect->setBaseFileName(target->project()->projectDirectory());
  d->m_buildDirectoryAspect->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(environment()));
  d->m_buildDirectoryAspect->setMacroExpanderProvider([this] { return macroExpander(); });
  connect(d->m_buildDirectoryAspect, &StringAspect::changed, this, &BuildConfiguration::emitBuildDirectoryChanged);
  connect(this, &BuildConfiguration::environmentChanged, this, [this] {
    d->m_buildDirectoryAspect->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(environment()));
    emit this->target()->buildEnvironmentChanged(this);
  });

  d->m_tooltipAspect = addAspect<StringAspect>();
  d->m_tooltipAspect->setLabelText(tr("Tooltip in target selector:"));
  d->m_tooltipAspect->setToolTip(tr("Appears as a tooltip when hovering the build configuration"));
  d->m_tooltipAspect->setDisplayStyle(StringAspect::LineEditDisplay);
  d->m_tooltipAspect->setSettingsKey("ProjectExplorer.BuildConfiguration.Tooltip");
  connect(d->m_tooltipAspect, &StringAspect::changed, this, [this] {
    setToolTip(d->m_tooltipAspect->value());
  });

  connect(target, &Target::parsingStarted, this, &BuildConfiguration::enabledChanged);
  connect(target, &Target::parsingFinished, this, &BuildConfiguration::enabledChanged);
  connect(this, &BuildConfiguration::enabledChanged, this, [this] {
    if (isActive() && project() == SessionManager::startupProject()) {
      ProjectExplorerPlugin::updateActions();
      ProjectExplorerPlugin::updateRunActions();
    }
  });
}

BuildConfiguration::~BuildConfiguration()
{
  delete d;
}

auto BuildConfiguration::buildDirectory() const -> FilePath
{
  auto path = FilePath::fromUserInput(environment().expandVariables(d->m_buildDirectoryAspect->value().trimmed()));
  path = macroExpander()->expand(path);
  path = path.cleanPath();

  const auto projectDir = target()->project()->projectDirectory();

  return projectDir.resolvePath(path);
}

auto BuildConfiguration::rawBuildDirectory() const -> FilePath
{
  return d->m_buildDirectoryAspect->filePath();
}

auto BuildConfiguration::setBuildDirectory(const FilePath &dir) -> void
{
  if (dir == d->m_buildDirectoryAspect->filePath())
    return;
  d->m_buildDirectoryAspect->setFilePath(dir);
  const auto fixedDir = BuildDirectoryAspect::fixupDir(buildDirectory());
  if (!fixedDir.isEmpty())
    d->m_buildDirectoryAspect->setFilePath(fixedDir);
  emitBuildDirectoryChanged();
}

auto BuildConfiguration::addConfigWidgets(const std::function<void(NamedWidget *)> &adder) -> void
{
  if (const auto generalConfigWidget = createConfigWidget())
    adder(generalConfigWidget);

  adder(new Internal::BuildStepListWidget(buildSteps()));
  adder(new Internal::BuildStepListWidget(cleanSteps()));

  auto subConfigWidgets = createSubConfigWidgets();
  foreach(NamedWidget *subConfigWidget, subConfigWidgets)
    adder(subConfigWidget);
}

auto BuildConfiguration::doInitialize(const BuildInfo &info) -> void
{
  setDisplayName(info.displayName);
  setDefaultDisplayName(info.displayName);
  setBuildDirectory(info.buildDirectory);

  d->m_initialBuildType = info.buildType;

  for (const auto id : qAsConst(d->m_initialBuildSteps))
    d->m_buildSteps.appendStep(id);

  for (const auto id : qAsConst(d->m_initialCleanSteps))
    d->m_cleanSteps.appendStep(id);

  acquaintAspects();

  if (d->m_initializer)
    d->m_initializer(info);
}

auto BuildConfiguration::macroExpander() const -> MacroExpander*
{
  return &d->m_macroExpander;
}

auto BuildConfiguration::createBuildDirectory() -> bool
{
  const auto result = buildDirectory().ensureWritableDir();
  buildDirectoryAspect()->validateInput();
  return result;
}

auto BuildConfiguration::setInitializer(const std::function<void(const BuildInfo &)> &initializer) -> void
{
  d->m_initializer = initializer;
}

auto BuildConfiguration::createConfigWidget() -> NamedWidget*
{
  const auto named = new NamedWidget(d->m_configWidgetDisplayName);

  QWidget *widget = nullptr;

  if (d->m_configWidgetHasFrame) {
    const auto container = new DetailsWidget(named);
    widget = new QWidget(container);
    container->setState(DetailsWidget::NoSummary);
    container->setWidget(widget);

    const auto vbox = new QVBoxLayout(named);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(container);
  } else {
    widget = named;
  }

  Layouting::Form builder;
  for (const auto aspect : aspects()) {
    if (aspect->isVisible())
      aspect->addToLayout(builder.finishRow());
  }
  builder.attachTo(widget, false);

  return named;
}

auto BuildConfiguration::createSubConfigWidgets() -> QList<NamedWidget*>
{
  return {new Internal::BuildEnvironmentWidget(this), new Internal::CustomParsersBuildWidget(this)};
}

auto BuildConfiguration::buildSystem() const -> BuildSystem*
{
  QTC_CHECK(target()->fallbackBuildSystem());
  return target()->fallbackBuildSystem();
}

auto BuildConfiguration::buildSteps() const -> BuildStepList*
{
  return &d->m_buildSteps;
}

auto BuildConfiguration::cleanSteps() const -> BuildStepList*
{
  return &d->m_cleanSteps;
}

auto BuildConfiguration::appendInitialBuildStep(Id id) -> void
{
  d->m_initialBuildSteps.append(id);
}

auto BuildConfiguration::appendInitialCleanStep(Id id) -> void
{
  d->m_initialCleanSteps.append(id);
}

auto BuildConfiguration::toMap() const -> QVariantMap
{
  auto map = ProjectConfiguration::toMap();

  map.insert(QLatin1String(CLEAR_SYSTEM_ENVIRONMENT_KEY), d->m_clearSystemEnvironment);
  map.insert(QLatin1String(USER_ENVIRONMENT_CHANGES_KEY), EnvironmentItem::toStringList(d->m_userEnvironmentChanges));

  map.insert(QLatin1String(BUILD_STEP_LIST_COUNT), 2);
  map.insert(QLatin1String(BUILD_STEP_LIST_PREFIX) + QString::number(0), d->m_buildSteps.toMap());
  map.insert(QLatin1String(BUILD_STEP_LIST_PREFIX) + QString::number(1), d->m_cleanSteps.toMap());

  map.insert(PARSE_STD_OUT_KEY, d->m_parseStdOut);
  map.insert(CUSTOM_PARSERS_KEY, transform(d->m_customParsers, &Id::toSetting));

  return map;
}

auto BuildConfiguration::fromMap(const QVariantMap &map) -> bool
{
  d->m_clearSystemEnvironment = map.value(QLatin1String(CLEAR_SYSTEM_ENVIRONMENT_KEY)).toBool();
  d->m_userEnvironmentChanges = EnvironmentItem::fromStringList(map.value(QLatin1String(USER_ENVIRONMENT_CHANGES_KEY)).toStringList());

  updateCacheAndEmitEnvironmentChanged();

  d->m_buildSteps.clear();
  d->m_cleanSteps.clear();

  const auto maxI = map.value(QLatin1String(BUILD_STEP_LIST_COUNT), 0).toInt();
  for (auto i = 0; i < maxI; ++i) {
    auto data = map.value(QLatin1String(BUILD_STEP_LIST_PREFIX) + QString::number(i)).toMap();
    if (data.isEmpty()) {
      qWarning() << "No data for build step list" << i << "found!";
      continue;
    }
    auto id = idFromMap(data);
    if (id == Constants::BUILDSTEPS_BUILD) {
      if (!d->m_buildSteps.fromMap(data))
        qWarning() << "Failed to restore build step list";
    } else if (id == Constants::BUILDSTEPS_CLEAN) {
      if (!d->m_cleanSteps.fromMap(data))
        qWarning() << "Failed to restore clean step list";
    } else {
      qWarning() << "Ignoring unknown step list";
    }
  }

  d->m_parseStdOut = map.value(PARSE_STD_OUT_KEY).toBool();
  d->m_customParsers = transform(map.value(CUSTOM_PARSERS_KEY).toList(), &Id::fromSetting);

  const auto res = ProjectConfiguration::fromMap(map);
  setToolTip(d->m_tooltipAspect->value());
  return res;
}

auto BuildConfiguration::updateCacheAndEmitEnvironmentChanged() -> void
{
  auto env = baseEnvironment();
  env.modify(userEnvironmentChanges());
  if (env == d->m_cachedEnvironment)
    return;
  d->m_cachedEnvironment = env;
  emit environmentChanged(); // might trigger buildDirectoryChanged signal!
}

auto BuildConfiguration::emitBuildDirectoryChanged() -> void
{
  if (buildDirectory() != d->m_lastEmittedBuildDirectory) {
    d->m_lastEmittedBuildDirectory = buildDirectory();
    emit buildDirectoryChanged();
  }
}

auto BuildConfiguration::buildDirectoryAspect() const -> BuildDirectoryAspect*
{
  return d->m_buildDirectoryAspect;
}

auto BuildConfiguration::setConfigWidgetDisplayName(const QString &display) -> void
{
  d->m_configWidgetDisplayName = display;
}

auto BuildConfiguration::setBuildDirectoryHistoryCompleter(const QString &history) -> void
{
  d->m_buildDirectoryAspect->setHistoryCompleter(history);
}

auto BuildConfiguration::setConfigWidgetHasFrame(bool configWidgetHasFrame) -> void
{
  d->m_configWidgetHasFrame = configWidgetHasFrame;
}

auto BuildConfiguration::setBuildDirectorySettingsKey(const QString &key) -> void
{
  d->m_buildDirectoryAspect->setSettingsKey(key);
}

auto BuildConfiguration::baseEnvironment() const -> Environment
{
  Environment result;
  if (useSystemEnvironment())
    result = Environment::systemEnvironment();
  addToEnvironment(result);
  kit()->addToBuildEnvironment(result);
  result.modify(project()->additionalEnvironment());
  return result;
}

auto BuildConfiguration::baseEnvironmentText() const -> QString
{
  if (useSystemEnvironment())
    return tr("System Environment");
  else
    return tr("Clean Environment");
}

auto BuildConfiguration::environment() const -> Environment
{
  return d->m_cachedEnvironment;
}

auto BuildConfiguration::setUseSystemEnvironment(bool b) -> void
{
  if (useSystemEnvironment() == b)
    return;
  d->m_clearSystemEnvironment = !b;
  updateCacheAndEmitEnvironmentChanged();
}

auto BuildConfiguration::addToEnvironment(Environment &env) const -> void
{
  Q_UNUSED(env)
}

auto BuildConfiguration::customParsers() const -> const QList<Id>
{
  return d->m_customParsers;
}

auto BuildConfiguration::setCustomParsers(const QList<Id> &parsers) -> void
{
  d->m_customParsers = parsers;
}

auto BuildConfiguration::parseStdOut() const -> bool { return d->m_parseStdOut; }
auto BuildConfiguration::setParseStdOut(bool b) -> void { d->m_parseStdOut = b; }

auto BuildConfiguration::useSystemEnvironment() const -> bool
{
  return !d->m_clearSystemEnvironment;
}

auto BuildConfiguration::userEnvironmentChanges() const -> EnvironmentItems
{
  return d->m_userEnvironmentChanges;
}

auto BuildConfiguration::setUserEnvironmentChanges(const EnvironmentItems &diff) -> void
{
  if (d->m_userEnvironmentChanges == diff)
    return;
  d->m_userEnvironmentChanges = diff;
  updateCacheAndEmitEnvironmentChanged();
}

auto BuildConfiguration::isEnabled() const -> bool
{
  return buildSystem()->hasParsingData();
}

auto BuildConfiguration::disabledReason() const -> QString
{
  if (!buildSystem()->hasParsingData())
    return (tr("The project was not parsed successfully."));
  return QString();
}

auto BuildConfiguration::regenerateBuildFiles(Node *node) -> bool
{
  Q_UNUSED(node)
  return false;
}

auto BuildConfiguration::restrictNextBuild(const RunConfiguration *rc) -> void
{
  Q_UNUSED(rc)
}

auto BuildConfiguration::buildType() const -> BuildType
{
  return d->m_initialBuildType;
}

auto BuildConfiguration::buildTypeName(BuildType type) -> QString
{
  switch (type) {
  case Debug:
    return QLatin1String("debug");
  case Profile:
    return QLatin1String("profile");
  case Release:
    return QLatin1String("release");
  case Unknown: // fallthrough
  default:
    return QLatin1String("unknown");
  }
}

auto BuildConfiguration::isActive() const -> bool
{
  return target()->isActive() && target()->activeBuildConfiguration() == this;
}

auto BuildConfiguration::buildDirectoryFromTemplate(const FilePath &projectDir, const FilePath &mainFilePath, const QString &projectName, const Kit *kit, const QString &bcName, BuildType buildType, SpaceHandling spaceHandling) -> FilePath
{
  MacroExpander exp;

  qCDebug(bcLog) << Q_FUNC_INFO << projectDir << mainFilePath << projectName << bcName;

  // TODO: Remove "Current" variants in ~4.16
  exp.registerFileVariables(Constants::VAR_CURRENTPROJECT_PREFIX, QCoreApplication::translate("ProjectExplorer", "Main file of current project"), [mainFilePath] { return mainFilePath; }, false);
  exp.registerFileVariables("Project", QCoreApplication::translate("ProjectExplorer", "Main file of the project"), [mainFilePath] { return mainFilePath; });
  exp.registerVariable(Constants::VAR_CURRENTPROJECT_NAME, QCoreApplication::translate("ProjectExplorer", "Name of current project"), [projectName] { return projectName; }, false);
  exp.registerVariable("Project:Name", QCoreApplication::translate("ProjectExplorer", "Name of the project"), [projectName] { return projectName; });
  exp.registerVariable(Constants::VAR_CURRENTBUILD_NAME, QCoreApplication::translate("ProjectExplorer", "Name of current build"), [bcName] { return bcName; }, false);
  exp.registerVariable("BuildConfig:Name", QCoreApplication::translate("ProjectExplorer", "Name of the project's active build configuration"), [bcName] { return bcName; });
  exp.registerVariable("CurrentBuild:Type", QCoreApplication::translate("ProjectExplorer", "Type of current build"), [buildType] { return buildTypeName(buildType); }, false);
  exp.registerVariable("BuildConfig:Type", QCoreApplication::translate("ProjectExplorer", "Type of the project's active build configuration"), [buildType] { return buildTypeName(buildType); });
  exp.registerSubProvider([kit] { return kit->macroExpander(); });

  auto buildDir = ProjectExplorerPlugin::buildDirectoryTemplate();
  qCDebug(bcLog) << "build dir template:" << buildDir;
  buildDir = exp.expand(buildDir);
  qCDebug(bcLog) << "expanded build:" << buildDir;
  if (spaceHandling == ReplaceSpaces)
    buildDir.replace(" ", "-");

  return projectDir.resolvePath(buildDir);
}

///
// IBuildConfigurationFactory
///

static QList<BuildConfigurationFactory*> g_buildConfigurationFactories;

BuildConfigurationFactory::BuildConfigurationFactory()
{
  // Note: Order matters as first-in-queue wins.
  g_buildConfigurationFactories.prepend(this);
}

BuildConfigurationFactory::~BuildConfigurationFactory()
{
  g_buildConfigurationFactories.removeOne(this);
}

auto BuildConfigurationFactory::reportIssues(Kit *kit, const QString &projectPath, const QString &buildDir) const -> const Tasks
{
  if (m_issueReporter)
    return m_issueReporter(kit, projectPath, buildDir);
  return {};
}

auto BuildConfigurationFactory::allAvailableBuilds(const Target *parent) const -> const QList<BuildInfo>
{
  QTC_ASSERT(m_buildGenerator, return {});
  auto list = m_buildGenerator(parent->kit(), parent->project()->projectFilePath(), false);
  for (auto &info : list) {
    info.factory = this;
    info.kitId = parent->kit()->id();
  }
  return list;
}

auto BuildConfigurationFactory::allAvailableSetups(const Kit *k, const FilePath &projectPath) const -> const QList<BuildInfo>
{
  QTC_ASSERT(m_buildGenerator, return {});
  auto list = m_buildGenerator(k, projectPath, /* forSetup = */ true);
  for (auto &info : list) {
    info.factory = this;
    info.kitId = k->id();
  }
  return list;
}

auto BuildConfigurationFactory::supportsTargetDeviceType(Id id) const -> bool
{
  if (m_supportedTargetDeviceTypes.isEmpty())
    return true;
  return m_supportedTargetDeviceTypes.contains(id);
}

// setup
auto BuildConfigurationFactory::find(const Kit *k, const FilePath &projectPath) -> BuildConfigurationFactory*
{
  QTC_ASSERT(k, return nullptr);
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);
  for (const auto factory : qAsConst(g_buildConfigurationFactories)) {
    if (mimeTypeForFile(projectPath).matchesName(factory->m_supportedProjectMimeTypeName) && factory->supportsTargetDeviceType(deviceType))
      return factory;
  }
  return nullptr;
}

// create
auto BuildConfigurationFactory::find(Target *parent) -> BuildConfigurationFactory*
{
  for (const auto factory : qAsConst(g_buildConfigurationFactories)) {
    if (factory->canHandle(parent))
      return factory;
  }
  return nullptr;
}

auto BuildConfigurationFactory::setSupportedProjectType(Id id) -> void
{
  m_supportedProjectType = id;
}

auto BuildConfigurationFactory::setSupportedProjectMimeTypeName(const QString &mimeTypeName) -> void
{
  m_supportedProjectMimeTypeName = mimeTypeName;
}

auto BuildConfigurationFactory::addSupportedTargetDeviceType(Id id) -> void
{
  m_supportedTargetDeviceTypes.append(id);
}

auto BuildConfigurationFactory::canHandle(const Target *target) const -> bool
{
  if (m_supportedProjectType.isValid() && m_supportedProjectType != target->project()->id())
    return false;

  if (containsType(target->project()->projectIssues(target->kit()), Task::TaskType::Error))
    return false;

  if (!supportsTargetDeviceType(DeviceTypeKitAspect::deviceTypeId(target->kit())))
    return false;

  return true;
}

auto BuildConfigurationFactory::setBuildGenerator(const BuildGenerator &buildGenerator) -> void
{
  m_buildGenerator = buildGenerator;
}

auto BuildConfigurationFactory::setIssueReporter(const IssueReporter &issueReporter) -> void
{
  m_issueReporter = issueReporter;
}

auto BuildConfigurationFactory::create(Target *parent, const BuildInfo &info) const -> BuildConfiguration*
{
  if (!canHandle(parent))
    return nullptr;
  QTC_ASSERT(m_creator, return nullptr);

  const auto bc = m_creator(parent);
  if (bc)
    bc->doInitialize(info);

  return bc;
}

auto BuildConfigurationFactory::restore(Target *parent, const QVariantMap &map) -> BuildConfiguration*
{
  const auto id = idFromMap(map);
  for (const auto factory : qAsConst(g_buildConfigurationFactories)) {
    QTC_ASSERT(factory->m_creator, return nullptr);
    if (!factory->canHandle(parent))
      continue;
    if (!id.name().startsWith(factory->m_buildConfigId.name()))
      continue;
    auto bc = factory->m_creator(parent);
    bc->acquaintAspects();
    QTC_ASSERT(bc, return nullptr);
    if (!bc->fromMap(map)) {
      delete bc;
      bc = nullptr;
    }
    return bc;
  }
  return nullptr;
}

auto BuildConfigurationFactory::clone(Target *parent, const BuildConfiguration *source) -> BuildConfiguration*
{
  return restore(parent, source->toMap());
}

} // namespace ProjectExplorer
