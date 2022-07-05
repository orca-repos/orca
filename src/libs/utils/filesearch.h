// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QDir>
#include <QFuture>
#include <QMap>
#include <QStack>
#include <QTextDocument>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QTextCodec)

namespace Utils {

ORCA_UTILS_EXPORT auto filterFileFunction(const QStringList &filterRegs, const QStringList &exclusionRegs) -> std::function<bool(const QString &)>;
ORCA_UTILS_EXPORT auto filterFilesFunction(const QStringList &filters, const QStringList &exclusionFilters) -> std::function<QStringList(const QStringList &)>;
ORCA_UTILS_EXPORT auto splitFilterUiText(const QString &text) -> QStringList;
ORCA_UTILS_EXPORT auto msgFilePatternLabel() -> QString;
ORCA_UTILS_EXPORT auto msgExclusionPatternLabel() -> QString;
ORCA_UTILS_EXPORT auto msgFilePatternToolTip() -> QString;

class ORCA_UTILS_EXPORT FileIterator {
public:
  class Item {
  public:
    Item() = default;
    Item(const QString &path, QTextCodec *codec) : filePath(path), encoding(codec) {}
    QString filePath;
    QTextCodec *encoding = nullptr;
  };

  using value_type = Item;

  class const_iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Item;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    const_iterator() = default;
    const_iterator(const FileIterator *parent, int id) : m_parent(parent), m_index(id) {}
    const_iterator(const const_iterator &) = default;
    auto operator=(const const_iterator &) -> const_iterator& = default;

    auto operator*() const -> reference { return m_parent->itemAt(m_index); }
    auto operator->() const -> pointer { return &m_parent->itemAt(m_index); }
    auto operator++() -> void { m_parent->advance(this); }

    auto operator==(const const_iterator &other) const -> bool
    {
      return m_parent == other.m_parent && m_index == other.m_index;
    }

    auto operator!=(const const_iterator &other) const -> bool { return !operator==(other); }

    const FileIterator *m_parent = nullptr;
    int m_index = -1; // -1 == end
  };

  virtual ~FileIterator() = default;
  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;

  virtual auto maxProgress() const -> int = 0;
  virtual auto currentProgress() const -> int = 0;

  auto advance(const_iterator *it) const -> void;
  virtual auto itemAt(int index) const -> const Item& = 0;

protected:
  virtual auto update(int requestedIndex) -> void = 0;
  virtual auto currentFileCount() const -> int = 0;
};

class ORCA_UTILS_EXPORT FileListIterator : public FileIterator {
public:
  explicit FileListIterator(const QStringList &fileList, QList<QTextCodec*> encodings);

  auto maxProgress() const -> int override;
  auto currentProgress() const -> int override;

protected:
  auto update(int requestedIndex) -> void override;
  auto currentFileCount() const -> int override;
  auto itemAt(int index) const -> const Item& override;

private:
  QVector<Item> m_items;
  int m_maxIndex;
};

class ORCA_UTILS_EXPORT SubDirFileIterator : public FileIterator {
public:
  SubDirFileIterator(const QStringList &directories, const QStringList &filters, const QStringList &exclusionFilters, QTextCodec *encoding = nullptr);
  ~SubDirFileIterator() override;

  auto maxProgress() const -> int override;
  auto currentProgress() const -> int override;

protected:
  auto update(int requestedIndex) -> void override;
  auto currentFileCount() const -> int override;
  auto itemAt(int index) const -> const Item& override;

private:
  std::function<QStringList(const QStringList &)> m_filterFiles;
  QTextCodec *m_encoding;
  QStack<QDir> m_dirs;
  QSet<QString> m_knownDirs;
  QStack<qreal> m_progressValues;
  QStack<bool> m_processedValues;
  qreal m_progress;
  // Use heap allocated objects directly because we want references to stay valid even after resize
  QList<Item*> m_items;
};

class ORCA_UTILS_EXPORT FileSearchResult {
public:
  FileSearchResult() = default;
  FileSearchResult(const QString &fileName, int lineNumber, const QString &matchingLine, int matchStart, int matchLength, const QStringList &regexpCapturedTexts) : fileName(fileName), lineNumber(lineNumber), matchingLine(matchingLine), matchStart(matchStart), matchLength(matchLength), regexpCapturedTexts(regexpCapturedTexts) {}

  auto operator==(const FileSearchResult &o) const -> bool
  {
    return fileName == o.fileName && lineNumber == o.lineNumber && matchingLine == o.matchingLine && matchStart == o.matchStart && matchLength == o.matchLength && regexpCapturedTexts == o.regexpCapturedTexts;
  }

  auto operator!=(const FileSearchResult &o) const -> bool { return !(*this == o); }

  QString fileName;
  int lineNumber;
  QString matchingLine;
  int matchStart;
  int matchLength;
  QStringList regexpCapturedTexts;
};

using FileSearchResultList = QList<FileSearchResult>;

ORCA_UTILS_EXPORT auto findInFiles(const QString &searchTerm, FileIterator *files, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap = QMap<QString, QString>()) -> QFuture<FileSearchResultList>;
ORCA_UTILS_EXPORT auto findInFilesRegExp(const QString &searchTerm, FileIterator *files, QTextDocument::FindFlags flags, const QMap<QString, QString> &fileToContentsMap = QMap<QString, QString>()) -> QFuture<FileSearchResultList>;
ORCA_UTILS_EXPORT auto expandRegExpReplacement(const QString &replaceText, const QStringList &capturedTexts) -> QString;
ORCA_UTILS_EXPORT auto matchCaseReplacement(const QString &originalText, const QString &replaceText) -> QString;

} // namespace Utils

Q_DECLARE_METATYPE(Utils::FileSearchResultList)
