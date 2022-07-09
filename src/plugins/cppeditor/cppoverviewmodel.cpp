// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppoverviewmodel.hpp"

#include <cplusplus/Icons.h>
#include <cplusplus/Literals.h>
#include <cplusplus/Overview.h>
#include <cplusplus/Scope.h>
#include <cplusplus/Symbols.h>

#include <utils/linecolumn.hpp>
#include <utils/link.hpp>

using namespace CPlusPlus;
namespace CppEditor::Internal {

class SymbolItem : public Utils::TreeItem {
public:
  SymbolItem() = default;
  explicit SymbolItem(CPlusPlus::Symbol *symbol) : symbol(symbol) {}

  auto data(int column, int role) const -> QVariant override
  {
    Q_UNUSED(column)

    if (!symbol && parent()) {
      // account for no symbol item
      switch (role) {
      case Qt::DisplayRole:
        if (parent()->childCount() > 1)
          return QString(QT_TRANSLATE_NOOP("CppEditor::OverviewModel", "<Select Symbol>"));
        return QString(QT_TRANSLATE_NOOP("CppEditor::OverviewModel", "<No Symbols>"));
      default:
        return QVariant();
      }
    }

    auto overviewModel = qobject_cast<const OverviewModel*>(model());
    if (!symbol || !overviewModel)
      return QVariant();

    switch (role) {
    case Qt::DisplayRole: {
      QString name = overviewModel->_overview.prettyName(symbol->name());
      if (name.isEmpty())
        name = QLatin1String("anonymous");
      if (symbol->isObjCForwardClassDeclaration())
        name = QLatin1String("@class ") + name;
      if (symbol->isObjCForwardProtocolDeclaration() || symbol->isObjCProtocol())
        name = QLatin1String("@protocol ") + name;
      if (symbol->isObjCClass()) {
        ObjCClass *clazz = symbol->asObjCClass();
        if (clazz->isInterface())
          name = QLatin1String("@interface ") + name;
        else
          name = QLatin1String("@implementation ") + name;

        if (clazz->isCategory()) {
          name += QString(" (%1)").arg(overviewModel->_overview.prettyName(clazz->categoryName()));
        }
      }
      if (symbol->isObjCPropertyDeclaration())
        name = QLatin1String("@property ") + name;
      // if symbol is a template we might change it now - so, use a copy instead as we're const
      Symbol *symbl = symbol;
      if (Template *t = symbl->asTemplate())
        if (Symbol *templateDeclaration = t->declaration()) {
          QStringList parameters;
          parameters.reserve(t->templateParameterCount());
          for (auto i = 0; i < t->templateParameterCount(); ++i) {
            parameters.append(overviewModel->_overview.prettyName(t->templateParameterAt(i)->name()));
          }
          name += QString("<%1>").arg(parameters.join(QLatin1String(", ")));
          symbl = templateDeclaration;
        }
      if (symbl->isObjCMethod()) {
        ObjCMethod *method = symbl->asObjCMethod();
        if (method->isStatic())
          name = QLatin1Char('+') + name;
        else
          name = QLatin1Char('-') + name;
      } else if (! symbl->isScope() || symbl->isFunction()) {
        QString type = overviewModel->_overview.prettyType(symbl->type());
        if (Function *f = symbl->type()->asFunctionType()) {
          name += type;
          type = overviewModel->_overview.prettyType(f->returnType());
        }
        if (! type.isEmpty())
          name += QLatin1String(": ") + type;
      }
      return name;
    }

    case Qt::EditRole: {
      QString name = overviewModel->_overview.prettyName(symbol->name());
      if (name.isEmpty())
        name = QLatin1String("anonymous");
      return name;
    }

    case Qt::DecorationRole:
      return Icons::iconForSymbol(symbol);

    case AbstractOverviewModel::FileNameRole:
      return QString::fromUtf8(symbol->fileName(), symbol->fileNameLength());

    case AbstractOverviewModel::LineNumberRole:
      return symbol->line();

    default:
      return QVariant();
    } // switch
  }

  CPlusPlus::Symbol *symbol = nullptr; // not owned
};

auto OverviewModel::hasDocument() const -> bool
{
  return !_cppDocument.isNull();
}

auto OverviewModel::globalSymbolCount() const -> int
{
  auto count = 0;
  if (_cppDocument)
    count += _cppDocument->globalSymbolCount();
  return count;
}

auto OverviewModel::globalSymbolAt(int index) const -> Symbol* { return _cppDocument->globalSymbolAt(index); }

auto OverviewModel::symbolFromIndex(const QModelIndex &index) const -> Symbol*
{
  if (!index.isValid())
    return nullptr;
  auto item = static_cast<const SymbolItem*>(itemForIndex(index));
  return item ? item->symbol : nullptr;
}

auto OverviewModel::rebuild(Document::Ptr doc) -> void
{
  beginResetModel();
  _cppDocument = doc;
  auto root = new SymbolItem;
  buildTree(root, true);
  setRootItem(root);
  endResetModel();
}

auto OverviewModel::isGenerated(const QModelIndex &sourceIndex) const -> bool
{
  CPlusPlus::Symbol *symbol = symbolFromIndex(sourceIndex);
  return symbol && symbol->isGenerated();
}

auto OverviewModel::linkFromIndex(const QModelIndex &sourceIndex) const -> Utils::Link
{
  CPlusPlus::Symbol *symbol = symbolFromIndex(sourceIndex);
  if (!symbol)
    return {};

  return symbol->toLink();
}

auto OverviewModel::lineColumnFromIndex(const QModelIndex &sourceIndex) const -> Utils::LineColumn
{
  Utils::LineColumn lineColumn;
  CPlusPlus::Symbol *symbol = symbolFromIndex(sourceIndex);
  if (!symbol)
    return lineColumn;
  lineColumn.line = symbol->line();
  lineColumn.column = symbol->column();
  return lineColumn;
}

auto OverviewModel::rangeFromIndex(const QModelIndex &sourceIndex) const -> OverviewModel::Range
{
  auto lineColumn = lineColumnFromIndex(sourceIndex);
  return std::make_pair(lineColumn, lineColumn);
}

auto OverviewModel::buildTree(SymbolItem *root, bool isRoot) -> void
{
  if (!root)
    return;

  if (isRoot) {
    auto rows = globalSymbolCount();
    for (auto row = 0; row < rows; ++row) {
      Symbol *symbol = globalSymbolAt(row);
      auto currentItem = new SymbolItem(symbol);
      buildTree(currentItem, false);
      root->appendChild(currentItem);
    }
    root->prependChild(new SymbolItem); // account for no symbol item
  } else {
    Symbol *symbol = root->symbol;
    if (Scope *scope = symbol->asScope()) {
      Scope::iterator it = scope->memberBegin();
      Scope::iterator end = scope->memberEnd();
      for (; it != end; ++it) {
        if (!((*it)->name()))
          continue;
        if ((*it)->asArgument())
          continue;
        auto currentItem = new SymbolItem(*it);
        buildTree(currentItem, false);
        root->appendChild(currentItem);
      }
    }
  }
}

} // namespace CppEditor::Internal
