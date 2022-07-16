// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppquickfix.hpp"
#include "cpprefactoringchanges.hpp"

#include <QString>
#include <QCoreApplication>
#include <QSharedPointer>
#include <QFutureWatcher>
#include <QTextCursor>

namespace CppEditor {
class CppEditorWidget;

namespace Internal {
class FunctionDeclDefLink;

class FunctionDeclDefLinkFinder : public QObject {
  Q_OBJECT

public:
  FunctionDeclDefLinkFinder(QObject *parent = nullptr);

  auto startFindLinkAt(QTextCursor cursor, const CPlusPlus::Document::Ptr &doc, const CPlusPlus::Snapshot &snapshot) -> void;
  auto scannedSelection() const -> QTextCursor;

signals:
  auto foundLink(QSharedPointer<FunctionDeclDefLink> link) -> void;

private:
  auto onFutureDone() -> void;

  QTextCursor m_scannedSelection;
  QTextCursor m_nameSelection;
  QScopedPointer<QFutureWatcher<QSharedPointer<FunctionDeclDefLink>>> m_watcher;
};

class FunctionDeclDefLink {
  Q_DECLARE_TR_FUNCTIONS(CppEditor::Internal::FunctionDeclDefLink)
  Q_DISABLE_COPY(FunctionDeclDefLink)
  FunctionDeclDefLink() = default;

public:
  auto isValid() const -> bool;
  auto isMarkerVisible() const -> bool;
  auto apply(CppEditorWidget *editor, bool jumpToMatch) -> void;
  auto hideMarker(CppEditorWidget *editor) -> void;
  auto showMarker(CppEditorWidget *editor) -> void;
  auto changes(const CPlusPlus::Snapshot &snapshot, int targetOffset = -1) -> Utils::ChangeSet;

  QTextCursor linkSelection;
  // stored to allow aborting when the name is changed
  QTextCursor nameSelection;
  QString nameInitial;

  // The 'source' prefix denotes information about the original state
  // of the function before the user did any edits.
  CPlusPlus::Document::Ptr sourceDocument;
  CPlusPlus::Function *sourceFunction = nullptr;
  CPlusPlus::DeclarationAST *sourceDeclaration = nullptr;
  CPlusPlus::FunctionDeclaratorAST *sourceFunctionDeclarator = nullptr;

  // The 'target' prefix denotes information about the remote declaration matching
  // the 'source' declaration, where we will try to apply the user changes.
  // 1-based line and column
  int targetLine = 0;
  int targetColumn = 0;
  QString targetInitial;

  CppRefactoringFileConstPtr targetFile;
  CPlusPlus::Function *targetFunction = nullptr;
  CPlusPlus::DeclarationAST *targetDeclaration = nullptr;
  CPlusPlus::FunctionDeclaratorAST *targetFunctionDeclarator = nullptr;

private:
  auto normalizedInitialName() const -> QString;
  bool hasMarker = false;

  friend class FunctionDeclDefLinkFinder;
};

} // namespace Internal
} // namespace CppEditor
