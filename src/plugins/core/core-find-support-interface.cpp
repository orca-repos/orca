// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-find-support-interface.hpp"

#include <utils/fadingindicator.hpp>
#include <utils/stylehelper.hpp>

namespace Orca::Plugin::Core {

/*!
    \class Orca::Plugin::Core::IFindSupport
    \inheaderfile coreplugin/find/ifindsupport.h
    \inmodule Orca

    \brief The IFindSupport class provides functions for searching in a document
     or widget.

    \sa Orca::Plugin::Core::BaseTextFind
*/

/*!
    \enum Orca::Plugin::Core::IFindSupport::Result
    This enum holds whether the search term was found within the search scope
    using the find flags.

    \value Found        The search term was found.
    \value NotFound     The search term was not found.
    \value NotYetFound  The search term has not been found yet.
*/

/*!
    \fn Orca::Plugin::Core::IFindSupport::IFindSupport()
    \internal
*/

/*!
    \fn Orca::Plugin::Core::IFindSupport::~IFindSupport()
    \internal
*/

/*!
    \fn bool Orca::Plugin::Core::IFindSupport::supportsReplace() const
    Returns whether the find filter supports search and replace.
*/

/*!
    \fn bool Orca::Plugin::Core::IFindSupport::supportsSelectAll() const
    Returns whether the find filter supports selecting all results.
*/
auto IFindSupport::supportsSelectAll() const -> bool
{
  return false;
}

/*!
    \fn Orca::Plugin::Core::FindFlags Orca::Plugin::Core::IFindSupport::supportedFindFlags() const
    Returns the find flags, such as whole words or regular expressions,
    that this find filter supports.

    Depending on the returned value, the default find option widgets are
    enabled or disabled.

    The default is Orca::Plugin::Core::FindBackward, Orca::Plugin::Core::FindCaseSensitively,
    Orca::Plugin::Core::FindRegularExpression, Orca::Plugin::Core::FindWholeWords, and
    Orca::Plugin::Core::FindPreserveCase.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::resetIncrementalSearch()
    Resets incremental search to start position.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::clearHighlights()
    Clears highlighting of search results in the searched widget.
*/

/*!
    \fn QString Orca::Plugin::Core::IFindSupport::currentFindString() const
    Returns the current search string.
*/

/*!
    \fn QString Orca::Plugin::Core::IFindSupport::completedFindString() const
    Returns the complete search string.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::highlightAll(const QString &txt, Orca::Plugin::Core::FindFlags findFlags)
    Highlights all search hits for \a txt when using \a findFlags.
*/

/*!
    \fn Orca::Plugin::Core::IFindSupport::Result Orca::Plugin::Core::IFindSupport::findIncremental(const QString &txt, Orca::Plugin::Core::FindFlags findFlags)
    Performs an incremental search of the search term \a txt using \a findFlags.
*/

/*!
    \fn Orca::Plugin::Core::IFindSupport::Result Orca::Plugin::Core::IFindSupport::findStep(const QString &txt, Orca::Plugin::Core::FindFlags findFlags)
    Searches for \a txt using \a findFlags.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::defineFindScope()
    Defines the find scope.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::clearFindScope()
    Clears the find scope.
*/

/*!
    \fn void Orca::Plugin::Core::IFindSupport::changed()
    This signal is emitted when the search changes.
*/

/*!
    Replaces \a before with \a after as specified by \a findFlags.
*/
auto IFindSupport::replace(const QString &before, const QString &after, const FindFlags find_flags) -> void
{
  Q_UNUSED(before)
  Q_UNUSED(after)
  Q_UNUSED(find_flags)
}

/*!
    Replaces \a before with \a after as specified by \a findFlags, and then
    performs findStep().

    Returns whether the find step found another match.
*/
auto IFindSupport::replaceStep(const QString &before, const QString &after, const FindFlags find_flags) -> bool
{
  Q_UNUSED(before)
  Q_UNUSED(after)
  Q_UNUSED(find_flags)
  return false;
}

/*!
    Finds and replaces all instances of \a before with \a after as specified by
    \a findFlags.
*/
auto IFindSupport::replaceAll(const QString &before, const QString &after, const FindFlags find_flags) -> int
{
  Q_UNUSED(before)
  Q_UNUSED(after)
  Q_UNUSED(find_flags)
  return 0;
}

/*!
    Finds and selects all instances of \a txt with specified \a findFlags.
*/
auto IFindSupport::selectAll(const QString &txt, const FindFlags find_flags) -> void
{
  Q_UNUSED(txt)
  Q_UNUSED(find_flags)
}

/*!
    Shows \a parent overlayed with the wrap indicator.
*/
auto IFindSupport::showWrapIndicator(QWidget *parent) -> void
{
  Utils::FadingIndicator::showPixmap(parent, Utils::StyleHelper::dpiSpecificImageFile(QLatin1String(":/find/images/wrapindicator.png")));
}

} // namespace Orca::Plugin::Core
