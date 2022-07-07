// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"
#include "fileutils.hpp"
#include "optional.hpp"
#include "outputformat.hpp"

#include <QObject>

#include <functional>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QRegularExpressionMatch;
class QTextCharFormat;
class QTextCursor;
QT_END_NAMESPACE

namespace Utils {
class FileInProjectFinder;
class FormattedText;
class Link;

class ORCA_UTILS_EXPORT OutputLineParser : public QObject {
  Q_OBJECT

public:
  OutputLineParser();
  ~OutputLineParser() override;

  enum class Status {
    Done,
    InProgress,
    NotHandled
  };

  class LinkSpec {
  public:
    LinkSpec() = default;
    LinkSpec(int sp, int l, const QString &t) : startPos(sp), length(l), target(t) {}
    int startPos = -1;
    int length = -1;
    QString target;
  };

  using LinkSpecs = QList<LinkSpec>;

  class Result {
  public:
    Result(Status s, const LinkSpecs &l = {}, const optional<QString> &c = {}, const optional<OutputFormat> &f = {}) : status(s), linkSpecs(l), newContent(c), formatOverride(f) {}
    Status status;
    LinkSpecs linkSpecs;
    optional<QString> newContent; // Hard content override. Only to be used in extreme cases.
    optional<OutputFormat> formatOverride;
  };

  static auto isLinkTarget(const QString &target) -> bool;
  static auto parseLinkTarget(const QString &target) -> Utils::Link;
  auto addSearchDir(const Utils::FilePath &dir) -> void;
  auto dropSearchDir(const Utils::FilePath &dir) -> void;
  auto searchDirectories() const -> const FilePaths;
  auto setFileFinder(Utils::FileInProjectFinder *finder) -> void;
  auto setDemoteErrorsToWarnings(bool demote) -> void;
  auto demoteErrorsToWarnings() const -> bool;

  // Represents a single line, without a trailing line feed character.
  // The input is to be considered "complete" for parsing purposes.
  virtual auto handleLine(const QString &line, OutputFormat format) -> Result = 0;

  virtual auto handleLink(const QString &href) -> bool
  {
    Q_UNUSED(href);
    return false;
  }

  virtual auto hasFatalErrors() const -> bool { return false; }
  virtual auto flush() -> void {}
  virtual auto runPostPrintActions(QPlainTextEdit *) -> void {}
  auto setRedirectionDetector(const OutputLineParser *detector) -> void;
  auto needsRedirection() const -> bool;
  virtual auto hasDetectedRedirection() const -> bool { return false; }

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
    void skipFileExistsCheck();
  #endif

protected:
  static auto rightTrimmed(const QString &in) -> QString;
  auto absoluteFilePath(const Utils::FilePath &filePath) const -> Utils::FilePath;
  static auto createLinkTarget(const FilePath &filePath, int line, int column) -> QString;
  static auto addLinkSpecForAbsoluteFilePath(LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, int pos, int len) -> void;
  static auto addLinkSpecForAbsoluteFilePath(LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, const QRegularExpressionMatch &match, int capIndex) -> void;
  static auto addLinkSpecForAbsoluteFilePath(LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, const QRegularExpressionMatch &match, const QString &capName) -> void;

signals:
  auto newSearchDirFound(const Utils::FilePath &dir) -> void;
  auto searchDirExpired(const Utils::FilePath &dir) -> void;

private:
  class Private;
  Private *const d;
};

class ORCA_UTILS_EXPORT OutputFormatter : public QObject {
  Q_OBJECT

public:
  OutputFormatter();
  ~OutputFormatter() override;

  using PostPrintAction = std::function<void(OutputLineParser *)>;

  auto plainTextEdit() const -> QPlainTextEdit*;
  auto setPlainTextEdit(QPlainTextEdit *plainText) -> void;

  // Forwards to line parsers. Add those before.
  auto addSearchDir(const FilePath &dir) -> void;
  auto dropSearchDir(const FilePath &dir) -> void;
  auto setLineParsers(const QList<OutputLineParser*> &parsers) -> void; // Takes ownership
  auto addLineParsers(const QList<OutputLineParser*> &parsers) -> void;
  auto addLineParser(OutputLineParser *parser) -> void;
  auto setFileFinder(const FileInProjectFinder &finder) -> void;
  auto setDemoteErrorsToWarnings(bool demote) -> void;
  auto overridePostPrintAction(const PostPrintAction &postPrintAction) -> void;
  auto appendMessage(const QString &text, OutputFormat format) -> void;
  auto flush() -> void; // Flushes in-flight data.
  auto clear() -> void; // Clears the text edit, if there is one.
  auto reset() -> void; // Wipes everything except the text edit.
  auto handleFileLink(const QString &href) -> bool;
  auto handleLink(const QString &href) -> void;
  auto setBoldFontEnabled(bool enabled) -> void;
  auto setForwardStdOutToStdError(bool enabled) -> void;
  auto hasFatalErrors() const -> bool;
  static auto linkifiedText(const QList<FormattedText> &text, const OutputLineParser::LinkSpecs &linkSpecs) -> const QList<Utils::FormattedText>;

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
    void overrideTextCharFormat(const QTextCharFormat &fmt);
    QList<OutputLineParser *> lineParsers() const;
  #endif

  #ifndef ORCA_BUILD_WITH_PLUGINS_TESTS
private:
  #endif
  auto charFormat(OutputFormat format) const -> QTextCharFormat;
  static auto linkFormat(const QTextCharFormat &inputFormat, const QString &href) -> QTextCharFormat;

signals:
  auto openInEditorRequested(const Utils::Link &link) -> void;

private:
  auto doAppendMessage(const QString &text, OutputFormat format) -> void;
  auto handleMessage(const QString &text, OutputFormat format, QList<OutputLineParser*> &involvedParsers) -> OutputLineParser::Result;
  auto append(const QString &text, const QTextCharFormat &format) -> void;
  auto initFormats() -> void;
  auto flushIncompleteLine() -> void;
  auto flushTrailingNewline() -> void;
  auto dumpIncompleteLine(const QString &line, OutputFormat format) -> void;
  auto clearLastLine() -> void;
  auto parseAnsi(const QString &text, const QTextCharFormat &format) -> QList<FormattedText>;
  auto outputTypeForParser(const OutputLineParser *parser, OutputFormat type) const -> OutputFormat;
  auto setupLineParser(OutputLineParser *parser) -> void;

  class Private;
  Private *const d;
};


} // namespace Utils
