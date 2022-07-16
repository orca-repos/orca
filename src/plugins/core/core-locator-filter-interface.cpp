// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-locator-filter-interface.hpp"

#include <utils/fuzzymatcher.hpp>

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>

using namespace Utils;

namespace Orca::Plugin::Core {

/*!
    \class Orca::Plugin::Core::ILocatorFilter
    \inheaderfile coreplugin/locator/ilocatorfilter.h
    \inmodule Orca

    \brief The ILocatorFilter class adds a locator filter.

    The filter is added to \uicontrol Tools > \uicontrol Locate.
*/

/*!
    \class Orca::Plugin::Core::LocatorFilterEntry
    \inmodule Orca
    \internal
*/

/*!
    \class Orca::Plugin::Core::LocatorFilterEntry::HighlightInfo
    \inmodule Orca
    \internal
*/

static QList<ILocatorFilter*> g_locator_filters;

/*!
    Constructs a locator filter with \a parent. Call from subclasses.
*/
ILocatorFilter::ILocatorFilter(QObject *parent): QObject(parent)
{
  g_locator_filters.append(this);
}

ILocatorFilter::~ILocatorFilter()
{
  g_locator_filters.removeOne(this);
}

/*!
    Returns the list of all locator filters.
*/
auto ILocatorFilter::allLocatorFilters() -> QList<ILocatorFilter*>
{
  return g_locator_filters;
}

/*!
    Specifies a shortcut string that can be used to explicitly choose this
    filter in the locator input field by preceding the search term with the
    shortcut string and a whitespace.

    The default value is an empty string.

    \sa setShortcutString()
*/
auto ILocatorFilter::shortcutString() const -> QString
{
  return m_shortcut;
}

/*!
    Performs actions that need to be done in the main thread before actually
    running the search for \a entry.

    Called on the main thread before matchesFor() is called in a separate
    thread.

    The default implementation does nothing.

    \sa matchesFor()
*/
auto ILocatorFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
}

/*!
    Sets the default \a shortcut string that can be used to explicitly choose
    this filter in the locator input field. Call for example from the
    constructor of subclasses.

    \sa shortcutString()
*/
auto ILocatorFilter::setDefaultShortcutString(const QString &shortcut) -> void
{
  m_default_shortcut = shortcut;
  m_shortcut = shortcut;
}

/*!
    Sets the current shortcut string of the filter to \a shortcut. Use
    setDefaultShortcutString() if you want to set the default shortcut string
    instead.

    \sa setDefaultShortcutString()
*/
auto ILocatorFilter::setShortcutString(const QString &shortcut) -> void
{
  m_shortcut = shortcut;
}

constexpr char k_shortcut_string_key[] = "shortcut";
constexpr char k_included_by_default_key[] = "includeByDefault";

/*!
    Returns data that can be used to restore the settings for this filter
    (for example at startup).
    By default, adds the base settings (shortcut string, included by default)
    and calls saveState() with a JSON object where subclasses should write
    their custom settings.

    \sa restoreState()
*/
auto ILocatorFilter::saveState() const -> QByteArray
{
  QJsonObject obj;

  if (shortcutString() != m_default_shortcut)
    obj.insert(k_shortcut_string_key, shortcutString());

  if (isIncludedByDefault() != m_default_included_by_default)
    obj.insert(k_included_by_default_key, isIncludedByDefault());

  saveState(obj);

  if (obj.isEmpty())
    return {};

  QJsonDocument doc;
  doc.setObject(obj);
  return doc.toJson(QJsonDocument::Compact);
}

/*!
    Restores the \a state of the filter from data previously created by
    saveState().

    \sa saveState()
*/
auto ILocatorFilter::restoreState(const QByteArray &state) -> void
{
  if (const auto doc = QJsonDocument::fromJson(state); state.isEmpty() || doc.isObject()) {
    const auto obj = doc.object();
    setShortcutString(obj.value(k_shortcut_string_key).toString(m_default_shortcut));
    setIncludedByDefault(obj.value(k_included_by_default_key).toBool(m_default_included_by_default));
    restoreState(obj);
  } else {
    // TODO read old settings, remove some time after Qt Creator 4.15
    m_shortcut = m_default_shortcut;
    m_included_by_default = m_default_included_by_default;

    // TODO this reads legacy settings from Qt Creator < 4.15
    QDataStream in(state);
    in >> m_shortcut;
    in >> m_included_by_default;
  }
}

/*!
    Opens a dialog for the \a parent widget that allows the user to configure
    various aspects of the filter. Called when the user requests to configure
    the filter.

    Set \a needsRefresh to \c true, if a refresh() should be done after
    closing the dialog. Return \c false if the user canceled the dialog.

    The default implementation allows changing the shortcut and whether the
    filter is included by default.

    \sa refresh()
*/
auto ILocatorFilter::openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool
{
  Q_UNUSED(needs_refresh)
  return openConfigDialog(parent, nullptr);
}

/*!
    Returns whether a case sensitive or case insensitive search should be
    performed for the search term \a str.
*/
auto ILocatorFilter::caseSensitivity(const QString &str) -> Qt::CaseSensitivity
{
  return str == str.toLower() ? Qt::CaseInsensitive : Qt::CaseSensitive;
}

/*!
    Creates the search term \a text as a regular expression with case
    sensitivity set to \a caseSensitivity.
*/
auto ILocatorFilter::createRegExp(const QString &text, const Qt::CaseSensitivity case_sensitivity) -> QRegularExpression
{
  return FuzzyMatcher::createRegExp(text, case_sensitivity);
}

/*!
    Returns information for highlighting the results of matching the regular
    expression, specified by \a match, for the data of the type \a dataType.
*/
auto ILocatorFilter::highlightInfo(const QRegularExpressionMatch &match, LocatorFilterEntry::HighlightInfo::DataType data_type) -> LocatorFilterEntry::HighlightInfo
{
  const auto [starts, lengths] = FuzzyMatcher::highlightingPositions(match);
  return {starts, lengths, data_type};
}

/*!
    Specifies a title for configuration dialogs.
*/
auto ILocatorFilter::msgConfigureDialogTitle() -> QString
{
  return tr("Filter Configuration");
}

/*!
    Specifies a label for the prefix input field in configuration dialogs.
*/
auto ILocatorFilter::msgPrefixLabel() -> QString
{
  return tr("Prefix:");
}

/*!
    Specifies a tooltip for the  prefix input field in configuration dialogs.
*/
auto ILocatorFilter::msgPrefixToolTip() -> QString
{
  return tr("Type the prefix followed by a space and search term to restrict search to the filter.");
}

/*!
    Specifies a label for the include by default input field in configuration
    dialogs.
*/
auto ILocatorFilter::msgIncludeByDefault() -> QString
{
  return tr("Include by default");
}

/*!
    Specifies a tooltip for the include by default input field in configuration
    dialogs.
*/
auto ILocatorFilter::msgIncludeByDefaultToolTip() -> QString
{
  return tr("Include the filter when not using a prefix for searches.");
}

/*!
    Returns whether a configuration dialog is available for this filter.

    The default is \c true.

    \sa setConfigurable()
*/
auto ILocatorFilter::isConfigurable() const -> bool
{
  return m_is_configurable;
}

/*!
    Returns whether using the shortcut string is required to use this filter.
    The default is \c false.

    \sa shortcutString()
    \sa setIncludedByDefault()
*/
auto ILocatorFilter::isIncludedByDefault() const -> bool
{
  return m_included_by_default;
}

/*!
    Sets the default setting for whether using the shortcut string is required
    to use this filter to \a includedByDefault.

    Call for example from the constructor of subclasses.

    \sa isIncludedByDefault()
*/
auto ILocatorFilter::setDefaultIncludedByDefault(const bool included_by_default) -> void
{
  m_default_included_by_default = included_by_default;
  m_included_by_default = included_by_default;
}

/*!
    Sets whether using the shortcut string is required to use this filter to
    \a includedByDefault. Use setDefaultIncludedByDefault() if you want to
    set the default value instead.

    \sa setDefaultIncludedByDefault()
*/
auto ILocatorFilter::setIncludedByDefault(const bool included_by_default) -> void
{
  m_included_by_default = included_by_default;
}

/*!
    Returns whether the filter should be hidden in the
    \uicontrol {Locator filters} filter, menus, and locator settings.

    The default is \c false.

    \sa setHidden()
*/
auto ILocatorFilter::isHidden() const -> bool
{
  return m_hidden;
}

/*!
    Sets the filter in the \uicontrol {Locator filters} filter, menus, and
    locator settings to \a hidden. Call in the constructor of subclasses.
*/
auto ILocatorFilter::setHidden(const bool hidden) -> void
{
  m_hidden = hidden;
}

/*!
    Returns whether the filter is currently available. Disabled filters are
    neither visible in menus nor included in searches, even when the search is
    prefixed with their shortcut string.

    The default is \c true.

    \sa setEnabled()
*/
auto ILocatorFilter::isEnabled() const -> bool
{
  return m_enabled;
}

/*!
    Returns the filter's unique ID.

    \sa setId()
*/
auto ILocatorFilter::id() const -> Id
{
  return m_id;
}

/*!
    Returns the filter's action ID.
*/
auto ILocatorFilter::actionId() const -> Id
{
  return m_id.withPrefix("Locator.");
}

/*!
    Returns the filter's translated display name.

    \sa setDisplayName()
*/
auto ILocatorFilter::displayName() const -> QString
{
  return m_display_name;
}

/*!
    Returns the priority that is used for ordering the results when multiple
    filters are used.

    The default is ILocatorFilter::Medium.

    \sa setPriority()
*/
auto ILocatorFilter::priority() const -> Priority
{
  return m_priority;
}

/*!
    Sets whether the filter is currently available to \a enabled.

    \sa isEnabled()
*/
auto ILocatorFilter::setEnabled(const bool enabled) -> void
{
  m_enabled = enabled;
}

/*!
    Sets the filter's unique \a id.
    Subclasses must set the ID in their constructor.

    \sa id()
*/
auto ILocatorFilter::setId(const Id id) -> void
{
  m_id = id;
}

/*!
    Sets the \a priority of results of this filter in the result list.

    \sa priority()
*/
auto ILocatorFilter::setPriority(const Priority priority) -> void
{
  m_priority = priority;
}

/*!
    Sets the translated display name of this filter to \a
    displayString.

    Subclasses must set the display name in their constructor.

    \sa displayName()
*/
auto ILocatorFilter::setDisplayName(const QString &display_string) -> void
{
  m_display_name = display_string;
}

/*!
    Returns a longer, human-readable description of what the filter does.

    \sa setDescription()
*/
auto ILocatorFilter::description() const -> QString
{
  return m_description;
}

/*!
    Sets the longer, human-readable \a description of what the filter does.

    \sa description()
*/
auto ILocatorFilter::setDescription(const QString &description) -> void
{
  m_description = description;
}

/*!
    Sets whether the filter provides a configuration dialog to \a configurable.
    Most filters should at least provide the default dialog.

    \sa isConfigurable()
*/
auto ILocatorFilter::setConfigurable(const bool configurable) -> void
{
  m_is_configurable = configurable;
}

/*!
    Shows the standard configuration dialog with options for the prefix string
    and for isIncludedByDefault(). The \a additionalWidget is added at the top.
    Ownership of \a additionalWidget stays with the caller, but its parent is
    reset to \c nullptr.

    Returns \c false if the user canceled the dialog.
*/
auto ILocatorFilter::openConfigDialog(QWidget *parent, QWidget *additional_widget) -> bool
{
  QDialog dialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
  dialog.setWindowTitle(msgConfigureDialogTitle());

  const auto vlayout = new QVBoxLayout(&dialog);
  const auto hlayout = new QHBoxLayout;
  const auto shortcut_edit = new QLineEdit(shortcutString());
  const auto include_by_default = new QCheckBox(msgIncludeByDefault());
  include_by_default->setToolTip(msgIncludeByDefaultToolTip());
  include_by_default->setChecked(isIncludedByDefault());

  const auto prefix_label = new QLabel(msgPrefixLabel());
  prefix_label->setToolTip(msgPrefixToolTip());
  hlayout->addWidget(prefix_label);
  hlayout->addWidget(shortcut_edit);
  hlayout->addWidget(include_by_default);

  const auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (additional_widget)
    vlayout->addWidget(additional_widget);

  vlayout->addLayout(hlayout);
  vlayout->addStretch();
  vlayout->addWidget(button_box);

  auto accepted = false;
  if (dialog.exec() == QDialog::Accepted) {
    setShortcutString(shortcut_edit->text().trimmed());
    setIncludedByDefault(include_by_default->isChecked());
    accepted = true;
  }

  if (additional_widget) {
    additional_widget->setVisible(false);
    additional_widget->setParent(nullptr);
  }

  return accepted;
}

/*!
    Saves the filter settings and state to the JSON \a object.

    The default implementation does nothing.

    Implementations should write key-value pairs to the \a object for their
    custom settings that changed from the default. Default values should
    never be saved.
*/
auto ILocatorFilter::saveState(QJsonObject &object) const -> void
{
  Q_UNUSED(object)
}

/*!
    Reads the filter settings and state from the JSON \a object

    The default implementation does nothing.

    Implementations should read their custom settings from the \a object,
    resetting any missing setting to its default value.
*/
auto ILocatorFilter::restoreState(const QJsonObject &object) -> void
{
  Q_UNUSED(object)
}

/*!
    Returns if \a state must be restored via pre-4.15 settings reading.
*/
auto ILocatorFilter::isOldSetting(const QByteArray &state) -> bool
{
  if (state.isEmpty())
    return false;

  const auto doc = QJsonDocument::fromJson(state);
  return !doc.isObject();
}

/*!
    \fn QList<Orca::Plugin::Core::LocatorFilterEntry> Orca::Plugin::Core::ILocatorFilter::matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry)

    Returns the list of results of this filter for the search term \a entry.
    This is run in a separate thread, but is guaranteed to only run in a single
    thread at any given time. Quickly running preparations can be done in the
    GUI thread in prepareSearch().

    Implementations should do a case sensitive or case insensitive search
    depending on caseSensitivity(). If \a future is \c canceled, the search
    should be aborted.

    \sa prepareSearch()
    \sa caseSensitivity()
*/

/*!
    \fn void Orca::Plugin::Core::ILocatorFilter::accept(Orca::Plugin::Core::const LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const

    Called with the entry specified by \a selection when the user activates it
    in the result list.
    Implementations can return a new search term \a newText, which has \a selectionLength characters
    starting from \a selectionStart preselected, and the cursor set to the end of the selection.
*/

/*!
    \fn void Orca::Plugin::Core::ILocatorFilter::refresh(QFutureInterface<void> &future)

    Refreshes cached data asynchronously.

    If \a future is \c canceled, the refresh should be aborted.
*/

/*!
    \enum Orca::Plugin::Core::ILocatorFilter::Priority

    This enum value holds the priority that is used for ordering the results
    when multiple filters are used.

    \value  Highest
            The results for this filter are placed above the results for filters
            that have other priorities.
    \value  High
    \value  Medium
            The default value.
    \value  Low
            The results for this filter are placed below the results for filters
            that have other priorities.
*/

/*!
    \enum Orca::Plugin::Core::ILocatorFilter::MatchLevel

    This enum value holds the level for ordering the results based on how well
    they match the search criteria.

    \value Best
           The result is the best match for the regular expression.
    \value Better
    \value Good
    \value Normal
    \value Count
           The result has the highest number of matches for the regular
           expression.
*/

} // namespace Orca::Plugin::Core
