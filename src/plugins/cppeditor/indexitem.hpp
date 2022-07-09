// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QIcon>
#include <QSharedPointer>
#include <QMetaType>

#include <functional>

namespace CppEditor {

class CPPEDITOR_EXPORT IndexItem {
  Q_DISABLE_COPY(IndexItem)

  IndexItem() = default;

public:
  enum ItemType {
    Enum = 1 << 0,
    Class = 1 << 1,
    Function = 1 << 2,
    Declaration = 1 << 3,
    All = Enum | Class | Function | Declaration
  };
  
  using Ptr = QSharedPointer<IndexItem>;

  static auto create(const QString &symbolName, const QString &symbolType, const QString &symbolScope, ItemType type, const QString &fileName, int line, int column, const QIcon &icon) -> Ptr;
  static auto create(const QString &fileName, int sizeHint) -> Ptr;

  auto scopedSymbolName() const -> QString
  {
    return m_symbolScope.isEmpty() ? m_symbolName : m_symbolScope + QLatin1String("::") + m_symbolName;
  }

  auto unqualifiedNameAndScope(const QString &defaultName, QString *name, QString *scope) const -> bool;
  auto representDeclaration() const -> QString;
  auto shortNativeFilePath() const -> QString;
  auto symbolName() const -> QString { return m_symbolName; }
  auto symbolType() const -> QString { return m_symbolType; }
  auto symbolScope() const -> QString { return m_symbolScope; }
  auto fileName() const -> QString { return m_fileName; }
  auto icon() const -> QIcon { return m_icon; }
  auto type() const -> ItemType { return m_type; }
  auto line() const -> int { return m_line; }
  auto column() const -> int { return m_column; }
  auto addChild(Ptr childItem) -> void { m_children.append(childItem); }
  auto squeeze() -> void;

  enum VisitorResult {
    Break,
    /// terminates traversal
    Continue,
    /// continues traversal with the next sibling
    Recurse,
    /// continues traversal with the children
  };

  using Visitor = std::function<VisitorResult (const Ptr &)>;

  auto visitAllChildren(Visitor callback) const -> VisitorResult
  {
    auto result = Recurse;
    foreach(const IndexItem::Ptr &child, m_children) {
      result = callback(child);
      switch (result) {
      case Break:
        return Break;
      case Continue:
        continue;
      case Recurse:
        if (!child->m_children.isEmpty()) {
          result = child->visitAllChildren(callback);
          if (result == Break)
            return Break;
        }
      }
    }
    return result;
  }

private:
  QString m_symbolName; // as found in the code, therefore might be qualified
  QString m_symbolType;
  QString m_symbolScope;
  QString m_fileName;
  QIcon m_icon;
  ItemType m_type = All;
  int m_line = 0;
  int m_column = 0;
  QVector<Ptr> m_children;
};

} // CppEditor namespace

Q_DECLARE_METATYPE(CppEditor::IndexItem::Ptr)
