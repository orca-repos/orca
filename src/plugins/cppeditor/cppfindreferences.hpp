// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <core/core-search-result-window.hpp>
#include <cplusplus/FindUsages.h>

#include <QObject>
#include <QPointer>
#include <QFuture>

QT_FORWARD_DECLARE_CLASS(QTimer)

namespace Orca::Plugin::Core {
class SearchResultItem;
class SearchResult;
} // namespace Orca::Plugin::Core

namespace ProjectExplorer {
class Node;
}

namespace CppEditor {
class CppModelManager;

CPPEDITOR_EXPORT auto colorStyleForUsageType(CPlusPlus::Usage::Type type) -> Orca::Plugin::Core::SearchResultColor::Style;
CPPEDITOR_EXPORT auto renameFilesForSymbol(const QString &oldSymbolName, const QString &newSymbolName, const QVector<ProjectExplorer::Node*> &files) -> void;

class CPPEDITOR_EXPORT CppSearchResultFilter : public Orca::Plugin::Core::SearchResultFilter {
  auto createWidget() -> QWidget* override;
  auto matches(const Orca::Plugin::Core::SearchResultItem &item) const -> bool override;
  auto setValue(bool &member, bool value) -> void;

  bool m_showReads = true;
  bool m_showWrites = true;
  bool m_showDecls = true;
  bool m_showOther = true;
};

namespace Internal {

class CppFindReferencesParameters {
public:
  QList<QByteArray> symbolId;
  QByteArray symbolFileName;
  QString prettySymbolName;
  QVector<ProjectExplorer::Node*> filesToRename;
  bool categorize = false;
};

class CppFindReferences : public QObject {
  Q_OBJECT

public:
  explicit CppFindReferences(CppModelManager *modelManager);
  ~CppFindReferences() override;

  auto references(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) const -> QList<int>;
  auto findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) -> void;
  auto renameUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, const QString &replacement = QString()) -> void;
  auto findMacroUses(const CPlusPlus::Macro &macro) -> void;
  auto renameMacroUses(const CPlusPlus::Macro &macro, const QString &replacement = QString()) -> void;

private:
  auto onReplaceButtonClicked(const QString &text, const QList<Orca::Plugin::Core::SearchResultItem> &items, bool preserveCase) -> void;
  auto searchAgain() -> void;
  auto findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, const QString &replacement, bool replace) -> void;
  auto findMacroUses(const CPlusPlus::Macro &macro, const QString &replacement, bool replace) -> void;
  auto findAll_helper(Orca::Plugin::Core::SearchResult *search, CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, bool categorize) -> void;
  auto createWatcher(const QFuture<CPlusPlus::Usage> &future, Orca::Plugin::Core::SearchResult *search) -> void;
  auto findSymbol(const CppFindReferencesParameters &parameters, const CPlusPlus::Snapshot &snapshot, CPlusPlus::LookupContext *context) -> CPlusPlus::Symbol*;

private:
  QPointer<CppModelManager> m_modelManager;
};

} // namespace Internal
} // namespace CppEditor

Q_DECLARE_METATYPE(CppEditor::Internal::CppFindReferencesParameters)
