// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/optional.hpp>

#include <QString>
#include <QUrl>

#include <vector>

namespace Orca::Plugin::Core {

class CORE_EXPORT HelpItem {
public:
  using Link = std::pair<QString, QUrl>;
  using Links = std::vector<Link>;

  enum Category {
    ClassOrNamespace,
    Enum,
    Typedef,
    Macro,
    Brief,
    Function,
    QmlComponent,
    QmlProperty,
    QMakeVariableOfFunction,
    Unknown
  };

  HelpItem();
  HelpItem(const char *help_id);
  HelpItem(const QString &help_id);
  HelpItem(const QString &help_id, const QString &doc_mark, Category category);
  HelpItem(const QStringList &help_ids, QString doc_mark, Category category);
  explicit HelpItem(QUrl url);
  HelpItem(QUrl url, QString doc_mark, Category category);

  auto setHelpUrl(const QUrl &url) -> void;
  auto helpUrl() const -> const QUrl&;
  auto setHelpIds(const QStringList &ids) -> void;
  auto helpIds() const -> const QStringList&;
  auto setDocMark(const QString &mark) -> void;
  auto docMark() const -> const QString&;
  auto setCategory(Category cat) -> void;
  auto category() const -> Category;
  auto isEmpty() const -> bool;
  auto isValid() const -> bool;
  auto firstParagraph() const -> QString;
  auto links() const -> const Links&;
  auto bestLinks() const -> Links;
  auto keyword() const -> QString;
  auto isFuzzyMatch() const -> bool;

private:
  auto extractContent(bool extended) const -> QString;

  QUrl m_help_url;
  QStringList m_help_ids;
  QString m_doc_mark;
  Category m_category = Unknown;
  mutable Utils::optional<Links> m_help_links; // cached help links
  mutable Utils::optional<QString> m_first_paragraph;
  mutable QString m_keyword;
  mutable bool m_is_fuzzy_match = false;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_METATYPE(Orca::Plugin::Core::HelpItem)
