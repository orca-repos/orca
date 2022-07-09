// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewparsertreeitem.hpp"
#include "classviewsymbollocation.hpp"
#include "classviewsymbolinformation.hpp"
#include "classviewconstants.hpp"
#include "classviewutils.hpp"

#include <cplusplus/Icons.h>
#include <cplusplus/Name.h>
#include <cplusplus/Overview.h>
#include <cplusplus/Symbol.h>
#include <cplusplus/Symbols.h>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/session.hpp>
#include <utils/algorithm.hpp>

#include <QHash>
#include <QPair>
#include <QIcon>
#include <QStandardItem>

#include <QDebug>

namespace ClassView {
namespace Internal {

static CPlusPlus::Overview g_overview;

///////////////////////////////// ParserTreeItemPrivate //////////////////////////////////

/*!
    \class ParserTreeItemPrivate
    \brief The ParserTreeItemPrivate class defines private class data for
    the ParserTreeItem class.
   \sa ParserTreeItem
 */
class ParserTreeItemPrivate {
public:
  auto mergeWith(const ParserTreeItem::ConstPtr &target) -> void;
  auto mergeSymbol(const CPlusPlus::Symbol *symbol) -> void;
  auto cloneTree() const -> ParserTreeItem::ConstPtr;

  QHash<SymbolInformation, ParserTreeItem::ConstPtr> m_symbolInformations;
  QSet<SymbolLocation> m_symbolLocations;
  const Utils::FilePath m_projectFilePath;
};

auto ParserTreeItemPrivate::mergeWith(const ParserTreeItem::ConstPtr &target) -> void
{
  if (target.isNull())
    return;

  m_symbolLocations.unite(target->d->m_symbolLocations);

  // merge children
  for (auto it = target->d->m_symbolInformations.cbegin(); it != target->d->m_symbolInformations.cend(); ++it) {
    const auto &inf = it.key();
    const auto &targetChild = it.value();

    auto child = m_symbolInformations.value(inf);
    if (!child.isNull()) {
      child->d->mergeWith(targetChild);
    } else {
      const auto clone = targetChild.isNull() ? ParserTreeItem::ConstPtr() : targetChild->d->cloneTree();
      m_symbolInformations.insert(inf, clone);
    }
  }
}

auto ParserTreeItemPrivate::mergeSymbol(const CPlusPlus::Symbol *symbol) -> void
{
  if (!symbol)
    return;

  // easy solution - lets add any scoped symbol and
  // any symbol which does not contain :: in the name

  //! \todo collect statistics and reorder to optimize
  if (symbol->isForwardClassDeclaration() || symbol->isExtern() || symbol->isFriend() || symbol->isGenerated() || symbol->isUsingNamespaceDirective() || symbol->isUsingDeclaration())
    return;

  const CPlusPlus::Name *symbolName = symbol->name();
  if (symbolName && symbolName->isQualifiedNameId())
    return;

  const QString name = g_overview.prettyName(symbolName).trimmed();
  const QString type = g_overview.prettyType(symbol->type()).trimmed();
  const int iconType = CPlusPlus::Icons::iconTypeForSymbol(symbol);

  const SymbolInformation information(name, type, iconType);

  // If next line will be removed, 5% speed up for the initial parsing.
  // But there might be a problem for some files ???
  // Better to improve qHash timing
  auto childItem = m_symbolInformations.value(information);

  if (childItem.isNull())
    childItem = ParserTreeItem::ConstPtr(new ParserTreeItem());

  // locations have 1-based column in Symbol, use the same here.
  const SymbolLocation location(QString::fromUtf8(symbol->fileName(), symbol->fileNameLength()), symbol->line(), symbol->column());

  childItem->d->m_symbolLocations.insert(location);

  // prevent showing a content of the functions
  if (!symbol->isFunction()) {
    if (const CPlusPlus::Scope *scope = symbol->asScope()) {
      CPlusPlus::Scope::iterator cur = scope->memberBegin();
      CPlusPlus::Scope::iterator last = scope->memberEnd();
      while (cur != last) {
        const CPlusPlus::Symbol *curSymbol = *cur;
        ++cur;
        if (!curSymbol)
          continue;

        childItem->d->mergeSymbol(curSymbol);
      }
    }
  }

  // if item is empty and has not to be added
  if (!symbol->isNamespace() || childItem->childCount())
    m_symbolInformations.insert(information, childItem);
}

/*!
    Creates a deep clone of this tree.
*/
auto ParserTreeItemPrivate::cloneTree() const -> ParserTreeItem::ConstPtr
{
  ParserTreeItem::ConstPtr newItem(new ParserTreeItem(m_projectFilePath));
  newItem->d->m_symbolLocations = m_symbolLocations;

  for (auto it = m_symbolInformations.cbegin(); it != m_symbolInformations.cend(); ++it) {
    auto child = it.value();
    if (child.isNull())
      continue;
    newItem->d->m_symbolInformations.insert(it.key(), child->d->cloneTree());
  }

  return newItem;
}

///////////////////////////////// ParserTreeItem //////////////////////////////////

/*!
    \class ParserTreeItem
    \brief The ParserTreeItem class is an item for the internal Class View tree.

    Not virtual - to speed up its work.
*/

ParserTreeItem::ParserTreeItem() : d(new ParserTreeItemPrivate()) {}

ParserTreeItem::ParserTreeItem(const Utils::FilePath &projectFilePath) : d(new ParserTreeItemPrivate({{}, {}, projectFilePath})) {}

ParserTreeItem::ParserTreeItem(const QHash<SymbolInformation, ConstPtr> &children) : d(new ParserTreeItemPrivate({children, {}, {}})) {}

ParserTreeItem::~ParserTreeItem()
{
  delete d;
}

auto ParserTreeItem::projectFilePath() const -> Utils::FilePath
{
  return d->m_projectFilePath;
}

/*!
    Gets information about symbol positions.
    \sa SymbolLocation, addSymbolLocation, removeSymbolLocation
*/

auto ParserTreeItem::symbolLocations() const -> QSet<SymbolLocation>
{
  return d->m_symbolLocations;
}

/*!
    Returns the child item specified by \a inf symbol information.
*/

auto ParserTreeItem::child(const SymbolInformation &inf) const -> ParserTreeItem::ConstPtr
{
  return d->m_symbolInformations.value(inf);
}

/*!
    Returns the amount of children of the tree item.
*/

auto ParserTreeItem::childCount() const -> int
{
  return d->m_symbolInformations.count();
}

auto ParserTreeItem::parseDocument(const CPlusPlus::Document::Ptr &doc) -> ParserTreeItem::ConstPtr
{
  ConstPtr item(new ParserTreeItem());

  const unsigned total = doc->globalSymbolCount();
  for (unsigned i = 0; i < total; ++i)
    item->d->mergeSymbol(doc->globalSymbolAt(i));

  return item;
}

auto ParserTreeItem::mergeTrees(const Utils::FilePath &projectFilePath, const QList<ConstPtr> &docTrees) -> ParserTreeItem::ConstPtr
{
  ConstPtr item(new ParserTreeItem(projectFilePath));
  for (const auto &docTree : docTrees)
    item->d->mergeWith(docTree);

  return item;
}

/*!
    Converts internal location container to QVariant compatible.
    \a locations specifies a set of symbol locations.
    Returns a list of variant locations that can be added to the data of an
    item.
*/

static auto locationsToRole(const QSet<SymbolLocation> &locations) -> QList<QVariant>
{
  QList<QVariant> locationsVar;
  for (const auto &loc : locations)
    locationsVar.append(QVariant::fromValue(loc));

  return locationsVar;
}

/*!
    Checks \a item in a QStandardItemModel for lazy data population.
    Make sure this method is called only from the GUI thread.
*/
auto ParserTreeItem::canFetchMore(QStandardItem *item) const -> bool
{
  if (!item)
    return false;
  return item->rowCount() < d->m_symbolInformations.count();
}

/*!
    Appends this item to the QStandardIten item \a item.
    Make sure this method is called only from the GUI thread.
*/
auto ParserTreeItem::fetchMore(QStandardItem *item) const -> void
{
  using ProjectExplorer::SessionManager;
  if (!item)
    return;

  // convert to map - to sort it
  QMap<SymbolInformation, ConstPtr> map;
  for (auto it = d->m_symbolInformations.cbegin(); it != d->m_symbolInformations.cend(); ++it)
    map.insert(it.key(), it.value());

  for (auto it = map.cbegin(); it != map.cend(); ++it) {
    const auto &inf = it.key();
    auto ptr = it.value();

    const auto add = new QStandardItem;
    add->setData(inf.name(), Constants::SymbolNameRole);
    add->setData(inf.type(), Constants::SymbolTypeRole);
    add->setData(inf.iconType(), Constants::IconTypeRole);

    if (!ptr.isNull()) {
      // icon
      const Utils::FilePath &filePath = ptr->projectFilePath();
      if (!filePath.isEmpty()) {
        ProjectExplorer::Project *project = SessionManager::projectForFile(filePath);
        if (project)
          add->setIcon(project->containerNode()->icon());
      }

      // draggable
      if (!ptr->symbolLocations().isEmpty())
        add->setFlags(add->flags() | Qt::ItemIsDragEnabled);

      // locations
      add->setData(locationsToRole(ptr->symbolLocations()), Constants::SymbolLocationsRole);
    }
    item->appendRow(add);
  }
}

/*!
    Debug dump.
*/

auto ParserTreeItem::debugDump(int indent) const -> void
{
  for (auto it = d->m_symbolInformations.cbegin(); it != d->m_symbolInformations.cend(); ++it) {
    const auto &inf = it.key();
    const auto &child = it.value();
    qDebug() << QString(2 * indent, QLatin1Char(' ')) << inf.iconType() << inf.name() << inf.type() << child.isNull();
    if (!child.isNull())
      child->debugDump(indent + 1);
  }
}

} // namespace Internal
} // namespace ClassView

