// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "indexitem.hpp"

#include <utils/fileutils.hpp>

namespace CppEditor {

auto IndexItem::create(const QString &symbolName, const QString &symbolType, const QString &symbolScope, IndexItem::ItemType type, const QString &fileName, int line, int column, const QIcon &icon) -> IndexItem::Ptr
{
  Ptr ptr(new IndexItem);

  ptr->m_symbolName = symbolName;
  ptr->m_symbolType = symbolType;
  ptr->m_symbolScope = symbolScope;
  ptr->m_type = type;
  ptr->m_fileName = fileName;
  ptr->m_line = line;
  ptr->m_column = column;
  ptr->m_icon = icon;

  return ptr;
}

auto IndexItem::create(const QString &fileName, int sizeHint) -> IndexItem::Ptr
{
  Ptr ptr(new IndexItem);

  ptr->m_fileName = fileName;
  ptr->m_type = Declaration;
  ptr->m_line = 0;
  ptr->m_column = 0;
  ptr->m_children.reserve(sizeHint);

  return ptr;
}

auto IndexItem::unqualifiedNameAndScope(const QString &defaultName, QString *name, QString *scope) const -> bool
{
  *name = defaultName;
  *scope = m_symbolScope;
  const auto qualifiedName = scopedSymbolName();
  const int colonColonPosition = qualifiedName.lastIndexOf(QLatin1String("::"));
  if (colonColonPosition != -1) {
    *name = qualifiedName.mid(colonColonPosition + 2);
    *scope = qualifiedName.left(colonColonPosition);
    return true;
  }
  return false;
}

auto IndexItem::representDeclaration() const -> QString
{
  if (m_symbolType.isEmpty())
    return QString();

  const auto padding = m_symbolType.endsWith(QLatin1Char('*')) ? QString() : QString(QLatin1Char(' '));
  return m_symbolType + padding + m_symbolName;
}

auto IndexItem::shortNativeFilePath() const -> QString
{
  return Utils::FilePath::fromString(m_fileName).shortNativePath();
}

auto IndexItem::squeeze() -> void
{
  m_children.squeeze();
  for (int i = 0, ei = m_children.size(); i != ei; ++i)
    m_children[i]->squeeze();
}

} // CppEditor namespace
