// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-help-item.hpp"

#include "core-help-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/htmldocextractor.hpp>

#include <QVersionNumber>

#include <ranges>

namespace Orca::Plugin::Core {

HelpItem::HelpItem() = default;
HelpItem::HelpItem(const char *help_id) : HelpItem(QStringList(QString::fromUtf8(help_id)), {}, Unknown) {}
HelpItem::HelpItem(const QString &help_id) : HelpItem(QStringList(help_id), {}, Unknown) {}
HelpItem::HelpItem(QUrl url) : m_help_url(std::move(url)) {}
HelpItem::HelpItem(QUrl url, QString doc_mark, const Category category) : m_help_url(std::move(url)), m_doc_mark(std::move(doc_mark)), m_category(category) {}
HelpItem::HelpItem(const QString &help_id, const QString &doc_mark, const Category category) : HelpItem(QStringList(help_id), doc_mark, category) {}

HelpItem::HelpItem(const QStringList &help_ids, QString doc_mark, const Category category) : m_doc_mark(std::move(doc_mark)), m_category(category)
{
  setHelpIds(help_ids);
}

auto HelpItem::setHelpUrl(const QUrl &url) -> void
{
  m_help_url = url;
}

auto HelpItem::helpUrl() const -> const QUrl&
{
  return m_help_url;
}

auto HelpItem::setHelpIds(const QStringList &ids) -> void
{
  m_help_ids = Utils::filteredUnique(Utils::filtered(ids, [](const QString &s) { return !s.isEmpty(); }));
}

auto HelpItem::helpIds() const -> const QStringList&
{
  return m_help_ids;
}

auto HelpItem::setDocMark(const QString &mark) -> void { m_doc_mark = mark; }
auto HelpItem::docMark() const -> const QString& { return m_doc_mark; }
auto HelpItem::setCategory(const Category cat) -> void { m_category = cat; }
auto HelpItem::category() const -> Category { return m_category; }

auto HelpItem::isEmpty() const -> bool
{
  return m_help_url.isEmpty() && m_help_ids.isEmpty();
}

auto HelpItem::isValid() const -> bool
{
  if (m_help_url.isEmpty() && m_help_ids.isEmpty())
    return false;

  return !links().empty();
}

auto HelpItem::firstParagraph() const -> QString
{
  if (!m_first_paragraph)
    m_first_paragraph = extractContent(false);

  return *m_first_paragraph;
}

auto HelpItem::extractContent(const bool extended) const -> QString
{
  Utils::HtmlDocExtractor html_extractor;

  if (extended)
    html_extractor.setMode(Utils::HtmlDocExtractor::Extended);
  else
    html_extractor.setMode(Utils::HtmlDocExtractor::FirstParagraph);

  QString contents;
  for (const auto &val : links() | std::views::values) {
    const auto url = val;
    const auto html = QString::fromUtf8(fileData(url));

    switch (m_category) {
    case Brief:
      contents = html_extractor.getClassOrNamespaceBrief(html, m_doc_mark);
      break;
    case ClassOrNamespace:
      contents = html_extractor.getClassOrNamespaceDescription(html, m_doc_mark);
      break;
    case Function:
      contents = html_extractor.getFunctionDescription(html, m_doc_mark);
      break;
    case Enum:
      contents = html_extractor.getEnumDescription(html, m_doc_mark);
      break;
    case Typedef:
      contents = html_extractor.getTypedefDescription(html, m_doc_mark);
      break;
    case Macro:
      contents = html_extractor.getMacroDescription(html, m_doc_mark);
      break;
    case QmlComponent:
      contents = html_extractor.getQmlComponentDescription(html, m_doc_mark);
      break;
    case QmlProperty:
      contents = html_extractor.getQmlPropertyDescription(html, m_doc_mark);
      break;
    case QMakeVariableOfFunction:
      contents = html_extractor.getQMakeVariableOrFunctionDescription(html, m_doc_mark);
      break;
    case Unknown:
      break;
    }

    if (!contents.isEmpty())
      break;
  }
  return contents;
}

// The following is only correct under the specific current conditions, and it will
// always be quite some guessing as long as the version information does not
// include separators for major vs minor vs patch version.
static auto qtVersionHeuristic(const QString &digits) -> QVersionNumber
{
  if (digits.count() > 6 || digits.count() < 3)
    return {}; // suspicious version number

  for (const auto &digit : digits)
    if (!digit.isDigit())
      return {}; // we should have only digits

  // When we have 3 digits, we split it like: ABC -> A.B.C
  // When we have 4 digits, we split it like: ABCD -> A.BC.D
  // When we have 5 digits, we split it like: ABCDE -> A.BC.DE
  // When we have 6 digits, we split it like: ABCDEF -> AB.CD.EF
  switch (digits.count()) {
  case 3:
    return QVersionNumber(digits.mid(0, 1).toInt(), digits.mid(1, 1).toInt(), digits.mid(2, 1).toInt());
  case 4:
    return QVersionNumber(digits.mid(0, 1).toInt(), digits.mid(1, 2).toInt(), digits.mid(3, 1).toInt());
  case 5:
    return QVersionNumber(digits.mid(0, 1).toInt(), digits.mid(1, 2).toInt(), digits.mid(3, 2).toInt());
  case 6:
    return QVersionNumber(digits.mid(0, 2).toInt(), digits.mid(2, 2).toInt(), digits.mid(4, 2).toInt());
  default:
    break;
  }

  return {};
}

static auto extractVersion(const QUrl &url) -> std::pair<QUrl, QVersionNumber>
{
  const auto host = url.host();

  if (const auto host_parts = host.split('.'); host_parts.size() == 4 && (host.startsWith("com.trolltech.") || host.startsWith("org.qt-project."))) {
    if (const auto version = qtVersionHeuristic(host_parts.at(3)); !version.isNull()) {
      auto url_without_version(url);
      url_without_version.setHost(host_parts.mid(0, 3).join('.'));
      return {url_without_version, version};
    }
  }

  return {url, {}};
}

// sort primary by "url without version" and seconday by "version"
static auto helpUrlLessThan(const QUrl &a, const QUrl &b) -> bool
{
  const auto va = extractVersion(a);
  const auto vb = extractVersion(b);
  const auto sa = va.first.toString();
  const auto sb = vb.first.toString();

  if (sa == sb)
    return va.second > vb.second;

  return sa < sb;
}

static auto linkLessThan(const HelpItem::Link &a, const HelpItem::Link &b) -> bool
{
  return helpUrlLessThan(a.second, b.second);
}

// links are sorted with highest "version" first (for Qt help urls)
auto HelpItem::links() const -> const Links&
{
  if (!m_help_links) {
    if (!m_help_url.isEmpty()) {
      m_keyword = m_help_url.toString();
      m_help_links.emplace(Links{{m_keyword, m_help_url}});
    } else {
      m_help_links.emplace(); // set a value even if there are no help IDs
      QMultiMap<QString, QUrl> help_links;
      for (const auto &id : m_help_ids) {
        help_links = linksForIdentifier(id);
        if (!help_links.isEmpty()) {
          m_keyword = id;
          break;
        }
      }
      if (help_links.isEmpty()) {
        // perform keyword lookup as well as a fallback
        for (const auto &id : m_help_ids) {
          help_links = linksForKeyword(id);
          if (!help_links.isEmpty()) {
            m_keyword = id;
            m_is_fuzzy_match = true;
            break;
          }
        }
      }
      for (auto it = help_links.cbegin(), end = help_links.cend(); it != end; ++it)
        m_help_links->emplace_back(it.key(), it.value());
    }
    Utils::sort(*m_help_links, linkLessThan);
  }
  return *m_help_links;
}

static auto getBestLinks(const HelpItem::Links &links) -> HelpItem::Links
{
  // extract the highest version (== first) link of each individual topic
  HelpItem::Links best_links;
  QUrl current_unversioned_url;

  for (const auto &link : links) {
    if (const auto unversioned_url = extractVersion(link.second).first; unversioned_url != current_unversioned_url) {
      current_unversioned_url = unversioned_url;
      best_links.push_back(link);
    }
  }

  return best_links;
}

static auto getBestLink(const HelpItem::Links &links) -> HelpItem::Links
{
  if (links.empty())
    return {};

  // Extract single link with highest version, from all topics.
  // This is to ensure that if we succeeded with an ID lookup, and we have e.g. Qt5 and Qt4
  // documentation, that we only return the Qt5 link even though the Qt5 and Qt4 URLs look
  // different.
  QVersionNumber highest_version;
  // Default to first link if version extraction failed, possibly because it is not a Qt doc link
  auto best_link = links.front();

  for (const auto &link : links) {
    if (const auto version = extractVersion(link.second).second; version > highest_version) {
      highest_version = version;
      best_link = link;
    }
  }
  return {best_link};
}

auto HelpItem::bestLinks() const -> Links
{
  if (isFuzzyMatch())
    return getBestLinks(links());
  return getBestLink(links());
}

auto HelpItem::keyword() const -> QString
{
  return m_keyword;
}

auto HelpItem::isFuzzyMatch() const -> bool
{
  // make sure m_isFuzzyMatch is correct
  links();
  return m_is_fuzzy_match;
}

} // namespace Orca::Plugin::Core
