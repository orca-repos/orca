// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "highlighter.hpp"

#include "highlightersettings.hpp"
#include "tabsettings.hpp"
#include "textdocumentlayout.hpp"
#include "texteditor.hpp"
#include "texteditorsettings.hpp"

#include <core/core-document-model.hpp>
#include <core/core-interface.hpp>
#include <core/core-message-manager.hpp>

#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stylehelper.hpp>

#include <DefinitionDownloader>
#include <FoldingRegion>
#include <Format>
#include <Repository>
#include <SyntaxHighlighter>

#include <QLoggingCategory>
#include <QMetaEnum>

using namespace Utils;

namespace TextEditor {

static Q_LOGGING_CATEGORY(highlighterLog, "qtc.editor.highlighter", QtWarningMsg)

constexpr char kDefinitionForMimeType[] = "definitionForMimeType";
constexpr char kDefinitionForExtension[] = "definitionForExtension";
constexpr char kDefinitionForFilePath[] = "definitionForFilePath";

static auto highlightRepository() -> KSyntaxHighlighting::Repository*
{
  static KSyntaxHighlighting::Repository *repository = nullptr;
  if (!repository) {
    repository = new KSyntaxHighlighting::Repository();
    repository->addCustomSearchPath(TextEditorSettings::highlighterSettings().definitionFilesPath().toString());
    const FilePath dir = Orca::Plugin::Core::ICore::resourcePath("generic-highlighter/syntax");
    if (dir.exists())
      repository->addCustomSearchPath(dir.parentDir().path());
  }
  return repository;
}

auto categoryForTextStyle(int style) -> TextStyle
{
  switch (style) {
  case KSyntaxHighlighting::Theme::Normal:
    return C_TEXT;
  case KSyntaxHighlighting::Theme::Keyword:
    return C_KEYWORD;
  case KSyntaxHighlighting::Theme::Function:
    return C_FUNCTION;
  case KSyntaxHighlighting::Theme::Variable:
    return C_LOCAL;
  case KSyntaxHighlighting::Theme::ControlFlow:
    return C_KEYWORD;
  case KSyntaxHighlighting::Theme::Operator:
    return C_OPERATOR;
  case KSyntaxHighlighting::Theme::BuiltIn:
    return C_PRIMITIVE_TYPE;
  case KSyntaxHighlighting::Theme::Extension:
    return C_GLOBAL;
  case KSyntaxHighlighting::Theme::Preprocessor:
    return C_PREPROCESSOR;
  case KSyntaxHighlighting::Theme::Attribute:
    return C_LOCAL;
  case KSyntaxHighlighting::Theme::Char:
    return C_STRING;
  case KSyntaxHighlighting::Theme::SpecialChar:
    return C_STRING;
  case KSyntaxHighlighting::Theme::String:
    return C_STRING;
  case KSyntaxHighlighting::Theme::VerbatimString:
    return C_STRING;
  case KSyntaxHighlighting::Theme::SpecialString:
    return C_STRING;
  case KSyntaxHighlighting::Theme::Import:
    return C_PREPROCESSOR;
  case KSyntaxHighlighting::Theme::DataType:
    return C_TYPE;
  case KSyntaxHighlighting::Theme::DecVal:
    return C_NUMBER;
  case KSyntaxHighlighting::Theme::BaseN:
    return C_NUMBER;
  case KSyntaxHighlighting::Theme::Float:
    return C_NUMBER;
  case KSyntaxHighlighting::Theme::Constant:
    return C_KEYWORD;
  case KSyntaxHighlighting::Theme::Comment:
    return C_COMMENT;
  case KSyntaxHighlighting::Theme::Documentation:
    return C_DOXYGEN_COMMENT;
  case KSyntaxHighlighting::Theme::Annotation:
    return C_DOXYGEN_TAG;
  case KSyntaxHighlighting::Theme::CommentVar:
    return C_DOXYGEN_TAG;
  case KSyntaxHighlighting::Theme::RegionMarker:
    return C_PREPROCESSOR;
  case KSyntaxHighlighting::Theme::Information:
    return C_WARNING;
  case KSyntaxHighlighting::Theme::Warning:
    return C_WARNING;
  case KSyntaxHighlighting::Theme::Alert:
    return C_ERROR;
  case KSyntaxHighlighting::Theme::Error:
    return C_ERROR;
  case KSyntaxHighlighting::Theme::Others:
    return C_TEXT;
  }
  return C_TEXT;
}

Highlighter::Highlighter()
{
  setTextFormatCategories(QMetaEnum::fromType<KSyntaxHighlighting::Theme::TextStyle>().keyCount(), &categoryForTextStyle);
}

auto Highlighter::definitionForName(const QString &name) -> Definition
{
  return highlightRepository()->definitionForName(name);
}

auto Highlighter::definitionsForDocument(const TextDocument *document) -> Definitions
{
  QTC_ASSERT(document, return {});
  // First try to find definitions for the file path, only afterwards try the MIME type.
  // An example where that is important is if there was a definition for "*.rb.xml", which
  // cannot be referred to with a MIME type (since there is none), but there is the definition
  // for XML files, which specifies a MIME type in addition to a glob pattern.
  // If we check the MIME type first and then skip the pattern, the definition for "*.rb.xml" is
  // never considered.
  // The KSyntaxHighlighting CLI also completely ignores MIME types.
  const FilePath &filePath = document->filePath();
  auto definitions = definitionsForFileName(filePath);
  if (definitions.isEmpty()) {
    // check for *.in filename since those are usually used for
    // cmake configure_file input filenames without the .in extension
    if (filePath.endsWith(".in"))
      definitions = definitionsForFileName(FilePath::fromString(filePath.completeBaseName()));
    if (filePath.fileName() == "qtquickcontrols2.conf")
      definitions = definitionsForFileName(filePath.stringAppended(".ini"));
  }
  if (definitions.isEmpty()) {
    const auto &mimeType = mimeTypeForName(document->mimeType());
    if (mimeType.isValid())
      definitions = definitionsForMimeType(mimeType.name());
  }

  return definitions;
}

static auto definitionForSetting(const QString &settingsKey, const QString &mapKey) -> Highlighter::Definition
{
  QSettings *settings = Orca::Plugin::Core::ICore::settings();
  settings->beginGroup(Constants::HIGHLIGHTER_SETTINGS_CATEGORY);
  const QString &definitionName = settings->value(settingsKey).toMap().value(mapKey).toString();
  settings->endGroup();
  return Highlighter::definitionForName(definitionName);
}

auto Highlighter::definitionsForMimeType(const QString &mimeType) -> Definitions
{
  auto definitions = highlightRepository()->definitionsForMimeType(mimeType).toList();
  if (definitions.size() > 1) {
    const auto &rememberedDefinition = definitionForSetting(kDefinitionForMimeType, mimeType);
    if (rememberedDefinition.isValid() && definitions.contains(rememberedDefinition))
      definitions = {rememberedDefinition};
  }
  return definitions;
}

auto Highlighter::definitionsForFileName(const FilePath &fileName) -> Definitions
{
  auto definitions = highlightRepository()->definitionsForFileName(fileName.fileName()).toList();

  if (definitions.size() > 1) {
    const auto &fileExtension = fileName.completeSuffix();
    const auto &rememberedDefinition = fileExtension.isEmpty() ? definitionForSetting(kDefinitionForFilePath, fileName.absoluteFilePath().toString()) : definitionForSetting(kDefinitionForExtension, fileExtension);
    if (rememberedDefinition.isValid() && definitions.contains(rememberedDefinition))
      definitions = {rememberedDefinition};
  }

  return definitions;
}

auto Highlighter::rememberDefinitionForDocument(const Definition &definition, const TextDocument *document) -> void
{
  QTC_ASSERT(document, return);
  if (!definition.isValid())
    return;
  const QString &mimeType = document->mimeType();
  const FilePath &path = document->filePath();
  const auto &fileExtension = path.completeSuffix();
  QSettings *settings = Orca::Plugin::Core::ICore::settings();
  settings->beginGroup(Constants::HIGHLIGHTER_SETTINGS_CATEGORY);
  const auto &fileNameDefinitions = definitionsForFileName(path);
  if (fileNameDefinitions.contains(definition)) {
    if (!fileExtension.isEmpty()) {
      const QString id(kDefinitionForExtension);
      QMap<QString, QVariant> map = settings->value(id).toMap();
      map.insert(fileExtension, definition.name());
      settings->setValue(id, map);
    } else if (!path.isEmpty()) {
      const QString id(kDefinitionForFilePath);
      QMap<QString, QVariant> map = settings->value(id).toMap();
      map.insert(path.absoluteFilePath().toString(), definition.name());
      settings->setValue(id, map);
    }
  } else if (!mimeType.isEmpty()) {
    const QString id(kDefinitionForMimeType);
    QMap<QString, QVariant> map = settings->value(id).toMap();
    map.insert(mimeType, definition.name());
    settings->setValue(id, map);
  }
  settings->endGroup();
}

auto Highlighter::clearDefinitionForDocumentCache() -> void
{
  QSettings *settings = Orca::Plugin::Core::ICore::settings();
  settings->beginGroup(Constants::HIGHLIGHTER_SETTINGS_CATEGORY);
  settings->remove(kDefinitionForMimeType);
  settings->remove(kDefinitionForExtension);
  settings->remove(kDefinitionForFilePath);
  settings->endGroup();
}

auto Highlighter::addCustomHighlighterPath(const FilePath &path) -> void
{
  highlightRepository()->addCustomSearchPath(path.toString());
}

auto Highlighter::downloadDefinitions(std::function<void()> callback) -> void
{
  auto downloader = new KSyntaxHighlighting::DefinitionDownloader(highlightRepository());
  connect(downloader, &KSyntaxHighlighting::DefinitionDownloader::done, [downloader, callback]() {
    Orca::Plugin::Core::MessageManager::writeFlashing(tr("Highlighter updates: done"));
    downloader->deleteLater();
    reload();
    if (callback)
      callback();
  });
  connect(downloader, &KSyntaxHighlighting::DefinitionDownloader::informationMessage, [](const QString &message) {
    Orca::Plugin::Core::MessageManager::writeSilently(tr("Highlighter updates:") + ' ' + message);
  });
  Orca::Plugin::Core::MessageManager::writeDisrupting(tr("Highlighter updates: starting"));
  downloader->start();
}

auto Highlighter::reload() -> void
{
  highlightRepository()->reload();
  for (auto editor : Orca::Plugin::Core::DocumentModel::editorsForOpenedDocuments()) {
    if (auto textEditor = qobject_cast<BaseTextEditor*>(editor)) {
      if (qobject_cast<Highlighter*>(textEditor->textDocument()->syntaxHighlighter()))
        textEditor->editorWidget()->configureGenericHighlighter();
    }
  }
}

auto Highlighter::handleShutdown() -> void
{
  delete highlightRepository();
}

static auto isOpeningParenthesis(QChar c) -> bool
{
  return c == QLatin1Char('{') || c == QLatin1Char('[') || c == QLatin1Char('(');
}

static auto isClosingParenthesis(QChar c) -> bool
{
  return c == QLatin1Char('}') || c == QLatin1Char(']') || c == QLatin1Char(')');
}

auto Highlighter::highlightBlock(const QString &text) -> void
{
  if (!definition().isValid()) {
    formatSpaces(text);
    return;
  }
  auto block = currentBlock();
  KSyntaxHighlighting::State state;
  TextDocumentLayout::setBraceDepth(block, TextDocumentLayout::braceDepth(block.previous()));
  if (const auto data = TextDocumentLayout::textUserData(block)) {
    state = data->syntaxState();
    data->setFoldingStartIncluded(false);
    data->setFoldingEndIncluded(false);
  }
  state = highlightLine(text, state);
  const auto nextBlock = block.next();

  Parentheses parentheses;
  auto pos = 0;
  for (const auto &c : text) {
    if (isOpeningParenthesis(c))
      parentheses.push_back(Parenthesis(Parenthesis::Opened, c, pos));
    else if (isClosingParenthesis(c))
      parentheses.push_back(Parenthesis(Parenthesis::Closed, c, pos));
    pos++;
  }
  TextDocumentLayout::setParentheses(currentBlock(), parentheses);

  if (nextBlock.isValid()) {
    const auto data = TextDocumentLayout::userData(nextBlock);
    if (data->syntaxState() != state) {
      data->setSyntaxState(state);
      setCurrentBlockState(currentBlockState() ^ 1); // force rehighlight of next block
    }
    data->setFoldingIndent(TextDocumentLayout::braceDepth(block));
  }

  formatSpaces(text);
}

auto Highlighter::applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) -> void
{
  const KSyntaxHighlighting::Theme defaultTheme;
  auto qformat = formatForCategory(format.textStyle());

  if (format.hasTextColor(defaultTheme)) {
    const auto textColor = format.textColor(defaultTheme);
    if (format.hasBackgroundColor(defaultTheme)) {
      const QColor backgroundColor = format.hasBackgroundColor(defaultTheme);
      if (StyleHelper::isReadableOn(backgroundColor, textColor)) {
        qformat.setForeground(textColor);
        qformat.setBackground(backgroundColor);
      } else if (StyleHelper::isReadableOn(qformat.background().color(), textColor)) {
        qformat.setForeground(textColor);
      }
    } else if (StyleHelper::isReadableOn(qformat.background().color(), textColor)) {
      qformat.setForeground(textColor);
    }
  } else if (format.hasBackgroundColor(defaultTheme)) {
    const QColor backgroundColor = format.hasBackgroundColor(defaultTheme);
    if (StyleHelper::isReadableOn(backgroundColor, qformat.foreground().color()))
      qformat.setBackground(backgroundColor);
  }

  if (format.isBold(defaultTheme))
    qformat.setFontWeight(QFont::Bold);

  if (format.isItalic(defaultTheme))
    qformat.setFontItalic(true);

  if (format.isUnderline(defaultTheme))
    qformat.setFontUnderline(true);

  if (format.isStrikeThrough(defaultTheme))
    qformat.setFontStrikeOut(true);
  setFormat(offset, length, qformat);
}

auto Highlighter::applyFolding(int offset, int length, KSyntaxHighlighting::FoldingRegion region) -> void
{
  if (!region.isValid())
    return;
  auto block = currentBlock();
  const auto &text = block.text();
  const auto data = TextDocumentLayout::userData(currentBlock());
  const auto fromStart = TabSettings::firstNonSpace(text) == offset;
  const auto toEnd = offset + length == text.length() - TabSettings::trailingWhitespaces(text);
  if (region.type() == KSyntaxHighlighting::FoldingRegion::Begin) {
    const auto newBraceDepth = TextDocumentLayout::braceDepth(block) + 1;
    TextDocumentLayout::setBraceDepth(block, newBraceDepth);
    qCDebug(highlighterLog) << "Found folding start from '" << offset << "' to '" << length << "' resulting in the bracedepth '" << newBraceDepth << "' in :";
    qCDebug(highlighterLog) << text;
    // if there is only a folding begin marker in the line move the current block into the fold
    if (fromStart && toEnd && length <= 1) {
      data->setFoldingIndent(TextDocumentLayout::braceDepth(block));
      data->setFoldingStartIncluded(true);
    }
  } else if (region.type() == KSyntaxHighlighting::FoldingRegion::End) {
    const auto newBraceDepth = qMax(0, TextDocumentLayout::braceDepth(block) - 1);
    qCDebug(highlighterLog) << "Found folding end from '" << offset << "' to '" << length << "' resulting in the bracedepth '" << newBraceDepth << "' in :";
    qCDebug(highlighterLog) << text;
    TextDocumentLayout::setBraceDepth(block, newBraceDepth);
    // if the folding end is at the end of the line move the current block into the fold
    if (toEnd)
      data->setFoldingEndIncluded(true);
    else
      data->setFoldingIndent(TextDocumentLayout::braceDepth(block));
  }
}

} // TextEditor
