// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mimemagicrulematcher_p.hpp"

#include "mimetype_p.hpp"

using namespace Utils;
using namespace Utils::Internal;

/*!
    \internal
    \class MimeMagicRuleMatcher
    \inmodule QtCore

    \brief The MimeMagicRuleMatcher class checks a number of rules based on operator "or".

    It is used for rules parsed from XML files.

    \sa MimeType, MimeDatabase, MagicRule, MagicStringRule, MagicByteRule, GlobPattern
    \sa MimeTypeParserBase, MimeTypeParser
*/

MimeMagicRuleMatcher::MimeMagicRuleMatcher(const QString &mime, unsigned thePriority) : m_list(), m_priority(thePriority), m_mimetype(mime) {}

auto MimeMagicRuleMatcher::operator==(const MimeMagicRuleMatcher &other) const -> bool
{
  return m_list == other.m_list && m_priority == other.m_priority;
}

auto MimeMagicRuleMatcher::addRule(const MimeMagicRule &rule) -> void
{
  m_list.append(rule);
}

auto MimeMagicRuleMatcher::addRules(const QList<MimeMagicRule> &rules) -> void
{
  m_list.append(rules);
}

auto MimeMagicRuleMatcher::magicRules() const -> QList<MimeMagicRule>
{
  return m_list;
}

// Check for a match on contents of a file
auto MimeMagicRuleMatcher::matches(const QByteArray &data) const -> bool
{
  for (const MimeMagicRule &magicRule : m_list) {
    if (magicRule.matches(data))
      return true;
  }

  return false;
}

// Return a priority value from 1..100
auto MimeMagicRuleMatcher::priority() const -> unsigned
{
  return m_priority;
}
