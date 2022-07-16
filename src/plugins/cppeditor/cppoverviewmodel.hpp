// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractoverviewmodel.hpp"

#include <cplusplus/CppDocument.h>
#include <cplusplus/Overview.h>

namespace CppEditor::Internal {
class SymbolItem;

class OverviewModel : public AbstractOverviewModel {
  Q_OBJECT

public:
  auto rebuild(CPlusPlus::Document::Ptr doc) -> void override;
  auto isGenerated(const QModelIndex &sourceIndex) const -> bool override;
  auto linkFromIndex(const QModelIndex &sourceIndex) const -> Utils::Link override;
  auto lineColumnFromIndex(const QModelIndex &sourceIndex) const -> Utils::LineColumn override;
  auto rangeFromIndex(const QModelIndex &sourceIndex) const -> Range override;

private:
  auto symbolFromIndex(const QModelIndex &index) const -> CPlusPlus::Symbol*;
  auto hasDocument() const -> bool;
  auto globalSymbolCount() const -> int;
  auto globalSymbolAt(int index) const -> CPlusPlus::Symbol*;
  auto buildTree(SymbolItem *root, bool isRoot) -> void;
  
  CPlusPlus::Document::Ptr _cppDocument;
  CPlusPlus::Overview _overview;

  friend class SymbolItem;
};

} // namespace CppEditor::Internal
