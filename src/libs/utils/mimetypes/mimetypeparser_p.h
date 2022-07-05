// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "mimedatabase_p.h"
#include "mimeprovider_p.h"

namespace Utils {
namespace Internal {

class MimeTypeParserBase {
  Q_DISABLE_COPY(MimeTypeParserBase)

public:
  MimeTypeParserBase() {}
  virtual ~MimeTypeParserBase() {}

  auto parse(const QByteArray &content, const QString &fileName, QString *errorMessage) -> bool;

protected:
  virtual auto mimeTypeExists(const QString &mimeTypeName) -> bool = 0;
  virtual auto process(const MimeType &t, QString *errorMessage) -> bool = 0;
  virtual auto process(const MimeGlobPattern &t, QString *errorMessage) -> bool = 0;
  virtual auto processParent(const QString &child, const QString &parent) -> void = 0;
  virtual auto processAlias(const QString &alias, const QString &name) -> void = 0;
  virtual auto processMagicMatcher(const MimeMagicRuleMatcher &matcher) -> void = 0;

private:
  enum ParseState {
    ParseBeginning,
    ParseMimeInfo,
    ParseMimeType,
    ParseComment,
    ParseGenericIcon,
    ParseIcon,
    ParseGlobPattern,
    ParseSubClass,
    ParseAlias,
    ParseMagic,
    ParseMagicMatchRule,
    ParseOtherMimeTypeSubTag,
    ParseError
  };

  static auto nextState(ParseState currentState, QStringView startElement) -> ParseState;
};

class MimeTypeParser : public MimeTypeParserBase {
public:
  explicit MimeTypeParser(MimeXMLProvider &provider) : m_provider(provider) {}

protected:
  auto mimeTypeExists(const QString &mimeTypeName) -> bool override
  {
    return m_provider.mimeTypeForName(mimeTypeName).isValid();
  }

  auto process(const MimeType &t, QString *) -> bool override
  {
    m_provider.addMimeType(t);
    return true;
  }

  auto process(const MimeGlobPattern &glob, QString *) -> bool override
  {
    m_provider.addGlobPattern(glob);
    return true;
  }

  auto processParent(const QString &child, const QString &parent) -> void override
  {
    m_provider.addParent(child, parent);
  }

  auto processAlias(const QString &alias, const QString &name) -> void override
  {
    m_provider.addAlias(alias, name);
  }

  auto processMagicMatcher(const MimeMagicRuleMatcher &matcher) -> void override
  {
    m_provider.addMagicMatcher(matcher);
  }

private:
  MimeXMLProvider &m_provider;
};

} // Internal
} // Utils
