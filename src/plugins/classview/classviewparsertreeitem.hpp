// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "classviewsymbollocation.hpp"
#include "classviewsymbolinformation.hpp"

#include <cplusplus/CppDocument.h>

#include <QSharedPointer>
#include <QHash>

QT_FORWARD_DECLARE_CLASS(QStandardItem)

namespace ClassView {
namespace Internal {

class ParserTreeItemPrivate;

class ParserTreeItem {
public:
  using ConstPtr = QSharedPointer<const ParserTreeItem>;

  ParserTreeItem();
  ParserTreeItem(const Utils::FilePath &projectFilePath);
  ParserTreeItem(const QHash<SymbolInformation, ConstPtr> &children);
  ~ParserTreeItem();

  static auto parseDocument(const CPlusPlus::Document::Ptr &doc) -> ConstPtr;
  static auto mergeTrees(const Utils::FilePath &projectFilePath, const QList<ConstPtr> &docTrees) -> ConstPtr;
  auto projectFilePath() const -> Utils::FilePath;
  auto symbolLocations() const -> QSet<SymbolLocation>;
  auto child(const SymbolInformation &inf) const -> ConstPtr;
  auto childCount() const -> int;
  // Make sure that below two methods are called only from the GUI thread
  auto canFetchMore(QStandardItem *item) const -> bool;
  auto fetchMore(QStandardItem *item) const -> void;
  auto debugDump(int indent = 0) const -> void;

private:
  friend class ParserTreeItemPrivate;
  ParserTreeItemPrivate *d;
};

} // namespace Internal
} // namespace ClassView

Q_DECLARE_METATYPE(ClassView::Internal::ParserTreeItem::ConstPtr)
