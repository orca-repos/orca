// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filesearch.hpp"

#include "algorithm.hpp"
#include "fileutils.hpp"
#include "mapreduce.hpp"
#include "qtcassert.hpp"
#include "stringutils.hpp"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMutex>
#include <QRegularExpression>
#include <QTextCodec>

#include <cctype>

Q_LOGGING_CATEGORY(log, "qtc.utils.filesearch", QtWarningMsg)

using namespace Utils;

static auto msgCanceled(const QString &searchTerm, int numMatches, int numFilesSearched) -> QString
{
  return QCoreApplication::translate("Utils::FileSearch", "%1: canceled. %n occurrences found in %2 files.", nullptr, numMatches).arg(searchTerm).arg(numFilesSearched);
}

static auto msgFound(const QString &searchTerm, int numMatches, int numFilesSearched) -> QString
{
  return QCoreApplication::translate("Utils::FileSearch", "%1: %n occurrences found in %2 files.", nullptr, numMatches).arg(searchTerm).arg(numFilesSearched);
}

namespace {

const int MAX_LINE_SIZE = 400;

auto clippedText(const QString &text, int maxLength) -> QString
{
  if (text.length() > maxLength)
    return text.left(maxLength) + QChar(0x2026); // '...'
  return text;
}

// returns success
auto getFileContent(const QString &filePath, QTextCodec *encoding, QString *tempString, const QMap<QString, QString> &fileToContentsMap) -> bool
{
  if (fileToContentsMap.contains(filePath)) {
    *tempString = fileToContentsMap.value(filePath);
  } else {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
      return false;
    const QByteArray content = file.readAll();
    *tempString = QTC_GUARD(encoding) ? encoding->toUnicode(content) : QTextCodec::codecForLocale()->toUnicode(content);
  }
  return true;
}

class FileSearch {
public:
  FileSearch(const QString &searchTerm, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap);
  auto operator()(QFutureInterface<FileSearchResultList> &futureInterface, const FileIterator::Item &item) const -> void;

private:
  QMap<QString, QString> fileToContentsMap;
  QString searchTermLower;
  QString searchTermUpper;
  int termMaxIndex;
  const QChar *termData;
  const QChar *termDataLower;
  const QChar *termDataUpper;
  bool caseSensitive;
  bool wholeWord;
};

class FileSearchRegExp {
public:
  FileSearchRegExp(const QString &searchTerm, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap);
  FileSearchRegExp(const FileSearchRegExp &other);
  auto operator()(QFutureInterface<FileSearchResultList> &futureInterface, const FileIterator::Item &item) const -> void;

private:
  auto doGuardedMatch(const QString &line, int offset) const -> QRegularExpressionMatch;

  QMap<QString, QString> fileToContentsMap;
  QRegularExpression expression;
  mutable QMutex mutex;
};

FileSearch::FileSearch(const QString &searchTerm, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap)
{
  this->fileToContentsMap = fileToContentsMap;
  caseSensitive = (flags & QTextDocument::FindCaseSensitively);
  wholeWord = (flags & QTextDocument::FindWholeWords);
  searchTermLower = searchTerm.toLower();
  searchTermUpper = searchTerm.toUpper();
  termMaxIndex = searchTerm.length() - 1;
  termData = searchTerm.constData();
  termDataLower = searchTermLower.constData();
  termDataUpper = searchTermUpper.constData();
}

auto FileSearch::operator()(QFutureInterface<FileSearchResultList> &futureInterface, const FileIterator::Item &item) const -> void
{
  if (futureInterface.isCanceled())
    return;
  qCDebug(log) << "Searching in" << item.filePath;
  futureInterface.setProgressRange(0, 1);
  futureInterface.setProgressValue(0);
  FileSearchResultList results;
  QString tempString;
  if (!getFileContent(item.filePath, item.encoding, &tempString, fileToContentsMap)) {
    qCDebug(log) << "- failed to get content for" << item.filePath;
    futureInterface.cancel(); // failure
    return;
  }
  QTextStream stream(&tempString);
  int lineNr = 0;

  while (!stream.atEnd()) {
    ++lineNr;
    const QString chunk = stream.readLine();
    const QString resultItemText = clippedText(chunk, MAX_LINE_SIZE);
    int chunkLength = chunk.length();
    const QChar *chunkPtr = chunk.constData();
    const QChar *chunkEnd = chunkPtr + chunkLength - 1;
    for (const QChar *regionPtr = chunkPtr; regionPtr + termMaxIndex <= chunkEnd; ++regionPtr) {
      const QChar *regionEnd = regionPtr + termMaxIndex;
      if ( /* optimization check for start and end of region */
        // case sensitive
        (caseSensitive && *regionPtr == termData[0] && *regionEnd == termData[termMaxIndex]) ||
        // case insensitive
        (!caseSensitive && (*regionPtr == termDataLower[0] || *regionPtr == termDataUpper[0]) && (*regionEnd == termDataLower[termMaxIndex] || *regionEnd == termDataUpper[termMaxIndex]))) {
        bool equal = true;

        // whole word check
        const QChar *beforeRegion = regionPtr - 1;
        const QChar *afterRegion = regionEnd + 1;
        if (wholeWord && (((beforeRegion >= chunkPtr) && (beforeRegion->isLetterOrNumber() || ((*beforeRegion) == QLatin1Char('_')))) || ((afterRegion <= chunkEnd) && (afterRegion->isLetterOrNumber() || ((*afterRegion) == QLatin1Char('_')))))) {
          equal = false;
        } else {
          // check all chars
          int regionIndex = 1;
          for (const QChar *regionCursor = regionPtr + 1; regionCursor < regionEnd; ++regionCursor, ++regionIndex) {
            if ( // case sensitive
              (caseSensitive && *regionCursor != termData[regionIndex]) ||
              // case insensitive
              (!caseSensitive && *regionCursor != termDataLower[regionIndex] && *regionCursor != termDataUpper[regionIndex])) {
              equal = false;
            }
          }
        }
        if (equal) {
          results << FileSearchResult(item.filePath, lineNr, resultItemText, regionPtr - chunkPtr, termMaxIndex + 1, QStringList());
          regionPtr += termMaxIndex; // another +1 done by for-loop
        }
      }
    }
    if (futureInterface.isPaused())
      futureInterface.waitForResume();
    if (futureInterface.isCanceled())
      break;
  }
  if (!futureInterface.isCanceled()) {
    futureInterface.reportResult(results);
    futureInterface.setProgressValue(1);
  }
  qCDebug(log) << "- finished searching in" << item.filePath;
}

FileSearchRegExp::FileSearchRegExp(const QString &searchTerm, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap)
{
  this->fileToContentsMap = fileToContentsMap;
  QString term = searchTerm;
  if (flags & QTextDocument::FindWholeWords)
    term = QString::fromLatin1("\\b%1\\b").arg(term);
  const QRegularExpression::PatternOptions patternOptions = (flags & QTextDocument::FindCaseSensitively) ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
  expression = QRegularExpression(term, patternOptions);
}

FileSearchRegExp::FileSearchRegExp(const FileSearchRegExp &other) : fileToContentsMap(other.fileToContentsMap), expression(other.expression) {}

auto FileSearchRegExp::doGuardedMatch(const QString &line, int offset) const -> QRegularExpressionMatch
{
  QMutexLocker lock(&mutex);
  return expression.match(line, offset);
}

auto FileSearchRegExp::operator()(QFutureInterface<FileSearchResultList> &futureInterface, const FileIterator::Item &item) const -> void
{
  if (!expression.isValid()) {
    futureInterface.cancel();
    return;
  }
  if (futureInterface.isCanceled())
    return;
  qCDebug(log) << "Searching in" << item.filePath;
  futureInterface.setProgressRange(0, 1);
  futureInterface.setProgressValue(0);
  FileSearchResultList results;
  QString tempString;
  if (!getFileContent(item.filePath, item.encoding, &tempString, fileToContentsMap)) {
    qCDebug(log) << "- failed to get content for" << item.filePath;
    futureInterface.cancel(); // failure
    return;
  }
  QTextStream stream(&tempString);
  int lineNr = 0;

  QString line;
  QRegularExpressionMatch match;
  while (!stream.atEnd()) {
    ++lineNr;
    line = stream.readLine();
    const QString resultItemText = clippedText(line, MAX_LINE_SIZE);
    int lengthOfLine = line.size();
    int pos = 0;
    while ((match = doGuardedMatch(line, pos)).hasMatch()) {
      pos = match.capturedStart();
      results << FileSearchResult(item.filePath, lineNr, resultItemText, pos, match.capturedLength(), match.capturedTexts());
      if (match.capturedLength() == 0)
        break;
      pos += match.capturedLength();
      if (pos >= lengthOfLine)
        break;
    }
    if (futureInterface.isPaused())
      futureInterface.waitForResume();
    if (futureInterface.isCanceled())
      break;
  }
  if (!futureInterface.isCanceled()) {
    futureInterface.reportResult(results);
    futureInterface.setProgressValue(1);
  }
  qCDebug(log) << "- finished searching in" << item.filePath;
}

struct SearchState {
  SearchState(const QString &term, FileIterator *iterator) : searchTerm(term), files(iterator) {}
  QString searchTerm;
  FileIterator *files = nullptr;
  FileSearchResultList cachedResults;
  int numFilesSearched = 0;
  int numMatches = 0;
};

auto initFileSearch(QFutureInterface<FileSearchResultList> &futureInterface, const QString &searchTerm, FileIterator *files) -> SearchState
{
  futureInterface.setProgressRange(0, files->maxProgress());
  futureInterface.setProgressValueAndText(files->currentProgress(), msgFound(searchTerm, 0, 0));
  return SearchState(searchTerm, files);
}

auto collectSearchResults(QFutureInterface<FileSearchResultList> &futureInterface, SearchState &state, const FileSearchResultList &results) -> void
{
  state.numMatches += results.size();
  state.cachedResults << results;
  state.numFilesSearched += 1;
  if (futureInterface.isProgressUpdateNeeded() || futureInterface.progressValue() == 0 /*workaround for regression in Qt*/) {
    if (!state.cachedResults.isEmpty()) {
      futureInterface.reportResult(state.cachedResults);
      state.cachedResults.clear();
    }
    futureInterface.setProgressRange(0, state.files->maxProgress());
    futureInterface.setProgressValueAndText(state.files->currentProgress(), msgFound(state.searchTerm, state.numMatches, state.numFilesSearched));
  }
}

auto cleanUpFileSearch(QFutureInterface<FileSearchResultList> &futureInterface, SearchState &state) -> void
{
  if (!state.cachedResults.isEmpty()) {
    futureInterface.reportResult(state.cachedResults);
    state.cachedResults.clear();
  }
  if (futureInterface.isCanceled()) {
    futureInterface.setProgressValueAndText(state.files->currentProgress(), msgCanceled(state.searchTerm, state.numMatches, state.numFilesSearched));
  } else {
    futureInterface.setProgressValueAndText(state.files->currentProgress(), msgFound(state.searchTerm, state.numMatches, state.numFilesSearched));
  }
  delete state.files;
}

} // namespace

auto Utils::findInFiles(const QString &searchTerm, FileIterator *files, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap) -> QFuture<FileSearchResultList>
{
  return mapReduce(files->begin(), files->end(), [searchTerm, files](QFutureInterface<FileSearchResultList> &futureInterface) {
    return initFileSearch(futureInterface, searchTerm, files);
  }, FileSearch(searchTerm, flags, fileToContentsMap), &collectSearchResults, &cleanUpFileSearch);
}

auto Utils::findInFilesRegExp(const QString &searchTerm, FileIterator *files, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap) -> QFuture<FileSearchResultList>
{
  return mapReduce(files->begin(), files->end(), [searchTerm, files](QFutureInterface<FileSearchResultList> &futureInterface) {
    return initFileSearch(futureInterface, searchTerm, files);
  }, FileSearchRegExp(searchTerm, flags, fileToContentsMap), &collectSearchResults, &cleanUpFileSearch);
}

auto Utils::expandRegExpReplacement(const QString &replaceText, const QStringList &capturedTexts) -> QString
{
  // handles \1 \\ \& \t \n $1 $$ $&
  QString result;
  const int numCaptures = capturedTexts.size() - 1;
  const int replaceLength = replaceText.length();
  for (int i = 0; i < replaceLength; ++i) {
    QChar c = replaceText.at(i);
    if (c == QLatin1Char('\\') && i < replaceLength - 1) {
      c = replaceText.at(++i);
      if (c == QLatin1Char('\\')) {
        result += QLatin1Char('\\');
      } else if (c == QLatin1Char('&')) {
        result += QLatin1Char('&');
      } else if (c == QLatin1Char('t')) {
        result += QLatin1Char('\t');
      } else if (c == QLatin1Char('n')) {
        result += QLatin1Char('\n');
      } else if (c.isDigit()) {
        int index = c.unicode() - '1';
        if (index < numCaptures) {
          result += capturedTexts.at(index + 1);
        } // else add nothing
      } else {
        result += QLatin1Char('\\');
        result += c;
      }
    } else if (c == '$' && i < replaceLength - 1) {
      c = replaceText.at(++i);
      if (c == '$') {
        result += '$';
      } else if (c == '&') {
        result += capturedTexts.at(0);
      } else if (c.isDigit()) {
        int index = c.unicode() - '1';
        if (index < numCaptures) {
          result += capturedTexts.at(index + 1);
        } // else add nothing
      } else {
        result += '$';
        result += c;
      }
    } else {
      result += c;
    }
  }
  return result;
}

namespace Utils {
namespace Internal {

auto matchCaseReplacement(const QString &originalText, const QString &replaceText) -> QString
{
  if (originalText.isEmpty() || replaceText.isEmpty())
    return replaceText;

  //Now proceed with actual case matching
  bool firstIsUpperCase = originalText.at(0).isUpper();
  bool firstIsLowerCase = originalText.at(0).isLower();
  bool restIsLowerCase = true; // to be verified
  bool restIsUpperCase = true; // to be verified

  for (int i = 1; i < originalText.length(); ++i) {
    if (originalText.at(i).isUpper())
      restIsLowerCase = false;
    else if (originalText.at(i).isLower())
      restIsUpperCase = false;

    if (!restIsLowerCase && !restIsUpperCase)
      break;
  }

  if (restIsLowerCase) {
    QString res = replaceText.toLower();
    if (firstIsUpperCase)
      res.replace(0, 1, res.at(0).toUpper());
    return res;
  } else if (restIsUpperCase) {
    QString res = replaceText.toUpper();
    if (firstIsLowerCase)
      res.replace(0, 1, res.at(0).toLower());
    return res;
  } else {
    return replaceText; // mixed
  }
}
} // namespace

static auto filtersToRegExps(const QStringList &filters) -> QList<QRegularExpression>
{
  return Utils::transform(filters, [](const QString &filter) {
    return QRegularExpression(Utils::wildcardToRegularExpression(filter), QRegularExpression::CaseInsensitiveOption);
  });
}

static auto matches(const QList<QRegularExpression> &exprList, const QString &filePath) -> bool
{
  return Utils::anyOf(exprList, [&filePath](const QRegularExpression &reg) {
    return (reg.match(filePath).hasMatch() || reg.match(FilePath::fromString(filePath).fileName()).hasMatch());
  });
}

static auto isFileIncluded(const QList<QRegularExpression> &filterRegs, const QList<QRegularExpression> &exclusionRegs, const QString &filePath) -> bool
{
  const bool isIncluded = filterRegs.isEmpty() || matches(filterRegs, filePath);
  return isIncluded && (exclusionRegs.isEmpty() || !matches(exclusionRegs, filePath));
}

auto filterFileFunction(const QStringList &filters, const QStringList &exclusionFilters) -> std::function<bool(const QString &)>
{
  const QList<QRegularExpression> filterRegs = filtersToRegExps(filters);
  const QList<QRegularExpression> exclusionRegs = filtersToRegExps(exclusionFilters);
  return [filterRegs, exclusionRegs](const QString &filePath) {
    return isFileIncluded(filterRegs, exclusionRegs, filePath);
  };
}

auto filterFilesFunction(const QStringList &filters, const QStringList &exclusionFilters) -> std::function<QStringList(const QStringList &)>
{
  const QList<QRegularExpression> filterRegs = filtersToRegExps(filters);
  const QList<QRegularExpression> exclusionRegs = filtersToRegExps(exclusionFilters);
  return [filterRegs, exclusionRegs](const QStringList &filePaths) {
    return Utils::filtered(filePaths, [&filterRegs, &exclusionRegs](const QString &filePath) {
      return isFileIncluded(filterRegs, exclusionRegs, filePath);
    });
  };
}

auto splitFilterUiText(const QString &text) -> QStringList
{
  const QStringList parts = text.split(',');
  const QStringList trimmedPortableParts = Utils::transform(parts, [](const QString &s) {
    return QDir::fromNativeSeparators(s.trimmed());
  });
  return Utils::filtered(trimmedPortableParts, [](const QString &s) { return !s.isEmpty(); });
}

auto msgFilePatternLabel() -> QString
{
  return QCoreApplication::translate("Utils::FileSearch", "Fi&le pattern:");
}

auto msgExclusionPatternLabel() -> QString
{
  return QCoreApplication::translate("Utils::FileSearch", "Excl&usion pattern:");
}

auto msgFilePatternToolTip() -> QString
{
  return QCoreApplication::translate("Utils::FileSearch", "List of comma separated wildcard filters. " "Files with file name or full file path matching any filter are included.");
}

auto matchCaseReplacement(const QString &originalText, const QString &replaceText) -> QString
{
  if (originalText.isEmpty())
    return replaceText;

  //Find common prefix & suffix: these will be unaffected
  const int replaceTextLen = replaceText.length();
  const int originalTextLen = originalText.length();

  int prefixLen = 0;
  for (; prefixLen < replaceTextLen && prefixLen < originalTextLen; ++prefixLen)
    if (replaceText.at(prefixLen).toLower() != originalText.at(prefixLen).toLower())
      break;

  int suffixLen = 0;
  for (; suffixLen < replaceTextLen - prefixLen && suffixLen < originalTextLen - prefixLen; ++suffixLen)
    if (replaceText.at(replaceTextLen - 1 - suffixLen).toLower() != originalText.at(originalTextLen - 1 - suffixLen).toLower())
      break;

  //keep prefix and suffix, and do actual replacement on the 'middle' of the string
  return originalText.left(prefixLen) + Internal::matchCaseReplacement(originalText.mid(prefixLen, originalTextLen - prefixLen - suffixLen), replaceText.mid(prefixLen, replaceTextLen - prefixLen - suffixLen)) + originalText.right(suffixLen);
}

// #pragma mark -- FileIterator

auto FileIterator::advance(FileIterator::const_iterator *it) const -> void
{
  if (it->m_index < 0) // == end
    return;
  ++it->m_index;
  const_cast<FileIterator*>(this)->update(it->m_index);
  if (it->m_index >= currentFileCount())
    it->m_index = -1; // == end
}

auto FileIterator::begin() const -> FileIterator::const_iterator
{
  const_cast<FileIterator*>(this)->update(0);
  if (currentFileCount() == 0)
    return end();
  return FileIterator::const_iterator(this, 0/*index*/);
}

auto FileIterator::end() const -> FileIterator::const_iterator
{
  return FileIterator::const_iterator(this, -1/*end*/);
}

// #pragma mark -- FileListIterator

auto encodingAt(const QList<QTextCodec*> &encodings, int index) -> QTextCodec*
{
  if (index >= 0 && index < encodings.size())
    return encodings.at(index);
  return QTextCodec::codecForLocale();
}

FileListIterator::FileListIterator(const QStringList &fileList, const QList<QTextCodec*> encodings) : m_maxIndex(-1)
{
  m_items.reserve(fileList.size());
  for (int i = 0; i < fileList.size(); ++i)
    m_items.append(Item(fileList.at(i), encodingAt(encodings, i)));
}

auto FileListIterator::update(int requestedIndex) -> void
{
  if (requestedIndex > m_maxIndex)
    m_maxIndex = requestedIndex;
}

auto FileListIterator::currentFileCount() const -> int
{
  return m_items.size();
}

auto FileListIterator::itemAt(int index) const -> const FileIterator::Item&
{
  return m_items.at(index);
}

auto FileListIterator::maxProgress() const -> int
{
  return m_items.size();
}

auto FileListIterator::currentProgress() const -> int
{
  return m_maxIndex + 1;
}

// #pragma mark -- SubDirFileIterator

namespace {
const int MAX_PROGRESS = 1000;
}

SubDirFileIterator::SubDirFileIterator(const QStringList &directories, const QStringList &filters, const QStringList &exclusionFilters, QTextCodec *encoding) : m_filterFiles(filterFilesFunction(filters, exclusionFilters)), m_progress(0)
{
  m_encoding = (encoding == nullptr ? QTextCodec::codecForLocale() : encoding);
  qreal maxPer = qreal(MAX_PROGRESS) / directories.count();
  for (const QString &directoryEntry : directories) {
    if (!directoryEntry.isEmpty()) {
      const QDir dir(directoryEntry);
      const QString canonicalPath = dir.canonicalPath();
      if (!canonicalPath.isEmpty() && dir.exists()) {
        m_dirs.push(dir);
        m_knownDirs.insert(canonicalPath);
        m_progressValues.push(maxPer);
        m_processedValues.push(false);
      }
    }
  }
}

SubDirFileIterator::~SubDirFileIterator()
{
  qDeleteAll(m_items);
}

auto SubDirFileIterator::update(int index) -> void
{
  if (index < m_items.size())
    return;
  // collect files from the directories until we have enough for the given index
  while (!m_dirs.isEmpty() && index >= m_items.size()) {
    QDir dir = m_dirs.pop();
    const qreal dirProgressMax = m_progressValues.pop();
    const bool processed = m_processedValues.pop();
    if (dir.exists()) {
      const QString dirPath = dir.path();
      using Dir = QString;
      using CanonicalDir = QString;
      std::vector<std::pair<Dir, CanonicalDir>> subDirs;
      if (!processed) {
        for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
          const QString canonicalDir = info.canonicalFilePath();
          if (!m_knownDirs.contains(canonicalDir))
            subDirs.emplace_back(info.filePath(), canonicalDir);
        }
      }
      if (subDirs.empty()) {
        const QStringList allFileEntries = dir.entryList(QDir::Files | QDir::Hidden);
        const QStringList allFilePaths = Utils::transform(allFileEntries, [&dirPath](const QString &entry) {
          return QString(dirPath + '/' + entry);
        });
        const QStringList filePaths = m_filterFiles(allFilePaths);
        m_items.reserve(m_items.size() + filePaths.size());
        Utils::reverseForeach(filePaths, [this](const QString &file) {
          m_items.append(new Item(file, m_encoding));
        });
        m_progress += dirProgressMax;
      } else {
        qreal subProgress = dirProgressMax / (subDirs.size() + 1);
        m_dirs.push(dir);
        m_progressValues.push(subProgress);
        m_processedValues.push(true);
        Utils::reverseForeach(subDirs, [this, subProgress](const std::pair<Dir, CanonicalDir> &dir) {
          m_dirs.push(QDir(dir.first));
          m_knownDirs.insert(dir.second);
          m_progressValues.push(subProgress);
          m_processedValues.push(false);
        });
      }
    } else {
      m_progress += dirProgressMax;
    }
  }
  if (index >= m_items.size())
    m_progress = MAX_PROGRESS;
}

auto SubDirFileIterator::currentFileCount() const -> int
{
  return m_items.size();
}

auto SubDirFileIterator::itemAt(int index) const -> const FileIterator::Item&
{
  return *m_items.at(index);
}

auto SubDirFileIterator::maxProgress() const -> int
{
  return MAX_PROGRESS;
}

auto SubDirFileIterator::currentProgress() const -> int
{
  return qMin(qRound(m_progress), MAX_PROGRESS);
}

}
