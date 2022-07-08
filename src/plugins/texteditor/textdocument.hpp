// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"
#include "blockrange.hpp"
#include "formatter.hpp"
#include "indenter.hpp"

#include <core/textdocument.hpp>

#include <utils/id.hpp>
#include <utils/link.hpp>
#include <utils/multitextcursor.hpp>

#include <QList>
#include <QMap>
#include <QSharedPointer>

#include <functional>

QT_BEGIN_NAMESPACE
class QAction;
class QTextCursor;
class QTextDocument;
QT_END_NAMESPACE

namespace TextEditor {

class CompletionAssistProvider;
class ExtraEncodingSettings;
class FontSettings;
class IAssistProvider;
class StorageSettings;
class SyntaxHighlighter;
class TabSettings;
class TextDocumentPrivate;
class TextMark;
class TypingSettings;

using TextMarks = QList<TextMark *>;

class TEXTEDITOR_EXPORT TextDocument : public Core::BaseTextDocument {
  Q_OBJECT

public:
  explicit TextDocument(Utils::Id id = Utils::Id());
  ~TextDocument() override;

  static auto openedTextDocumentContents() -> QMap<QString, QString>;
  static auto openedTextDocumentEncodings() -> QMap<QString, QTextCodec*>;
  static auto currentTextDocument() -> TextDocument*;
  static auto textDocumentForFilePath(const Utils::FilePath &filePath) -> TextDocument*;
  virtual auto plainText() const -> QString;
  virtual auto textAt(int pos, int length) const -> QString;
  virtual auto characterAt(int pos) const -> QChar;
  auto setTypingSettings(const TypingSettings &typingSettings) -> void;
  auto setStorageSettings(const StorageSettings &storageSettings) -> void;
  auto setExtraEncodingSettings(const ExtraEncodingSettings &extraEncodingSettings) -> void;
  auto typingSettings() const -> const TypingSettings&;
  auto storageSettings() const -> const StorageSettings&;
  virtual auto tabSettings() const -> TabSettings;
  auto extraEncodingSettings() const -> const ExtraEncodingSettings&;
  auto fontSettings() const -> const FontSettings&;
  auto setIndenter(Indenter *indenter) -> void;
  auto indenter() const -> Indenter*;
  auto autoIndent(const QTextCursor &cursor, QChar typedChar = QChar::Null, int currentCursorPosition = -1) -> void;
  auto autoReindent(const QTextCursor &cursor, int currentCursorPosition = -1) -> void;
  auto autoFormatOrIndent(const QTextCursor &cursor) -> void;
  auto indent(const Utils::MultiTextCursor &cursor) -> Utils::MultiTextCursor;
  auto unindent(const Utils::MultiTextCursor &cursor) -> Utils::MultiTextCursor;
  auto setFormatter(Formatter *indenter) -> void; // transfers ownership
  auto autoFormat(const QTextCursor &cursor) -> void;
  auto applyChangeSet(const Utils::ChangeSet &changeSet) -> bool;

  // the blocks list must be sorted
  auto setIfdefedOutBlocks(const QList<BlockRange> &blocks) -> void;
  auto marks() const -> TextMarks;
  auto addMark(TextMark *mark) -> bool;
  auto marksAt(int line) const -> TextMarks;
  auto removeMark(TextMark *mark) -> void;
  auto updateMark(TextMark *mark) -> void;
  auto moveMark(TextMark *mark, int previousLine) -> void;
  auto removeMarkFromMarksCache(TextMark *mark) -> void;

  // IDocument implementation.
  auto save(QString *errorString, const Utils::FilePath &filePath, bool autoSave) -> bool override;
  auto contents() const -> QByteArray override;
  auto setContents(const QByteArray &contents) -> bool override;
  auto shouldAutoSave() const -> bool override;
  auto isModified() const -> bool override;
  auto isSaveAsAllowed() const -> bool override;
  auto reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool override;
  auto setFilePath(const Utils::FilePath &newName) -> void override;
  auto reloadBehavior(ChangeTrigger state, ChangeType type) const -> ReloadBehavior override;
  auto fallbackSaveAsPath() const -> Utils::FilePath override;
  auto fallbackSaveAsFileName() const -> QString override;
  auto setFallbackSaveAsPath(const Utils::FilePath &fallbackSaveAsPath) -> void;
  auto setFallbackSaveAsFileName(const QString &fallbackSaveAsFileName) -> void;
  auto open(QString *errorString, const Utils::FilePath &filePath, const Utils::FilePath &realFilePath) -> OpenResult override;
  virtual auto reload(QString *errorString) -> bool;
  auto reload(QString *errorString, const Utils::FilePath &realFilePath) -> bool;
  auto setPlainText(const QString &text) -> bool;
  auto document() const -> QTextDocument*;
  auto setSyntaxHighlighter(SyntaxHighlighter *highlighter) -> void;
  auto syntaxHighlighter() const -> SyntaxHighlighter*;
  auto reload(QString *errorString, QTextCodec *codec) -> bool;
  auto cleanWhitespace(const QTextCursor &cursor) -> void;
  virtual auto triggerPendingUpdates() -> void;
  virtual auto setCompletionAssistProvider(CompletionAssistProvider *provider) -> void;
  virtual auto completionAssistProvider() const -> CompletionAssistProvider*;
  virtual auto setFunctionHintAssistProvider(CompletionAssistProvider *provider) -> void;
  virtual auto functionHintAssistProvider() const -> CompletionAssistProvider*;
  auto setQuickFixAssistProvider(IAssistProvider *provider) const -> void;
  virtual auto quickFixAssistProvider() const -> IAssistProvider*;
  auto setTabSettings(const TabSettings &tabSettings) -> void;
  auto setFontSettings(const FontSettings &fontSettings) -> void;
  static auto createDiffAgainstCurrentFileAction(QObject *parent, const std::function<Utils::FilePath()> &filePath) -> QAction*;

  #ifdef WITH_TESTS
    void setSilentReload();
  #endif

signals:
  auto aboutToOpen(const Utils::FilePath &filePath, const Utils::FilePath &realFilePath) -> void;
  auto openFinishedSuccessfully() -> void;
  auto contentsChangedWithPosition(int position, int charsRemoved, int charsAdded) -> void;
  auto tabSettingsChanged() -> void;
  auto fontSettingsChanged() -> void;
  auto markRemoved(TextMark *mark) -> void;

  #ifdef WITH_TESTS
    void ifdefedOutBlocksChanged(const QList<BlockRange> &blocks);
  #endif

protected:
  virtual auto applyFontSettings() -> void;

private:
  auto openImpl(QString *errorString, const Utils::FilePath &filePath, const Utils::FilePath &realFileName, bool reload) -> OpenResult;
  auto cleanWhitespace(QTextCursor &cursor, bool inEntireDocument, bool cleanIndentation) -> void;
  auto ensureFinalNewLine(QTextCursor &cursor) -> void;
  auto modificationChanged(bool modified) -> void;
  auto updateLayout() const -> void;

  TextDocumentPrivate *d;
};

using TextDocumentPtr = QSharedPointer<TextDocument>;

} // namespace TextEditor
