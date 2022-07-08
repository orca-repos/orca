// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runconfigurationaspects.hpp"

#include "environmentaspect.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorersettings.hpp"
#include "runconfiguration.hpp"
#include "target.hpp"

#include <utils/detailsbutton.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/utilsicons.hpp>

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QToolButton>

using namespace Utils;

namespace ProjectExplorer {

/*!
    \class ProjectExplorer::TerminalAspect
    \inmodule QtCreator

    \brief The TerminalAspect class lets a user specify that an executable
    should be run in a separate terminal.

    The initial value is provided as a hint from the build systems.
*/

TerminalAspect::TerminalAspect()
{
  setDisplayName(tr("Terminal"));
  setId("TerminalAspect");
  setSettingsKey("RunConfiguration.UseTerminal");
  calculateUseTerminal();
  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::settingsChanged, this, &TerminalAspect::calculateUseTerminal);
}

/*!
    \reimp
*/
auto TerminalAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!m_checkBox);
  m_checkBox = new QCheckBox(tr("Run in terminal"));
  m_checkBox->setChecked(m_useTerminal);
  builder.addItems({{}, m_checkBox.data()});
  connect(m_checkBox.data(), &QAbstractButton::clicked, this, [this] {
    m_userSet = true;
    m_useTerminal = m_checkBox->isChecked();
    emit changed();
  });
}

/*!
    \reimp
*/
auto TerminalAspect::fromMap(const QVariantMap &map) -> void
{
  if (map.contains(settingsKey())) {
    m_useTerminal = map.value(settingsKey()).toBool();
    m_userSet = true;
  } else {
    m_userSet = false;
  }

  if (m_checkBox)
    m_checkBox->setChecked(m_useTerminal);
}

/*!
    \reimp
*/
auto TerminalAspect::toMap(QVariantMap &data) const -> void
{
  if (m_userSet)
    data.insert(settingsKey(), m_useTerminal);
}

auto TerminalAspect::calculateUseTerminal() -> void
{
  if (m_userSet)
    return;
  bool useTerminal;
  switch (ProjectExplorerPlugin::projectExplorerSettings().terminalMode) {
  case Internal::TerminalMode::On:
    useTerminal = true;
    break;
  case Internal::TerminalMode::Off:
    useTerminal = false;
    break;
  default:
    useTerminal = m_useTerminalHint;
  }
  if (m_useTerminal != useTerminal) {
    m_useTerminal = useTerminal;
    emit changed();
  }
  if (m_checkBox)
    m_checkBox->setChecked(m_useTerminal);
}

/*!
    Returns whether a separate terminal should be used.
*/
auto TerminalAspect::useTerminal() const -> bool
{
  return m_useTerminal;
}

/*!
    Sets the initial value to \a hint.
*/
auto TerminalAspect::setUseTerminalHint(bool hint) -> void
{
  m_useTerminalHint = hint;
  calculateUseTerminal();
}

/*!
    Returns whether the user set the value.
*/
auto TerminalAspect::isUserSet() const -> bool
{
  return m_userSet;
}

/*!
    \class ProjectExplorer::WorkingDirectoryAspect
    \inmodule QtCreator

    \brief The WorkingDirectoryAspect class lets the user specify a
    working directory for running the executable.
*/

WorkingDirectoryAspect::WorkingDirectoryAspect()
{
  setDisplayName(tr("Working Directory"));
  setId("WorkingDirectoryAspect");
  setSettingsKey("RunConfiguration.WorkingDirectory");
}

/*!
    \reimp
*/
auto WorkingDirectoryAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!m_chooser);
  m_chooser = new PathChooser;
  if (QTC_GUARD(m_macroExpander))
    m_chooser->setMacroExpander(m_macroExpander);
  m_chooser->setHistoryCompleter(settingsKey());
  m_chooser->setExpectedKind(PathChooser::Directory);
  m_chooser->setPromptDialogTitle(tr("Select Working Directory"));
  m_chooser->setBaseDirectory(m_defaultWorkingDirectory);
  m_chooser->setFilePath(m_workingDirectory.isEmpty() ? m_defaultWorkingDirectory : m_workingDirectory);
  connect(m_chooser.data(), &PathChooser::pathChanged, this, [this]() {
    m_workingDirectory = m_chooser->rawFilePath();
    m_resetButton->setEnabled(m_workingDirectory != m_defaultWorkingDirectory);
  });

  m_resetButton = new QToolButton;
  m_resetButton->setToolTip(tr("Reset to Default"));
  m_resetButton->setIcon(Icons::RESET.icon());
  connect(m_resetButton.data(), &QAbstractButton::clicked, this, &WorkingDirectoryAspect::resetPath);
  m_resetButton->setEnabled(m_workingDirectory != m_defaultWorkingDirectory);

  if (m_envAspect) {
    connect(m_envAspect, &EnvironmentAspect::environmentChanged, m_chooser.data(), [this] {
      m_chooser->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(m_envAspect->environment()));
    });
    m_chooser->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(m_envAspect->environment()));
  }

  builder.addItems({tr("Working directory:"), m_chooser.data(), m_resetButton.data()});
}

auto WorkingDirectoryAspect::acquaintSiblings(const AspectContainer &siblings) -> void
{
  m_envAspect = siblings.aspect<EnvironmentAspect>();
}

auto WorkingDirectoryAspect::resetPath() -> void
{
  m_chooser->setFilePath(m_defaultWorkingDirectory);
}

/*!
    \reimp
*/
auto WorkingDirectoryAspect::fromMap(const QVariantMap &map) -> void
{
  m_workingDirectory = FilePath::fromString(map.value(settingsKey()).toString());
  m_defaultWorkingDirectory = FilePath::fromString(map.value(settingsKey() + ".default").toString());

  if (m_workingDirectory.isEmpty())
    m_workingDirectory = m_defaultWorkingDirectory;

  if (m_chooser)
    m_chooser->setFilePath(m_workingDirectory.isEmpty() ? m_defaultWorkingDirectory : m_workingDirectory);
}

/*!
    \reimp
*/
auto WorkingDirectoryAspect::toMap(QVariantMap &data) const -> void
{
  const auto wd = m_workingDirectory == m_defaultWorkingDirectory ? QString() : m_workingDirectory.toString();
  saveToMap(data, wd, QString(), settingsKey());
  saveToMap(data, m_defaultWorkingDirectory.toString(), QString(), settingsKey() + ".default");
}

/*!
    Returns the selected directory.

    Macros in the value are expanded using \a expander.
*/
auto WorkingDirectoryAspect::workingDirectory() const -> FilePath
{
  const auto env = m_envAspect ? m_envAspect->environment() : Environment::systemEnvironment();
  auto res = m_workingDirectory;
  auto workingDir = m_workingDirectory.path();
  if (m_macroExpander)
    workingDir = m_macroExpander->expandProcessArgs(workingDir);
  res.setPath(PathChooser::expandedDirectory(workingDir, env, QString()));
  return res;
}

auto WorkingDirectoryAspect::defaultWorkingDirectory() const -> FilePath
{
  return m_defaultWorkingDirectory;
}

/*!
    Returns the selected directory.

    Macros in the value are not expanded.
*/
auto WorkingDirectoryAspect::unexpandedWorkingDirectory() const -> FilePath
{
  return m_workingDirectory;
}

/*!
    Sets the default value to \a defaultWorkingDir.
*/
auto WorkingDirectoryAspect::setDefaultWorkingDirectory(const FilePath &defaultWorkingDir) -> void
{
  if (defaultWorkingDir == m_defaultWorkingDirectory)
    return;

  const auto oldDefaultDir = m_defaultWorkingDirectory;
  m_defaultWorkingDirectory = defaultWorkingDir;
  if (m_chooser)
    m_chooser->setBaseDirectory(m_defaultWorkingDirectory);

  if (m_workingDirectory.isEmpty() || m_workingDirectory == oldDefaultDir) {
    if (m_chooser)
      m_chooser->setFilePath(m_defaultWorkingDirectory);
    m_workingDirectory = defaultWorkingDir;
  }
}

auto WorkingDirectoryAspect::setMacroExpander(MacroExpander *macroExpander) -> void
{
  m_macroExpander = macroExpander;
}

/*!
    \internal
*/
auto WorkingDirectoryAspect::pathChooser() const -> PathChooser*
{
  return m_chooser;
}

/*!
    \class ProjectExplorer::ArgumentsAspect
    \inmodule QtCreator

    \brief The ArgumentsAspect class lets a user specify command line
    arguments for an executable.
*/

ArgumentsAspect::ArgumentsAspect()
{
  setDisplayName(tr("Arguments"));
  setId("ArgumentsAspect");
  setSettingsKey("RunConfiguration.Arguments");
  m_labelText = tr("Command line arguments:");
}

/*!
    Returns the main value of this aspect.

    Macros in the value are expanded using \a expander.
*/
auto ArgumentsAspect::arguments(const MacroExpander *expander) const -> QString
{
  QTC_ASSERT(expander, return m_arguments);
  if (m_currentlyExpanding)
    return m_arguments;

  m_currentlyExpanding = true;
  const auto expanded = expander->expandProcessArgs(m_arguments);
  m_currentlyExpanding = false;
  return expanded;
}

/*!
    Returns the main value of this aspect.

    Macros in the value are not expanded.
*/
auto ArgumentsAspect::unexpandedArguments() const -> QString
{
  return m_arguments;
}

/*!
    Sets the main value of this aspect to \a arguments.
*/
auto ArgumentsAspect::setArguments(const QString &arguments) -> void
{
  if (arguments != m_arguments) {
    m_arguments = arguments;
    emit changed();
  }
  if (m_chooser && m_chooser->text() != arguments)
    m_chooser->setText(arguments);
  if (m_multiLineChooser && m_multiLineChooser->toPlainText() != arguments)
    m_multiLineChooser->setPlainText(arguments);
}

/*!
    Sets the displayed label text to \a labelText.
*/
auto ArgumentsAspect::setLabelText(const QString &labelText) -> void
{
  m_labelText = labelText;
}

/*!
    Adds a button to reset the main value of this aspect to the value
    computed by \a resetter.
*/
auto ArgumentsAspect::setResetter(const std::function<QString()> &resetter) -> void
{
  m_resetter = resetter;
}

/*!
    Resets the main value of this aspect.
*/
auto ArgumentsAspect::resetArguments() -> void
{
  QString arguments;
  if (m_resetter)
    arguments = m_resetter();
  setArguments(arguments);
}

/*!
    \reimp
*/
auto ArgumentsAspect::fromMap(const QVariantMap &map) -> void
{
  const auto args = map.value(settingsKey());
  // Until 3.7 a QStringList was stored for Remote Linux
  if (args.type() == QVariant::StringList)
    m_arguments = ProcessArgs::joinArgs(args.toStringList(), OsTypeLinux);
  else
    m_arguments = args.toString();

  m_multiLine = map.value(settingsKey() + ".multi", false).toBool();

  if (m_multiLineButton)
    m_multiLineButton->setChecked(m_multiLine);
  if (!m_multiLine && m_chooser)
    m_chooser->setText(m_arguments);
  if (m_multiLine && m_multiLineChooser)
    m_multiLineChooser->setPlainText(m_arguments);
}

/*!
    \reimp
*/
auto ArgumentsAspect::toMap(QVariantMap &map) const -> void
{
  saveToMap(map, m_arguments, QString(), settingsKey());
  saveToMap(map, m_multiLine, false, settingsKey() + ".multi");
}

/*!
    \internal
*/
auto ArgumentsAspect::setupChooser() -> QWidget*
{
  if (m_multiLine) {
    if (!m_multiLineChooser) {
      m_multiLineChooser = new QPlainTextEdit;
      connect(m_multiLineChooser.data(), &QPlainTextEdit::textChanged, this, [this] { setArguments(m_multiLineChooser->toPlainText()); });
    }
    m_multiLineChooser->setPlainText(m_arguments);
    return m_multiLineChooser.data();
  }
  if (!m_chooser) {
    m_chooser = new FancyLineEdit;
    m_chooser->setHistoryCompleter(settingsKey());
    connect(m_chooser.data(), &QLineEdit::textChanged, this, &ArgumentsAspect::setArguments);
  }
  m_chooser->setText(m_arguments);
  return m_chooser.data();
}

/*!
    \reimp
*/
auto ArgumentsAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!m_chooser && !m_multiLineChooser && !m_multiLineButton);

  const auto container = new QWidget;
  const auto containerLayout = new QHBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->addWidget(setupChooser());
  m_multiLineButton = new ExpandButton;
  m_multiLineButton->setToolTip(tr("Toggle multi-line mode."));
  m_multiLineButton->setChecked(m_multiLine);
  connect(m_multiLineButton, &QCheckBox::clicked, this, [this](bool checked) {
    if (m_multiLine == checked)
      return;
    m_multiLine = checked;
    setupChooser();
    QWidget *oldWidget = nullptr;
    QWidget *newWidget = nullptr;
    if (m_multiLine) {
      oldWidget = m_chooser.data();
      newWidget = m_multiLineChooser.data();
    } else {
      oldWidget = m_multiLineChooser.data();
      newWidget = m_chooser.data();
    }
    QTC_ASSERT(!oldWidget == !newWidget, return);
    if (oldWidget) {
      QTC_ASSERT(oldWidget->parentWidget()->layout(), return);
      oldWidget->parentWidget()->layout()->replaceWidget(oldWidget, newWidget);
      delete oldWidget;
    }
  });
  containerLayout->addWidget(m_multiLineButton);
  containerLayout->setAlignment(m_multiLineButton, Qt::AlignTop);

  if (m_resetter) {
    m_resetButton = new QToolButton;
    m_resetButton->setToolTip(tr("Reset to Default"));
    m_resetButton->setIcon(Icons::RESET.icon());
    connect(m_resetButton.data(), &QAbstractButton::clicked, this, &ArgumentsAspect::resetArguments);
    containerLayout->addWidget(m_resetButton);
    containerLayout->setAlignment(m_resetButton, Qt::AlignTop);
  }

  builder.addItems({m_labelText, container});
}

/*!
    \class ProjectExplorer::ExecutableAspect
    \inmodule QtCreator

    \brief The ExecutableAspect class provides a building block to provide an
    executable for a RunConfiguration.

    It combines a StringAspect that is typically updated automatically
    by the build system's parsing results with an optional manual override.
*/

ExecutableAspect::ExecutableAspect()
{
  setDisplayName(tr("Executable"));
  setId("ExecutableAspect");
  setExecutablePathStyle(HostOsInfo::hostOs());
  m_executable.setPlaceHolderText(tr("<unknown>"));
  m_executable.setLabelText(tr("Executable:"));
  m_executable.setDisplayStyle(StringAspect::LabelDisplay);

  connect(&m_executable, &StringAspect::changed, this, &ExecutableAspect::changed);
}

/*!
    \internal
*/
ExecutableAspect::~ExecutableAspect()
{
  delete m_alternativeExecutable;
  m_alternativeExecutable = nullptr;
}

/*!
   Sets the display style of the paths to the default used on \a osType,
   backslashes on Windows, forward slashes elsewhere.

   \sa Utils::StringAspect::setDisplayFilter()
*/
auto ExecutableAspect::setExecutablePathStyle(OsType osType) -> void
{
  m_executable.setDisplayFilter([osType](const QString &pathName) {
    return OsSpecificAspects::pathWithNativeSeparators(osType, pathName);
  });
}

/*!
   Sets the settings key for history completion to \a historyCompleterKey.

   \sa Utils::PathChooser::setHistoryCompleter()
*/
auto ExecutableAspect::setHistoryCompleter(const QString &historyCompleterKey) -> void
{
  m_executable.setHistoryCompleter(historyCompleterKey);
  if (m_alternativeExecutable)
    m_alternativeExecutable->setHistoryCompleter(historyCompleterKey);
}

/*!
   Sets the acceptable kind of path values to \a expectedKind.

   \sa Utils::PathChooser::setExpectedKind()
*/
auto ExecutableAspect::setExpectedKind(const PathChooser::Kind expectedKind) -> void
{
  m_executable.setExpectedKind(expectedKind);
  if (m_alternativeExecutable)
    m_alternativeExecutable->setExpectedKind(expectedKind);
}

/*!
   Sets the environment in which paths will be searched when the expected kind
   of paths is chosen as PathChooser::Command or PathChooser::ExistingCommand
   to \a env.

   \sa Utils::StringAspect::setEnvironmentChange()
*/
auto ExecutableAspect::setEnvironmentChange(const EnvironmentChange &change) -> void
{
  m_executable.setEnvironmentChange(change);
  if (m_alternativeExecutable)
    m_alternativeExecutable->setEnvironmentChange(change);
}

/*!
   Sets the display \a style for aspect.

   \sa Utils::StringAspect::setDisplayStyle()
*/
auto ExecutableAspect::setDisplayStyle(StringAspect::DisplayStyle style) -> void
{
  m_executable.setDisplayStyle(style);
}

/*!
   Makes an auto-detected executable overridable by the user.

   The \a overridingKey specifies the settings key for the user-provided executable,
   the \a useOverridableKey the settings key for the fact that it
   is actually overridden the user.

   \sa Utils::StringAspect::makeCheckable()
*/
auto ExecutableAspect::makeOverridable(const QString &overridingKey, const QString &useOverridableKey) -> void
{
  QTC_ASSERT(!m_alternativeExecutable, return);
  m_alternativeExecutable = new StringAspect;
  m_alternativeExecutable->setDisplayStyle(StringAspect::LineEditDisplay);
  m_alternativeExecutable->setLabelText(tr("Alternate executable on device:"));
  m_alternativeExecutable->setSettingsKey(overridingKey);
  m_alternativeExecutable->makeCheckable(StringAspect::CheckBoxPlacement::Right, tr("Use this command instead"), useOverridableKey);
  connect(m_alternativeExecutable, &StringAspect::changed, this, &ExecutableAspect::changed);
}

/*!
    Returns the path of the executable specified by this aspect. In case
    the user selected a manual override this will be the value specified
    by the user.

    \sa makeOverridable()
 */
auto ExecutableAspect::executable() const -> FilePath
{
  if (m_alternativeExecutable && m_alternativeExecutable->isChecked())
    return m_alternativeExecutable->filePath();

  return m_executable.filePath();
}

/*!
    \reimp
*/
auto ExecutableAspect::addToLayout(LayoutBuilder &builder) -> void
{
  m_executable.addToLayout(builder);
  if (m_alternativeExecutable)
    m_alternativeExecutable->addToLayout(builder.finishRow());
}

/*!
    Sets the label text for the main chooser to
    \a labelText.

    \sa Utils::StringAspect::setLabelText()
*/
auto ExecutableAspect::setLabelText(const QString &labelText) -> void
{
  m_executable.setLabelText(labelText);
}

/*!
    Sets the place holder text for the main chooser to
    \a placeHolderText.

    \sa Utils::StringAspect::setPlaceHolderText()
*/
auto ExecutableAspect::setPlaceHolderText(const QString &placeHolderText) -> void
{
  m_executable.setPlaceHolderText(placeHolderText);
}

/*!
    Sets the value of the main chooser to \a executable.
*/
auto ExecutableAspect::setExecutable(const FilePath &executable) -> void
{
  m_executable.setFilePath(executable);
  m_executable.setShowToolTipOnLabel(true);
}

/*!
    Sets the settings key to \a key.
*/
auto ExecutableAspect::setSettingsKey(const QString &key) -> void
{
  BaseAspect::setSettingsKey(key);
  m_executable.setSettingsKey(key);
}

/*!
  \reimp
*/
auto ExecutableAspect::fromMap(const QVariantMap &map) -> void
{
  m_executable.fromMap(map);
  if (m_alternativeExecutable)
    m_alternativeExecutable->fromMap(map);
}

/*!
   \reimp
*/
auto ExecutableAspect::toMap(QVariantMap &map) const -> void
{
  m_executable.toMap(map);
  if (m_alternativeExecutable)
    m_alternativeExecutable->toMap(map);
}

/*!
    \class ProjectExplorer::UseLibraryPathsAspect
    \inmodule QtCreator

    \brief The UseLibraryPathsAspect class lets a user specify whether build
    library search paths should be added to the relevant environment
    variables.

    This modifies DYLD_LIBRARY_PATH and DYLD_FRAMEWORK_PATH on Mac, PATH
    on Windows and LD_LIBRARY_PATH everywhere else.
*/

UseLibraryPathsAspect::UseLibraryPathsAspect()
{
  setId("UseLibraryPath");
  setSettingsKey("RunConfiguration.UseLibrarySearchPath");
  if (HostOsInfo::isMacHost()) {
    setLabel(tr("Add build library search path to DYLD_LIBRARY_PATH and DYLD_FRAMEWORK_PATH"), LabelPlacement::AtCheckBox);
  } else if (HostOsInfo::isWindowsHost()) {
    setLabel(tr("Add build library search path to PATH"), LabelPlacement::AtCheckBox);
  } else {
    setLabel(tr("Add build library search path to LD_LIBRARY_PATH"), LabelPlacement::AtCheckBox);
  }
  setValue(ProjectExplorerPlugin::projectExplorerSettings().addLibraryPathsToRunEnv);
}

/*!
    \class ProjectExplorer::UseDyldSuffixAspect
    \inmodule QtCreator

    \brief The UseDyldSuffixAspect class lets a user specify whether the
    DYLD_IMAGE_SUFFIX environment variable should be used on Mac.
*/

UseDyldSuffixAspect::UseDyldSuffixAspect()
{
  setId("UseDyldSuffix");
  setSettingsKey("RunConfiguration.UseDyldImageSuffix");
  setLabel(tr("Use debug version of frameworks (DYLD_IMAGE_SUFFIX=_debug)"), LabelPlacement::AtCheckBox);
}

/*!
    \class ProjectExplorer::RunAsRootAspect
    \inmodule QtCreator

    \brief The RunAsRootAspect class lets a user specify whether the
    application should run with root permissions.
*/

RunAsRootAspect::RunAsRootAspect()
{
  setId("RunAsRoot");
  setSettingsKey("RunConfiguration.RunAsRoot");
  setLabel(tr("Run as root user"), LabelPlacement::AtCheckBox);
}

} // namespace ProjectExplorer
