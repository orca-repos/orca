// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findplugin.hpp"
#include "currentdocumentfind.hpp"
#include "findtoolbar.hpp"
#include "findtoolwindow.hpp"
#include "searchresultwindow.hpp"
#include "ifindfilter.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreconstants.hpp>
#include <core/icontext.hpp>
#include <core/icore.hpp>
#include <core/coreplugin.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <QMenu>
#include <QStringListModel>
#include <QAction>
#include <QSettings>

/*!
    \namespace Core::Internal::ItemDataRoles
    \internal
*/

/*!
    \class Core::Find
    \inmodule Orca
    \internal
*/

Q_DECLARE_METATYPE(Core::IFindFilter*)

using namespace Qt;
using namespace Utils;

namespace {
constexpr int max_completions = 50;
}

namespace Core {

struct CompletionEntry {
  friend auto operator<<(QDebug d, const CompletionEntry &e) -> QDebug
  {
    QDebugStateSaver saver(d);
    d.noquote();
    d.nospace();
    d << "CompletionEntry(\"" << e.text << "\", flags=" << "0x" << QString::number(e.find_flags, 16) << ')';
    return d;
  }

  QString text;
  FindFlags find_flags;
};

class CompletionModel final : public QAbstractListModel {
public:
  explicit CompletionModel(QObject *p = nullptr) : QAbstractListModel(p) {}

  auto rowCount(const QModelIndex & = QModelIndex()) const -> int override { return m_entries.size(); }
  auto data(const QModelIndex &index, int role = DisplayRole) const -> QVariant override;
  auto writeSettings(QSettings *settings) const -> void;
  auto readSettings(QSettings *settings) -> void;
  auto updateCompletion(const QString &text, FindFlags f) -> void;

private:
  QVector<CompletionEntry> m_entries;
};

auto CompletionModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (index.isValid()) {
    const auto & [text, find_flags] = m_entries.at(index.row());
    switch (role) {
    case DisplayRole:
    case EditRole:
      return {text};
    case Find::completion_model_find_flags_role:
      return {static_cast<int>(find_flags)};
    default:
      break;
    }
  }
  return {};
}

static auto completionSettingsArrayPrefix() -> QString { return QStringLiteral("FindCompletions"); }
static auto completionSettingsTextKey() -> QString { return QStringLiteral("Text"); }
static auto completionSettingsFlagsKey() -> QString { return QStringLiteral("Flags"); }

auto CompletionModel::writeSettings(QSettings *settings) const -> void
{
  if (m_entries.isEmpty()) {
    settings->remove(completionSettingsArrayPrefix());
  } else {
    const auto size = m_entries.size();
    settings->beginWriteArray(completionSettingsArrayPrefix(), static_cast<int>(size));
    for (auto i = 0; i < size; ++i) {
      settings->setArrayIndex(i);
      settings->setValue(completionSettingsTextKey(), m_entries.at(i).text);
      settings->setValue(completionSettingsFlagsKey(), static_cast<int>(m_entries.at(i).find_flags));
    }
    settings->endArray();
  }
}

auto CompletionModel::readSettings(QSettings *settings) -> void
{
  beginResetModel();
  const auto size = settings->beginReadArray(completionSettingsArrayPrefix());
  m_entries.clear();
  m_entries.reserve(size);

  for (auto i = 0; i < size; ++i) {
    settings->setArrayIndex(i);
    CompletionEntry entry;
    entry.text = settings->value(completionSettingsTextKey()).toString();
    entry.find_flags = FindFlags(settings->value(completionSettingsFlagsKey(), 0).toInt());
    if (!entry.text.isEmpty())
      m_entries.append(entry);
  }

  settings->endArray();
  endResetModel();
}

auto CompletionModel::updateCompletion(const QString &text, const FindFlags f) -> void
{
  if (text.isEmpty())
    return;

  beginResetModel();
  Utils::erase(m_entries, equal(&CompletionEntry::text, text));
  m_entries.prepend({text, f});

  while (m_entries.size() > max_completions)
    m_entries.removeLast();

  endResetModel();
}

class FindPrivate final : public QObject {
  Q_DECLARE_TR_FUNCTIONS(Core::Find)

public:
  auto isAnyFilterEnabled() const -> bool;
  auto writeSettings() const -> void;
  auto setFindFlag(FindFlag flag, bool enabled) -> void;
  static auto updateCompletion(const QString &text, QStringList &completions, QStringListModel *model) -> void;
  auto setupMenu() -> void;
  auto setupFilterMenuItems() -> void;
  auto readSettings() -> void;

  Internal::CurrentDocumentFind *m_current_document_find = nullptr;
  Internal::FindToolBar *m_find_tool_bar = nullptr;
  Internal::FindToolWindow *m_find_dialog = nullptr;
  SearchResultWindow *m_search_result_window = nullptr;
  FindFlags m_find_flags;
  CompletionModel m_find_completion_model;
  QStringListModel m_replace_completion_model;
  QStringList m_replace_completions;
  QAction *m_open_find_dialog = nullptr;
};

Find *m_instance = nullptr;
FindPrivate *d = nullptr;

auto Find::destroy() -> void
{
  delete m_instance;
  m_instance = nullptr;
  if (d) {
    delete d->m_current_document_find;
    delete d->m_find_tool_bar;
    delete d->m_find_dialog;
    ExtensionSystem::PluginManager::removeObject(d->m_search_result_window);
    delete d->m_search_result_window;
    delete d;
  }
}

auto Find::instance() -> Find*
{
  return m_instance;
}

auto Find::initialize() -> void
{
  QTC_ASSERT(!m_instance, return);
  m_instance = new Find;

  d = new FindPrivate;
  d->setupMenu();
  d->m_current_document_find = new Internal::CurrentDocumentFind;
  d->m_find_tool_bar = new Internal::FindToolBar(d->m_current_document_find);

  auto *find_tool_bar_context = new IContext(m_instance);
  find_tool_bar_context->setWidget(d->m_find_tool_bar);
  find_tool_bar_context->setContext(Context(Constants::c_findtoolbar));
  ICore::addContextObject(find_tool_bar_context);

  d->m_find_dialog = new Internal::FindToolWindow;
  d->m_search_result_window = new SearchResultWindow(d->m_find_dialog);
  ExtensionSystem::PluginManager::addObject(d->m_search_result_window);
  connect(ICore::instance(), &ICore::saveSettingsRequested, d, &FindPrivate::writeSettings);
}

auto Find::extensionsInitialized() -> void
{
  d->setupFilterMenuItems();
  d->readSettings();
}

auto Find::aboutToShutdown() -> void
{
  d->m_find_tool_bar->setVisible(false);
  d->m_find_tool_bar->setParent(nullptr);
  d->m_current_document_find->removeConnections();
}

auto FindPrivate::isAnyFilterEnabled() const -> bool
{
  return anyOf(m_find_dialog->findFilters(), &IFindFilter::isEnabled);
}

auto Find::openFindDialog(IFindFilter *filter) -> void
{
  d->m_current_document_find->acceptCandidate();

  if (const auto current_find_string = d->m_current_document_find->isEnabled() ? d->m_current_document_find->currentFindString() : QString(); !current_find_string.isEmpty())
    d->m_find_dialog->setFindText(current_find_string);

  d->m_find_dialog->setCurrentFilter(filter);
  SearchResultWindow::instance()->openNewSearchPanel();
}

auto FindPrivate::setupMenu() -> void
{
  const auto medit = ActionManager::actionContainer(Constants::M_EDIT);
  const auto mfind = ActionManager::createMenu(Constants::m_find);
  medit->addMenu(mfind, Constants::G_EDIT_FIND);
  mfind->menu()->setTitle(tr("&Find/Replace"));
  mfind->appendGroup(Constants::g_find_currentdocument);
  mfind->appendGroup(Constants::g_find_filters);
  mfind->appendGroup(Constants::g_find_flags);
  mfind->appendGroup(Constants::g_find_actions);
  mfind->addSeparator(Constants::g_find_flags);
  mfind->addSeparator(Constants::g_find_actions);

  const auto mfindadvanced = ActionManager::createMenu(Constants::m_find_advanced);
  mfindadvanced->menu()->setTitle(tr("Advanced Find"));
  mfind->addMenu(mfindadvanced, Constants::g_find_filters);
  m_open_find_dialog = new QAction(tr("Open Advanced Find..."), this);
  m_open_find_dialog->setIconText(tr("Advanced..."));
  const auto cmd = ActionManager::registerAction(m_open_find_dialog, Constants::advanced_find);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+F")));
  mfindadvanced->addAction(cmd);
  connect(m_open_find_dialog, &QAction::triggered, this, [] { Find::openFindDialog(nullptr); });
}

static auto filterActionName(const IFindFilter *filter) -> QString
{
  return QLatin1String("    ") + filter->displayName();
}

auto FindPrivate::setupFilterMenuItems() -> void
{
  const auto mfindadvanced = ActionManager::actionContainer(Constants::m_find_advanced);
  auto have_enabled_filters = false;
  const Id base("FindFilter.");
  auto sorted_filters = IFindFilter::allFindFilters();
  sort(sorted_filters, &IFindFilter::displayName);

  for(auto &filter: sorted_filters) {
    auto action = new QAction(filterActionName(filter), this);
    const auto is_enabled = filter->isEnabled();

    if (is_enabled)
      have_enabled_filters = true;

    action->setEnabled(is_enabled);
    const auto cmd = ActionManager::registerAction(action, base.withSuffix(filter->id()));
    cmd->setDefaultKeySequence(filter->defaultShortcut());
    cmd->setAttribute(Command::ca_update_text);
    mfindadvanced->addAction(cmd);

    connect(action, &QAction::triggered, this, [filter] { Find::openFindDialog(filter); });
    connect(filter, &IFindFilter::enabledChanged, this, [filter, action] {
      action->setEnabled(filter->isEnabled());
      d->m_open_find_dialog->setEnabled(d->isAnyFilterEnabled());
    });
    connect(filter, &IFindFilter::displayNameChanged, this, [filter, action] { action->setText(filterActionName(filter)); });
  }
  d->m_find_dialog->setFindFilters(sorted_filters);
  d->m_open_find_dialog->setEnabled(have_enabled_filters);
}

auto Find::findFlags() -> FindFlags
{
  return d->m_find_flags;
}

auto Find::setCaseSensitive(const bool sensitive) -> void
{
  d->setFindFlag(FindCaseSensitively, sensitive);
}

auto Find::setWholeWord(const bool whole_only) -> void
{
  d->setFindFlag(FindWholeWords, whole_only);
}

auto Find::setBackward(const bool backward) -> void
{
  d->setFindFlag(FindBackward, backward);
}

auto Find::setRegularExpression(const bool reg_exp) -> void
{
  d->setFindFlag(FindRegularExpression, reg_exp);
}

auto Find::setPreserveCase(const bool preserve_case) -> void
{
  d->setFindFlag(FindPreserveCase, preserve_case);
}

auto FindPrivate::setFindFlag(const FindFlag flag, const bool enabled) -> void
{
  if (const bool has_flag = m_find_flags & flag; (has_flag && enabled) || (!has_flag && !enabled))
    return;

  if (enabled)
    m_find_flags |= flag;
  else
    m_find_flags &= ~flag;

  if (flag != FindBackward) emit m_instance->findFlagsChanged();
}

auto Find::hasFindFlag(const FindFlag flag) -> bool
{
  return d->m_find_flags & flag;
}

auto FindPrivate::writeSettings() const -> void
{
  const auto settings = ICore::settings();
  settings->beginGroup(QLatin1String("Find"));
  settings->setValueWithDefault("Backward", static_cast<bool>(m_find_flags & FindBackward), false);
  settings->setValueWithDefault("CaseSensitively", static_cast<bool>(m_find_flags & FindCaseSensitively), false);
  settings->setValueWithDefault("WholeWords", static_cast<bool>(m_find_flags & FindWholeWords), false);
  settings->setValueWithDefault("RegularExpression", static_cast<bool>(m_find_flags & FindRegularExpression), false);
  settings->setValueWithDefault("PreserveCase", static_cast<bool>(m_find_flags & FindPreserveCase), false);
  m_find_completion_model.writeSettings(settings);
  settings->setValueWithDefault("ReplaceStrings", m_replace_completions);
  settings->endGroup();
  m_find_tool_bar->writeSettings();
  m_find_dialog->writeSettings();
  m_search_result_window->writeSettings();
}

auto FindPrivate::readSettings() -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(QLatin1String("Find"));
  
  QSignalBlocker blocker(m_instance);
  Find::setBackward(settings->value(QLatin1String("Backward"), false).toBool());
  Find::setCaseSensitive(settings->value(QLatin1String("CaseSensitively"), false).toBool());
  Find::setWholeWord(settings->value(QLatin1String("WholeWords"), false).toBool());
  Find::setRegularExpression(settings->value(QLatin1String("RegularExpression"), false).toBool());
  Find::setPreserveCase(settings->value(QLatin1String("PreserveCase"), false).toBool());

  m_find_completion_model.readSettings(settings);
  m_replace_completions = settings->value(QLatin1String("ReplaceStrings")).toStringList();
  m_replace_completion_model.setStringList(m_replace_completions);
  settings->endGroup();
  m_find_tool_bar->readSettings();
  m_find_dialog->readSettings();
  emit m_instance->findFlagsChanged(); // would have been done in the setXXX methods above
}

auto Find::updateFindCompletion(const QString &text, const FindFlags flags) -> void
{
  d->m_find_completion_model.updateCompletion(text, flags);
}

auto Find::updateReplaceCompletion(const QString &text) -> void
{
  FindPrivate::updateCompletion(text, d->m_replace_completions, &d->m_replace_completion_model);
}

auto FindPrivate::updateCompletion(const QString &text, QStringList &completions, QStringListModel *model) -> void
{
  if (text.isEmpty())
    return;

  completions.removeAll(text);
  completions.prepend(text);

  while (completions.size() > max_completions)
    completions.removeLast();

  model->setStringList(completions);
}

auto Find::setUseFakeVim(const bool on) -> void
{
  if (d->m_find_tool_bar)
    d->m_find_tool_bar->setUseFakeVim(on);
}

auto Find::openFindToolBar(const find_direction direction) -> void
{
  if (d->m_find_tool_bar) {
    d->m_find_tool_bar->setBackward(direction == find_backward_direction);
    d->m_find_tool_bar->openFindToolBar();
  }
}

auto Find::findCompletionModel() -> QAbstractListModel*
{
  return &(d->m_find_completion_model);
}

auto Find::replaceCompletionModel() -> QStringListModel*
{
  return &(d->m_replace_completion_model);
}

// declared in textfindconstants.h
auto textDocumentFlagsForFindFlags(const FindFlags flags) -> QTextDocument::FindFlags
{
  QTextDocument::FindFlags text_doc_flags;
  if (flags & FindBackward)
    text_doc_flags |= QTextDocument::FindBackward;
  if (flags & FindCaseSensitively)
    text_doc_flags |= QTextDocument::FindCaseSensitively;
  if (flags & FindWholeWords)
    text_doc_flags |= QTextDocument::FindWholeWords;
  return text_doc_flags;
}

} // namespace Core
