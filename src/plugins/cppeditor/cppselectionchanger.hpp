// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <cplusplus/ASTPath.h>
#include <cplusplus/CppDocument.h>
#include <cplusplus/TranslationUnit.h>

#include <QObject>
#include <QTextCursor>

namespace CppEditor {

class ASTNodePositions {
public:
  ASTNodePositions() = default;
  explicit ASTNodePositions(CPlusPlus::AST *_ast) : ast(_ast) {}

  operator bool() const { return ast; }

  CPlusPlus::AST *ast = nullptr;
  unsigned firstTokenIndex = 0;
  unsigned lastTokenIndex = 0;
  unsigned secondToLastTokenIndex = 0;
  int astPosStart = -1;
  int astPosEnd = -1;
};

class CPPEDITOR_EXPORT CppSelectionChanger : public QObject {
  Q_OBJECT

public:
  explicit CppSelectionChanger(QObject *parent = nullptr);

  enum Direction {
    ExpandSelection,
    ShrinkSelection
  };

  enum NodeIndexAndStepState {
    NodeIndexAndStepNotSet,
    NodeIndexAndStepWholeDocument,
  };

  auto changeSelection(Direction direction, QTextCursor &cursorToModify, const CPlusPlus::Document::Ptr doc) -> bool;
  auto startChangeSelection() -> void;
  auto stopChangeSelection() -> void;

public slots:
  void onCursorPositionChanged(const QTextCursor &newCursor);

protected slots:
  void fineTuneForStatementPositions(unsigned firstParensTokenIndex, unsigned lastParensTokenIndex, ASTNodePositions &positions) const;

private:
  auto performSelectionChange(QTextCursor &cursorToModify) -> bool;
  auto getASTPositions(CPlusPlus::AST *ast, const QTextCursor &cursor) const -> ASTNodePositions;
  auto updateCursorSelection(QTextCursor &cursorToModify, ASTNodePositions positions) -> void;
  auto possibleASTStepCount(CPlusPlus::AST *ast) const -> int;
  auto currentASTStep() const -> int;
  auto findNextASTStepPositions(const QTextCursor &cursor) -> ASTNodePositions;
  auto fineTuneASTNodePositions(ASTNodePositions &positions) const -> void;
  auto getFineTunedASTPositions(CPlusPlus::AST *ast, const QTextCursor &cursor) const -> ASTNodePositions;
  auto getFirstCurrentStepForASTNode(CPlusPlus::AST *ast) const -> int;
  auto isLastPossibleStepForASTNode(CPlusPlus::AST *ast) const -> bool;
  auto findRelevantASTPositionsFromCursor(const QList<CPlusPlus::AST*> &astPath, const QTextCursor &cursor, int startingFromNodeIndex = -1) -> ASTNodePositions;
  auto findRelevantASTPositionsFromCursorWhenNodeIndexNotSet(const QList<CPlusPlus::AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions;
  auto findRelevantASTPositionsFromCursorWhenWholeDocumentSelected(const QList<CPlusPlus::AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions;
  auto findRelevantASTPositionsFromCursorFromPreviousNodeIndex(const QList<CPlusPlus::AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions;
  auto shouldSkipASTNodeBasedOnPosition(const ASTNodePositions &positions, const QTextCursor &cursor) const -> bool;
  auto setNodeIndexAndStep(NodeIndexAndStepState state) -> void;
  auto getTokenStartCursorPosition(unsigned tokenIndex, const QTextCursor &cursor) const -> int;
  auto getTokenEndCursorPosition(unsigned tokenIndex, const QTextCursor &cursor) const -> int;
  auto printTokenDebugInfo(unsigned tokenIndex, const QTextCursor &cursor, QString prefix) const -> void;

  QTextCursor m_initialChangeSelectionCursor;
  QTextCursor m_workingCursor;
  CPlusPlus::Document::Ptr m_doc;
  CPlusPlus::TranslationUnit *m_unit = nullptr;
  Direction m_direction = ExpandSelection;
  int m_changeSelectionNodeIndex = -1;
  int m_nodeCurrentStep = -1;
  bool m_inChangeSelection = false;
};

} // namespace CppEditor
