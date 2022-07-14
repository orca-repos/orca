// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-output-pane-manager.hpp"

#include "core-action-container.hpp"
#include "core-action-manager.hpp"
#include "core-command-button.hpp"
#include "core-command.hpp"
#include "core-editor-interface.hpp"
#include "core-editor-manager.hpp"
#include "core-find-placeholder.hpp"
#include "core-interface.hpp"
#include "core-mode-manager.hpp"
#include "core-options-popup.hpp"
#include "core-output-pane-interface.hpp"
#include "core-output-pane.hpp"
#include "core-status-bar-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/proxyaction.hpp>
#include <utils/qtcassert.hpp>
#include <utils/styledbar.hpp>
#include <utils/stylehelper.hpp>
#include <utils/utilsicons.hpp>
#include <utils/theme/theme.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeLine>
#include <QToolButton>

using namespace Utils;

namespace Orca::Plugin::Core {

class OutputPaneData {
public:
  explicit OutputPaneData(IOutputPane *pane = nullptr) : pane(pane) {}

  IOutputPane *pane = nullptr;
  Id id;
  OutputPaneToggleButton *button = nullptr;
  QAction *action = nullptr;
};

static QVector<OutputPaneData> g_output_panes;
static bool g_manager_constructed = false; // For debugging reasons.

IOutputPane::IOutputPane(QObject *parent) : QObject(parent), m_zoom_in_button(new CommandButton), m_zoom_out_button(new CommandButton)
{
  // We need all pages first. Ignore latecomers and shout.
  QTC_ASSERT(!g_manager_constructed, return);
  g_output_panes.append(OutputPaneData(this));

  m_zoom_in_button->setIcon(Icons::PLUS_TOOLBAR.icon());
  m_zoom_in_button->setCommandId(ZOOM_IN);
  connect(m_zoom_in_button, &QToolButton::clicked, this, [this] { emit zoomInRequested(1); });

  m_zoom_out_button->setIcon(Icons::MINUS.icon());
  m_zoom_out_button->setCommandId(ZOOM_OUT);
  connect(m_zoom_out_button, &QToolButton::clicked, this, [this] { emit zoomOutRequested(1); });
}

IOutputPane::~IOutputPane()
{
  const auto i = indexOf(g_output_panes, equal(&OutputPaneData::pane, this));
  QTC_ASSERT(i >= 0, return);
  delete g_output_panes.at(i).button;
  g_output_panes.removeAt(i);
  delete m_zoom_in_button;
  delete m_zoom_out_button;
}

auto IOutputPane::toolBarWidgets() const -> QList<QWidget*>
{
  QList<QWidget*> widgets;

  if (m_filter_output_line_edit)
    widgets << m_filter_output_line_edit;

  return widgets << m_zoom_in_button << m_zoom_out_button;
}

auto IOutputPane::visibilityChanged(bool /*visible*/) -> void {}

auto IOutputPane::setFont(const QFont &font) -> void
{
  emit fontChanged(font);
}

auto IOutputPane::setWheelZoomEnabled(const bool enabled) -> void
{
  emit wheelZoomEnabledChanged(enabled);
}

auto IOutputPane::setupFilterUi(const QString &history_key) -> void
{
  m_filter_output_line_edit = new FancyLineEdit;
  m_filter_action_regexp = new QAction(this);
  m_filter_action_regexp->setCheckable(true);
  m_filter_action_regexp->setText(tr("Use Regular Expressions"));
  connect(m_filter_action_regexp, &QAction::toggled, this, &IOutputPane::setRegularExpressions);
  ActionManager::registerAction(m_filter_action_regexp, filterRegexpActionId());

  m_filter_action_case_sensitive = new QAction(this);
  m_filter_action_case_sensitive->setCheckable(true);
  m_filter_action_case_sensitive->setText(tr("Case Sensitive"));
  connect(m_filter_action_case_sensitive, &QAction::toggled, this, &IOutputPane::setCaseSensitive);
  ActionManager::registerAction(m_filter_action_case_sensitive, filterCaseSensitivityActionId());

  m_invert_filter_action = new QAction(this);
  m_invert_filter_action->setCheckable(true);
  m_invert_filter_action->setText(tr("Show Non-matching Lines"));
  connect(m_invert_filter_action, &QAction::toggled, this, [this] {
    m_invert_filter = m_invert_filter_action->isChecked();
    updateFilter();
  });
  ActionManager::registerAction(m_invert_filter_action, filterInvertedActionId());

  m_filter_output_line_edit->setPlaceholderText(tr("Filter output..."));
  m_filter_output_line_edit->setButtonVisible(FancyLineEdit::Left, true);
  m_filter_output_line_edit->setButtonIcon(FancyLineEdit::Left, Icons::MAGNIFIER.icon());
  m_filter_output_line_edit->setFiltering(true);
  m_filter_output_line_edit->setEnabled(false);
  m_filter_output_line_edit->setHistoryCompleter(history_key);

  connect(m_filter_output_line_edit, &FancyLineEdit::textChanged, this, &IOutputPane::updateFilter);
  connect(m_filter_output_line_edit, &FancyLineEdit::returnPressed, this, &IOutputPane::updateFilter);
  connect(m_filter_output_line_edit, &FancyLineEdit::leftButtonClicked, this, &IOutputPane::filterOutputButtonClicked);
}

auto IOutputPane::filterText() const -> QString
{
  return m_filter_output_line_edit->text();
}

auto IOutputPane::setFilteringEnabled(const bool enable) const -> void
{
  m_filter_output_line_edit->setEnabled(enable);
}

auto IOutputPane::setupContext(const char *context, QWidget *widget) -> void
{
  QTC_ASSERT(!m_context, return);
  m_context = new IContext(this);
  m_context->setContext(Context(context));
  m_context->setWidget(widget);
  ICore::addContextObject(m_context);

  const auto zoom_in_action = new QAction(this);
  ActionManager::registerAction(zoom_in_action, ZOOM_IN, m_context->context());
  connect(zoom_in_action, &QAction::triggered, this, [this] { emit zoomInRequested(1); });

  const auto zoom_out_action = new QAction(this);
  ActionManager::registerAction(zoom_out_action, ZOOM_OUT, m_context->context());
  connect(zoom_out_action, &QAction::triggered, this, [this] { emit zoomOutRequested(1); });

  const auto reset_zoom_action = new QAction(this);
  ActionManager::registerAction(reset_zoom_action, ZOOM_RESET, m_context->context());
  connect(reset_zoom_action, &QAction::triggered, this, &IOutputPane::resetZoomRequested);
}

auto IOutputPane::setZoomButtonsEnabled(const bool enabled) const -> void
{
  m_zoom_in_button->setEnabled(enabled);
  m_zoom_out_button->setEnabled(enabled);
}

auto IOutputPane::updateFilter() -> void
{
  QTC_ASSERT(false, qDebug() << "updateFilter() needs to get re-implemented");
}

auto IOutputPane::filterOutputButtonClicked() const -> void
{
  const auto popup = new OptionsPopup(m_filter_output_line_edit, {filterRegexpActionId(), filterCaseSensitivityActionId(), filterInvertedActionId()});
  popup->show();
}

auto IOutputPane::setRegularExpressions(const bool regular_expressions) -> void
{
  m_filter_regexp = regular_expressions;
  updateFilter();
}

auto IOutputPane::filterRegexpActionId() const -> Id
{
  return Id("OutputFilter.RegularExpressions").withSuffix(metaObject()->className());
}

auto IOutputPane::filterCaseSensitivityActionId() const -> Id
{
  return Id("OutputFilter.CaseSensitive").withSuffix(metaObject()->className());
}

auto IOutputPane::filterInvertedActionId() const -> Id
{
  return Id("OutputFilter.Invert").withSuffix(metaObject()->className());
}

auto IOutputPane::setCaseSensitive(const bool case_sensitive) -> void
{
  m_filter_case_sensitivity = case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  updateFilter();
}

constexpr char g_output_pane_settings_key_c[] = "OutputPaneVisibility";
constexpr char g_output_pane_id_key_c[] = "id";
constexpr char g_output_pane_visible_key_c[] = "visible";
constexpr int  g_button_border_width = 3;

static auto numberAreaWidth() -> int
{
  return orcaTheme()->flag(Theme::FlatToolBars) ? 15 : 19;
}

static OutputPaneManager *m_instance = nullptr;

auto OutputPaneManager::create() -> void
{
  m_instance = new OutputPaneManager;
}

auto OutputPaneManager::destroy() -> void
{
  delete m_instance;
  m_instance = nullptr;
}

auto OutputPaneManager::instance() -> OutputPaneManager*
{
  return m_instance;
}

auto OutputPaneManager::updateStatusButtons(const bool visible) const -> void
{
  const auto idx = currentIndex();

  if (idx == -1)
    return;

  QTC_ASSERT(idx < g_output_panes.size(), return);
  const auto &data = g_output_panes.at(idx);
  QTC_ASSERT(data.button, return);
  data.button->setChecked(visible);
  data.pane->visibilityChanged(visible);
}

auto OutputPaneManager::updateMaximizeButton(const bool maximized) -> void
{
  if (maximized) {
    m_instance->m_minMaxAction->setIcon(m_instance->m_minimizeIcon);
    m_instance->m_minMaxAction->setText(tr("Minimize Output Pane"));
  } else {
    m_instance->m_minMaxAction->setIcon(m_instance->m_maximizeIcon);
    m_instance->m_minMaxAction->setText(tr("Maximize Output Pane"));
  }
}

// Return shortcut as Alt+<number> or Cmd+<number> if number is a non-zero digit
static auto paneShortCut(const int number) -> QKeySequence
{
  if (number < 1 || number > 9)
    return {};

  constexpr int modifier = Qt::ALT;
  return {modifier | Qt::Key_0 + number};
}

OutputPaneManager::OutputPaneManager(QWidget *parent) : QWidget(parent), m_titleLabel(new QLabel), m_manageButton(new OutputPaneManageButton), m_closeButton(new QToolButton), m_minMaxButton(new QToolButton), m_outputWidgetPane(new QStackedWidget), m_opToolBarWidgets(new QStackedWidget), m_minimizeIcon(Icons::ARROW_DOWN.icon()), m_maximizeIcon(Icons::ARROW_UP.icon())
{
  setWindowTitle(tr("Output"));

  m_titleLabel->setContentsMargins(5, 0, 5, 0);
  m_clearAction = new QAction(this);
  m_clearAction->setIcon(Icons::CLEAN.icon());
  m_clearAction->setText(tr("Clear"));
  connect(m_clearAction, &QAction::triggered, this, &OutputPaneManager::clearPage);

  m_nextAction = new QAction(this);
  m_nextAction->setIcon(Icons::ARROW_DOWN_TOOLBAR.icon());
  m_nextAction->setText(tr("Next Item"));
  connect(m_nextAction, &QAction::triggered, this, &OutputPaneManager::slotNext);

  m_prevAction = new QAction(this);
  m_prevAction->setIcon(Icons::ARROW_UP_TOOLBAR.icon());
  m_prevAction->setText(tr("Previous Item"));
  connect(m_prevAction, &QAction::triggered, this, &OutputPaneManager::slotPrev);

  m_minMaxAction = new QAction(this);
  m_minMaxAction->setIcon(m_maximizeIcon);
  m_minMaxAction->setText(tr("Maximize Output Pane"));

  m_closeButton->setIcon(Icons::CLOSE_SPLIT_BOTTOM.icon());
  connect(m_closeButton, &QAbstractButton::clicked, this, &OutputPaneManager::slotHide);
  connect(ICore::instance(), &ICore::saveSettingsRequested, this, &OutputPaneManager::saveSettings);

  const auto main_layout = new QVBoxLayout;
  main_layout->setSpacing(0);
  main_layout->setContentsMargins(0, 0, 0, 0);
  m_toolBar = new StyledBar;
  const auto tool_layout = new QHBoxLayout(m_toolBar);
  tool_layout->setContentsMargins(0, 0, 0, 0);
  tool_layout->setSpacing(0);
  tool_layout->addWidget(m_titleLabel);
  tool_layout->addWidget(new StyledSeparator);
  m_clearButton = new QToolButton;
  tool_layout->addWidget(m_clearButton);
  m_prevToolButton = new QToolButton;
  tool_layout->addWidget(m_prevToolButton);
  m_nextToolButton = new QToolButton;
  tool_layout->addWidget(m_nextToolButton);
  tool_layout->addWidget(m_opToolBarWidgets);
  tool_layout->addWidget(m_minMaxButton);
  tool_layout->addWidget(m_closeButton);
  main_layout->addWidget(m_toolBar);
  main_layout->addWidget(m_outputWidgetPane, 10);
  main_layout->addWidget(new FindToolBarPlaceHolder(this));
  setLayout(main_layout);

  m_buttonsWidget = new QWidget;
  m_buttonsWidget->setObjectName("OutputPaneButtons"); // used for UI introduction
  m_buttonsWidget->setLayout(new QHBoxLayout);
  m_buttonsWidget->layout()->setContentsMargins(5, 0, 0, 0);
  m_buttonsWidget->layout()->setSpacing(orcaTheme()->flag(Theme::FlatToolBars) ? 9 : 4);

  StatusBarManager::addStatusBarWidget(m_buttonsWidget, StatusBarManager::Second);
  const auto mview = ActionManager::actionContainer(M_VIEW);

  // Window->Output Panes
  const auto mpanes = ActionManager::createMenu(M_VIEW_PANES);
  mview->addMenu(mpanes, G_VIEW_PANES);
  mpanes->menu()->setTitle(tr("Output &Panes"));
  mpanes->appendGroup("Coreplugin.OutputPane.ActionsGroup");
  mpanes->appendGroup("Coreplugin.OutputPane.PanesGroup");

  auto cmd = ActionManager::registerAction(m_clearAction, OUTPUTPANE_CLEAR);
  m_clearButton->setDefaultAction(ProxyAction::proxyActionWithIcon(m_clearAction, Icons::CLEAN_TOOLBAR.icon()));
  mpanes->addAction(cmd, "Coreplugin.OutputPane.ActionsGroup");

  cmd = ActionManager::registerAction(m_prevAction, "Coreplugin.OutputPane.previtem");
  cmd->setDefaultKeySequence(QKeySequence(tr("Shift+F6")));
  m_prevToolButton->setDefaultAction(ProxyAction::proxyActionWithIcon(m_prevAction, Icons::ARROW_UP_TOOLBAR.icon()));
  mpanes->addAction(cmd, "Coreplugin.OutputPane.ActionsGroup");

  cmd = ActionManager::registerAction(m_nextAction, "Coreplugin.OutputPane.nextitem");
  m_nextToolButton->setDefaultAction(ProxyAction::proxyActionWithIcon(m_nextAction, Icons::ARROW_DOWN_TOOLBAR.icon()));
  cmd->setDefaultKeySequence(QKeySequence(tr("F6")));
  mpanes->addAction(cmd, "Coreplugin.OutputPane.ActionsGroup");

  cmd = ActionManager::registerAction(m_minMaxAction, "Coreplugin.OutputPane.minmax");
  cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+9")));
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setAttribute(Command::CA_UpdateIcon);
  mpanes->addAction(cmd, "Coreplugin.OutputPane.ActionsGroup");
  connect(m_minMaxAction, &QAction::triggered, this, &OutputPaneManager::toggleMaximized);
  m_minMaxButton->setDefaultAction(cmd->action());

  mpanes->addSeparator("Coreplugin.OutputPane.ActionsGroup");
}

auto OutputPaneManager::initialize() -> void
{
  const auto mpanes = ActionManager::actionContainer(M_VIEW_PANES);
  const auto title_fm = m_instance->m_titleLabel->fontMetrics();
  auto min_title_width = 0;

  sort(g_output_panes, [](const OutputPaneData &d1, const OutputPaneData &d2) {
    return d1.pane->priorityInStatusBar() > d2.pane->priorityInStatusBar();
  });

  const auto n = static_cast<int>(g_output_panes.size());
  auto shortcut_number = 1;
  const Id base_id = "Orca.Pane.";

  for (auto i = 0; i != n; ++i) {
    auto &data = g_output_panes[i];
    auto out_pane = data.pane;
    const auto idx = m_instance->m_outputWidgetPane->addWidget(out_pane->outputWidget(m_instance));
    QTC_CHECK(idx == i);

    connect(out_pane, &IOutputPane::showPage, m_instance, [idx](int flags) {
      m_instance->showPage(idx, flags);
    });
    connect(out_pane, &IOutputPane::hidePage, m_instance, &OutputPaneManager::slotHide);
    connect(out_pane, &IOutputPane::togglePage, m_instance, [idx](int flags) {
      if (OutputPanePlaceHolder::isCurrentVisible() && m_instance->currentIndex() == idx)
        m_instance->slotHide();
      else
        m_instance->showPage(idx, flags);
    });
    connect(out_pane, &IOutputPane::navigateStateUpdate, m_instance, [idx, out_pane] {
      if (m_instance->currentIndex() == idx) {
        m_instance->m_prevAction->setEnabled(out_pane->canNavigate() && out_pane->canPrevious());
        m_instance->m_nextAction->setEnabled(out_pane->canNavigate() && out_pane->canNext());
      }
    });

    const auto tool_buttons_container = new QWidget(m_instance->m_opToolBarWidgets);
    const auto tool_buttons_layout = new QHBoxLayout;

    tool_buttons_layout->setContentsMargins(0, 0, 0, 0);
    tool_buttons_layout->setSpacing(0);

    for(const auto tool_button: out_pane->toolBarWidgets())
      tool_buttons_layout->addWidget(tool_button);

    tool_buttons_layout->addStretch(5);
    tool_buttons_container->setLayout(tool_buttons_layout);

    m_instance->m_opToolBarWidgets->addWidget(tool_buttons_container);
    min_title_width = qMax(min_title_width, title_fm.horizontalAdvance(out_pane->displayName()));

    auto suffix = out_pane->displayName().simplified();
    suffix.remove(QLatin1Char(' '));
    data.id = base_id.withSuffix(suffix);
    data.action = new QAction(out_pane->displayName(), m_instance);
    const auto cmd = ActionManager::registerAction(data.action, data.id);

    mpanes->addAction(cmd, "Coreplugin.OutputPane.PanesGroup");

    cmd->setDefaultKeySequence(paneShortCut(shortcut_number));
    auto button = new OutputPaneToggleButton(shortcut_number, out_pane->displayName(), cmd->action());
    data.button = button;

    connect(out_pane, &IOutputPane::flashButton, button, [button] { button->flash(); });
    connect(out_pane, &IOutputPane::setBadgeNumber, button, &OutputPaneToggleButton::setIconBadgeNumber);

    ++shortcut_number;
    m_instance->m_buttonsWidget->layout()->addWidget(data.button);
    connect(data.button, &QAbstractButton::clicked, m_instance, [i] {
      m_instance->buttonTriggered(i);
    });

    const auto visible = out_pane->priorityInStatusBar() != -1;
    data.button->setVisible(visible);

    connect(data.action, &QAction::triggered, m_instance, [i] {
      m_instance->shortcutTriggered(i);
    });
  }

  m_instance->m_titleLabel->setMinimumWidth(min_title_width + m_instance->m_titleLabel->contentsMargins().left() + m_instance->m_titleLabel->contentsMargins().right());
  m_instance->m_buttonsWidget->layout()->addWidget(m_instance->m_manageButton);
  connect(m_instance->m_manageButton, &QAbstractButton::clicked, m_instance, &OutputPaneManager::popupMenu);

  m_instance->readSettings();
}

OutputPaneManager::~OutputPaneManager() = default;

auto OutputPaneManager::shortcutTriggered(const int idx) -> void
{
  const auto output_pane = g_output_panes.at(idx).pane;
  // Now check the special case, the output window is already visible,
  // we are already on that page but the outputpane doesn't have focus
  // then just give it focus.
  if (const auto current = currentIndex(); OutputPanePlaceHolder::isCurrentVisible() && current == idx) {
    if ((!m_outputWidgetPane->isActiveWindow() || !output_pane->hasFocus()) && output_pane->canFocus()) {
      output_pane->setFocus();
      ICore::raiseWindow(m_outputWidgetPane);
    } else {
      slotHide();
    }
  } else {
    // Else do the same as clicking on the button does.
    buttonTriggered(idx);
  }
}

auto OutputPaneManager::outputPaneHeightSetting() -> int
{
  return m_instance->m_outputPaneHeightSetting;
}

auto OutputPaneManager::setOutputPaneHeightSetting(const int value) -> void
{
  m_instance->m_outputPaneHeightSetting = value;
}

auto OutputPaneManager::toggleMaximized() -> void
{
  const auto ph = OutputPanePlaceHolder::getCurrent();
  QTC_ASSERT(ph, return);

  if (!ph->isVisible()) // easier than disabling/enabling the action
    return;

  ph->setMaximized(!ph->isMaximized());
}

auto OutputPaneManager::buttonTriggered(const int idx) -> void
{
  QTC_ASSERT(idx >= 0, return);

  if (idx == currentIndex() && OutputPanePlaceHolder::isCurrentVisible()) {
    // we should toggle and the page is already visible and we are actually closeable
    slotHide();
  } else {
    showPage(idx, IOutputPane::ModeSwitch | IOutputPane::WithFocus);
  }
}

auto OutputPaneManager::readSettings() -> void
{
  QSettings *settings = ICore::settings();
  const auto num = settings->beginReadArray(QLatin1String(g_output_pane_settings_key_c));

  for (auto i = 0; i < num; ++i) {
    settings->setArrayIndex(i);
    const auto id = Id::fromSetting(settings->value(QLatin1String(g_output_pane_id_key_c)));
    const auto idx = indexOf(g_output_panes, equal(&OutputPaneData::id, id));
    if (idx < 0) // happens for e.g. disabled plugins (with outputpanes) that were loaded before
      continue;
    const auto visible = settings->value(QLatin1String(g_output_pane_visible_key_c)).toBool();
    g_output_panes[idx].button->setVisible(visible);
  }

  settings->endArray();
  m_outputPaneHeightSetting = settings->value(QLatin1String("OutputPanePlaceHolder/Height"), 0).toInt();
}

auto OutputPaneManager::slotNext() -> void
{
  const auto idx = currentIndex();
  ensurePageVisible(idx);

  if (const auto out = g_output_panes.at(idx).pane; out->canNext())
    out->goToNext();
}

auto OutputPaneManager::slotPrev() -> void
{
  const auto idx = currentIndex();
  ensurePageVisible(idx);

  if (const auto out = g_output_panes.at(idx).pane; out->canPrevious())
    out->goToPrev();
}

auto OutputPaneManager::slotHide() const -> void
{
  if (const auto ph = OutputPanePlaceHolder::getCurrent()) {
    emit ph->visibilityChangeRequested(false);
    ph->setVisible(false);
    const auto idx = currentIndex();
    QTC_ASSERT(idx >= 0, return);
    g_output_panes.at(idx).button->setChecked(false);
    g_output_panes.at(idx).pane->visibilityChanged(false);
    if (const auto editor = EditorManager::currentEditor()) {
      auto w = editor->widget()->focusWidget();
      if (!w)
        w = editor->widget();
      w->setFocus();
    }
  }
}

auto OutputPaneManager::ensurePageVisible(const int idx) -> void
{
  setCurrentIndex(idx);
}

auto OutputPaneManager::showPage(const int idx, const int flags) -> void
{
  QTC_ASSERT(idx >= 0, return);
  auto ph = OutputPanePlaceHolder::getCurrent();

  if (!ph && flags & IOutputPane::ModeSwitch) {
    // In this mode we don't have a placeholder
    // switch to the output mode and switch the page
    ModeManager::activateMode(Id(MODE_EDIT));
    ph = OutputPanePlaceHolder::getCurrent();
  }

  if (!ph || g_output_panes.at(currentIndex()).pane->hasFocus() && !(flags & IOutputPane::WithFocus) && idx != currentIndex()) {
    g_output_panes.at(idx).button->flash();
  } else {
    emit ph->visibilityChangeRequested(true);
    // make the page visible
    ph->setVisible(true);
    ensurePageVisible(idx);
    const auto out = g_output_panes.at(idx).pane;
    if (flags & IOutputPane::WithFocus) {
      if (out->canFocus())
        out->setFocus();
      ICore::raiseWindow(m_outputWidgetPane);
    }
    if (flags & IOutputPane::EnsureSizeHint)
      ph->ensureSizeHintAsMinimum();
  }
}

auto OutputPaneManager::focusInEvent(QFocusEvent *e) -> void
{
  if (const auto w = m_outputWidgetPane->currentWidget())
    w->setFocus(e->reason());
}

auto OutputPaneManager::setCurrentIndex(const int idx) const -> void
{
  static auto last_index = -1;

  if (last_index != -1) {
    g_output_panes.at(last_index).button->setChecked(false);
    g_output_panes.at(last_index).pane->visibilityChanged(false);
  }

  if (idx != -1) {
    m_outputWidgetPane->setCurrentIndex(idx);
    m_opToolBarWidgets->setCurrentIndex(idx);
    const auto &data = g_output_panes[idx];
    const auto pane = data.pane;
    data.button->show();
    pane->visibilityChanged(true);
    const auto can_navigate = pane->canNavigate();
    m_prevAction->setEnabled(can_navigate && pane->canPrevious());
    m_nextAction->setEnabled(can_navigate && pane->canNext());
    g_output_panes.at(idx).button->setChecked(OutputPanePlaceHolder::isCurrentVisible());
    m_titleLabel->setText(pane->displayName());
  }

  last_index = idx;
}

auto OutputPaneManager::popupMenu() -> void
{
  QMenu menu;
  auto idx = 0;

  for (const auto &data : g_output_panes) {
    const auto act = menu.addAction(data.pane->displayName());
    act->setCheckable(true);
    act->setChecked(data.button->isPaneVisible());
    act->setData(idx);
    ++idx;
  }

  const auto result = menu.exec(QCursor::pos());

  if (!result)
    return;

  idx = result->data().toInt();
  QTC_ASSERT(idx >= 0 && idx < g_output_panes.size(), return);

  if (const auto &data = g_output_panes[idx]; data.button->isPaneVisible()) {
    data.pane->visibilityChanged(false);
    data.button->setChecked(false);
    data.button->hide();
  } else {
    showPage(idx, IOutputPane::ModeSwitch);
  }
}

auto OutputPaneManager::saveSettings() const -> void
{
  QSettings *settings = ICore::settings();
  const auto n = static_cast<int>(g_output_panes.size());
  settings->beginWriteArray(QLatin1String(g_output_pane_settings_key_c), n);

  for (auto i = 0; i < n; ++i) {
    const auto &data = g_output_panes.at(i);
    settings->setArrayIndex(i);
    settings->setValue(QLatin1String(g_output_pane_id_key_c), data.id.toSetting());
    settings->setValue(QLatin1String(g_output_pane_visible_key_c), data.button->isPaneVisible());
  }

  settings->endArray();
  auto height_setting = m_outputPaneHeightSetting;

  // update if possible
  if (const auto curr = OutputPanePlaceHolder::getCurrent())
    height_setting = curr->nonMaximizedSize();

  settings->setValue(QLatin1String("OutputPanePlaceHolder/Height"), height_setting);
}

auto OutputPaneManager::clearPage() const -> void
{
  if (const auto idx = currentIndex(); idx >= 0)
    g_output_panes.at(idx).pane->clearContents();
}

auto OutputPaneManager::currentIndex() const -> int
{
  return m_outputWidgetPane->currentIndex();
}

OutputPaneToggleButton::OutputPaneToggleButton(const int number, QString text, QAction *action, QWidget *parent) : QToolButton(parent), m_number(QString::number(number)), m_text(std::move(text)), m_action(action), m_flashTimer(new QTimeLine(1000, this))
{
  setFocusPolicy(Qt::NoFocus);
  setCheckable(true);
  const auto fnt = QApplication::font();
  setFont(fnt);

  if (m_action)
    connect(m_action, &QAction::changed, this, &OutputPaneToggleButton::updateToolTip);

  m_flashTimer->setDirection(QTimeLine::Forward);
  m_flashTimer->setEasingCurve(QEasingCurve::SineCurve);
  m_flashTimer->setFrameRange(0, 92);

  const auto update_slot = QOverload<>::of(&QWidget::update);

  connect(m_flashTimer, &QTimeLine::valueChanged, this, update_slot);
  connect(m_flashTimer, &QTimeLine::finished, this, update_slot);

  updateToolTip();
}

auto OutputPaneToggleButton::updateToolTip() -> void
{
  QTC_ASSERT(m_action, return);
  setToolTip(m_action->toolTip());
}

auto OutputPaneToggleButton::sizeHint() const -> QSize
{
  ensurePolished();
  auto s = fontMetrics().size(Qt::TextSingleLine, m_text);

  // Expand to account for border image
  s.rwidth() += numberAreaWidth() + 1 + g_button_border_width + g_button_border_width;
  if (!m_badgeNumberLabel.text().isNull())
    s.rwidth() += m_badgeNumberLabel.sizeHint().width() + 1;

  return s;
}

auto OutputPaneToggleButton::paintEvent(QPaintEvent *) -> void
{
  const auto fm = fontMetrics();
  const auto base_line = (height() - fm.height() + 1) / 2 + fm.ascent();
  const auto number_width = fm.horizontalAdvance(m_number);

  QPainter p(this);
  QStyleOption style_option;

  style_option.initFrom(this);
  const auto hovered = style_option.state & QStyle::State_MouseOver;

  if (orcaTheme()->flag(Theme::FlatToolBars)) {
    auto c = Theme::BackgroundColorDark;
    if (hovered)
      c = Theme::BackgroundColorHover;
    else if (isDown() || isChecked())
      c = Theme::BackgroundColorSelected;
    if (c != Theme::BackgroundColorDark)
      p.fillRect(rect(), orcaTheme()->color(c));
  } else {
    const QImage *image = nullptr;
    if (isDown()) {
      static const QImage pressed(StyleHelper::dpiSpecificImageFile(":/utils/images/panel_button_pressed.png"));
      image = &pressed;
    } else if (isChecked()) {
      if (hovered) {
        static const QImage checked_hover(StyleHelper::dpiSpecificImageFile(":/utils/images/panel_button_checked_hover.png"));
        image = &checked_hover;
      } else {
        static const QImage checked(StyleHelper::dpiSpecificImageFile(":/utils/images/panel_button_checked.png"));
        image = &checked;
      }
    } else {
      if (hovered) {
        static const QImage hover(StyleHelper::dpiSpecificImageFile(":/utils/images/panel_button_hover.png"));
        image = &hover;
      } else {
        static const QImage button(StyleHelper::dpiSpecificImageFile(":/utils/images/panel_button.png"));
        image = &button;
      }
    }
    if (image)
      StyleHelper::drawCornerImage(*image, &p, rect(), numberAreaWidth(), g_button_border_width, g_button_border_width, g_button_border_width);
  }

  if (m_flashTimer->state() == QTimeLine::Running) {
    auto c = orcaTheme()->color(Theme::OutputPaneButtonFlashColor);
    c.setAlpha(m_flashTimer->currentFrame());
    const auto r = orcaTheme()->flag(Theme::FlatToolBars) ? rect() : rect().adjusted(numberAreaWidth(), 1, -1, -1);
    p.fillRect(r, c);
  }

  p.setFont(font());
  p.setPen(orcaTheme()->color(Theme::OutputPaneToggleButtonTextColorChecked));
  p.drawText((numberAreaWidth() - number_width) / 2, base_line, m_number);

  if (!isChecked())
    p.setPen(orcaTheme()->color(Theme::OutputPaneToggleButtonTextColorUnchecked));

  const auto left_part = numberAreaWidth() + g_button_border_width;
  auto label_width = 0;

  if (!m_badgeNumberLabel.text().isEmpty()) {
    const auto label_size = m_badgeNumberLabel.sizeHint();
    label_width = label_size.width() + 3;
    m_badgeNumberLabel.paint(&p, width() - label_width, (height() - label_size.height()) / 2, isChecked());
  }

  p.drawText(left_part, base_line, fm.elidedText(m_text, Qt::ElideRight, width() - left_part - 1 - label_width));
}

auto OutputPaneToggleButton::checkStateSet() -> void
{
  //Stop flashing when button is checked
  QToolButton::checkStateSet();
  m_flashTimer->stop();
}

auto OutputPaneToggleButton::flash(const int count) -> void
{
  setVisible(true);
  //Start flashing if button is not checked
  if (!isChecked()) {
    m_flashTimer->setLoopCount(count);
    if (m_flashTimer->state() != QTimeLine::Running)
      m_flashTimer->start();
    update();
  }
}

auto OutputPaneToggleButton::setIconBadgeNumber(const int number) -> void
{
  const auto text = number ? QString::number(number) : QString();
  m_badgeNumberLabel.setText(text);
  updateGeometry();
}

auto OutputPaneToggleButton::isPaneVisible() const -> bool
{
  return isVisibleTo(parentWidget());
}

OutputPaneManageButton::OutputPaneManageButton()
{
  setFocusPolicy(Qt::NoFocus);
  setCheckable(true);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

auto OutputPaneManageButton::sizeHint() const -> QSize
{
  ensurePolished();
  return {numberAreaWidth(), 16};
}

auto OutputPaneManageButton::paintEvent(QPaintEvent *) -> void
{
  QPainter p(this);

  if (!orcaTheme()->flag(Theme::FlatToolBars)) {
    static const QImage button(StyleHelper::dpiSpecificImageFile(QStringLiteral(":/utils/images/panel_manage_button.png")));
    StyleHelper::drawCornerImage(button, &p, rect(), g_button_border_width, g_button_border_width, g_button_border_width, g_button_border_width);
  }

  const auto s = style();
  QStyleOption arrow_opt;
  arrow_opt.initFrom(this);
  arrow_opt.rect = QRect(6, rect().center().y() - 3, 8, 8);
  arrow_opt.rect.translate(0, -3);
  s->drawPrimitive(QStyle::PE_IndicatorArrowUp, &arrow_opt, &p, this);
  arrow_opt.rect.translate(0, 6);
  s->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrow_opt, &p, this);
}

BadgeLabel::BadgeLabel()
{
  m_font = QApplication::font();
  m_font.setBold(true);
  m_font.setPixelSize(11);
}

auto BadgeLabel::paint(QPainter *p, const int x, const int y, const bool is_checked) const -> void
{
  const QRectF rect(QRect(QPoint(x, y), m_size));

  p->save();
  p->setBrush(orcaTheme()->color(is_checked ? Theme::BadgeLabelBackgroundColorChecked : Theme::BadgeLabelBackgroundColorUnchecked));
  p->setPen(Qt::NoPen);
  p->setRenderHint(QPainter::Antialiasing, true);
  p->drawRoundedRect(rect, m_padding, m_padding, Qt::AbsoluteSize);
  p->setFont(m_font);
  p->setPen(orcaTheme()->color(is_checked ? Theme::BadgeLabelTextColorChecked : Theme::BadgeLabelTextColorUnchecked));
  p->drawText(rect, Qt::AlignCenter, m_text);
  p->restore();
}

auto BadgeLabel::setText(const QString &text) -> void
{
  m_text = text;
  calculateSize();
}

auto BadgeLabel::text() const -> QString
{
  return m_text;
}

auto BadgeLabel::sizeHint() const -> QSize
{
  return m_size;
}

auto BadgeLabel::calculateSize() -> void
{
  const QFontMetrics fm(m_font);
  m_size = fm.size(Qt::TextSingleLine, m_text);
  m_size.setWidth(static_cast<int>(m_size.width() + m_padding * 1.5));
  m_size.setHeight(2 * m_padding + 1); // Needs to be uneven for pixel perfect vertical centering in the button
}

} // namespace Orca::Plugin::Core
