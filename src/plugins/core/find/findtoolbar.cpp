// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findtoolbar.h"
#include "ifindfilter.h"
#include "findplugin.h"
#include "optionspopup.h"

#include <core/coreicons.h>
#include <core/coreplugin.h>
#include <core/icontext.h>
#include <core/icore.h>
#include <core/actionmanager/actionmanager.h>
#include <core/actionmanager/actioncontainer.h>
#include <core/actionmanager/command.h>
#include <core/findplaceholder.h>

#include <utils/hostosinfo.h>
#include <utils/utilsicons.h>

#include <QSettings>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCompleter>
#include <QKeyEvent>
#include <QMenu>
#include <QStringListModel>

Q_DECLARE_METATYPE(QStringList)
Q_DECLARE_METATYPE(Core::IFindFilter*)

static constexpr int minimum_width_for_complex_layout = 150;
static constexpr int findbutton_spacer_width = 20;

namespace Core {
namespace Internal {

FindToolBar::FindToolBar(CurrentDocumentFind *current_document_find) : m_current_document_find(current_document_find), m_find_completer(new QCompleter(this)), m_replace_completer(new QCompleter(this)), m_find_incremental_timer(this), m_find_step_timer(this)
{
  //setup ui
  m_ui.setupUi(this);
  // compensate for a vertically expanding spacer below the label
  m_ui.replaceLabel->setMinimumHeight(m_ui.replaceEdit->sizeHint().height());
  m_ui.mainLayout->setColumnStretch(1, 10);

  setFocusProxy(m_ui.findEdit);
  setProperty("topBorder", true);
  setSingleRow(false);

  m_ui.findEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_ui.replaceEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_ui.replaceEdit->setFiltering(true);

  connect(m_ui.findEdit, &Utils::FancyLineEdit::editingFinished, this, &FindToolBar::invokeResetIncrementalSearch);
  connect(m_ui.findEdit, &Utils::FancyLineEdit::textChanged, this, &FindToolBar::updateFindReplaceEnabled);
  connect(m_ui.close, &QToolButton::clicked, this, &FindToolBar::hideAndResetFocus);

  m_find_completer->setModel(Find::findCompletionModel());
  m_replace_completer->setModel(Find::replaceCompletionModel());

  m_ui.findEdit->setSpecialCompleter(m_find_completer);
  m_ui.replaceEdit->setSpecialCompleter(m_replace_completer);
  m_ui.findEdit->setButtonVisible(Utils::FancyLineEdit::Left, true);
  m_ui.findEdit->setFiltering(true);
  m_ui.findEdit->setPlaceholderText(QString());
  m_ui.findEdit->button(Utils::FancyLineEdit::Left)->setFocusPolicy(Qt::TabFocus);
  m_ui.findEdit->setValidationFunction([this](Utils::FancyLineEdit *, QString *) {
    return m_last_result != IFindSupport::NotFound;
  });
  m_ui.replaceEdit->setPlaceholderText(QString());

  connect(m_ui.findEdit, &Utils::FancyLineEdit::textChanged, this, &FindToolBar::invokeFindIncremental);
  connect(m_ui.findEdit, &Utils::FancyLineEdit::leftButtonClicked, this, &FindToolBar::findEditButtonClicked);

  // invoke{Find,Replace}Helper change the completion model. QueuedConnection is used to perform these
  // changes only after the completer's activated() signal is handled (ORCABUG-8408)
  connect(m_ui.findEdit, &Utils::FancyLineEdit::returnPressed, this, &FindToolBar::invokeFindEnter, Qt::QueuedConnection);
  connect(m_ui.replaceEdit, &Utils::FancyLineEdit::returnPressed, this, &FindToolBar::invokeReplaceEnter, Qt::QueuedConnection);
  connect(m_find_completer, QOverload<const QModelIndex&>::of(&QCompleter::activated), this, &FindToolBar::findCompleterActivated);

  auto shift_enter_action = new QAction(m_ui.findEdit);
  shift_enter_action->setShortcut(QKeySequence(tr("Shift+Enter")));
  shift_enter_action->setShortcutContext(Qt::WidgetShortcut);
  connect(shift_enter_action, &QAction::triggered, this, &FindToolBar::invokeFindPrevious);
  m_ui.findEdit->addAction(shift_enter_action);

  auto shift_return_action = new QAction(m_ui.findEdit);
  shift_return_action->setShortcut(QKeySequence(tr("Shift+Return")));
  shift_return_action->setShortcutContext(Qt::WidgetShortcut);
  connect(shift_return_action, &QAction::triggered, this, &FindToolBar::invokeFindPrevious);
  m_ui.findEdit->addAction(shift_return_action);

  auto shift_enter_replace_action = new QAction(m_ui.replaceEdit);
  shift_enter_replace_action->setShortcut(QKeySequence(tr("Shift+Enter")));
  shift_enter_replace_action->setShortcutContext(Qt::WidgetShortcut);
  connect(shift_enter_replace_action, &QAction::triggered, this, &FindToolBar::invokeReplacePrevious);
  m_ui.replaceEdit->addAction(shift_enter_replace_action);

  auto shift_return_replace_action = new QAction(m_ui.replaceEdit);
  shift_return_replace_action->setShortcut(QKeySequence(tr("Shift+Return")));
  shift_return_replace_action->setShortcutContext(Qt::WidgetShortcut);
  connect(shift_return_replace_action, &QAction::triggered, this, &FindToolBar::invokeReplacePrevious);
  m_ui.replaceEdit->addAction(shift_return_replace_action);

  // need to make sure QStringList is registered as metatype
  QMetaTypeId<QStringList>::qt_metatype_id();

  // register actions
  Context findcontext(Constants::c_findtoolbar);
  auto mfind = ActionManager::actionContainer(Constants::m_find);

  m_ui.advancedButton->setDefaultAction(ActionManager::command(Constants::advanced_find)->action());

  m_go_to_current_find_action = new QAction(this);
  ActionManager::registerAction(m_go_to_current_find_action, Constants::S_RETURNTOEDITOR, findcontext);
  connect(m_go_to_current_find_action, &QAction::triggered, this, &FindToolBar::setFocusToCurrentFindSupport);

  Command *cmd;
  auto icon = QIcon::fromTheme(QLatin1String("edit-find-replace"));
  m_find_in_document_action = new QAction(icon, tr("Find/Replace"), this);
  cmd = ActionManager::registerAction(m_find_in_document_action, Constants::find_in_document);
  cmd->setDefaultKeySequence(QKeySequence::Find);
  mfind->addAction(cmd, Constants::g_find_currentdocument);
  connect(m_find_in_document_action, &QAction::triggered, this, [this] { openFind(); });

  // Pressing the find shortcut while focus is in the tool bar should not change the search text,
  // so register a different find action for the tool bar
  auto local_find_action = new QAction(this);
  ActionManager::registerAction(local_find_action, Constants::find_in_document, findcontext);
  connect(local_find_action, &QAction::triggered, this, [this] {
    openFindToolBar(OpenFlags(update_all & ~update_find_text));
  });

  if (QApplication::clipboard()->supportsFindBuffer()) {
    m_enter_find_string_action = new QAction(tr("Enter Find String"), this);
    cmd = ActionManager::registerAction(m_enter_find_string_action, "Find.EnterFindString");
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+E")));
    mfind->addAction(cmd, Constants::g_find_actions);
    connect(m_enter_find_string_action, &QAction::triggered, this, &FindToolBar::putSelectionToFindClipboard);
    connect(QApplication::clipboard(), &QClipboard::findBufferChanged, this, &FindToolBar::updateFromFindClipboard);
  }

  m_find_next_action = new QAction(tr("Find Next"), this);
  cmd = ActionManager::registerAction(m_find_next_action, Constants::find_next);
  cmd->setDefaultKeySequence(QKeySequence::FindNext);
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_find_next_action, &QAction::triggered, this, &FindToolBar::invokeGlobalFindNext);
  m_local_find_next_action = new QAction(m_find_next_action->text(), this);

  cmd = ActionManager::registerAction(m_local_find_next_action, Constants::find_next, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_find_next_action);
  connect(m_local_find_next_action, &QAction::triggered, this, &FindToolBar::invokeFindNext);
  m_ui.findNextButton->setDefaultAction(m_local_find_next_action);

  m_find_previous_action = new QAction(tr("Find Previous"), this);
  cmd = ActionManager::registerAction(m_find_previous_action, Constants::find_previous);
  cmd->setDefaultKeySequence(QKeySequence::FindPrevious);
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_find_previous_action, &QAction::triggered, this, &FindToolBar::invokeGlobalFindPrevious);
  m_local_find_previous_action = new QAction(m_find_previous_action->text(), this);

  cmd = ActionManager::registerAction(m_local_find_previous_action, Constants::find_previous, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_find_previous_action);
  connect(m_local_find_previous_action, &QAction::triggered, this, &FindToolBar::invokeFindPrevious);
  m_ui.findPreviousButton->setDefaultAction(m_local_find_previous_action);

  m_find_next_selected_action = new QAction(tr("Find Next (Selected)"), this);
  cmd = ActionManager::registerAction(m_find_next_selected_action, Constants::find_next_selected);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+F3")));
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_find_next_selected_action, &QAction::triggered, this, &FindToolBar::findNextSelected);
  m_find_previous_selected_action = new QAction(tr("Find Previous (Selected)"), this);

  cmd = ActionManager::registerAction(m_find_previous_selected_action, Constants::find_prev_selected);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+F3")));
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_find_previous_selected_action, &QAction::triggered, this, &FindToolBar::findPreviousSelected);
  m_select_all_action = new QAction(tr("Select All"), this);

  cmd = ActionManager::registerAction(m_select_all_action, Constants::find_select_all);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+Return")));
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_select_all_action, &QAction::triggered, this, &FindToolBar::selectAll);
  m_local_select_all_action = new QAction(m_select_all_action->text(), this);

  cmd = ActionManager::registerAction(m_local_select_all_action, Constants::find_select_all, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_select_all_action);
  connect(m_local_select_all_action, &QAction::triggered, this, &FindToolBar::selectAll);
  m_ui.selectAllButton->setDefaultAction(m_local_select_all_action);

  m_replace_action = new QAction(tr("Replace"), this);
  cmd = ActionManager::registerAction(m_replace_action, Constants::replace);
  cmd->setDefaultKeySequence(QKeySequence());
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_replace_action, &QAction::triggered, this, &FindToolBar::invokeGlobalReplace);
  m_local_replace_action = new QAction(m_replace_action->text(), this);

  cmd = ActionManager::registerAction(m_local_replace_action, Constants::replace, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_replace_action);
  connect(m_local_replace_action, &QAction::triggered, this, &FindToolBar::invokeReplace);
  m_ui.replaceButton->setDefaultAction(m_local_replace_action);

  m_replace_next_action = new QAction(tr("Replace && Find"), this);
  cmd = ActionManager::registerAction(m_replace_next_action, Constants::replace_next);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+=")));
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_replace_next_action, &QAction::triggered, this, &FindToolBar::invokeGlobalReplaceNext);
  m_local_replace_next_action = new QAction(m_replace_next_action->text(), this);

  m_local_replace_next_action->setIconText(m_replace_next_action->text()); // Workaround QTBUG-23396
  cmd = ActionManager::registerAction(m_local_replace_next_action, Constants::replace_next, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_replace_next_action);
  connect(m_local_replace_next_action, &QAction::triggered, this, &FindToolBar::invokeReplaceNext);
  m_ui.replaceNextButton->setDefaultAction(m_local_replace_next_action);

  m_replace_previous_action = new QAction(tr("Replace && Find Previous"), this);
  cmd = ActionManager::registerAction(m_replace_previous_action, Constants::replace_previous);
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_replace_previous_action, &QAction::triggered, this, &FindToolBar::invokeGlobalReplacePrevious);
  m_local_replace_previous_action = new QAction(m_replace_previous_action->text(), this);

  cmd = ActionManager::registerAction(m_local_replace_previous_action, Constants::replace_previous, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_replace_previous_action);
  connect(m_local_replace_previous_action, &QAction::triggered, this, &FindToolBar::invokeReplacePrevious);
  m_replace_all_action = new QAction(tr("Replace All"), this);

  cmd = ActionManager::registerAction(m_replace_all_action, Constants::replace_all);
  mfind->addAction(cmd, Constants::g_find_actions);
  connect(m_replace_all_action, &QAction::triggered, this, &FindToolBar::invokeGlobalReplaceAll);
  m_local_replace_all_action = new QAction(m_replace_all_action->text(), this);

  cmd = ActionManager::registerAction(m_local_replace_all_action, Constants::replace_all, findcontext);
  cmd->augmentActionWithShortcutToolTip(m_local_replace_all_action);
  connect(m_local_replace_all_action, &QAction::triggered, this, &FindToolBar::invokeReplaceAll);
  m_ui.replaceAllButton->setDefaultAction(m_local_replace_all_action);

  m_case_sensitive_action = new QAction(tr("Case Sensitive"), this);
  m_case_sensitive_action->setIcon(Icons::FIND_CASE_INSENSITIVELY.icon());
  m_case_sensitive_action->setCheckable(true);
  m_case_sensitive_action->setChecked(false);
  cmd = ActionManager::registerAction(m_case_sensitive_action, Constants::case_sensitive);
  mfind->addAction(cmd, Constants::g_find_flags);
  connect(m_case_sensitive_action, &QAction::toggled, this, &FindToolBar::setCaseSensitive);

  m_whole_word_action = new QAction(tr("Whole Words Only"), this);
  m_whole_word_action->setIcon(Icons::FIND_WHOLE_WORD.icon());
  m_whole_word_action->setCheckable(true);
  m_whole_word_action->setChecked(false);
  cmd = ActionManager::registerAction(m_whole_word_action, Constants::whole_words);
  mfind->addAction(cmd, Constants::g_find_flags);
  connect(m_whole_word_action, &QAction::toggled, this, &FindToolBar::setWholeWord);

  m_regular_expression_action = new QAction(tr("Use Regular Expressions"), this);
  m_regular_expression_action->setIcon(Icons::FIND_REGEXP.icon());
  m_regular_expression_action->setCheckable(true);
  m_regular_expression_action->setChecked(false);
  cmd = ActionManager::registerAction(m_regular_expression_action, Constants::regular_expressions);
  mfind->addAction(cmd, Constants::g_find_flags);
  connect(m_regular_expression_action, &QAction::toggled, this, &FindToolBar::setRegularExpressions);

  m_preserve_case_action = new QAction(tr("Preserve Case when Replacing"), this);
  m_preserve_case_action->setIcon(Icons::FIND_PRESERVE_CASE.icon());
  m_preserve_case_action->setCheckable(true);
  m_preserve_case_action->setChecked(false);
  cmd = ActionManager::registerAction(m_preserve_case_action, Constants::preserve_case);
  mfind->addAction(cmd, Constants::g_find_flags);
  connect(m_preserve_case_action, &QAction::toggled, this, &FindToolBar::setPreserveCase);
  connect(m_current_document_find, &CurrentDocumentFind::candidateChanged, this, &FindToolBar::adaptToCandidate);
  connect(m_current_document_find, &CurrentDocumentFind::changed, this, &FindToolBar::updateActions);
  connect(m_current_document_find, &CurrentDocumentFind::changed, this, &FindToolBar::updateToolBar);

  updateActions();
  updateToolBar();

  m_find_incremental_timer.setSingleShot(true);
  m_find_step_timer.setSingleShot(true);

  connect(&m_find_incremental_timer, &QTimer::timeout, this, &FindToolBar::invokeFindIncremental);
  connect(&m_find_step_timer, &QTimer::timeout, this, &FindToolBar::invokeFindStep);

  setLightColoredIcon(isLightColored());
}

FindToolBar::~FindToolBar() = default;

auto FindToolBar::findCompleterActivated(const QModelIndex &index) -> void
{
  const auto find_flags_i = index.data(Find::completion_model_find_flags_role).toInt();
  const FindFlags find_flags(find_flags_i);
  setFindFlag(FindCaseSensitively, find_flags.testFlag(FindCaseSensitively));
  setFindFlag(FindBackward, find_flags.testFlag(FindBackward));
  setFindFlag(FindWholeWords, find_flags.testFlag(FindWholeWords));
  setFindFlag(FindRegularExpression, find_flags.testFlag(FindRegularExpression));
  setFindFlag(FindPreserveCase, find_flags.testFlag(FindPreserveCase));
}

auto FindToolBar::installEventFilters() -> void
{
  if (!m_event_filters_installed) {
    m_find_completer->popup()->installEventFilter(this);
    m_ui.findEdit->installEventFilter(this);
    m_ui.replaceEdit->installEventFilter(this);
    this->installEventFilter(this);
    m_event_filters_installed = true;
  }
}

auto FindToolBar::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (event->type() == QEvent::KeyPress) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Down) {
      if (obj == m_ui.findEdit) {
        if (m_ui.findEdit->text().isEmpty())
          m_find_completer->setCompletionPrefix(QString());
        m_find_completer->complete();
      } else if (obj == m_ui.replaceEdit) {
        if (m_ui.replaceEdit->text().isEmpty())
          m_replace_completer->setCompletionPrefix(QString());
        m_replace_completer->complete();
      }
    }
  }

  if ((obj == m_ui.findEdit || obj == m_find_completer->popup()) && event->type() == QEvent::KeyPress) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Space && ke->modifiers() & Utils::HostOsInfo::controlModifier()) {
      if (const auto completed_text = m_current_document_find->completedFindString(); !completed_text.isEmpty()) {
        setFindText(completed_text);
        ke->accept();
        return true;
      }
    }
  } else if (obj == this && event->type() == QEvent::ShortcutOverride) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Space && ke->modifiers() & Utils::HostOsInfo::controlModifier()) {
      event->accept();
      return true;
    }
  } else if (obj == this && event->type() == QEvent::Hide) {
    invokeClearResults();
    if (m_current_document_find->isEnabled())
      m_current_document_find->clearFindScope();
  }
  return StyledBar::eventFilter(obj, event);
}

auto FindToolBar::adaptToCandidate() -> void
{
  updateActions();
  if (findToolBarPlaceHolder() == FindToolBarPlaceHolder::getCurrent()) {
    m_current_document_find->acceptCandidate();
    if (isVisible() && m_current_document_find->isEnabled())
      m_current_document_find->highlightAll(getFindText(), effectiveFindFlags());
  }
}

auto FindToolBar::updateActions() -> void
{
  const auto candidate = m_current_document_find->candidate();
  const auto enabled = candidate != nullptr;

  m_find_in_document_action->setEnabled(enabled || toolBarHasFocus() && isEnabled());
  m_find_next_selected_action->setEnabled(enabled);
  m_find_previous_selected_action->setEnabled(enabled);

  if (m_enter_find_string_action)
    m_enter_find_string_action->setEnabled(enabled);

  updateFindReplaceEnabled();
  m_select_all_action->setEnabled(m_current_document_find->supportsSelectAll());
}

auto FindToolBar::updateToolBar() const -> void
{
  const auto enabled = m_current_document_find->isEnabled();
  const auto replace_enabled = enabled && m_current_document_find->supportsReplace();
  const auto style = controlStyle(replace_enabled);
  const auto show_all_controls = style != control_style::hidden;

  setFindButtonStyle(style == control_style::text ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly);

  m_case_sensitive_action->setEnabled(enabled);
  m_whole_word_action->setEnabled(enabled);
  m_regular_expression_action->setEnabled(enabled);
  m_preserve_case_action->setEnabled(replace_enabled && !hasFindFlag(FindRegularExpression));

  const auto replace_focus = m_ui.replaceEdit->hasFocus();

  m_ui.findLabel->setEnabled(enabled);
  m_ui.findLabel->setVisible(show_all_controls);
  m_ui.findEdit->setEnabled(enabled);
  m_ui.findEdit->setPlaceholderText(show_all_controls ? QString() : tr("Search for..."));
  m_ui.findPreviousButton->setEnabled(enabled);
  m_ui.findPreviousButton->setVisible(show_all_controls);
  m_ui.findNextButton->setEnabled(enabled);
  m_ui.findNextButton->setVisible(show_all_controls);
  m_ui.selectAllButton->setVisible(style == control_style::text && m_current_document_find->supportsSelectAll());
  m_ui.horizontalSpacer->changeSize(show_all_controls ? findbutton_spacer_width : 0, 0, QSizePolicy::Expanding, QSizePolicy::Ignored);
  m_ui.findButtonLayout->invalidate(); // apply spacer change
  m_ui.replaceLabel->setEnabled(replace_enabled);
  m_ui.replaceLabel->setVisible(replace_enabled && show_all_controls);
  m_ui.replaceEdit->setEnabled(replace_enabled);
  m_ui.replaceEdit->setPlaceholderText(show_all_controls ? QString() : tr("Replace with..."));
  m_ui.replaceEdit->setVisible(replace_enabled);
  m_ui.replaceButtonsWidget->setVisible(replace_enabled && show_all_controls);
  m_ui.advancedButton->setVisible(replace_enabled && show_all_controls);

  layout()->invalidate();

  if (!replace_enabled && enabled && replace_focus)
    m_ui.findEdit->setFocus();

  updateIcons();
  updateFlagMenus();
}

auto FindToolBar::invokeFindEnter() -> void
{
  if (m_current_document_find->isEnabled()) {
    if (m_use_fake_vim)
      setFocusToCurrentFindSupport();
    else
      invokeFindNext();
  }
}

auto FindToolBar::invokeReplaceEnter() -> void
{
  if (m_current_document_find->isEnabled() && m_current_document_find->supportsReplace())
    invokeReplaceNext();
}

auto FindToolBar::invokeClearResults() const -> void
{
  if (m_current_document_find->isEnabled())
    m_current_document_find->clearHighlights();
}

auto FindToolBar::invokeFindNext() -> void
{
  setFindFlag(FindBackward, false);
  invokeFindStep();
}

auto FindToolBar::invokeGlobalFindNext() -> void
{
  if (getFindText().isEmpty()) {
    openFind();
  } else {
    acceptCandidateAndMoveToolBar();
    invokeFindNext();
  }
}

auto FindToolBar::invokeFindPrevious() -> void
{
  setFindFlag(FindBackward, true);
  invokeFindStep();
}

auto FindToolBar::invokeGlobalFindPrevious() -> void
{
  if (getFindText().isEmpty()) {
    openFind();
  } else {
    acceptCandidateAndMoveToolBar();
    invokeFindPrevious();
  }
}

auto FindToolBar::getFindText() const -> QString
{
  return m_ui.findEdit->text();
}

auto FindToolBar::getReplaceText() const -> QString
{
  return m_ui.replaceEdit->text();
}

auto FindToolBar::setFindText(const QString &text) -> void
{
  disconnect(m_ui.findEdit, &Utils::FancyLineEdit::textChanged, this, &FindToolBar::invokeFindIncremental);

  if (hasFindFlag(FindRegularExpression))
    m_ui.findEdit->setText(QRegularExpression::escape(text));
  else
    m_ui.findEdit->setText(text);

  connect(m_ui.findEdit, &Utils::FancyLineEdit::textChanged, this, &FindToolBar::invokeFindIncremental);
}

auto FindToolBar::selectFindText() const -> void
{
  m_ui.findEdit->selectAll();
}

auto FindToolBar::invokeFindStep() -> void
{
  m_find_step_timer.stop();
  m_find_incremental_timer.stop();

  if (m_current_document_find->isEnabled()) {
    const auto ef = effectiveFindFlags();
    Find::updateFindCompletion(getFindText(), ef);
    const auto result = m_current_document_find->findStep(getFindText(), ef);
    indicateSearchState(result);
    if (result == IFindSupport::NotYetFound)
      m_find_step_timer.start(50);
  }
}

auto FindToolBar::invokeFindIncremental() -> void
{
  m_find_incremental_timer.stop();
  m_find_step_timer.stop();

  if (m_current_document_find->isEnabled()) {
    const auto text = getFindText();
    const auto result = m_current_document_find->findIncremental(text, effectiveFindFlags());
    indicateSearchState(result);
    if (result == IFindSupport::NotYetFound)
      m_find_incremental_timer.start(50);
    if (text.isEmpty())
      m_current_document_find->clearHighlights();
  }
}

auto FindToolBar::invokeReplace() -> void
{
  setFindFlag(FindBackward, false);

  if (m_current_document_find->isEnabled() && m_current_document_find->supportsReplace()) {
    const auto ef = effectiveFindFlags();
    Find::updateFindCompletion(getFindText(), ef);
    Find::updateReplaceCompletion(getReplaceText());
    m_current_document_find->replace(getFindText(), getReplaceText(), ef);
  }
}

auto FindToolBar::invokeGlobalReplace() -> void
{
  acceptCandidateAndMoveToolBar();
  invokeReplace();
}

auto FindToolBar::invokeReplaceNext() -> void
{
  setFindFlag(FindBackward, false);
  invokeReplaceStep();
}

auto FindToolBar::invokeGlobalReplaceNext() -> void
{
  acceptCandidateAndMoveToolBar();
  invokeReplaceNext();
}

auto FindToolBar::invokeReplacePrevious() -> void
{
  setFindFlag(FindBackward, true);
  invokeReplaceStep();
}

auto FindToolBar::invokeGlobalReplacePrevious() -> void
{
  acceptCandidateAndMoveToolBar();
  invokeReplacePrevious();
}

auto FindToolBar::invokeReplaceStep() const -> void
{
  if (m_current_document_find->isEnabled() && m_current_document_find->supportsReplace()) {
    const auto ef = effectiveFindFlags();
    Find::updateFindCompletion(getFindText(), ef);
    Find::updateReplaceCompletion(getReplaceText());
    m_current_document_find->replaceStep(getFindText(), getReplaceText(), ef);
  }
}

auto FindToolBar::invokeReplaceAll() const -> void
{
  const auto ef = effectiveFindFlags();
  Find::updateFindCompletion(getFindText(), ef);
  Find::updateReplaceCompletion(getReplaceText());

  if (m_current_document_find->isEnabled() && m_current_document_find->supportsReplace())
    m_current_document_find->replaceAll(getFindText(), getReplaceText(), ef);
}

auto FindToolBar::invokeGlobalReplaceAll() -> void
{
  acceptCandidateAndMoveToolBar();
  invokeReplaceAll();
}

auto FindToolBar::invokeResetIncrementalSearch() -> void
{
  m_find_incremental_timer.stop();
  m_find_step_timer.stop();

  if (m_current_document_find->isEnabled())
    m_current_document_find->resetIncrementalSearch();
}

auto FindToolBar::putSelectionToFindClipboard() -> void
{
  openFind(false);
  const auto text = m_current_document_find->currentFindString();
  QApplication::clipboard()->setText(text, QClipboard::FindBuffer);
}

auto FindToolBar::updateFromFindClipboard() -> void
{
  if (QApplication::clipboard()->supportsFindBuffer()) {
    QSignalBlocker blocker(m_ui.findEdit);
    setFindText(QApplication::clipboard()->text(QClipboard::FindBuffer));
  }
}

auto FindToolBar::findFlagsChanged() const -> void
{
  updateIcons();
  updateFlagMenus();
  invokeClearResults();

  if (isVisible())
    m_current_document_find->highlightAll(getFindText(), effectiveFindFlags());
}

auto FindToolBar::findEditButtonClicked() const -> void
{
  const auto popup = new OptionsPopup(m_ui.findEdit, {Constants::case_sensitive, Constants::whole_words, Constants::regular_expressions, Constants::preserve_case});
  popup->show();
}

auto FindToolBar::updateIcons() const -> void
{
  const auto effective_flags = effectiveFindFlags();
  const bool casesensitive = effective_flags & FindCaseSensitively;
  const bool wholewords = effective_flags & FindWholeWords;
  const bool regexp = effective_flags & FindRegularExpression;

  if (const bool preserve_case = effective_flags & FindPreserveCase; !casesensitive && !wholewords && !regexp && !preserve_case) {
    const auto icon = Utils::Icons::MAGNIFIER.icon();
    m_ui.findEdit->setButtonIcon(Utils::FancyLineEdit::Left, icon);
  } else {
    m_ui.findEdit->setButtonIcon(Utils::FancyLineEdit::Left, IFindFilter::pixmapForFindFlags(effective_flags));
  }
}

auto FindToolBar::effectiveFindFlags() const -> FindFlags
{
  FindFlags supported_flags;
  auto supports_replace = true;

  if (m_current_document_find->isEnabled()) {
    supported_flags = m_current_document_find->supportedFindFlags();
    supports_replace = m_current_document_find->supportsReplace();
  } else {
    supported_flags = static_cast<FindFlags>(0xFFFFFF);
  }

  if (!supports_replace || m_find_flags & FindRegularExpression)
    supported_flags &= ~FindPreserveCase;

  return supported_flags & m_find_flags;
}

auto FindToolBar::updateFlagMenus() const -> void
{
  const bool whole_only = m_find_flags & FindWholeWords;
  const bool sensitive = m_find_flags & FindCaseSensitively;
  const bool regexp = m_find_flags & FindRegularExpression;
  const bool preserve_case = m_find_flags & FindPreserveCase;

  if (m_whole_word_action->isChecked() != whole_only)
    m_whole_word_action->setChecked(whole_only);
  if (m_case_sensitive_action->isChecked() != sensitive)
    m_case_sensitive_action->setChecked(sensitive);
  if (m_regular_expression_action->isChecked() != regexp)
    m_regular_expression_action->setChecked(regexp);
  if (m_preserve_case_action->isChecked() != preserve_case)
    m_preserve_case_action->setChecked(preserve_case);

  FindFlags supported_flags;
  if (m_current_document_find->isEnabled())
    supported_flags = m_current_document_find->supportedFindFlags();

  m_whole_word_action->setEnabled(supported_flags & FindWholeWords);
  m_case_sensitive_action->setEnabled(supported_flags & FindCaseSensitively);
  m_regular_expression_action->setEnabled(supported_flags & FindRegularExpression);

  const auto replace_enabled = m_current_document_find->isEnabled() && m_current_document_find->supportsReplace();

  m_preserve_case_action->setEnabled(supported_flags & FindPreserveCase && !regexp && replace_enabled);
}

auto FindToolBar::setFocusToCurrentFindSupport() const -> void
{
  if (!m_current_document_find->setFocusToCurrentFindSupport())
    if (const auto w = focusWidget())
      w->clearFocus();
}

auto FindToolBar::hideAndResetFocus() -> void
{
  m_current_document_find->setFocusToCurrentFindSupport();
  hide();
}

auto FindToolBar::findToolBarPlaceHolder() -> FindToolBarPlaceHolder*
{
  const auto placeholders = FindToolBarPlaceHolder::allFindToolbarPlaceHolders();
  auto candidate = QApplication::focusWidget();

  while (candidate) {
    for (const auto ph : placeholders) {
      if (ph->owner() == candidate)
        return ph;
    }
    candidate = candidate->parentWidget();
  }
  return nullptr;
}

auto FindToolBar::toolBarHasFocus() const -> bool
{
  return QApplication::focusWidget() == focusWidget();
}

auto FindToolBar::controlStyle(const bool replace_is_visible) const -> control_style
{
  const auto current_find_button_style = m_ui.findNextButton->toolButtonStyle();
  const auto full_width = width();

  if (replace_is_visible) {
    // Since the replace buttons do not collapse to icons, they have precedence, here.
    const auto replace_fixed_width = m_ui.replaceLabel->sizeHint().width() + m_ui.replaceButton->sizeHint().width() + m_ui.replaceNextButton->sizeHint().width() + m_ui.replaceAllButton->sizeHint().width() + m_ui.advancedButton->sizeHint().width();
    return full_width - replace_fixed_width >= minimum_width_for_complex_layout ? control_style::text : control_style::hidden;
  }

  const auto find_width = [this] {
    const auto select_all_width = m_current_document_find->supportsSelectAll() ? m_ui.selectAllButton->sizeHint().width() : 0;
    return m_ui.findLabel->sizeHint().width() + m_ui.findNextButton->sizeHint().width() + m_ui.findPreviousButton->sizeHint().width() + select_all_width + findbutton_spacer_width + m_ui.close->sizeHint().width();
  };

  setFindButtonStyle(Qt::ToolButtonTextOnly);
  const auto find_with_text_width = find_width();
  setFindButtonStyle(Qt::ToolButtonIconOnly);
  const auto find_with_icons_width = find_width();
  setFindButtonStyle(current_find_button_style);

  if (full_width - find_with_icons_width < minimum_width_for_complex_layout)
    return control_style::hidden;
  if (full_width - find_with_text_width < minimum_width_for_complex_layout)
    return control_style::icon;

  return control_style::text;
}

auto FindToolBar::setFindButtonStyle(const Qt::ToolButtonStyle style) const -> void
{
  m_ui.findPreviousButton->setToolButtonStyle(style);
  m_ui.findNextButton->setToolButtonStyle(style);
}

/*!
    Accepts the candidate find of the current focus widget (if any), and moves the tool bar
    there, if it was visible before.
*/
auto FindToolBar::acceptCandidateAndMoveToolBar() -> void
{
  if (!m_current_document_find->candidate())
    return;
  if (isVisible()) {
    openFindToolBar(update_highlight);
  } else {
    // Make sure we are really hidden, and not just because our parent was hidden.
    // Otherwise when the tool bar gets visible again, it will be in a different widget than
    // the current document find it acts on.
    // Test case: Open find in navigation side bar, hide side bar, click into editor,
    // trigger find next, show side bar
    hide();
    m_current_document_find->acceptCandidate();
  }
}

auto FindToolBar::indicateSearchState(const IFindSupport::Result search_state) -> void
{
  m_last_result = search_state;
  m_ui.findEdit->validate();
}

auto FindToolBar::openFind(const bool focus) -> void
{
  setBackward(false);
  OpenFlags flags = update_all;

  if (!focus) // remove focus flag
    flags = flags & ~update_focus_and_select;

  openFindToolBar(flags);
}

auto FindToolBar::openFindToolBar(const OpenFlags flags) -> void
{
  installEventFilters();
  const auto holder = findToolBarPlaceHolder();

  if (!holder)
    return;

  if (const auto previousHolder = FindToolBarPlaceHolder::getCurrent(); previousHolder != holder) {
    if (previousHolder)
      previousHolder->setWidget(nullptr);
    holder->setWidget(this);
    FindToolBarPlaceHolder::setCurrent(holder);
  }

  m_current_document_find->acceptCandidate();
  holder->setVisible(true);
  setVisible(true);

  //     We do not want to change the text when we currently have the focus and user presses the
  //     find shortcut
  //    if (!focus || !toolBarHasFocus()) {
  if (flags & update_find_text) {
    if (const auto text = m_current_document_find->currentFindString(); !text.isEmpty())
      setFindText(text);
  }

  if (flags & update_focus_and_select)
    setFocus();

  if (flags & update_find_scope)
    m_current_document_find->defineFindScope();

  if (flags & update_highlight)
    m_current_document_find->highlightAll(getFindText(), effectiveFindFlags());

  if (flags & update_focus_and_select)
    selectFindText();
}

auto FindToolBar::findNextSelected() -> void
{
  openFindToolBar(OpenFlags(update_all & ~update_focus_and_select));
  invokeFindNext();
}

auto FindToolBar::findPreviousSelected() -> void
{
  openFindToolBar(OpenFlags(update_all & ~update_focus_and_select));
  invokeFindPrevious();
}

auto FindToolBar::selectAll() const -> void
{
  if (m_current_document_find->isEnabled()) {
    const auto ef = effectiveFindFlags();
    Find::updateFindCompletion(getFindText(), ef);
    m_current_document_find->selectAll(getFindText(), ef);
  }
}

auto FindToolBar::focusNextPrevChild(const bool next) -> bool
{
  const auto options_button = m_ui.findEdit->button(Utils::FancyLineEdit::Left);

  // close tab order
  if (next && m_ui.advancedButton->hasFocus())
    options_button->setFocus(Qt::TabFocusReason);
  else if (next && options_button->hasFocus())
    m_ui.findEdit->setFocus(Qt::TabFocusReason);
  else if (!next && options_button->hasFocus())
    m_ui.advancedButton->setFocus(Qt::TabFocusReason);
  else if (!next && m_ui.findEdit->hasFocus())
    options_button->setFocus(Qt::TabFocusReason);
  else
    return StyledBar::focusNextPrevChild(next);

  return true;
}

auto FindToolBar::resizeEvent(QResizeEvent *event) -> void
{
  Q_UNUSED(event)
  QMetaObject::invokeMethod(this, &FindToolBar::updateToolBar, Qt::QueuedConnection);
}

auto FindToolBar::writeSettings() const -> void
{
  const auto settings = ICore::settings();
  settings->beginGroup("Find");
  settings->beginGroup("FindToolBar");
  settings->setValueWithDefault("Backward", (m_find_flags & FindBackward) != 0, false);
  settings->setValueWithDefault("CaseSensitively", (m_find_flags & FindCaseSensitively) != 0, false);
  settings->setValueWithDefault("WholeWords", (m_find_flags & FindWholeWords) != 0, false);
  settings->setValueWithDefault("RegularExpression", (m_find_flags & FindRegularExpression) != 0, false);
  settings->setValueWithDefault("PreserveCase", (m_find_flags & FindPreserveCase) != 0, false);
  settings->endGroup();
  settings->endGroup();
}

auto FindToolBar::readSettings() -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(QLatin1String("Find"));
  settings->beginGroup(QLatin1String("FindToolBar"));
  FindFlags flags;

  if (settings->value(QLatin1String("Backward"), false).toBool())
    flags |= FindBackward;

  if (settings->value(QLatin1String("CaseSensitively"), false).toBool())
    flags |= FindCaseSensitively;

  if (settings->value(QLatin1String("WholeWords"), false).toBool())
    flags |= FindWholeWords;

  if (settings->value(QLatin1String("RegularExpression"), false).toBool())
    flags |= FindRegularExpression;

  if (settings->value(QLatin1String("PreserveCase"), false).toBool())
    flags |= FindPreserveCase;

  settings->endGroup();
  settings->endGroup();
  m_find_flags = flags;
  findFlagsChanged();
}

auto FindToolBar::setUseFakeVim(const bool on) -> void
{
  m_use_fake_vim = on;
}

auto FindToolBar::setFindFlag(const FindFlag flag, const bool enabled) -> void
{
  if (const auto has_flag = hasFindFlag(flag); has_flag && enabled || !has_flag && !enabled)
    return;

  if (enabled)
    m_find_flags |= flag;
  else
    m_find_flags &= ~flag;

  if (flag != FindBackward)
    findFlagsChanged();
}

auto FindToolBar::hasFindFlag(const FindFlag flag) const -> bool
{
  return m_find_flags & flag;
}

auto FindToolBar::setCaseSensitive(const bool sensitive) -> void
{
  setFindFlag(FindCaseSensitively, sensitive);
}

auto FindToolBar::setWholeWord(const bool whole_only) -> void
{
  setFindFlag(FindWholeWords, whole_only);
}

auto FindToolBar::setRegularExpressions(const bool regexp) -> void
{
  setFindFlag(FindRegularExpression, regexp);
}

auto FindToolBar::setPreserveCase(const bool preserve_case) -> void
{
  setFindFlag(FindPreserveCase, preserve_case);
}

auto FindToolBar::setBackward(const bool backward) -> void
{
  setFindFlag(FindBackward, backward);
}

auto FindToolBar::setLightColoredIcon(const bool light_colored) const -> void
{
  m_local_find_next_action->setIcon(light_colored ? Utils::Icons::NEXT.icon() : Utils::Icons::NEXT_TOOLBAR.icon());
  m_local_find_previous_action->setIcon(light_colored ? Utils::Icons::PREV.icon() : Utils::Icons::PREV_TOOLBAR.icon());
  m_ui.close->setIcon(light_colored ? Utils::Icons::CLOSE_FOREGROUND.icon() : Utils::Icons::CLOSE_TOOLBAR.icon());
}

auto FindToolBar::updateFindReplaceEnabled() -> void
{
  const auto enabled = !getFindText().isEmpty();

  if (enabled != m_find_enabled) {
    m_local_find_next_action->setEnabled(enabled);
    m_local_find_previous_action->setEnabled(enabled);
    m_find_enabled = enabled;
  }

  m_local_select_all_action->setEnabled(enabled && m_current_document_find->supportsSelectAll());
  m_find_next_action->setEnabled(enabled && m_find_in_document_action->isEnabled());
  m_find_previous_action->setEnabled(enabled && m_find_in_document_action->isEnabled());

  updateReplaceEnabled();
}

auto FindToolBar::updateReplaceEnabled() const -> void
{
  const auto enabled = m_find_enabled && m_current_document_find->supportsReplace();

  m_local_replace_action->setEnabled(enabled);
  m_local_replace_all_action->setEnabled(enabled);
  m_local_replace_next_action->setEnabled(enabled);
  m_local_replace_previous_action->setEnabled(enabled);

  const auto current_candidate = m_current_document_find->candidate();
  const auto globals_enabled = current_candidate ? current_candidate->supportsReplace() : false;

  m_replace_action->setEnabled(globals_enabled);
  m_replace_all_action->setEnabled(globals_enabled);
  m_replace_next_action->setEnabled(globals_enabled);
  m_replace_previous_action->setEnabled(globals_enabled);
}

} // namespace Internal
} // namespace Core
