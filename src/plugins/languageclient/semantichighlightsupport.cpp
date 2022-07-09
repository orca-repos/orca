// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "semantichighlightsupport.hpp"

#include "client.hpp"
#include "languageclientmanager.hpp"

#include <texteditor/fontsettings.hpp>
#include <texteditor/syntaxhighlighter.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QTextDocument>

using namespace LanguageServerProtocol;
using namespace TextEditor;

namespace LanguageClient {

static Q_LOGGING_CATEGORY(LOGLSPHIGHLIGHT, "qtc.languageclient.highlight", QtWarningMsg);

constexpr int tokenTypeBitOffset = 16;

SemanticTokenSupport::SemanticTokenSupport(Client *client) : m_client(client)
{
  QObject::connect(TextEditorSettings::instance(), &TextEditorSettings::fontSettingsChanged, client, [this]() { updateFormatHash(); });
  QObject::connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, this, &SemanticTokenSupport::onCurrentEditorChanged);
}

auto SemanticTokenSupport::refresh() -> void
{
  qCDebug(LOGLSPHIGHLIGHT) << "refresh all semantic highlights for" << m_client->name();
  m_tokens.clear();
  for (const auto editor : Core::EditorManager::visibleEditors())
    onCurrentEditorChanged(editor);
}

auto SemanticTokenSupport::reloadSemanticTokens(TextDocument *textDocument) -> void
{
  reloadSemanticTokensImpl(textDocument);
}

auto SemanticTokenSupport::reloadSemanticTokensImpl(TextDocument *textDocument, int remainingRerequests) -> void
{
  const auto supportedRequests = supportedSemanticRequests(textDocument);
  if (supportedRequests.testFlag(SemanticRequestType::None))
    return;
  const auto filePath = textDocument->filePath();
  const TextDocumentIdentifier docId(DocumentUri::fromFilePath(filePath));
  auto responseCallback = [this, remainingRerequests, filePath, documentVersion = m_client->documentVersion(filePath)](const SemanticTokensFullRequest::Response &response) {
    if (const auto error = response.error()) {
      qCDebug(LOGLSPHIGHLIGHT) << "received error" << error->code() << error->message() << "for" << filePath;
      if (remainingRerequests > 0) {
        if (const auto document = TextDocument::textDocumentForFilePath(filePath))
          reloadSemanticTokensImpl(document, remainingRerequests - 1);
      }
    } else {
      handleSemanticTokens(filePath, response.result().value_or(nullptr), documentVersion);
    }
  };
  /*if (supportedRequests.testFlag(SemanticRequestType::Range)) {
      const int start = widget->firstVisibleBlockNumber();
      const int end = widget->lastVisibleBlockNumber();
      const int pageSize = end - start;
      // request one extra page upfront and after the current visible range
      Range range(Position(qMax(0, start - pageSize), 0),
                  Position(qMin(widget->blockCount() - 1, end + pageSize), 0));
      SemanticTokensRangeParams params;
      params.setTextDocument(docId);
      params.setRange(range);
      SemanticTokensRangeRequest request(params);
      request.setResponseCallback(responseCallback);
      m_client->sendContent(request);
  } else */
  if (supportedRequests.testFlag(SemanticRequestType::Full)) {
    SemanticTokensParams params;
    params.setTextDocument(docId);
    SemanticTokensFullRequest request(params);
    request.setResponseCallback(responseCallback);
    qCDebug(LOGLSPHIGHLIGHT) << "Requesting all tokens for" << filePath << "with version" << m_client->documentVersion(filePath);
    m_client->sendContent(request);
  }
}

auto SemanticTokenSupport::updateSemanticTokens(TextDocument *textDocument) -> void
{
  updateSemanticTokensImpl(textDocument);
}

auto SemanticTokenSupport::updateSemanticTokensImpl(TextDocument *textDocument, int remainingRerequests) -> void
{
  const auto supportedRequests = supportedSemanticRequests(textDocument);
  if (supportedRequests.testFlag(SemanticRequestType::FullDelta)) {
    const auto filePath = textDocument->filePath();
    const auto versionedToken = m_tokens.value(filePath);
    const auto &previousResultId = versionedToken.tokens.resultId().value_or(QString());
    if (!previousResultId.isEmpty()) {
      const auto documentVersion = m_client->documentVersion(filePath);
      if (documentVersion == versionedToken.version)
        return;
      SemanticTokensDeltaParams params;
      params.setTextDocument(TextDocumentIdentifier(DocumentUri::fromFilePath(filePath)));
      params.setPreviousResultId(previousResultId);
      SemanticTokensFullDeltaRequest request(params);
      request.setResponseCallback([this, filePath, documentVersion, remainingRerequests](const SemanticTokensFullDeltaRequest::Response &response) {
        if (const auto error = response.error()) {
          qCDebug(LOGLSPHIGHLIGHT) << "received error" << error->code() << error->message() << "for" << filePath;
          if (const auto document = TextDocument::textDocumentForFilePath(filePath)) {
            if (remainingRerequests > 0)
              updateSemanticTokensImpl(document, remainingRerequests - 1);
            else
              reloadSemanticTokensImpl(document, 1); // try a full reload once
          }
        } else {
          handleSemanticTokensDelta(filePath, response.result().value_or(nullptr), documentVersion);
        }
      });
      qCDebug(LOGLSPHIGHLIGHT) << "Requesting delta for" << filePath << "with version" << documentVersion;
      m_client->sendContent(request);
      return;
    }
  }
  reloadSemanticTokens(textDocument);
}

auto SemanticTokenSupport::clearHighlight(TextEditor::TextDocument *doc) -> void
{
  if (m_tokens.contains(doc->filePath())) {
    if (const auto highlighter = doc->syntaxHighlighter())
      highlighter->clearAllExtraFormats();
  }
}

auto SemanticTokenSupport::rehighlight() -> void
{
  for (const auto &filePath : m_tokens.keys())
    highlight(filePath, true);
}

auto addModifiers(int key, QHash<int, QTextCharFormat> *formatHash, TextStyles styles, QList<int> tokenModifiers, const TextEditor::FontSettings &fs) -> void
{
  if (tokenModifiers.isEmpty())
    return;
  const auto modifier = tokenModifiers.takeLast();
  if (modifier < 0)
    return;
  auto addModifier = [&](TextStyle style) {
    if (key & modifier) // already there don't add twice
      return;
    key = key | modifier;
    styles.mixinStyles.push_back(style);
    formatHash->insert(key, fs.toTextCharFormat(styles));
  };
  switch (modifier) {
  case declarationModifier:
    addModifier(C_DECLARATION);
    break;
  case definitionModifier:
    addModifier(C_FUNCTION_DEFINITION);
    break;
  default:
    break;
  }
  addModifiers(key, formatHash, styles, tokenModifiers, fs);
}

auto SemanticTokenSupport::setLegend(const LanguageServerProtocol::SemanticTokensLegend &legend) -> void
{
  m_tokenTypeStrings = legend.tokenTypes();
  m_tokenModifierStrings = legend.tokenModifiers();
  m_tokenTypes = Utils::transform(legend.tokenTypes(), [&](const QString &tokenTypeString) {
    return m_tokenTypesMap.value(tokenTypeString, -1);
  });
  m_tokenModifiers = Utils::transform(legend.tokenModifiers(), [&](const QString &tokenModifierString) {
    return m_tokenModifiersMap.value(tokenModifierString, -1);
  });
  updateFormatHash();
}

auto SemanticTokenSupport::updateFormatHash() -> void
{
  const auto fontSettings = TextEditorSettings::fontSettings();
  for (auto tokenType : qAsConst(m_tokenTypes)) {
    if (tokenType < 0)
      continue;
    TextStyle style;
    switch (tokenType) {
    case typeToken:
      style = C_TYPE;
      break;
    case classToken:
      style = C_TYPE;
      break;
    case enumMemberToken:
      style = C_ENUMERATION;
      break;
    case typeParameterToken:
      style = C_FIELD;
      break;
    case parameterToken:
      style = C_PARAMETER;
      break;
    case variableToken:
      style = C_LOCAL;
      break;
    case functionToken:
      style = C_FUNCTION;
      break;
    case methodToken:
      style = C_FUNCTION;
      break;
    case macroToken:
      style = C_PREPROCESSOR;
      break;
    case keywordToken:
      style = C_KEYWORD;
      break;
    case commentToken:
      style = C_COMMENT;
      break;
    case stringToken:
      style = C_STRING;
      break;
    case numberToken:
      style = C_NUMBER;
      break;
    case operatorToken:
      style = C_OPERATOR;
      break;
    default:
      style = m_additionalTypeStyles.value(tokenType, C_TEXT);
      break;
    }
    auto mainHashPart = tokenType << tokenTypeBitOffset;
    m_formatHash[mainHashPart] = fontSettings.toTextCharFormat(style);
    TextStyles styles;
    styles.mainStyle = style;
    styles.mixinStyles.initializeElements();
    addModifiers(mainHashPart, &m_formatHash, styles, m_tokenModifiers, fontSettings);
  }
  rehighlight();
}

auto SemanticTokenSupport::onCurrentEditorChanged(Core::IEditor *editor) -> void
{
  if (const auto textEditor = qobject_cast<BaseTextEditor*>(editor))
    updateSemanticTokens(textEditor->textDocument());
}

auto SemanticTokenSupport::setTokenTypesMap(const QMap<QString, int> &tokenTypesMap) -> void
{
  m_tokenTypesMap = tokenTypesMap;
}

auto SemanticTokenSupport::setTokenModifiersMap(const QMap<QString, int> &tokenModifiersMap) -> void
{
  m_tokenModifiersMap = tokenModifiersMap;
}

auto SemanticTokenSupport::setAdditionalTokenTypeStyles(const QHash<int, TextStyle> &typeStyles) -> void
{
  m_additionalTypeStyles = typeStyles;
}

//void SemanticTokenSupport::setAdditionalTokenModifierStyles(
//    const QHash<int, TextStyle> &modifierStyles)
//{
//    m_additionalModifierStyles = modifierStyles;
//}

auto SemanticTokenSupport::supportedSemanticRequests(TextDocument *document) const -> SemanticRequestTypes
{
  if (!m_client->documentOpen(document))
    return SemanticRequestType::None;
  auto supportedRequests = [&](const QJsonObject &options) -> SemanticRequestTypes {
    const TextDocumentRegistrationOptions docOptions(options);
    if (docOptions.isValid() && docOptions.filterApplies(document->filePath(), Utils::mimeTypeForName(document->mimeType()))) {
      return SemanticRequestType::None;
    }
    const SemanticTokensOptions semanticOptions(options);
    return semanticOptions.supportedRequests();
  };
  const QString dynamicMethod = "textDocument/semanticTokens";
  const auto &dynamicCapabilities = m_client->dynamicCapabilities();
  if (const auto registered = dynamicCapabilities.isRegistered(dynamicMethod); registered.has_value()) {
    if (!registered.value())
      return SemanticRequestType::None;
    return supportedRequests(dynamicCapabilities.option(dynamicMethod).toObject());
  }
  if (m_client->capabilities().semanticTokensProvider().has_value())
    return supportedRequests(m_client->capabilities().semanticTokensProvider().value());
  return SemanticRequestType::None;
}

auto SemanticTokenSupport::handleSemanticTokens(const Utils::FilePath &filePath, const SemanticTokensResult &result, int documentVersion) -> void
{
  if (const auto tokens = Utils::get_if<SemanticTokens>(&result)) {
    m_tokens[filePath] = {*tokens, documentVersion};
    highlight(filePath);
  }
}

auto SemanticTokenSupport::handleSemanticTokensDelta(const Utils::FilePath &filePath, const LanguageServerProtocol::SemanticTokensDeltaResult &result, int documentVersion) -> void
{
  qCDebug(LOGLSPHIGHLIGHT) << "Handle Tokens for " << filePath;
  if (const auto tokens = Utils::get_if<SemanticTokens>(&result)) {
    m_tokens[filePath] = {*tokens, documentVersion};
    qCDebug(LOGLSPHIGHLIGHT) << "New Data " << tokens->data();
  } else if (const auto tokensDelta = Utils::get_if<SemanticTokensDelta>(&result)) {
    m_tokens[filePath].version = documentVersion;
    auto edits = tokensDelta->edits();
    if (edits.isEmpty()) {
      highlight(filePath);
      return;
    }

    Utils::sort(edits, &SemanticTokensEdit::start);

    auto &tokens = m_tokens[filePath].tokens;
    const auto &data = tokens.data();

    int newDataSize = data.size();
    for (const auto &edit : qAsConst(edits))
      newDataSize += edit.dataSize() - edit.deleteCount();
    QList<int> newData;
    newData.reserve(newDataSize);

    auto it = data.begin();
    const auto end = data.end();
    qCDebug(LOGLSPHIGHLIGHT) << "Edit Tokens";
    qCDebug(LOGLSPHIGHLIGHT) << "Data before edit " << data;
    for (const auto &edit : qAsConst(edits)) {
      if (edit.start() > data.size()) // prevent edits after the previously reported data
        return;
      for (const auto start = data.begin() + edit.start(); it < start; ++it)
        newData.append(*it);
      const auto editData = edit.data();
      if (editData.has_value()) {
        newData.append(editData.value());
        qCDebug(LOGLSPHIGHLIGHT) << edit.start() << edit.deleteCount() << editData.value();
      } else {
        qCDebug(LOGLSPHIGHLIGHT) << edit.start() << edit.deleteCount();
      }
      const auto deleteCount = edit.deleteCount();
      if (deleteCount > std::distance(it, end)) {
        qCDebug(LOGLSPHIGHLIGHT) << "We shall delete more highlight data entries than we actually have, " "so we are out of sync with the server. " "Request full semantic tokens again.";
        const auto doc = TextDocument::textDocumentForFilePath(filePath);
        if (doc && LanguageClientManager::clientForDocument(doc) == m_client)
          reloadSemanticTokens(doc);
        return;
      }
      it += deleteCount;
    }
    for (; it != end; ++it)
      newData.append(*it);

    qCDebug(LOGLSPHIGHLIGHT) << "New Data " << newData;
    tokens.setData(newData);
    tokens.setResultId(tokensDelta->resultId());
  }
  highlight(filePath);
}

auto SemanticTokenSupport::highlight(const Utils::FilePath &filePath, bool force) -> void
{
  qCDebug(LOGLSPHIGHLIGHT) << "highlight" << filePath;
  const auto doc = TextDocument::textDocumentForFilePath(filePath);
  if (!doc || LanguageClientManager::clientForDocument(doc) != m_client)
    return;
  const auto highlighter = doc->syntaxHighlighter();
  if (!highlighter)
    return;
  const auto versionedTokens = m_tokens.value(filePath);
  const auto tokens = versionedTokens.tokens.toTokens(m_tokenTypes, m_tokenModifiers);
  if (m_tokensHandler) {
    qCDebug(LOGLSPHIGHLIGHT) << "use tokens handler" << filePath;
    auto line = 1;
    auto column = 1;
    QList<ExpandedSemanticToken> expandedTokens;
    for (const auto &token : tokens) {
      line += token.deltaLine;
      if (token.deltaLine != 0) // reset the current column when we change the current line
        column = 1;
      column += token.deltaStart;
      if (token.tokenIndex >= m_tokenTypeStrings.length())
        continue;
      ExpandedSemanticToken expandedToken;
      expandedToken.type = m_tokenTypeStrings.at(token.tokenIndex);
      auto modifiers = token.rawTokenModifiers;
      for (auto bitPos = 0; modifiers && bitPos < m_tokenModifierStrings.length(); ++bitPos, modifiers >>= 1) {
        if (modifiers & 0x1)
          expandedToken.modifiers << m_tokenModifierStrings.at(bitPos);
      }
      expandedToken.line = line;
      expandedToken.column = column;
      expandedToken.length = token.length;
      expandedTokens << expandedToken;
    };
    if (LOGLSPHIGHLIGHT().isDebugEnabled()) {
      qCDebug(LOGLSPHIGHLIGHT) << "Expanded Tokens for " << filePath;
      for (const auto &token : qAsConst(expandedTokens)) {
        qCDebug(LOGLSPHIGHLIGHT) << token.line << token.column << token.length << token.type << token.modifiers;
      }
    }

    m_tokensHandler(doc, expandedTokens, versionedTokens.version, force);
    return;
  }
  auto line = 1;
  auto column = 1;
  auto toResult = [&](const SemanticToken &token) {
    line += token.deltaLine;
    if (token.deltaLine != 0) // reset the current column when we change the current line
      column = 1;
    column += token.deltaStart;
    const auto tokenKind = token.tokenType << tokenTypeBitOffset | token.tokenModifiers;
    return HighlightingResult(line, column, token.length, tokenKind);
  };
  const auto results = Utils::transform(tokens, toResult);
  SemanticHighlighter::setExtraAdditionalFormats(highlighter, results, m_formatHash);
}

} // namespace LanguageClient
