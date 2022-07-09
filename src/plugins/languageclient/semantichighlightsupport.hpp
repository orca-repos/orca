// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/servercapabilities.h>
#include <texteditor/semantichighlighter.hpp>
#include <texteditor/textdocument.hpp>

#include <QTextCharFormat>

#include <functional>

namespace Core {
class IEditor;
}

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT ExpandedSemanticToken {
public:
  friend auto operator==(const ExpandedSemanticToken &t1, const ExpandedSemanticToken &t2) -> bool
  {
    return t1.line == t2.line && t1.column == t2.column && t1.length == t2.length && t1.type == t2.type && t1.modifiers == t2.modifiers;
  }

  int line = -1;
  int column = -1;
  int length = -1;
  QString type;
  QStringList modifiers;
};

using SemanticTokensHandler = std::function<void(TextEditor::TextDocument *, const QList<ExpandedSemanticToken> &, int, bool)>;

class SemanticTokenSupport : public QObject {
public:
  explicit SemanticTokenSupport(Client *client);

  auto refresh() -> void;
  auto reloadSemanticTokens(TextEditor::TextDocument *doc) -> void;
  auto updateSemanticTokens(TextEditor::TextDocument *doc) -> void;
  auto clearHighlight(TextEditor::TextDocument *doc) -> void;
  auto rehighlight() -> void;
  auto setLegend(const LanguageServerProtocol::SemanticTokensLegend &legend) -> void;
  auto setTokenTypesMap(const QMap<QString, int> &tokenTypesMap) -> void;
  auto setTokenModifiersMap(const QMap<QString, int> &tokenModifiersMap) -> void;
  auto setAdditionalTokenTypeStyles(const QHash<int, TextEditor::TextStyle> &typeStyles) -> void;

  // TODO: currently only declaration and definition modifiers are supported. The TextStyles
  // mixin capabilities need to be extended to be able to support more
  //    void setAdditionalTokenModifierStyles(const QHash<int, TextEditor::TextStyle> &modifierStyles);

  auto setTokensHandler(const SemanticTokensHandler &handler) -> void { m_tokensHandler = handler; }

private:
  auto reloadSemanticTokensImpl(TextEditor::TextDocument *doc, int remainingRerequests = 3) -> void;
  auto updateSemanticTokensImpl(TextEditor::TextDocument *doc, int remainingRerequests = 3) -> void;
  auto supportedSemanticRequests(TextEditor::TextDocument *document) const -> LanguageServerProtocol::SemanticRequestTypes;
  auto handleSemanticTokens(const Utils::FilePath &filePath, const LanguageServerProtocol::SemanticTokensResult &result, int documentVersion) -> void;
  auto handleSemanticTokensDelta(const Utils::FilePath &filePath, const LanguageServerProtocol::SemanticTokensDeltaResult &result, int documentVersion) -> void;
  auto highlight(const Utils::FilePath &filePath, bool force = false) -> void;
  auto updateFormatHash() -> void;
  auto currentEditorChanged() -> void;
  auto onCurrentEditorChanged(Core::IEditor *editor) -> void;

  Client *m_client = nullptr;

  struct VersionedTokens {
    LanguageServerProtocol::SemanticTokens tokens;
    int version;
  };

  QHash<Utils::FilePath, VersionedTokens> m_tokens;
  QList<int> m_tokenTypes;
  QList<int> m_tokenModifiers;
  QHash<int, QTextCharFormat> m_formatHash;
  QHash<int, TextEditor::TextStyle> m_additionalTypeStyles;
  //    QHash<int, TextEditor::TextStyle> m_additionalModifierStyles;
  QMap<QString, int> m_tokenTypesMap;
  QMap<QString, int> m_tokenModifiersMap;
  SemanticTokensHandler m_tokensHandler;
  QStringList m_tokenTypeStrings;
  QStringList m_tokenModifierStrings;
};

} // namespace LanguageClient
