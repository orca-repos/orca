// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchsymbols.hpp"

#include <core/core-find-filter-interface.hpp>
#include <core/core-search-result-item.hpp>
#include <core/core-search-result-window.hpp>

#include <QFutureWatcher>
#include <QPointer>
#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>

namespace Orca::Plugin::Core {
class SearchResult;
}

namespace CppEditor {

class CppModelManager;

namespace Internal {

class SymbolsFindFilter : public Orca::Plugin::Core::IFindFilter {
  Q_OBJECT

public:
  using SearchScope = SymbolSearcher::SearchScope;
  
  explicit SymbolsFindFilter(CppModelManager *manager);

  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto isEnabled() const -> bool override;
  auto findAll(const QString &txt, Orca::Plugin::Core::FindFlags findFlags) -> void override;
  auto createConfigWidget() -> QWidget* override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;
  auto setSymbolsToSearch(const SearchSymbols::SymbolTypes &types) -> void { m_symbolsToSearch = types; }
  auto symbolsToSearch() const -> SearchSymbols::SymbolTypes { return m_symbolsToSearch; }
  auto setSearchScope(SearchScope scope) -> void { m_scope = scope; }
  auto searchScope() const -> SearchScope { return m_scope; }

signals:
  auto symbolsToSearchChanged() -> void;

private:
  auto openEditor(const Orca::Plugin::Core::SearchResultItem &item) -> void;
  auto addResults(int begin, int end) -> void;
  auto finish() -> void;
  auto cancel() -> void;
  auto setPaused(bool paused) -> void;
  auto onTaskStarted(Utils::Id type) -> void;
  auto onAllTasksFinished(Utils::Id type) -> void;
  auto searchAgain() -> void;
  auto label() const -> QString;
  auto toolTip(Orca::Plugin::Core::FindFlags findFlags) const -> QString;
  auto startSearch(Orca::Plugin::Core::SearchResult *search) -> void;

  CppModelManager *m_manager;
  bool m_enabled;
  QMap<QFutureWatcher<Orca::Plugin::Core::SearchResultItem>*, QPointer<Orca::Plugin::Core::SearchResult>> m_watchers;
  QPointer<Orca::Plugin::Core::SearchResult> m_currentSearch;
  SearchSymbols::SymbolTypes m_symbolsToSearch;
  SearchScope m_scope;
};

class SymbolsFindFilterConfigWidget : public QWidget {
  Q_OBJECT

public:
  explicit SymbolsFindFilterConfigWidget(SymbolsFindFilter *filter);

private:
  auto setState() const -> void;
  auto getState() -> void;

  SymbolsFindFilter *m_filter;
  QCheckBox *m_typeClasses;
  QCheckBox *m_typeMethods;
  QCheckBox *m_typeEnums;
  QCheckBox *m_typeDeclarations;
  QRadioButton *m_searchGlobal;
  QRadioButton *m_searchProjectsOnly;
  QButtonGroup *m_searchGroup;
};

} // Internal
} // CppEditor
