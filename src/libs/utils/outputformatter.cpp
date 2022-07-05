// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "outputformatter.h"

#include "algorithm.h"
#include "ansiescapecodehandler.h"
#include "fileinprojectfinder.h"
#include "link.h"
#include "qtcassert.h"
#include "qtcprocess.h"
#include "theme/theme.h"

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpressionMatch>
#include <QTextCursor>

#include <numeric>

namespace Utils {

class OutputLineParser::Private {
public:
  FilePaths searchDirs;
  QPointer<const OutputLineParser> redirectionDetector;
  bool skipFileExistsCheck = false;
  bool demoteErrorsToWarnings = false;
  FileInProjectFinder *fileFinder = nullptr;
};

OutputLineParser::OutputLineParser() : d(new Private) { }
OutputLineParser::~OutputLineParser() { delete d; }

Q_GLOBAL_STATIC_WITH_ARGS(QString, linkPrefix, {"olpfile://"})
Q_GLOBAL_STATIC_WITH_ARGS(QString, linkSep, {"::"})

auto OutputLineParser::createLinkTarget(const FilePath &filePath, int line = -1, int column = -1) -> QString
{
  return *linkPrefix() + filePath.toString() + *linkSep() + QString::number(line) + *linkSep() + QString::number(column);
}

auto OutputLineParser::isLinkTarget(const QString &target) -> bool
{
  return target.startsWith(*linkPrefix());
}

auto OutputLineParser::parseLinkTarget(const QString &target) -> Link
{
  const QStringList parts = target.mid(linkPrefix()->length()).split(*linkSep());
  if (parts.isEmpty())
    return {};
  return Link(FilePath::fromString(parts.first()), parts.length() > 1 ? parts.at(1).toInt() : 0, parts.length() > 2 ? parts.at(2).toInt() : 0);
}

// The redirection mechanism is needed for broken build tools (e.g. xcodebuild) that get invoked
// indirectly as part of the build process and redirect their child processes' stderr output
// to stdout. A parser might be able to detect this condition and inform interested
// other parsers that they need to interpret stdout data as stderr.
auto OutputLineParser::setRedirectionDetector(const OutputLineParser *detector) -> void
{
  d->redirectionDetector = detector;
}

auto OutputLineParser::needsRedirection() const -> bool
{
  return d->redirectionDetector && (d->redirectionDetector->hasDetectedRedirection() || d->redirectionDetector->needsRedirection());
}

auto OutputLineParser::addSearchDir(const FilePath &dir) -> void
{
  d->searchDirs << dir;
}

auto OutputLineParser::dropSearchDir(const FilePath &dir) -> void
{
  const int idx = d->searchDirs.lastIndexOf(dir);

  // TODO: This apparently triggers. Find out why and either remove the assertion (if it's legit)
  //       or fix the culprit.
  QTC_ASSERT(idx != -1, return);

  d->searchDirs.removeAt(idx);
}

auto OutputLineParser::searchDirectories() const -> const FilePaths
{
  return d->searchDirs;
}

auto OutputLineParser::setFileFinder(FileInProjectFinder *finder) -> void
{
  d->fileFinder = finder;
}

auto OutputLineParser::setDemoteErrorsToWarnings(bool demote) -> void
{
  d->demoteErrorsToWarnings = demote;
}

auto OutputLineParser::demoteErrorsToWarnings() const -> bool
{
  return d->demoteErrorsToWarnings;
}

auto OutputLineParser::absoluteFilePath(const FilePath &filePath) const -> FilePath
{
  if (filePath.isEmpty())
    return filePath;
  if (filePath.toFileInfo().isAbsolute())
    return filePath.cleanPath();
  FilePaths candidates;
  for (const FilePath &dir : searchDirectories()) {
    FilePath candidate = dir.pathAppended(filePath.toString());
    if (candidate.exists() || d->skipFileExistsCheck) {
      candidate = FilePath::fromString(QDir::cleanPath(candidate.toString()));
      if (!candidates.contains(candidate))
        candidates << candidate;
    }
  }
  if (candidates.count() == 1)
    return candidates.first();

  QString fp = filePath.toString();
  while (fp.startsWith("../"))
    fp.remove(0, 3);
  bool found = false;
  candidates = d->fileFinder->findFile(QUrl::fromLocalFile(fp), &found);
  if (found && candidates.size() == 1)
    return candidates.first();

  return filePath;
}

auto OutputLineParser::addLinkSpecForAbsoluteFilePath(OutputLineParser::LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, int pos, int len) -> void
{
  if (filePath.toFileInfo().isAbsolute())
    linkSpecs.append({pos, len, createLinkTarget(filePath, lineNo)});
}

auto OutputLineParser::addLinkSpecForAbsoluteFilePath(OutputLineParser::LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, const QRegularExpressionMatch &match, int capIndex) -> void
{
  addLinkSpecForAbsoluteFilePath(linkSpecs, filePath, lineNo, match.capturedStart(capIndex), match.capturedLength(capIndex));
}

auto OutputLineParser::addLinkSpecForAbsoluteFilePath(OutputLineParser::LinkSpecs &linkSpecs, const FilePath &filePath, int lineNo, const QRegularExpressionMatch &match, const QString &capName) -> void
{
  addLinkSpecForAbsoluteFilePath(linkSpecs, filePath, lineNo, match.capturedStart(capName), match.capturedLength(capName));
}

auto OutputLineParser::rightTrimmed(const QString &in) -> QString
{
  int pos = in.length();
  for (; pos > 0; --pos) {
    if (!in.at(pos - 1).isSpace())
      break;
  }
  return in.mid(0, pos);
}

#ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
void OutputLineParser::skipFileExistsCheck()
{
    d->skipFileExistsCheck = true;
}
#endif

class OutputFormatter::Private {
public:
  QPlainTextEdit *plainTextEdit = nullptr;
  QTextCharFormat formats[NumberOfFormats];
  QTextCursor cursor;
  AnsiEscapeCodeHandler escapeCodeHandler;
  QPair<QString, OutputFormat> incompleteLine;
  optional<QTextCharFormat> formatOverride;
  QList<OutputLineParser*> lineParsers;
  OutputLineParser *nextParser = nullptr;
  FileInProjectFinder fileFinder;
  PostPrintAction postPrintAction;
  bool boldFontEnabled = true;
  bool prependCarriageReturn = false;
  bool prependLineFeed = false;
  bool forwardStdOutToStdError = false;
};

OutputFormatter::OutputFormatter() : d(new Private) { }

OutputFormatter::~OutputFormatter()
{
  qDeleteAll(d->lineParsers);
  delete d;
}

auto OutputFormatter::plainTextEdit() const -> QPlainTextEdit*
{
  return d->plainTextEdit;
}

auto OutputFormatter::setPlainTextEdit(QPlainTextEdit *plainText) -> void
{
  d->plainTextEdit = plainText;
  d->cursor = plainText ? plainText->textCursor() : QTextCursor();
  d->cursor.movePosition(QTextCursor::End);
  initFormats();
}

auto OutputFormatter::setLineParsers(const QList<OutputLineParser*> &parsers) -> void
{
  flush();
  qDeleteAll(d->lineParsers);
  d->lineParsers.clear();
  d->nextParser = nullptr;
  addLineParsers(parsers);
}

auto OutputFormatter::addLineParsers(const QList<OutputLineParser*> &parsers) -> void
{
  for (OutputLineParser *const p : qAsConst(parsers))
    addLineParser(p);
}

auto OutputFormatter::addLineParser(OutputLineParser *parser) -> void
{
  setupLineParser(parser);
  d->lineParsers << parser;
}

auto OutputFormatter::setupLineParser(OutputLineParser *parser) -> void
{
  parser->setFileFinder(&d->fileFinder);
  connect(parser, &OutputLineParser::newSearchDirFound, this, &OutputFormatter::addSearchDir);
  connect(parser, &OutputLineParser::searchDirExpired, this, &OutputFormatter::dropSearchDir);
}

auto OutputFormatter::setFileFinder(const FileInProjectFinder &finder) -> void
{
  d->fileFinder = finder;
}

auto OutputFormatter::setDemoteErrorsToWarnings(bool demote) -> void
{
  for (OutputLineParser *const p : qAsConst(d->lineParsers))
    p->setDemoteErrorsToWarnings(demote);
}

auto OutputFormatter::overridePostPrintAction(const PostPrintAction &postPrintAction) -> void
{
  d->postPrintAction = postPrintAction;
}

auto OutputFormatter::doAppendMessage(const QString &text, OutputFormat format) -> void
{
  QTextCharFormat charFmt = charFormat(format);

  QList<FormattedText> formattedText = parseAnsi(text, charFmt);
  const QString cleanLine = std::accumulate(formattedText.begin(), formattedText.end(), QString(), [](const FormattedText &t1, const FormattedText &t2) -> QString { return t1.text + t2.text; });
  QList<OutputLineParser*> involvedParsers;
  const OutputLineParser::Result res = handleMessage(cleanLine, format, involvedParsers);

  // If the line was recognized by a parser and a redirection was detected for that parser,
  // then our formatting should reflect that redirection as well, i.e. print in red
  // even if the nominal format is stdout.
  if (!involvedParsers.isEmpty()) {
    const OutputFormat formatForParser = res.formatOverride ? *res.formatOverride : outputTypeForParser(involvedParsers.last(), format);
    if (formatForParser != format && cleanLine == text && formattedText.length() == 1) {
      charFmt = charFormat(formatForParser);
      formattedText.first().format = charFmt;
    }
  }

  if (res.newContent) {
    append(res.newContent.value(), charFmt);
    return;
  }

  const QList<FormattedText> linkified = linkifiedText(formattedText, res.linkSpecs);
  for (const FormattedText &output : linkified)
    append(output.text, output.format);
  if (linkified.isEmpty())
    append({}, charFmt); // This might cause insertion of a newline character.

  for (OutputLineParser *const p : qAsConst(involvedParsers)) {
    if (d->postPrintAction)
      d->postPrintAction(p);
    else
      p->runPostPrintActions(plainTextEdit());
  }
}

auto OutputFormatter::handleMessage(const QString &text, OutputFormat format, QList<OutputLineParser*> &involvedParsers) -> OutputLineParser::Result
{
  // We only invoke the line parsers for stdout and stderr
  // Bad: on Windows we may get stdout and stdErr only as DebugFormat as e.g. GUI applications
  // print them Windows-internal and we retrieve this separately
  if (format != StdOutFormat && format != StdErrFormat && format != DebugFormat)
    return OutputLineParser::Status::NotHandled;
  const OutputLineParser *const oldNextParser = d->nextParser;
  if (d->nextParser) {
    involvedParsers << d->nextParser;
    const OutputLineParser::Result res = d->nextParser->handleLine(text, outputTypeForParser(d->nextParser, format));
    switch (res.status) {
    case OutputLineParser::Status::Done:
      d->nextParser = nullptr;
      return res;
    case OutputLineParser::Status::InProgress:
      return res;
    case OutputLineParser::Status::NotHandled:
      d->nextParser = nullptr;
      break;
    }
  }
  QTC_CHECK(!d->nextParser);
  for (OutputLineParser *const parser : qAsConst(d->lineParsers)) {
    if (parser == oldNextParser) // We tried that one already.
      continue;
    const OutputLineParser::Result res = parser->handleLine(text, outputTypeForParser(parser, format));
    switch (res.status) {
    case OutputLineParser::Status::Done:
      involvedParsers << parser;
      return res;
    case OutputLineParser::Status::InProgress:
      involvedParsers << parser;
      d->nextParser = parser;
      return res;
    case OutputLineParser::Status::NotHandled:
      break;
    }
  }
  return OutputLineParser::Status::NotHandled;
}

auto OutputFormatter::charFormat(OutputFormat format) const -> QTextCharFormat
{
  return d->formatOverride ? d->formatOverride.value() : d->formats[format];
}

auto OutputFormatter::parseAnsi(const QString &text, const QTextCharFormat &format) -> QList<FormattedText>
{
  return d->escapeCodeHandler.parseText(FormattedText(text, format));
}

auto OutputFormatter::linkifiedText(const QList<FormattedText> &text, const OutputLineParser::LinkSpecs &linkSpecs) -> const QList<FormattedText>
{
  if (linkSpecs.isEmpty())
    return text;

  QList<FormattedText> linkified;
  int totalTextLengthSoFar = 0;
  int nextLinkSpecIndex = 0;

  for (const FormattedText &t : text) {
    const int totalPreviousTextLength = totalTextLengthSoFar;

    // There is no more linkification work to be done. Just copy the text as-is.
    if (nextLinkSpecIndex >= linkSpecs.size()) {
      linkified << t;
      continue;
    }

    for (int nextLocalTextPos = 0; nextLocalTextPos < t.text.size();) {

      // There are no more links in this part, so copy the rest of the text as-is.
      if (nextLinkSpecIndex >= linkSpecs.size()) {
        linkified << FormattedText(t.text.mid(nextLocalTextPos), t.format);
        totalTextLengthSoFar += t.text.length() - nextLocalTextPos;
        break;
      }

      const OutputLineParser::LinkSpec &linkSpec = linkSpecs.at(nextLinkSpecIndex);
      const int localLinkStartPos = linkSpec.startPos - totalPreviousTextLength;
      ++nextLinkSpecIndex;

      // We ignore links that would cross format boundaries.
      if (localLinkStartPos < nextLocalTextPos || localLinkStartPos + linkSpec.length > t.text.length()) {
        linkified << FormattedText(t.text.mid(nextLocalTextPos), t.format);
        totalTextLengthSoFar += t.text.length() - nextLocalTextPos;
        break;
      }

      // Now we know we have a link that is fully inside this part of the text.
      // Split the text so that the link part gets the appropriate format.
      const int prefixLength = localLinkStartPos - nextLocalTextPos;
      const QString textBeforeLink = t.text.mid(nextLocalTextPos, prefixLength);
      linkified << FormattedText(textBeforeLink, t.format);
      const QString linkedText = t.text.mid(localLinkStartPos, linkSpec.length);
      linkified << FormattedText(linkedText, linkFormat(t.format, linkSpec.target));
      nextLocalTextPos = localLinkStartPos + linkSpec.length;
      totalTextLengthSoFar += prefixLength + linkSpec.length;
    }
  }
  return linkified;
}

auto OutputFormatter::append(const QString &text, const QTextCharFormat &format) -> void
{
  if (!plainTextEdit())
    return;
  flushTrailingNewline();
  int startPos = 0;
  int crPos = -1;
  while ((crPos = text.indexOf('\r', startPos)) >= 0) {
    d->cursor.insertText(text.mid(startPos, crPos - startPos), format);
    d->cursor.clearSelection();
    d->cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    startPos = crPos + 1;
  }
  if (startPos < text.count())
    d->cursor.insertText(text.mid(startPos), format);
}

auto OutputFormatter::linkFormat(const QTextCharFormat &inputFormat, const QString &href) -> QTextCharFormat
{
  QTextCharFormat result = inputFormat;
  result.setForeground(orcaTheme()->color(Theme::TextColorLink));
  result.setUnderlineStyle(QTextCharFormat::SingleUnderline);
  result.setAnchor(true);
  result.setAnchorHref(href);
  return result;
}

#ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
void OutputFormatter::overrideTextCharFormat(const QTextCharFormat &fmt)
{
    d->formatOverride = fmt;
}

QList<OutputLineParser *> OutputFormatter::lineParsers() const
{
    return d->lineParsers;
}
#endif // ORCA_BUILD_WITH_PLUGINS_TESTS

auto OutputFormatter::clearLastLine() -> void
{
  // Note that this approach will fail if the text edit is not read-only and users
  // have messed with the last line between programmatic inputs.
  // We live with this risk, as all the alternatives are worse.
  if (!d->cursor.atEnd())
    d->cursor.movePosition(QTextCursor::End);
  d->cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
  d->cursor.removeSelectedText();
}

auto OutputFormatter::initFormats() -> void
{
  if (!plainTextEdit())
    return;

  Theme *theme = orcaTheme();
  d->formats[NormalMessageFormat].setForeground(theme->color(Theme::OutputPanes_NormalMessageTextColor));
  d->formats[ErrorMessageFormat].setForeground(theme->color(Theme::OutputPanes_ErrorMessageTextColor));
  d->formats[LogMessageFormat].setForeground(theme->color(Theme::OutputPanes_WarningMessageTextColor));
  d->formats[StdOutFormat].setForeground(theme->color(Theme::OutputPanes_StdOutTextColor));
  d->formats[StdErrFormat].setForeground(theme->color(Theme::OutputPanes_StdErrTextColor));
  d->formats[DebugFormat].setForeground(theme->color(Theme::OutputPanes_DebugTextColor));
  d->formats[GeneralMessageFormat].setForeground(theme->color(Theme::OutputPanes_DebugTextColor));
  setBoldFontEnabled(d->boldFontEnabled);
}

auto OutputFormatter::flushIncompleteLine() -> void
{
  clearLastLine();
  doAppendMessage(d->incompleteLine.first, d->incompleteLine.second);
  d->incompleteLine.first.clear();
}

auto Utils::OutputFormatter::flushTrailingNewline() -> void
{
  if (d->prependLineFeed) {
    d->cursor.insertText("\n");
    d->prependLineFeed = false;
  }
}

auto OutputFormatter::dumpIncompleteLine(const QString &line, OutputFormat format) -> void
{
  if (line.isEmpty())
    return;
  append(line, charFormat(format));
  d->incompleteLine.first.append(line);
  d->incompleteLine.second = format;
}

auto OutputFormatter::handleFileLink(const QString &href) -> bool
{
  if (!OutputLineParser::isLinkTarget(href))
    return false;

  Link link = OutputLineParser::parseLinkTarget(href);
  QTC_ASSERT(!link.targetFilePath.isEmpty(), return false);
  emit openInEditorRequested(link);
  return true;
}

auto OutputFormatter::handleLink(const QString &href) -> void
{
  QTC_ASSERT(!href.isEmpty(), return);
  // We can handle absolute file paths ourselves. Other types of references are forwarded
  // to the line parsers.
  if (handleFileLink(href))
    return;
  for (OutputLineParser *const f : qAsConst(d->lineParsers)) {
    if (f->handleLink(href))
      return;
  }
}

auto OutputFormatter::clear() -> void
{
  if (plainTextEdit())
    plainTextEdit()->clear();
}

auto OutputFormatter::reset() -> void
{
  d->prependCarriageReturn = false;
  d->incompleteLine.first.clear();
  d->nextParser = nullptr;
  qDeleteAll(d->lineParsers);
  d->lineParsers.clear();
  d->fileFinder = FileInProjectFinder();
  d->formatOverride.reset();
  d->escapeCodeHandler = AnsiEscapeCodeHandler();
}

auto OutputFormatter::setBoldFontEnabled(bool enabled) -> void
{
  d->boldFontEnabled = enabled;
  const QFont::Weight fontWeight = enabled ? QFont::Bold : QFont::Normal;
  d->formats[NormalMessageFormat].setFontWeight(fontWeight);
  d->formats[ErrorMessageFormat].setFontWeight(fontWeight);
}

auto OutputFormatter::setForwardStdOutToStdError(bool enabled) -> void
{
  d->forwardStdOutToStdError = enabled;
}

auto OutputFormatter::flush() -> void
{
  if (!d->incompleteLine.first.isEmpty())
    flushIncompleteLine();
  flushTrailingNewline();
  d->escapeCodeHandler.endFormatScope();
  for (OutputLineParser *const p : qAsConst(d->lineParsers))
    p->flush();
  if (d->nextParser)
    d->nextParser->runPostPrintActions(plainTextEdit());
}

auto OutputFormatter::hasFatalErrors() const -> bool
{
  return anyOf(d->lineParsers, [](const OutputLineParser *p) {
    return p->hasFatalErrors();
  });
}

auto OutputFormatter::addSearchDir(const FilePath &dir) -> void
{
  for (OutputLineParser *const p : qAsConst(d->lineParsers))
    p->addSearchDir(dir);
}

auto OutputFormatter::dropSearchDir(const FilePath &dir) -> void
{
  for (OutputLineParser *const p : qAsConst(d->lineParsers))
    p->dropSearchDir(dir);
}

auto OutputFormatter::outputTypeForParser(const OutputLineParser *parser, OutputFormat type) const -> OutputFormat
{
  if (type == StdOutFormat && (parser->needsRedirection() || d->forwardStdOutToStdError))
    return StdErrFormat;
  return type;
}

auto OutputFormatter::appendMessage(const QString &text, OutputFormat format) -> void
{
  if (text.isEmpty())
    return;

  // If we have an existing incomplete line and its format is different from this one,
  // then we consider the two messages unrelated. We re-insert the previous incomplete line,
  // possibly formatted now, and start from scratch with the new input.
  if (!d->incompleteLine.first.isEmpty() && d->incompleteLine.second != format)
    flushIncompleteLine();

  QString out = text;
  if (d->prependCarriageReturn) {
    d->prependCarriageReturn = false;
    out.prepend('\r');
  }
  out = QtcProcess::normalizeNewlines(out);
  if (out.endsWith('\r')) {
    d->prependCarriageReturn = true;
    out.chop(1);
  }

  // If the input is a single incomplete line, we do not forward it to the specialized
  // formatting code, but simply dump it as-is. Once it becomes complete or it needs to
  // be flushed for other reasons, we remove the unformatted part and re-insert it, this
  // time with proper formatting.
  if (!out.contains('\n')) {
    dumpIncompleteLine(out, format);
    return;
  }

  // We have at least one complete line, so let's remove the previously dumped
  // incomplete line and prepend it to the first line of our new input.
  if (!d->incompleteLine.first.isEmpty()) {
    clearLastLine();
    out.prepend(d->incompleteLine.first);
    d->incompleteLine.first.clear();
  }

  // Forward all complete lines to the specialized formatting code, and handle a
  // potential trailing incomplete line the same way as above.
  for (int startPos = 0; ;) {
    const int eolPos = out.indexOf('\n', startPos);
    if (eolPos == -1) {
      dumpIncompleteLine(out.mid(startPos), format);
      break;
    }
    doAppendMessage(out.mid(startPos, eolPos - startPos), format);
    d->prependLineFeed = true;
    startPos = eolPos + 1;
  }
}

} // namespace Utils
