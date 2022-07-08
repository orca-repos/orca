// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"
#include <utils/filesearch.hpp>

#include <core/find/ifindfilter.hpp>
#include <core/find/searchresultwindow.hpp>

#include <QFuture>

namespace Utils {
class FileIterator;
}

namespace Core {
class IEditor;
class SearchResult;
class SearchResultItem;
} // namespace Core

namespace TextEditor {

namespace Internal {
class BaseFileFindPrivate;
class SearchEnginePrivate;
} // Internal

class TEXTEDITOR_EXPORT FileFindParameters {
public:
  QString text;
  QStringList nameFilters;
  QStringList exclusionFilters;
  QVariant additionalParameters;
  QVariant searchEngineParameters;
  int searchEngineIndex;
  Core::FindFlags flags;
};

class BaseFileFind;

class TEXTEDITOR_EXPORT SearchEngine : public QObject {
  Q_OBJECT

public:
  SearchEngine(QObject *parent = nullptr);
  ~SearchEngine() override;

  virtual auto title() const -> QString = 0;
  virtual auto toolTip() const -> QString = 0; // add %1 placeholder where the find flags should be put
  virtual auto widget() const -> QWidget* = 0;
  virtual auto parameters() const -> QVariant = 0;
  virtual auto readSettings(QSettings *settings) -> void = 0;
  virtual auto writeSettings(QSettings *settings) const -> void = 0;
  virtual auto executeSearch(const FileFindParameters &parameters, BaseFileFind *baseFileFind) -> QFuture<Utils::FileSearchResultList> = 0;
  virtual auto openEditor(const Core::SearchResultItem &item, const FileFindParameters &parameters) -> Core::IEditor* = 0;
  auto isEnabled() const -> bool;
  auto setEnabled(bool enabled) -> void;

signals:
  auto enabledChanged(bool enabled) -> void;

private:
  Internal::SearchEnginePrivate *d;
};

class TEXTEDITOR_EXPORT BaseFileFind : public Core::IFindFilter {
  Q_OBJECT

public:
  BaseFileFind();
  ~BaseFileFind() override;

  auto isEnabled() const -> bool override;
  auto isReplaceSupported() const -> bool override { return true; }
  auto findAll(const QString &txt, Core::FindFlags findFlags) -> void override;
  auto replaceAll(const QString &txt, Core::FindFlags findFlags) -> void override;
  auto addSearchEngine(SearchEngine *searchEngine) -> void;
  /* returns the list of unique files that were passed in items */
  static auto replaceAll(const QString &txt, const QList<Core::SearchResultItem> &items, bool preserveCase = false) -> Utils::FilePaths;
  virtual auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* = 0;

protected:
  virtual auto additionalParameters() const -> QVariant = 0;
  static auto getAdditionalParameters(Core::SearchResult *search) -> QVariant;
  virtual auto label() const -> QString = 0;   // see Core::SearchResultWindow::startNewSearch
  virtual auto toolTip() const -> QString = 0; // see Core::SearchResultWindow::startNewSearch,
  // add %1 placeholder where the find flags should be put
  auto executeSearch(const FileFindParameters &parameters) -> QFuture<Utils::FileSearchResultList>;
  auto writeCommonSettings(QSettings *settings) -> void;
  auto readCommonSettings(QSettings *settings, const QString &defaultFilter, const QString &defaultExclusionFilter) -> void;
  auto createPatternWidgets() -> QList<QPair<QWidget*, QWidget*>>;
  auto fileNameFilters() const -> QStringList;
  auto fileExclusionFilters() const -> QStringList;
  auto currentSearchEngine() const -> SearchEngine*;
  auto searchEngines() const -> QVector<SearchEngine*>;
  auto setCurrentSearchEngine(int index) -> void;
  virtual auto syncSearchEngineCombo(int /*selectedSearchEngineIndex*/) -> void {}

signals:
  auto currentSearchEngineChanged() -> void;

private:
  auto openEditor(Core::SearchResult *result, const Core::SearchResultItem &item) -> void;
  auto doReplace(const QString &txt, const QList<Core::SearchResultItem> &items, bool preserveCase) -> void;
  auto hideHighlightAll(bool visible) -> void;
  auto searchAgain(Core::SearchResult *search) -> void;
  auto recheckEnabled(Core::SearchResult *search) -> void;
  auto runNewSearch(const QString &txt, Core::FindFlags findFlags, Core::SearchResultWindow::SearchMode searchMode) -> void;
  auto runSearch(Core::SearchResult *search) -> void;

  Internal::BaseFileFindPrivate *d;
};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::FileFindParameters)
