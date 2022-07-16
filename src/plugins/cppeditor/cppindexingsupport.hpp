// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "cppmodelmanager.hpp"

#include <core/core-text-find-constants.hpp>

#include <QFuture>
#include <QStringList>

namespace Orca::Plugin::Core {
class SearchResultItem;
}

namespace CppEditor {

class CPPEDITOR_EXPORT SymbolSearcher : public QObject {
  Q_OBJECT

public:
  enum SymbolType {
    Classes = 0x1,
    Functions = 0x2,
    Enums = 0x4,
    Declarations = 0x8,
    TypeAliases = 0x16,
  };

  Q_DECLARE_FLAGS(SymbolTypes, SymbolType)

  enum SearchScope {
    SearchProjectsOnly,
    SearchGlobal
  };

  struct Parameters {
    QString text;
    Orca::Plugin::Core::FindFlags flags;
    SymbolTypes types;
    SearchScope scope;
  };

  SymbolSearcher(QObject *parent = nullptr);
  ~SymbolSearcher() override = 0;

  virtual auto runSearch(QFutureInterface<Orca::Plugin::Core::SearchResultItem> &future) -> void = 0;
};

class CPPEDITOR_EXPORT CppIndexingSupport {
public:
  virtual ~CppIndexingSupport() = 0;
  virtual auto refreshSourceFiles(const QSet<QString> &sourceFiles, CppModelManager::ProgressNotificationMode mode) -> QFuture<void> = 0;
  virtual auto createSymbolSearcher(const SymbolSearcher::Parameters &parameters, const QSet<QString> &fileNames) -> SymbolSearcher* = 0;
};

} // namespace CppEditor

Q_DECLARE_METATYPE(CppEditor::SymbolSearcher::SearchScope)
Q_DECLARE_METATYPE(CppEditor::SymbolSearcher::Parameters)
Q_DECLARE_METATYPE(CppEditor::SymbolSearcher::SymbolTypes)
