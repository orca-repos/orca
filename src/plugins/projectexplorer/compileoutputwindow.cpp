// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "compileoutputwindow.hpp"

#include "buildmanager.hpp"
#include "ioutputparser.hpp"
#include "projectexplorer.hpp"
#include "projectexplorericons.hpp"
#include "projectexplorersettings.hpp"
#include "showoutputtaskhandler.hpp"
#include "task.hpp"
#include "taskhub.hpp"

#include <core/core-output-window.hpp>
#include <core/core-interface.hpp>
#include <core/core-constants.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/behaviorsettings.hpp>
#include <utils/algorithm.hpp>
#include <utils/outputformatter.hpp>
#include <utils/proxyaction.hpp>
#include <utils/theme/theme.hpp>
#include <utils/utilsicons.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace ProjectExplorer {
namespace Internal {

constexpr char SETTINGS_KEY[] = "ProjectExplorer/CompileOutput/Zoom";
constexpr char C_COMPILE_OUTPUT[] = "ProjectExplorer.CompileOutput";
constexpr char POP_UP_KEY[] = "ProjectExplorer/Settings/ShowCompilerOutput";
constexpr char WRAP_OUTPUT_KEY[] = "ProjectExplorer/Settings/WrapBuildOutput";
constexpr char MAX_LINES_KEY[] = "ProjectExplorer/Settings/MaxBuildOutputLines";
constexpr char OPTIONS_PAGE_ID[] = "C.ProjectExplorer.CompileOutputOptions";

CompileOutputWindow::CompileOutputWindow(QAction *cancelBuildAction) : m_cancelBuildButton(new QToolButton), m_settingsButton(new QToolButton)
{
  const Orca::Plugin::Core::Context context(C_COMPILE_OUTPUT);
  m_outputWindow = new Orca::Plugin::Core::OutputWindow(context, SETTINGS_KEY);
  m_outputWindow->setWindowTitle(displayName());
  m_outputWindow->setWindowIcon(Icons::WINDOW.icon());
  m_outputWindow->setReadOnly(true);
  m_outputWindow->setUndoRedoEnabled(false);
  m_outputWindow->setMaxCharCount(Orca::Plugin::Core::DEFAULT_MAX_CHAR_COUNT);

  const auto cancelBuildProxyButton = Utils::ProxyAction::proxyActionWithIcon(cancelBuildAction, Utils::Icons::STOP_SMALL_TOOLBAR.icon());
  m_cancelBuildButton->setDefaultAction(cancelBuildProxyButton);
  m_settingsButton->setToolTip(tr("Open Settings Page"));
  m_settingsButton->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());

  auto updateFontSettings = [this] {
    m_outputWindow->setBaseFont(TextEditor::TextEditorSettings::fontSettings().font());
  };

  auto updateZoomEnabled = [this] {
    m_outputWindow->setWheelZoomEnabled(TextEditor::TextEditorSettings::behaviorSettings().m_scrollWheelZooming);
  };

  updateFontSettings();
  updateZoomEnabled();
  setupFilterUi("CompileOutputPane.Filter");
  setFilteringEnabled(true);

  connect(this, &IOutputPane::zoomInRequested, m_outputWindow, &Orca::Plugin::Core::OutputWindow::zoomIn);
  connect(this, &IOutputPane::zoomOutRequested, m_outputWindow, &Orca::Plugin::Core::OutputWindow::zoomOut);
  connect(this, &IOutputPane::resetZoomRequested, m_outputWindow, &Orca::Plugin::Core::OutputWindow::resetZoom);
  connect(TextEditor::TextEditorSettings::instance(), &TextEditor::TextEditorSettings::fontSettingsChanged, this, updateFontSettings);
  connect(TextEditor::TextEditorSettings::instance(), &TextEditor::TextEditorSettings::behaviorSettingsChanged, this, updateZoomEnabled);

  connect(m_settingsButton, &QToolButton::clicked, this, [] {
    Orca::Plugin::Core::ICore::showOptionsDialog(OPTIONS_PAGE_ID);
  });

  qRegisterMetaType<QTextCharFormat>("QTextCharFormat");

  m_handler = new ShowOutputTaskHandler(this, tr("Show Compile &Output"), tr("Show the output that generated this issue in the Compile Output pane."), tr("O"));
  ExtensionSystem::PluginManager::addObject(m_handler);
  setupContext(C_COMPILE_OUTPUT, m_outputWindow);
  loadSettings();
  updateFromSettings();
}

CompileOutputWindow::~CompileOutputWindow()
{
  ExtensionSystem::PluginManager::removeObject(m_handler);
  delete m_handler;
  delete m_cancelBuildButton;
  delete m_settingsButton;
}

auto CompileOutputWindow::updateFromSettings() -> void
{
  m_outputWindow->setWordWrapEnabled(m_settings.wrapOutput);
  m_outputWindow->setMaxCharCount(m_settings.maxCharCount);
}

auto CompileOutputWindow::hasFocus() const -> bool
{
  return m_outputWindow->window()->focusWidget() == m_outputWindow;
}

auto CompileOutputWindow::canFocus() const -> bool
{
  return true;
}

auto CompileOutputWindow::setFocus() -> void
{
  m_outputWindow->setFocus();
}

auto CompileOutputWindow::outputWidget(QWidget *) -> QWidget*
{
  return m_outputWindow;
}

auto CompileOutputWindow::toolBarWidgets() const -> QList<QWidget*>
{
  return QList<QWidget*>{m_cancelBuildButton, m_settingsButton} + IOutputPane::toolBarWidgets();
}

auto CompileOutputWindow::appendText(const QString &text, BuildStep::OutputFormat format) -> void
{
  auto fmt = Utils::NormalMessageFormat;
  switch (format) {
  case BuildStep::OutputFormat::Stdout:
    fmt = Utils::StdOutFormat;
    break;
  case BuildStep::OutputFormat::Stderr:
    fmt = Utils::StdErrFormat;
    break;
  case BuildStep::OutputFormat::NormalMessage:
    fmt = Utils::NormalMessageFormat;
    break;
  case BuildStep::OutputFormat::ErrorMessage:
    fmt = Utils::ErrorMessageFormat;
    break;

  }

  m_outputWindow->appendMessage(text, fmt);
}

auto CompileOutputWindow::clearContents() -> void
{
  m_outputWindow->clear();
}

auto CompileOutputWindow::priorityInStatusBar() const -> int
{
  return 50;
}

auto CompileOutputWindow::canNext() const -> bool
{
  return false;
}

auto CompileOutputWindow::canPrevious() const -> bool
{
  return false;
}

auto CompileOutputWindow::goToNext() -> void { }

auto CompileOutputWindow::goToPrev() -> void { }

auto CompileOutputWindow::canNavigate() const -> bool
{
  return false;
}

auto CompileOutputWindow::registerPositionOf(const Task &task, int linkedOutputLines, int skipLines, int offset) -> void
{
  m_outputWindow->registerPositionOf(task.taskId, linkedOutputLines, skipLines, offset);
}

auto CompileOutputWindow::flush() -> void
{
  m_outputWindow->flush();
}

auto CompileOutputWindow::reset() -> void
{
  m_outputWindow->reset();
}

auto CompileOutputWindow::setSettings(const CompileOutputSettings &settings) -> void
{
  m_settings = settings;
  storeSettings();
  updateFromSettings();
}

auto CompileOutputWindow::outputFormatter() const -> Utils::OutputFormatter*
{
  return m_outputWindow->outputFormatter();
}

auto CompileOutputWindow::updateFilter() -> void
{
  m_outputWindow->updateFilterProperties(filterText(), filterCaseSensitivity(), filterUsesRegexp(), filterIsInverted());
}

const bool kPopUpDefault = false;
const bool kWrapOutputDefault = true;

auto CompileOutputWindow::loadSettings() -> void
{
  const QSettings *const s = Orca::Plugin::Core::ICore::settings();
  m_settings.popUp = s->value(POP_UP_KEY, kPopUpDefault).toBool();
  m_settings.wrapOutput = s->value(WRAP_OUTPUT_KEY, kWrapOutputDefault).toBool();
  m_settings.maxCharCount = s->value(MAX_LINES_KEY, Orca::Plugin::Core::DEFAULT_MAX_CHAR_COUNT).toInt() * 100;
}

auto CompileOutputWindow::storeSettings() const -> void
{
  const auto s = Orca::Plugin::Core::ICore::settings();
  s->setValueWithDefault(POP_UP_KEY, m_settings.popUp, kPopUpDefault);
  s->setValueWithDefault(WRAP_OUTPUT_KEY, m_settings.wrapOutput, kWrapOutputDefault);
  s->setValueWithDefault(MAX_LINES_KEY, m_settings.maxCharCount / 100, Orca::Plugin::Core::DEFAULT_MAX_CHAR_COUNT);
}

class CompileOutputSettingsWidget : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::CompileOutputSettingsPage)
public:
  CompileOutputSettingsWidget()
  {
    const auto &settings = BuildManager::compileOutputSettings();
    m_wrapOutputCheckBox.setText(tr("Word-wrap output"));
    m_wrapOutputCheckBox.setChecked(settings.wrapOutput);
    m_popUpCheckBox.setText(tr("Open pane when building"));
    m_popUpCheckBox.setChecked(settings.popUp);
    m_maxCharsBox.setMaximum(100000000);
    m_maxCharsBox.setValue(settings.maxCharCount);
    const auto layout = new QVBoxLayout(this);
    layout->addWidget(&m_wrapOutputCheckBox);
    layout->addWidget(&m_popUpCheckBox);
    const auto maxCharsLayout = new QHBoxLayout;
    const auto msg = tr("Limit output to %1 characters");
    const auto parts = msg.split("%1") << QString() << QString();
    maxCharsLayout->addWidget(new QLabel(parts.at(0).trimmed()));
    maxCharsLayout->addWidget(&m_maxCharsBox);
    maxCharsLayout->addWidget(new QLabel(parts.at(1).trimmed()));
    maxCharsLayout->addStretch(1);
    layout->addLayout(maxCharsLayout);
    layout->addStretch(1);
  }

  auto apply() -> void final
  {
    CompileOutputSettings s;
    s.wrapOutput = m_wrapOutputCheckBox.isChecked();
    s.popUp = m_popUpCheckBox.isChecked();
    s.maxCharCount = m_maxCharsBox.value();
    BuildManager::setCompileOutputSettings(s);
  }

private:
  QCheckBox m_wrapOutputCheckBox;
  QCheckBox m_popUpCheckBox;
  QSpinBox m_maxCharsBox;
};

CompileOutputSettingsPage::CompileOutputSettingsPage()
{
  setId(OPTIONS_PAGE_ID);
  setDisplayName(CompileOutputSettingsWidget::tr("Compile Output"));
  setCategory(Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new CompileOutputSettingsWidget; });
}

} // Internal
} // ProjectExplorer
