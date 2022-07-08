// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "genericproposalmodel.hpp"
#include "assistproposalitem.hpp"

#include <texteditor/texteditorsettings.hpp>
#include <texteditor/completionsettings.hpp>

#include <utils/algorithm.hpp>

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QtAlgorithms>
#include <QHash>

#include <algorithm>

using namespace TextEditor;

QT_BEGIN_NAMESPACE
auto qHash(const AssistProposalItem &item)
{
    return qHash(item.text());
}
QT_END_NAMESPACE

namespace {

constexpr int kMaxSort = 1000;
constexpr int kMaxPrefixFilter = 100;

struct ContentLessThan {
  ContentLessThan(const QString &prefix) : m_prefix(prefix) {}

  auto operator()(const AssistProposalItemInterface *a, const AssistProposalItemInterface *b) -> bool
  {
    // The order is case-insensitive in principle, but case-sensitive when this
    // would otherwise mean equality
    const auto &lowera = a->text().toLower();
    const auto &lowerb = b->text().toLower();
    const auto &lowerprefix = m_prefix.toLower();

    // All continuations should go before all fuzzy matches
    if (const auto diff = lowera.startsWith(lowerprefix) - lowerb.startsWith(lowerprefix))
      return diff > 0;
    if (const auto diff = a->text().startsWith(m_prefix) - b->text().startsWith(m_prefix))
      return diff > 0;

    // If order is different, show higher ones first.
    if (a->order() != b->order())
      return a->order() > b->order();

    if (lowera == lowerb)
      return lessThan(a->text(), b->text());
    return lessThan(lowera, lowerb);
  }

  auto lessThan(const QString &a, const QString &b) -> bool
  {
    auto pa = a.begin();
    auto pb = b.begin();

    CharLessThan charLessThan;
    enum {
      Letter,
      SmallerNumber,
      BiggerNumber
    } state = Letter;
    for (; pa != a.end() && pb != b.end(); ++pa, ++pb) {
      if (*pa == *pb)
        continue;
      if (state != Letter) {
        if (!pa->isDigit() || !pb->isDigit())
          break;
      } else if (pa->isDigit() && pb->isDigit()) {
        if (charLessThan(*pa, *pb))
          state = SmallerNumber;
        else
          state = BiggerNumber;
      } else {
        return charLessThan(*pa, *pb);
      }
    }

    if (state == Letter)
      return pa == a.end() && pb != b.end();
    if (pa != a.end() && pa->isDigit())
      return false; //more digits
    if (pb != b.end() && pb->isDigit())
      return true;                 //fewer digits
    return state == SmallerNumber; //same length, compare first different digit in the sequence
  }

  struct CharLessThan {
    auto operator()(const QChar &a, const QChar &b) -> bool
    {
      if (a == '_')
        return false;
      if (b == '_')
        return true;
      return a < b;
    }
  };

private:
  QString m_prefix;
};

} // Anonymous

GenericProposalModel::GenericProposalModel() = default;

GenericProposalModel::~GenericProposalModel()
{
  qDeleteAll(m_originalItems);
}

auto GenericProposalModel::loadContent(const QList<AssistProposalItemInterface*> &items) -> void
{
  m_originalItems = items;
  m_currentItems = items;
  m_duplicatesRemoved = false;
  for (auto i = 0; i < m_originalItems.size(); ++i)
    m_idByText.insert(m_originalItems.at(i)->text(), i);
}

auto GenericProposalModel::hasItemsToPropose(const QString &prefix, AssistReason reason) const -> bool
{
  return size() != 0 && (keepPerfectMatch(reason) || !isPerfectMatch(prefix));
}

static auto cleanText(const QString &original) -> QString
{
  auto clean = original;
  auto ignore = 0;
  for (int i = clean.length() - 1; i >= 0; --i, ++ignore) {
    const auto &c = clean.at(i);
    if (c.isLetterOrNumber() || c == '_' || c.isHighSurrogate() || c.isLowSurrogate()) {
      break;
    }
  }
  if (ignore)
    clean.chop(ignore);
  return clean;
}

static auto textStartsWith(CaseSensitivity cs, const QString &text, const QString &prefix) -> bool
{
  switch (cs) {
  case CaseInsensitive:
    return text.startsWith(prefix, Qt::CaseInsensitive);
  case CaseSensitive:
    return text.startsWith(prefix, Qt::CaseSensitive);
  case FirstLetterCaseSensitive:
    return prefix.at(0) == text.at(0) && QStringView(text).mid(1).startsWith(QStringView(prefix).mid(1), Qt::CaseInsensitive);
  }

  return false;
}

enum class PerfectMatchType {
  None,
  StartsWith,
  Full,
};

static auto perfectMatch(CaseSensitivity cs, const QString &text, const QString &prefix) -> PerfectMatchType
{
  if (textStartsWith(cs, text, prefix))
    return prefix.size() == text.size() ? PerfectMatchType::Full : PerfectMatchType::StartsWith;

  return PerfectMatchType::None;
}

auto GenericProposalModel::isPerfectMatch(const QString &prefix) const -> bool
{
  if (prefix.isEmpty())
    return false;

  const auto cs = TextEditorSettings::completionSettings().m_caseSensitivity;
  auto hasFullMatch = false;

  for (auto i = 0; i < size(); ++i) {
    const auto &current = cleanText(text(i));
    if (current.isEmpty())
      continue;

    const auto match = perfectMatch(cs, current, prefix);
    if (match == PerfectMatchType::StartsWith)
      return false;

    if (match == PerfectMatchType::Full) {
      if (proposalItem(i)->isKeyword())
        return true;

      if (!proposalItem(i)->isSnippet())
        hasFullMatch = true;
    }
  }

  return hasFullMatch;
}

auto GenericProposalModel::isPrefiltered(const QString &prefix) const -> bool
{
  return !m_prefilterPrefix.isEmpty() && prefix == m_prefilterPrefix;
}

auto GenericProposalModel::setPrefilterPrefix(const QString &prefix) -> void
{
  m_prefilterPrefix = prefix;
}

auto GenericProposalModel::reset() -> void
{
  m_prefilterPrefix.clear();
  m_currentItems = m_originalItems;
}

auto GenericProposalModel::size() const -> int
{
  return m_currentItems.size();
}

auto GenericProposalModel::text(int index) const -> QString
{
  return m_currentItems.at(index)->text();
}

auto GenericProposalModel::icon(int index) const -> QIcon
{
  return m_currentItems.at(index)->icon();
}

auto GenericProposalModel::detail(int index) const -> QString
{
  return m_currentItems.at(index)->detail();
}

auto GenericProposalModel::detailFormat(int index) const -> Qt::TextFormat
{
  return m_currentItems.at(index)->detailFormat();
}

auto GenericProposalModel::removeDuplicates() -> void
{
  if (m_duplicatesRemoved)
    return;

  QHash<QString, quint64> unique;
  auto it = m_originalItems.begin();
  while (it != m_originalItems.end()) {
    const AssistProposalItemInterface *item = *it;
    if (unique.contains(item->text()) && unique.value(item->text()) == item->hash()) {
      delete *it;
      it = m_originalItems.erase(it);
    } else {
      unique.insert(item->text(), item->hash());
      ++it;
    }
  }

  m_duplicatesRemoved = true;
}

auto GenericProposalModel::filter(const QString &prefix) -> void
{
  if (prefix.isEmpty())
    return;

  const auto caseSensitivity = convertCaseSensitivity(TextEditorSettings::completionSettings().m_caseSensitivity);
  const auto regExp = FuzzyMatcher::createRegExp(prefix, caseSensitivity);

  QElapsedTimer timer;
  timer.start();

  m_currentItems.clear();
  const auto lowerPrefix = prefix.toLower();
  const auto checkInfix = prefix.size() >= 3;
  for (const auto &item : qAsConst(m_originalItems)) {
    const auto &text = item->filterText();

    // Direct match?
    if (text.startsWith(prefix)) {
      m_currentItems.append(item);
      item->setProposalMatch(text.length() == prefix.length() ? AssistProposalItemInterface::ProposalMatch::Full : AssistProposalItemInterface::ProposalMatch::Exact);
      continue;
    }

    if (text.startsWith(lowerPrefix, Qt::CaseInsensitive)) {
      m_currentItems.append(item);
      item->setProposalMatch(AssistProposalItemInterface::ProposalMatch::Prefix);
      continue;
    }

    if (checkInfix && text.contains(lowerPrefix, Qt::CaseInsensitive)) {
      m_currentItems.append(item);
      item->setProposalMatch(AssistProposalItemInterface::ProposalMatch::Infix);
      continue;
    }

    // Our fuzzy matcher can become unusably slow with certain inputs, so skip it
    // if we'd become unresponsive. See QTCREATORBUG-25419.
    if (timer.elapsed() > 100)
      continue;

    const auto match = regExp.match(text);
    const auto hasPrefixMatch = match.capturedStart() == 0;
    const auto hasInfixMatch = checkInfix && match.hasMatch();
    if (hasPrefixMatch || hasInfixMatch)
      m_currentItems.append(item);
  }
}

auto GenericProposalModel::convertCaseSensitivity(CaseSensitivity textEditorCaseSensitivity) -> FuzzyMatcher::CaseSensitivity
{
  switch (textEditorCaseSensitivity) {
  case CaseSensitive:
    return FuzzyMatcher::CaseSensitivity::CaseSensitive;
  case FirstLetterCaseSensitive:
    return FuzzyMatcher::CaseSensitivity::FirstLetterCaseSensitive;
  default:
    return FuzzyMatcher::CaseSensitivity::CaseInsensitive;
  }
}

auto GenericProposalModel::isSortable(const QString &prefix) const -> bool
{
  Q_UNUSED(prefix)

  if (m_currentItems.size() < kMaxSort)
    return true;
  return false;
}

auto GenericProposalModel::sort(const QString &prefix) -> void
{
  std::stable_sort(m_currentItems.begin(), m_currentItems.end(), ContentLessThan(prefix));
}

auto GenericProposalModel::persistentId(int index) const -> int
{
  return m_idByText.value(m_currentItems.at(index)->text());
}

auto GenericProposalModel::containsDuplicates() const -> bool
{
  return true;
}

auto GenericProposalModel::supportsPrefixExpansion() const -> bool
{
  return true;
}

auto GenericProposalModel::keepPerfectMatch(AssistReason reason) const -> bool
{
  return reason != IdleEditor;
}

auto GenericProposalModel::proposalPrefix() const -> QString
{
  if (m_currentItems.size() >= kMaxPrefixFilter || m_currentItems.isEmpty())
    return QString();

  // Compute common prefix
  auto commonPrefix = m_currentItems.first()->text();
  for (int i = 1, ei = m_currentItems.size(); i < ei; ++i) {
    auto nextItem = m_currentItems.at(i)->text();
    const int length = qMin(commonPrefix.length(), nextItem.length());
    commonPrefix.truncate(length);
    nextItem.truncate(length);

    while (commonPrefix != nextItem) {
      commonPrefix.chop(1);
      nextItem.chop(1);
    }

    if (commonPrefix.isEmpty()) // there is no common prefix, so return.
      return commonPrefix;
  }

  return commonPrefix;
}

auto GenericProposalModel::proposalItem(int index) const -> AssistProposalItemInterface*
{
  return m_currentItems.at(index);
}

auto GenericProposalModel::indexOf(const std::function<bool(AssistProposalItemInterface *)> &predicate) const -> int
{
  for (int index = 0, end = m_currentItems.size(); index < end; ++index) {
    if (predicate(m_currentItems.at(index)))
      return index;
  }
  return -1;
}
