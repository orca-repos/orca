// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ioptionspage.h"

#include <core/icore.h>

#include <utils/aspects.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>

using namespace Utils;

namespace Core {

/*!
    \class Core::IOptionsPageProvider
    \inmodule Orca
    \internal
*/
/*!
    \class Core::IOptionsPageWidget
    \inmodule Orca
    \internal
*/

/*!
    \class Core::IOptionsPage
    \inheaderfile coreplugin/dialogs/ioptionspage.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The IOptionsPage class is an interface for providing pages for the
    \uicontrol Options dialog (called \uicontrol Preferences on \macos).

    \image orca-options-dialog.png
*/

/*!

    \fn Utils::Id Core::IOptionsPage::id() const

    Returns a unique identifier for referencing the options page.
*/

/*!
    \fn QString Core::IOptionsPage::displayName() const

    Returns the translated display name of the options page.
*/

/*!
    \fn Utils::Id Core::IOptionsPage::category() const

    Returns the unique id for the category that the options page should be displayed in. This id is
    used for sorting the list on the left side of the \uicontrol Options dialog.
*/

/*!
    \fn QString Core::IOptionsPage::displayCategory() const

    Returns the translated category name of the options page. This name is displayed in the list on
    the left side of the \uicontrol Options dialog.
*/

/*!
    Returns the category icon of the options page. This icon is displayed in the list on the left
    side of the \uicontrol Options dialog.
*/
auto IOptionsPage::categoryIcon() const -> QIcon
{
  return m_category_icon.icon();
}

/*!
    Sets the \a widgetCreator callback to create page widgets on demand. The
    widget will be destroyed on finish().
 */
auto IOptionsPage::setWidgetCreator(const widget_creator &widget_creator) -> void
{
  m_widget_creator = widget_creator;
}

/*!
    Returns the widget to show in the \uicontrol Options dialog. You should create a widget lazily here,
    and delete it again in the finish() method. This method can be called multiple times, so you
    should only create a new widget if the old one was deleted.

    Alternatively, use setWidgetCreator() to set a callback function that is used to
    lazily create a widget in time.

    Either override this function in a derived class, or set a widget creator.
*/

auto IOptionsPage::widget() -> QWidget*
{
  if (!m_widget) {
    if (m_widget_creator) {
      m_widget = m_widget_creator();
    } else if (m_layouter) {
      m_widget = new QWidget;
      m_layouter(m_widget);
    } else {
      QTC_CHECK(false);
    }
  }
  return m_widget;
}

/*!
    Called when selecting the \uicontrol Apply button on the options page dialog.
    Should detect whether any changes were made and store those.

    Either override this function in a derived class, or set a widget creator.

    \sa setWidgetCreator()
*/

auto IOptionsPage::apply() -> void
{
  if (const auto widget = qobject_cast<IOptionsPageWidget*>(m_widget)) {
    widget->apply();
  } else if (m_settings) {
    if (m_settings->isDirty()) {
      m_settings->apply();
      m_settings->writeSettings(ICore::settings());
    }
  }
}

/*!
    Called directly before the \uicontrol Options dialog closes. Here you should
    delete the widget that was created in widget() to free resources.

    Either override this function in a derived class, or set a widget creator.

    \sa setWidgetCreator()
*/

auto IOptionsPage::finish() -> void
{
  if (const auto widget = qobject_cast<IOptionsPageWidget*>(m_widget))
    widget->finish();
  else if (m_settings)
    m_settings->finish();

  delete m_widget;
}

/*!
    Sets \a categoryIconPath as the path to the category icon of the options
    page.
*/
auto IOptionsPage::setCategoryIconPath(const FilePath &category_icon_path) -> void
{
  m_category_icon = Icon({{category_icon_path, Theme::PanelTextColorDark}}, Icon::Tint);
}

auto IOptionsPage::setSettings(AspectContainer *settings) -> void
{
  m_settings = settings;
}

auto IOptionsPage::setLayouter(const std::function<void(QWidget *w)> &layouter) -> void
{
  m_layouter = layouter;
}

/*!
    \fn void Core::IOptionsPage::setId(Utils::Id id)

    Sets the \a id of the options page.
*/

/*!
    \fn void Core::IOptionsPage::setDisplayName(const QString &displayName)

    Sets \a displayName as the display name of the options page.
*/

/*!
    \fn void Core::IOptionsPage::setCategory(Utils::Id category)

    Uses \a category to sort the options pages.
*/

/*!
    \fn void Core::IOptionsPage::setDisplayCategory(const QString &displayCategory)

    Sets \a displayCategory as the display category of the options page.
*/

/*!
    \fn void Core::IOptionsPage::setCategoryIcon(const Utils::Icon &categoryIcon)

    Sets \a categoryIcon as the category icon of the options page.
*/

static QList<IOptionsPage*> g_optionsPages;

/*!
    Constructs an options page with the given \a parent and registers it
    at the global options page pool if \a registerGlobally is \c true.
*/
IOptionsPage::IOptionsPage(QObject *parent, const bool register_globally) : QObject(parent)
{
  if (register_globally)
    g_optionsPages.append(this);
}

/*!
    \internal
 */
IOptionsPage::~IOptionsPage()
{
  g_optionsPages.removeOne(this);
}

/*!
    Returns a list of all options pages.
 */
auto IOptionsPage::allOptionsPages() -> QList<IOptionsPage*>
{
  return g_optionsPages;
}

/*!
    Is used by the \uicontrol Options dialog search filter to match \a regexp to this options
    page. This defaults to take the widget and then looks for all child labels, check boxes, push
    buttons, and group boxes. Should return \c true when a match is found.
*/
auto IOptionsPage::matches(const QRegularExpression &regexp) const -> bool
{
  if (!m_keywords_initialized) {
    const auto that = const_cast<IOptionsPage*>(this);
    const auto widget = that->widget();

    if (!widget)
      return false;

    // find common subwidgets
    for (const auto label : widget->findChildren<QLabel*>())
      m_keywords << stripAccelerator(label->text());
    for (const auto checkbox : widget->findChildren<QCheckBox*>())
      m_keywords << stripAccelerator(checkbox->text());
    for (const auto push_button : widget->findChildren<QPushButton*>())
      m_keywords << stripAccelerator(push_button->text());
    for (const auto group_box : widget->findChildren<QGroupBox*>())
      m_keywords << stripAccelerator(group_box->title());

    m_keywords_initialized = true;
  }

  for (const auto &keyword : qAsConst(m_keywords))
    if (keyword.contains(regexp))
      return true;

  return false;
}

static QList<IOptionsPageProvider*> g_options_pages_providers;

IOptionsPageProvider::IOptionsPageProvider(QObject *parent) : QObject(parent)
{
  g_options_pages_providers.append(this);
}

IOptionsPageProvider::~IOptionsPageProvider()
{
  g_options_pages_providers.removeOne(this);
}

auto IOptionsPageProvider::allOptionsPagesProviders() -> QList<IOptionsPageProvider*>
{
  return g_options_pages_providers;
}

auto IOptionsPageProvider::categoryIcon() const -> QIcon
{
  return m_category_icon.icon();
}

} // Core
