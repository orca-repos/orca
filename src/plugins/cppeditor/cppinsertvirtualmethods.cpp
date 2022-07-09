// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppinsertvirtualmethods.hpp"

#include "cppcodestylesettings.hpp"
#include "cppquickfixassistant.hpp"
#include "cpptoolsreuse.hpp"
#include "functionutils.hpp"
#include "insertionpointlocator.hpp"

#include <core/icore.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <cplusplus/CppRewriter.h>
#include <cplusplus/Overview.h>

#include <utils/algorithm.hpp>
#include <utils/changeset.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPointer>
#include <QQueue>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#ifdef WITH_TESTS
#include "cppeditorplugin.hpp"
#include "cppquickfix_test.hpp"
#include <QtTest>
#endif

using namespace CPlusPlus;
using namespace TextEditor;

namespace CppEditor {
namespace Internal {

class InsertVirtualMethodsModel;
class VirtualMethodsSettings;

class InsertVirtualMethodsDialog : public QDialog {
  Q_OBJECT

public:
  enum CustomItemRoles {
    Reimplemented = Qt::UserRole
  };

  enum ImplementationMode {
    ModeOnlyDeclarations = 0x00000001,
    ModeInsideClass = 0x00000002,
    ModeOutsideClass = 0x00000004,
    ModeImplementationFile = 0x00000008
  };

  InsertVirtualMethodsDialog(QWidget *parent = nullptr);
  ~InsertVirtualMethodsDialog() override;

  auto initGui() -> void;
  auto initData() -> void;
  virtual auto saveSettings() -> void;
  auto settings() const -> const VirtualMethodsSettings*;
  auto setHasImplementationFile(bool file) -> void;
  auto setHasReimplementedFunctions(bool functions) -> void;
  virtual auto gather() -> bool;

protected:
  auto setInsertOverrideReplacement(bool insert) -> void;
  auto setOverrideReplacement(const QString &replacements) -> void;

private:
  auto setHideReimplementedFunctions(bool hide) -> void;
  auto updateOverrideReplacementsComboBox() -> void;
  
  QTreeView *m_view = nullptr;
  QLineEdit *m_filter = nullptr;
  QCheckBox *m_hideReimplementedFunctions = nullptr;
  QComboBox *m_insertMode = nullptr;
  QCheckBox *m_virtualKeyword = nullptr;
  QCheckBox *m_overrideReplacementCheckBox = nullptr;
  QComboBox *m_overrideReplacementComboBox = nullptr;
  QToolButton *m_clearUserAddedReplacementsButton = nullptr;
  QDialogButtonBox *m_buttons = nullptr;
  QList<bool> m_expansionStateNormal;
  QList<bool> m_expansionStateReimp;
  QStringList m_availableOverrideReplacements;
  bool m_hasImplementationFile = false;
  bool m_hasReimplementedFunctions = false;

protected:
  VirtualMethodsSettings *m_settings;

  auto saveExpansionState() -> void;
  auto restoreExpansionState() -> void;

public:
  InsertVirtualMethodsModel *classFunctionModel;
  QSortFilterProxyModel *classFunctionFilterModel;
};

} // namespace Internal
} // namespace CppEditor

Q_DECLARE_METATYPE(CppEditor::Internal::InsertVirtualMethodsDialog::ImplementationMode)

namespace {

class InsertVirtualMethodsItem {
public:
  InsertVirtualMethodsItem(InsertVirtualMethodsItem *parent) : m_parent(parent) {}

  virtual ~InsertVirtualMethodsItem() = default;
  virtual auto description() const -> QString = 0;
  virtual auto flags() const -> Qt::ItemFlags = 0;
  virtual auto checkState() const -> Qt::CheckState = 0;

  auto parent() -> InsertVirtualMethodsItem* { return m_parent; }

  int row = -1;

private:
  InsertVirtualMethodsItem *m_parent = nullptr;
};

class FunctionItem;

class ClassItem : public InsertVirtualMethodsItem {
public:
  ClassItem(const QString &className, const Class *clazz);
  ~ClassItem() override;

  auto description() const -> QString override { return name; }
  auto flags() const -> Qt::ItemFlags override;
  auto checkState() const -> Qt::CheckState override;
  auto removeFunction(int row) -> void;

  const Class *klass;
  const QString name;
  QList<FunctionItem*> functions;
};

class FunctionItem : public InsertVirtualMethodsItem {
public:
  FunctionItem(const Function *func, const QString &functionName, ClassItem *parent);
  auto description() const -> QString override;
  auto flags() const -> Qt::ItemFlags override;
  auto checkState() const -> Qt::CheckState override { return checked ? Qt::Checked : Qt::Unchecked; }

  const Function *function = nullptr;
  CppEditor::InsertionPointLocator::AccessSpec accessSpec = CppEditor::InsertionPointLocator::Invalid;
  bool reimplemented = false;
  bool alreadyFound = false;
  bool checked = false;
  FunctionItem *nextOverride = nullptr;

private:
  QString name;
};

ClassItem::ClassItem(const QString &className, const Class *clazz) : InsertVirtualMethodsItem(nullptr), klass(clazz), name(className) {}

ClassItem::~ClassItem()
{
  qDeleteAll(functions);
  functions.clear();
}

auto ClassItem::flags() const -> Qt::ItemFlags
{
  foreach(FunctionItem *func, functions) {
    if (!func->alreadyFound)
      return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  }

  return Qt::ItemIsSelectable;
}

auto ClassItem::checkState() const -> Qt::CheckState
{
  if (functions.isEmpty())
    return Qt::Unchecked;
  auto state = functions.first()->checkState();
  foreach(FunctionItem *function, functions) {
    auto functionState = function->checkState();
    if (functionState != state)
      return Qt::PartiallyChecked;
  }
  return state;
}

auto ClassItem::removeFunction(int row) -> void
{
  QTC_ASSERT(row >= 0 && row < functions.count(), return);
  functions.removeAt(row);
  // Update row number for all the following functions
  for (int r = row, total = functions.count(); r < total; ++r)
    functions[r]->row = r;
}

FunctionItem::FunctionItem(const Function *func, const QString &functionName, ClassItem *parent) : InsertVirtualMethodsItem(parent), function(func), nextOverride(this)
{
  name = functionName;
}

auto FunctionItem::description() const -> QString
{
  return name;
}

auto FunctionItem::flags() const -> Qt::ItemFlags
{
  Qt::ItemFlags res = Qt::NoItemFlags;
  if (!alreadyFound)
    res |= Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  return res;
}

auto defaultOverrideReplacements() -> QStringList
{
  return {QLatin1String("override"), QLatin1String("Q_DECL_OVERRIDE")};
}

auto sortedAndTrimmedStringListWithoutEmptyElements(const QStringList &list) -> QStringList
{
  QStringList result;
  foreach(const QString &replacement, list) {
    const auto trimmedReplacement = replacement.trimmed();
    if (!trimmedReplacement.isEmpty())
      result << trimmedReplacement;
  }
  result.sort();
  return result;
}

} // namespace

namespace CppEditor {
namespace Internal {

const bool kInsertVirtualKeywordDefault = false;
const bool kHideReimplementedFunctionsDefault = false;
const bool kInsertOVerrideReplacementDefault = false;
const int kOverrideReplacementIndexDefault = 0;

const InsertVirtualMethodsDialog::ImplementationMode kImplementationModeDefault = InsertVirtualMethodsDialog::ModeOnlyDeclarations;

class VirtualMethodsSettings {
public:
  auto read() -> void
  {
    QSettings *s = Core::ICore::settings();
    s->beginGroup(group());
    insertVirtualKeyword = s->value(insertVirtualKeywordKey(), kInsertVirtualKeywordDefault).toBool();
    hideReimplementedFunctions = s->value(hideReimplementedFunctionsKey(), kHideReimplementedFunctionsDefault).toBool();
    insertOverrideReplacement = s->value(insertOverrideReplacementKey(), kInsertOVerrideReplacementDefault).toBool();
    overrideReplacementIndex = s->value(overrideReplacementIndexKey(), kOverrideReplacementIndexDefault).toInt();
    userAddedOverrideReplacements = s->value(userAddedOverrideReplacementsKey()).toStringList();
    implementationMode = static_cast<InsertVirtualMethodsDialog::ImplementationMode>(s->value(implementationModeKey(), int(kImplementationModeDefault)).toInt());
    s->endGroup();
  }

  auto write() const -> void
  {
    auto s = Core::ICore::settings();
    s->beginGroup(group());
    s->setValueWithDefault(insertVirtualKeywordKey(), insertVirtualKeyword, kInsertVirtualKeywordDefault);
    s->setValueWithDefault(hideReimplementedFunctionsKey(), hideReimplementedFunctions, kHideReimplementedFunctionsDefault);
    s->setValueWithDefault(insertOverrideReplacementKey(), insertOverrideReplacement, kInsertOVerrideReplacementDefault);
    s->setValueWithDefault(overrideReplacementIndexKey(), overrideReplacementIndex, kOverrideReplacementIndexDefault);
    s->setValueWithDefault(userAddedOverrideReplacementsKey(), userAddedOverrideReplacements);
    s->setValueWithDefault(implementationModeKey(), int(implementationMode), int(kImplementationModeDefault));
    s->endGroup();
  }

  QString overrideReplacement; // internal
  QStringList userAddedOverrideReplacements;
  InsertVirtualMethodsDialog::ImplementationMode implementationMode = kImplementationModeDefault;
  int overrideReplacementIndex = kOverrideReplacementIndexDefault;
  bool insertVirtualKeyword = kInsertVirtualKeywordDefault;
  bool hideReimplementedFunctions = kHideReimplementedFunctionsDefault;
  bool insertOverrideReplacement = kInsertOVerrideReplacementDefault;

private:
  using _ = QLatin1String;
  static auto group() -> QString { return _("QuickFix/InsertVirtualMethods"); }
  static auto insertVirtualKeywordKey() -> QString { return _("insertKeywordVirtual"); }
  static auto insertOverrideReplacementKey() -> QString { return _("insertOverrideReplacement"); }
  static auto overrideReplacementIndexKey() -> QString { return _("overrideReplacementIndex"); }
  static auto userAddedOverrideReplacementsKey() -> QString { return _("userAddedOverrideReplacements"); }
  static auto implementationModeKey() -> QString { return _("implementationMode"); }
  static auto hideReimplementedFunctionsKey() -> QString { return _("hideReimplementedFunctions"); }
};

class InsertVirtualMethodsModel : public QAbstractItemModel {
public:
  InsertVirtualMethodsModel(QObject *parent = nullptr) : QAbstractItemModel(parent)
  {
    const auto &fs = TextEditorSettings::fontSettings();
    formatReimpFunc = fs.formatFor(C_DISABLED_CODE);
  }

  ~InsertVirtualMethodsModel() override
  {
    clear();
  }

  auto clear() -> void
  {
    beginResetModel();
    qDeleteAll(classes);
    classes.clear();
    endResetModel();
  }

  auto index(int row, int column, const QModelIndex &parent) const -> QModelIndex override
  {
    if (column != 0)
      return {};
    if (parent.isValid()) {
      auto classItem = static_cast<ClassItem*>(parent.internalPointer());
      return createIndex(row, column, classItem->functions.at(row));
    }
    return createIndex(row, column, classes.at(row));
  }

  auto parent(const QModelIndex &child) const -> QModelIndex override
  {
    if (!child.isValid())
      return {};
    auto parent = itemForIndex(child)->parent();
    return parent ? createIndex(parent->row, 0, parent) : QModelIndex();
  }

  auto rowCount(const QModelIndex &parent) const -> int override
  {
    if (!parent.isValid())
      return classes.count();
    auto item = itemForIndex(parent);
    if (item->parent()) // function -> no children
      return 0;
    return static_cast<ClassItem*>(item)->functions.count();
  }

  auto columnCount(const QModelIndex &) const -> int override
  {
    return 1;
  }

  auto addClass(ClassItem *classItem) -> void
  {
    int row = classes.count();
    classItem->row = row;
    beginInsertRows(QModelIndex(), row, row);
    classes.append(classItem);
    endInsertRows();
  }

  auto removeFunction(FunctionItem *funcItem) -> void
  {
    auto classItem = static_cast<ClassItem*>(funcItem->parent());
    beginRemoveRows(createIndex(classItem->row, 0, classItem), funcItem->row, funcItem->row);
    classItem->removeFunction(funcItem->row);
    endRemoveRows();
  }

  auto data(const QModelIndex &index, int role) const -> QVariant override
  {
    if (!index.isValid())
      return QVariant();

    auto item = itemForIndex(index);
    switch (role) {
    case Qt::DisplayRole:
      return item->description();
    case Qt::CheckStateRole:
      return item->checkState();
    case Qt::ForegroundRole:
      if (item->parent() && static_cast<FunctionItem*>(item)->alreadyFound)
        return formatReimpFunc.foreground();
      break;
    case Qt::BackgroundRole:
      if (item->parent() && static_cast<FunctionItem*>(item)->alreadyFound) {
        const auto background = formatReimpFunc.background();
        if (background.isValid())
          return background;
      }
      break;
    case InsertVirtualMethodsDialog::Reimplemented:
      if (item->parent()) {
        auto function = static_cast<FunctionItem*>(item);
        return QVariant(function->alreadyFound);
      }

    }
    return QVariant();
  }

  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool override
  {
    if (!index.isValid())
      return false;

    auto item = itemForIndex(index);
    switch (role) {
    case Qt::CheckStateRole: {
      auto checked = value.toInt() == Qt::Checked;
      if (item->parent()) {
        auto funcItem = static_cast<FunctionItem*>(item);
        while (funcItem->checked != checked) {
          funcItem->checked = checked;
          const auto funcIndex = createIndex(funcItem->row, 0, funcItem);
          emit dataChanged(funcIndex, funcIndex);
          const auto parentIndex = createIndex(funcItem->parent()->row, 0, funcItem->parent());
          emit dataChanged(parentIndex, parentIndex);
          funcItem = funcItem->nextOverride;
        }
      } else {
        auto classItem = static_cast<ClassItem*>(item);
        foreach(FunctionItem *funcItem, classItem->functions) {
          if (funcItem->alreadyFound || funcItem->checked == checked)
            continue;
          auto funcIndex = createIndex(funcItem->row, 0, funcItem);
          setData(funcIndex, value, role);
        }
      }
      return true;
    }
    }
    return QAbstractItemModel::setData(index, value, role);
  }

  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override
  {
    if (!index.isValid())
      return Qt::NoItemFlags;
    return itemForIndex(index)->flags();
  }

  QList<ClassItem*> classes;

private:
  Format formatReimpFunc;

  auto itemForIndex(const QModelIndex &index) const -> InsertVirtualMethodsItem*
  {
    return static_cast<InsertVirtualMethodsItem*>(index.internalPointer());
  }
};

class InsertVirtualMethodsOp : public CppQuickFixOperation {
public:
  InsertVirtualMethodsOp(const CppQuickFixInterface &interface, InsertVirtualMethodsDialog *factory) : CppQuickFixOperation(interface, 0), m_factory(factory)
  {
    setDescription(QCoreApplication::translate("CppEditor::QuickFix", "Insert Virtual Functions of Base Classes"));

    const QList<AST*> &path = interface.path();
    const int pathSize = path.size();
    if (pathSize < 2)
      return;

    // Determine if cursor is on a class or a base class
    if (SimpleNameAST *nameAST = path.at(pathSize - 1)->asSimpleName()) {
      if (!interface.isCursorOn(nameAST))
        return;

      if (!(m_classAST = path.at(pathSize - 2)->asClassSpecifier())) {
        // normal class
        int index = pathSize - 2;
        const BaseSpecifierAST *baseAST = path.at(index)->asBaseSpecifier(); // simple bclass
        if (!baseAST) {
          if (index > 0 && path.at(index)->asQualifiedName()) // namespaced base class
            baseAST = path.at(--index)->asBaseSpecifier();
        }
        --index;
        if (baseAST && index >= 0)
          m_classAST = path.at(index)->asClassSpecifier();
      }
    }

    // Also offer the operation if we are on some "empty" part of the class declaration.
    if (!m_classAST)
      m_classAST = path.at(pathSize - 1)->asClassSpecifier();

    if (!m_classAST || !m_classAST->base_clause_list)
      return;

    // Determine insert positions
    const int endOfClassAST = interface.currentFile()->endOf(m_classAST);
    m_insertPosDecl = endOfClassAST - 1;    // Skip last "}"
    m_insertPosOutside = endOfClassAST + 1; // Step over ";"

    // Determine base classes
    QList<const Class*> baseClasses;
    QQueue<ClassOrNamespace*> baseClassQueue;
    QSet<ClassOrNamespace*> visitedBaseClasses;
    if (ClassOrNamespace *clazz = interface.context().lookupType(m_classAST->symbol))
      baseClassQueue.enqueue(clazz);
    while (!baseClassQueue.isEmpty()) {
      ClassOrNamespace *clazz = baseClassQueue.dequeue();
      visitedBaseClasses.insert(clazz);
      const QList<ClassOrNamespace*> bases = clazz->usings();
      foreach(ClassOrNamespace *baseClass, bases) {
        foreach(Symbol *symbol, baseClass->symbols()) {
          Class *base = symbol->asClass();
          if (base && (clazz = interface.context().lookupType(symbol)) && !visitedBaseClasses.contains(clazz) && !baseClasses.contains(base)) {
            baseClasses.prepend(base);
            baseClassQueue.enqueue(clazz);
          }
        }
      }
    }

    // Determine virtual functions
    m_factory->classFunctionModel->clear();
    auto printer = CppCodeStyleSettings::currentProjectCodeStyleOverview();
    printer.showFunctionSignatures = true;
    QHash<const Function*, FunctionItem*> virtualFunctions;
    foreach(const Class *clazz, baseClasses) {
      auto itemBase = new ClassItem(printer.prettyName(clazz->name()), clazz);
      for (Scope::iterator it = clazz->memberBegin(); it != clazz->memberEnd(); ++it) {
        if (const Function *func = (*it)->type()->asFunctionType()) {
          // Filter virtual destructors
          const Name *name = func->name();
          if (!name || name->asDestructorNameId())
            continue;

          QList<const Function*> firstVirtuals;
          const bool isVirtual = FunctionUtils::isVirtualFunction(func, interface.context(), &firstVirtuals);
          if (!isVirtual)
            continue;

          if (func->isFinal()) {
            for (auto firstVirtual : qAsConst(firstVirtuals)) {
              if (auto first = virtualFunctions[firstVirtual]) {
                FunctionItem *next = nullptr;
                for (auto removed = first; next != first; removed = next) {
                  next = removed->nextOverride;
                  m_factory->classFunctionModel->removeFunction(removed);
                  delete removed;
                };
                virtualFunctions.remove(firstVirtual);
              }
            }
            continue;
          }
          // Filter OQbject's
          //   - virtual const QMetaObject *metaObject() const;
          //   - virtual void *qt_metacast(const char *);
          //   - virtual int qt_metacall(QMetaObject::Call, int, void **);
          auto skip = false;
          for (auto firstVirtual : qAsConst(firstVirtuals)) {
            if (printer.prettyName(firstVirtual->enclosingClass()->name()) == "QObject" && magicQObjectFunctions().contains(printer.prettyName(func->name()))) {
              skip = true;
              break;
            }
          }
          if (skip)
            continue;

          // Do not implement existing functions inside target class
          auto funcExistsInClass = false;
          const Name *funcName = func->name();
          const OperatorNameId *const opName = funcName->asOperatorNameId();
          Symbol *symbol = opName ? m_classAST->symbol->find(opName->kind()) : m_classAST->symbol->find(funcName->identifier());
          for (; symbol; symbol = symbol->next()) {
            if (!opName && (!symbol->name() || !funcName->identifier()->match(symbol->identifier()))) {
              continue;
            }
            if (symbol->type().match(func->type())) {
              funcExistsInClass = true;
              break;
            }
          }

          // Construct function item
          const bool isReimplemented = !firstVirtuals.contains(func);
          const bool isPureVirtual = func->isPureVirtual();
          QString itemName = printer.prettyType(func->type(), func->name());
          if (isPureVirtual)
            itemName += QLatin1String(" = 0");
          const QString itemReturnTypeString = printer.prettyType(func->returnType());
          itemName += QLatin1String(" : ") + itemReturnTypeString;
          if (isReimplemented)
            itemName += QLatin1String(" (redeclared)");
          auto funcItem = new FunctionItem(func, itemName, itemBase);
          if (isReimplemented) {
            factory->setHasReimplementedFunctions(true);
            funcItem->reimplemented = true;
            funcItem->alreadyFound = funcExistsInClass;
            for (auto firstVirtual : qAsConst(firstVirtuals)) {
              if (auto first = virtualFunctions[firstVirtual]) {
                if (!first->alreadyFound) {
                  while (first->checked != isPureVirtual) {
                    first->checked = isPureVirtual;
                    first = first->nextOverride;
                  }
                }
                funcItem->checked = first->checked;

                FunctionItem *prev = funcItem;
                for (FunctionItem *next = funcItem->nextOverride; next && next != funcItem; next = next->nextOverride) {
                  prev = next;
                }
                prev->nextOverride = first->nextOverride;
                first->nextOverride = funcItem;
              }
            }
          } else {
            if (!funcExistsInClass) {
              funcItem->checked = isPureVirtual;
            } else {
              funcItem->alreadyFound = true;
              funcItem->checked = true;
              factory->setHasReimplementedFunctions(true);
            }
          }

          funcItem->accessSpec = acessSpec(*it);
          funcItem->row = itemBase->functions.count();
          itemBase->functions.append(funcItem);

          virtualFunctions[func] = funcItem;

          // update internal counters
          if (!funcExistsInClass)
            ++m_functionCount;
        }
      }

      if (itemBase->functions.isEmpty())
        delete itemBase;
      else
        m_factory->classFunctionModel->addClass(itemBase);
    }
    if (m_factory->classFunctionModel->classes.isEmpty() || m_functionCount == 0)
      return;

    auto isHeaderFile = false;
    m_cppFileName = correspondingHeaderOrSource(interface.filePath().toString(), &isHeaderFile);
    m_factory->setHasImplementationFile(isHeaderFile && !m_cppFileName.isEmpty());

    m_valid = true;
  }

  auto isValid() const -> bool
  {
    return m_valid;
  }

  auto acessSpec(const Symbol *symbol) -> InsertionPointLocator::AccessSpec
  {
    const Function *func = symbol->type()->asFunctionType();
    if (!func)
      return InsertionPointLocator::Invalid;
    if (func->isSignal())
      return InsertionPointLocator::Signals;

    auto spec = InsertionPointLocator::Invalid;
    if (symbol->isPrivate())
      spec = InsertionPointLocator::Private;
    else if (symbol->isProtected())
      spec = InsertionPointLocator::Protected;
    else if (symbol->isPublic())
      spec = InsertionPointLocator::Public;
    else
      return InsertionPointLocator::Invalid;

    if (func->isSlot()) {
      switch (spec) {
      case InsertionPointLocator::Private:
        return InsertionPointLocator::PrivateSlot;
      case InsertionPointLocator::Protected:
        return InsertionPointLocator::ProtectedSlot;
      case InsertionPointLocator::Public:
        return InsertionPointLocator::PublicSlot;
      default:
        return spec;
      }
    }
    return spec;
  }

  auto perform() -> void override
  {
    if (!m_factory->gather())
      return;

    m_factory->saveSettings();

    // Insert declarations (and definition if Inside-/OutsideClass)
    auto printer = CppCodeStyleSettings::currentProjectCodeStyleOverview();
    printer.showFunctionSignatures = true;
    printer.showReturnTypes = true;
    printer.showArgumentNames = true;
    printer.showTemplateParameters = true;
    Utils::ChangeSet headerChangeSet;
    const CppRefactoringChanges refactoring(snapshot());
    const auto filePath = currentFile()->filePath();
    const CppRefactoringFilePtr headerFile = refactoring.file(filePath);
    const LookupContext targetContext(headerFile->cppDocument(), snapshot());

    const Class *targetClass = m_classAST->symbol;
    ClassOrNamespace *targetCoN = targetContext.lookupType(targetClass->enclosingScope());
    if (!targetCoN)
      targetCoN = targetContext.globalNamespace();
    UseMinimalNames useMinimalNames(targetCoN);
    Control *control = context().bindings()->control().data();
    QList<const Function*> insertedFunctions;
    foreach(ClassItem *classItem, m_factory->classFunctionModel->classes) {
      if (classItem->checkState() == Qt::Unchecked)
        continue;

      // Insert Declarations (+ definitions)
      QString lastAccessSpecString;
      auto first = true;
      foreach(FunctionItem *funcItem, classItem->functions) {
        if (funcItem->reimplemented || funcItem->alreadyFound || !funcItem->checked)
          continue;

        const auto cmp = [funcItem](const Function *f) {
          return f->name()->match(funcItem->function->name()) && f->type().match(funcItem->function->type());
        };
        if (Utils::contains(insertedFunctions, cmp))
          continue;
        insertedFunctions.append(funcItem->function);

        if (first) {
          // Add comment
          const QString comment = QLatin1String("\n// ") + printer.prettyName(classItem->klass->name()) + QLatin1String(" interface\n");
          headerChangeSet.insert(m_insertPosDecl, comment);
          first = false;
        }

        // Function type minimalization: As base class and derived class could be in
        // different namespaces, we must first make the type fully qualified before
        // it can get minimized.
        Clone cloner(control);
        Function newFunc(&cloner, nullptr, const_cast<Function*>(funcItem->function));
        newFunc.setEnclosingScope(const_cast<Class*>(targetClass));
        SubstitutionEnvironment envQualified;
        envQualified.setContext(context());
        envQualified.switchScope(classItem->klass->enclosingScope());
        UseQualifiedNames useQualifiedNames;
        envQualified.enter(&useQualifiedNames);
        newFunc.setReturnType(rewriteType(newFunc.returnType(), &envQualified, control));
        const int argc = newFunc.argumentCount();
        for (auto i = 0; i < argc; ++i) {
          Argument *const arg = newFunc.argumentAt(i)->asArgument();
          QTC_ASSERT(arg, continue);
          arg->setType(rewriteType(arg->type(), &envQualified, control));
        }
        SubstitutionEnvironment envMinimized;
        envMinimized.setContext(context());
        envMinimized.switchScope(targetClass->enclosingScope());
        envMinimized.enter(&useMinimalNames);
        const FullySpecifiedType tn = rewriteType(newFunc.type(), &envMinimized, control);
        QString declaration = printer.prettyType(tn, newFunc.unqualifiedName());

        if (m_factory->settings()->insertVirtualKeyword)
          declaration = QLatin1String("virtual ") + declaration;
        if (m_factory->settings()->insertOverrideReplacement) {
          const auto overrideReplacement = m_factory->settings()->overrideReplacement;
          if (!overrideReplacement.isEmpty())
            declaration += QLatin1Char(' ') + overrideReplacement;
        }
        if (m_factory->settings()->implementationMode & InsertVirtualMethodsDialog::ModeInsideClass) {
          declaration += QLatin1String("\n{\n}\n");
        } else {
          declaration += QLatin1String(";\n");
        }

        const auto accessSpecString = InsertionPointLocator::accessSpecToString(funcItem->accessSpec);
        if (accessSpecString != lastAccessSpecString) {
          declaration = accessSpecString + QLatin1String(":\n") + declaration;
          if (!lastAccessSpecString.isEmpty()) // separate if not direct after the comment
            declaration = QLatin1String("\n") + declaration;
          lastAccessSpecString = accessSpecString;
        }
        headerChangeSet.insert(m_insertPosDecl, declaration);

        // Insert definition outside class
        if (m_factory->settings()->implementationMode & InsertVirtualMethodsDialog::ModeOutsideClass) {
          const QString name = printer.prettyName(targetClass->name()) + QLatin1String("::") + printer.prettyName(funcItem->function->name());
          const QString defText = printer.prettyType(tn, name) + QLatin1String("\n{\n}");
          headerChangeSet.insert(m_insertPosOutside, QLatin1String("\n\n") + defText);
        }
      }
    }

    // Write header file
    if (!headerChangeSet.isEmpty()) {
      headerFile->setChangeSet(headerChangeSet);
      headerFile->appendIndentRange(Utils::ChangeSet::Range(m_insertPosDecl, m_insertPosDecl + 1));
      headerFile->setOpenEditor(true, m_insertPosDecl);
      headerFile->apply();
    }

    // Insert in implementation file
    if (m_factory->settings()->implementationMode & InsertVirtualMethodsDialog::ModeImplementationFile) {
      const Symbol *symbol = headerFile->cppDocument()->lastVisibleSymbolAt(targetClass->line(), targetClass->column());
      if (!symbol)
        return;
      const Class *clazz = symbol->asClass();
      if (!clazz)
        return;

      CppRefactoringFilePtr implementationFile = refactoring.file(Utils::FilePath::fromString(m_cppFileName));
      Utils::ChangeSet implementationChangeSet;
      const int insertPos = qMax(0, implementationFile->document()->characterCount() - 1);

      // make target lookup context
      Document::Ptr implementationDoc = implementationFile->cppDocument();
      int line, column;
      implementationDoc->translationUnit()->getPosition(insertPos, &line, &column);
      Scope *targetScope = implementationDoc->scopeAt(line, column);
      const LookupContext targetContext(implementationDoc, snapshot());
      ClassOrNamespace *targetCoN = targetContext.lookupType(targetScope);
      if (!targetCoN)
        targetCoN = targetContext.globalNamespace();

      // Loop through inserted declarations
      for (int i = targetClass->memberCount(); i < clazz->memberCount(); ++i) {
        Declaration *decl = clazz->memberAt(i)->asDeclaration();
        if (!decl)
          continue;

        // setup rewriting to get minimally qualified names
        SubstitutionEnvironment env;
        env.setContext(context());
        env.switchScope(decl->enclosingScope());
        UseMinimalNames q(targetCoN);
        env.enter(&q);
        Control *control = context().bindings()->control().data();

        // rewrite the function type and name + create definition
        const FullySpecifiedType type = rewriteType(decl->type(), &env, control);
        const QString name = printer.prettyName(LookupContext::minimalName(decl, targetCoN, control));
        const QString defText = printer.prettyType(type, name) + QLatin1String("\n{\n}");

        implementationChangeSet.insert(insertPos, QLatin1String("\n\n") + defText);
      }

      if (!implementationChangeSet.isEmpty()) {
        implementationFile->setChangeSet(implementationChangeSet);
        implementationFile->appendIndentRange(Utils::ChangeSet::Range(insertPos, insertPos + 1));
        implementationFile->apply();
      }
    }
  }

private:
  InsertVirtualMethodsDialog *m_factory = nullptr;
  const ClassSpecifierAST *m_classAST = nullptr;
  bool m_valid = false;
  QString m_cppFileName;
  int m_insertPosDecl = 0;
  int m_insertPosOutside = 0;
  unsigned m_functionCount = 0;
};

class InsertVirtualMethodsFilterModel : public QSortFilterProxyModel {
  Q_OBJECT public:
  InsertVirtualMethodsFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

  auto filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const -> bool override
  {
    auto index = sourceModel()->index(sourceRow, 0, sourceParent);

    // Handle base class
    if (!sourceParent.isValid()) {
      // check if any child is valid
      if (!sourceModel()->hasChildren(index))
        return false;
      if (!m_hideReimplemented)
        return true;

      for (auto i = 0; i < sourceModel()->rowCount(index); ++i) {
        const auto child = sourceModel()->index(i, 0, index);
        if (!child.data(InsertVirtualMethodsDialog::Reimplemented).toBool())
          return true;
      }
      return false;
    }

    if (!QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent))
      return false;
    if (m_hideReimplemented)
      return !index.data(InsertVirtualMethodsDialog::Reimplemented).toBool();
    return true;
  }

  auto hideReimplemented() const -> bool
  {
    return m_hideReimplemented;
  }

  auto setHideReimplementedFunctions(bool show) -> void
  {
    m_hideReimplemented = show;
    invalidateFilter();
  }

private:
  bool m_hideReimplemented = false;
};

InsertVirtualMethodsDialog::InsertVirtualMethodsDialog(QWidget *parent) : QDialog(parent), m_settings(new VirtualMethodsSettings), classFunctionModel(new InsertVirtualMethodsModel(this)), classFunctionFilterModel(new InsertVirtualMethodsFilterModel(this))
{
  classFunctionFilterModel->setSourceModel(classFunctionModel);
  classFunctionFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

InsertVirtualMethodsDialog::~InsertVirtualMethodsDialog()
{
  delete m_settings;
}

auto InsertVirtualMethodsDialog::initGui() -> void
{
  if (m_view)
    return;

  setWindowTitle(tr("Insert Virtual Functions"));
  auto globalVerticalLayout = new QVBoxLayout;

  // View
  auto groupBoxView = new QGroupBox(tr("&Functions to insert:"), this);
  auto groupBoxViewLayout = new QVBoxLayout(groupBoxView);
  m_filter = new QLineEdit(this);
  m_filter->setClearButtonEnabled(true);
  m_filter->setPlaceholderText(tr("Filter"));
  groupBoxViewLayout->addWidget(m_filter);
  m_view = new QTreeView(this);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_view->setHeaderHidden(true);
  groupBoxViewLayout->addWidget(m_view);
  m_hideReimplementedFunctions = new QCheckBox(tr("&Hide reimplemented functions"), this);
  groupBoxViewLayout->addWidget(m_hideReimplementedFunctions);

  // Insertion options
  auto groupBoxImplementation = new QGroupBox(tr("&Insertion options:"), this);
  auto groupBoxImplementationLayout = new QVBoxLayout(groupBoxImplementation);
  m_insertMode = new QComboBox(this);
  m_insertMode->addItem(tr("Insert only declarations"), ModeOnlyDeclarations);
  m_insertMode->addItem(tr("Insert definitions inside class"), ModeInsideClass);
  m_insertMode->addItem(tr("Insert definitions outside class"), ModeOutsideClass);
  m_insertMode->addItem(tr("Insert definitions in implementation file"), ModeImplementationFile);
  m_virtualKeyword = new QCheckBox(tr("Add \"&virtual\" to function declaration"), this);
  m_overrideReplacementCheckBox = new QCheckBox(tr("Add \"override\" equivalent to function declaration:"), this);
  m_overrideReplacementComboBox = new QComboBox(this);
  auto sizePolicy = m_overrideReplacementComboBox->sizePolicy();
  sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
  m_overrideReplacementComboBox->setSizePolicy(sizePolicy);
  m_overrideReplacementComboBox->setEditable(true);
  connect(m_overrideReplacementCheckBox, &QCheckBox::clicked, m_overrideReplacementComboBox, &QComboBox::setEnabled);

  auto clearUserAddedReplacements = new QAction(this);
  clearUserAddedReplacements->setIcon(Utils::Icons::CLEAN_TOOLBAR.icon());
  clearUserAddedReplacements->setText(tr("Clear Added \"override\" Equivalents"));
  connect(clearUserAddedReplacements, &QAction::triggered, [this]() {
    m_availableOverrideReplacements = defaultOverrideReplacements();
    updateOverrideReplacementsComboBox();
    m_clearUserAddedReplacementsButton->setEnabled(false);
  });
  m_clearUserAddedReplacementsButton = new QToolButton(this);
  m_clearUserAddedReplacementsButton->setDefaultAction(clearUserAddedReplacements);

  auto overrideWidgetsLayout = new QHBoxLayout(this);
  overrideWidgetsLayout->setSpacing(0);
  overrideWidgetsLayout->setContentsMargins(0, 0, 0, 0);
  overrideWidgetsLayout->addWidget(m_overrideReplacementCheckBox);
  overrideWidgetsLayout->addWidget(m_overrideReplacementComboBox);
  overrideWidgetsLayout->addWidget(m_clearUserAddedReplacementsButton);
  auto overrideWidgets = new QWidget(groupBoxImplementation);
  overrideWidgets->setLayout(overrideWidgetsLayout);

  groupBoxImplementationLayout->addWidget(m_insertMode);
  groupBoxImplementationLayout->addWidget(m_virtualKeyword);
  groupBoxImplementationLayout->addWidget(overrideWidgets);
  groupBoxImplementationLayout->addStretch(99);

  // Bottom button box
  m_buttons = new QDialogButtonBox(this);
  m_buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  globalVerticalLayout->addWidget(groupBoxView, 9);
  globalVerticalLayout->addWidget(groupBoxImplementation, 0);
  globalVerticalLayout->addWidget(m_buttons, 0);
  setLayout(globalVerticalLayout);

  connect(m_hideReimplementedFunctions, &QAbstractButton::toggled, this, &InsertVirtualMethodsDialog::setHideReimplementedFunctions);
  connect(m_filter, &QLineEdit::textChanged, classFunctionFilterModel, &QSortFilterProxyModel::setFilterWildcard);
}

auto InsertVirtualMethodsDialog::initData() -> void
{
  m_settings->read();
  m_filter->clear();
  m_hideReimplementedFunctions->setChecked(m_settings->hideReimplementedFunctions);
  const auto alwaysPresentReplacements = defaultOverrideReplacements();
  m_availableOverrideReplacements = alwaysPresentReplacements;
  m_availableOverrideReplacements += m_settings->userAddedOverrideReplacements;

  m_view->setModel(classFunctionFilterModel);
  m_expansionStateNormal.clear();
  m_expansionStateReimp.clear();
  m_hideReimplementedFunctions->setEnabled(m_hasReimplementedFunctions);
  m_virtualKeyword->setChecked(m_settings->insertVirtualKeyword);
  m_insertMode->setCurrentIndex(m_insertMode->findData(m_settings->implementationMode));

  m_overrideReplacementCheckBox->setChecked(m_settings->insertOverrideReplacement);
  updateOverrideReplacementsComboBox();
  const auto canClear = m_availableOverrideReplacements.size() > alwaysPresentReplacements.size();
  m_clearUserAddedReplacementsButton->setEnabled(canClear);
  auto overrideReplacementIndex = m_settings->overrideReplacementIndex;
  if (overrideReplacementIndex >= m_overrideReplacementComboBox->count())
    overrideReplacementIndex = 0;
  m_overrideReplacementComboBox->setCurrentIndex(overrideReplacementIndex);

  setHideReimplementedFunctions(m_hideReimplementedFunctions->isChecked());

  if (m_hasImplementationFile) {
    if (m_insertMode->count() == 3) {
      m_insertMode->addItem(tr("Insert definitions in implementation file"), ModeImplementationFile);
    }
  } else {
    if (m_insertMode->count() == 4)
      m_insertMode->removeItem(3);
  }
}

auto InsertVirtualMethodsDialog::saveSettings() -> void
{
  m_settings->insertVirtualKeyword = m_virtualKeyword->isChecked();
  m_settings->implementationMode = static_cast<InsertVirtualMethodsDialog::ImplementationMode>(m_insertMode->itemData(m_insertMode->currentIndex()).toInt());
  m_settings->hideReimplementedFunctions = m_hideReimplementedFunctions->isChecked();
  m_settings->insertOverrideReplacement = m_overrideReplacementCheckBox->isChecked();
  m_settings->overrideReplacementIndex = m_overrideReplacementComboBox->currentIndex();
  if (m_overrideReplacementComboBox && m_overrideReplacementComboBox->isEnabled())
    m_settings->overrideReplacement = m_overrideReplacementComboBox->currentText().trimmed();
  auto addedReplacements = Utils::toSet(m_availableOverrideReplacements);
  addedReplacements.insert(m_settings->overrideReplacement);
  addedReplacements.subtract(Utils::toSet(defaultOverrideReplacements()));
  m_settings->userAddedOverrideReplacements = sortedAndTrimmedStringListWithoutEmptyElements(Utils::toList(addedReplacements));
  m_settings->write();
}

auto InsertVirtualMethodsDialog::settings() const -> const VirtualMethodsSettings*
{
  return m_settings;
}

auto InsertVirtualMethodsDialog::gather() -> bool
{
  initGui();
  initData();
  m_filter->setFocus();

  // Expand the dialog a little bit
  adjustSize();
  resize(size() * 1.5);

  QPointer<InsertVirtualMethodsDialog> that(this);
  const auto ret = exec();
  if (!that)
    return false;

  return (ret == QDialog::Accepted);
}

auto InsertVirtualMethodsDialog::setHasImplementationFile(bool file) -> void
{
  m_hasImplementationFile = file;
}

auto InsertVirtualMethodsDialog::setHasReimplementedFunctions(bool functions) -> void
{
  m_hasReimplementedFunctions = functions;
}

auto InsertVirtualMethodsDialog::setHideReimplementedFunctions(bool hide) -> void
{
  auto model = qobject_cast<InsertVirtualMethodsFilterModel*>(classFunctionFilterModel);

  if (m_expansionStateNormal.isEmpty() && m_expansionStateReimp.isEmpty()) {
    model->setHideReimplementedFunctions(hide);
    m_view->expandAll();
    saveExpansionState();
    return;
  }

  if (model->hideReimplemented() == hide)
    return;

  saveExpansionState();
  model->setHideReimplementedFunctions(hide);
  restoreExpansionState();
}

auto InsertVirtualMethodsDialog::updateOverrideReplacementsComboBox() -> void
{
  m_overrideReplacementComboBox->clear();
  foreach(const QString &replacement, m_availableOverrideReplacements)
    m_overrideReplacementComboBox->addItem(replacement);
}

auto InsertVirtualMethodsDialog::saveExpansionState() -> void
{
  auto model = qobject_cast<InsertVirtualMethodsFilterModel*>(classFunctionFilterModel);

  auto &state = model->hideReimplemented() ? m_expansionStateReimp : m_expansionStateNormal;
  state.clear();
  for (auto i = 0; i < model->rowCount(); ++i)
    state << m_view->isExpanded(model->index(i, 0));
}

auto InsertVirtualMethodsDialog::restoreExpansionState() -> void
{
  auto model = qobject_cast<InsertVirtualMethodsFilterModel*>(classFunctionFilterModel);

  const auto &state = model->hideReimplemented() ? m_expansionStateReimp : m_expansionStateNormal;
  const int stateCount = state.count();
  for (auto i = 0; i < model->rowCount(); ++i) {
    if (i < stateCount && !state.at(i)) {
      m_view->collapse(model->index(i, 0));
      continue;
    }
    m_view->expand(model->index(i, 0));
  }
}

InsertVirtualMethods::InsertVirtualMethods(InsertVirtualMethodsDialog *dialog) : m_dialog(dialog)
{
  if (!dialog)
    m_dialog = new InsertVirtualMethodsDialog;
}

InsertVirtualMethods::~InsertVirtualMethods()
{
  m_dialog->deleteLater();
}

auto InsertVirtualMethods::match(const CppQuickFixInterface &interface, QuickFixOperations &result) -> void
{
  QSharedPointer<InsertVirtualMethodsOp> op(new InsertVirtualMethodsOp(interface, m_dialog));
  if (op->isValid())
    result.append(op);
}

#ifdef WITH_TESTS

namespace Tests {

typedef QByteArray _;

/// Stub dialog of InsertVirtualMethodsDialog that does not pop up anything.
class InsertVirtualMethodsDialogTest : public InsertVirtualMethodsDialog
{
public:
    InsertVirtualMethodsDialogTest(ImplementationMode mode,
                                   bool insertVirtualKeyword,
                                   bool insertOverrideKeyword,
                                   QWidget *parent = 0)
        : InsertVirtualMethodsDialog(parent)
    {
        m_settings->implementationMode = mode;
        m_settings->insertVirtualKeyword = insertVirtualKeyword;
        m_settings->insertOverrideReplacement = insertOverrideKeyword;
        m_settings->overrideReplacement = QLatin1String("override");
    }

    bool gather() { return true; }
    void saveSettings() { }
};

void InsertVirtualMethodsTest::test_data()
{
    QTest::addColumn<InsertVirtualMethodsDialog::ImplementationMode>("implementationMode");
    QTest::addColumn<bool>("insertVirtualKeyword");
    QTest::addColumn<bool>("insertOverrideKeyword");
    QTest::addColumn<QByteArray>("original");
    QTest::addColumn<QByteArray>("expected");

    // Check: Insert only declarations
    QTest::newRow("onlyDecl")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA();\n"
        "};\n"
    );

    // Check: Insert only declarations without virtual keyword but with override
    QTest::newRow("onlyDeclWithoutVirtual")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << false << true << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    int virtualFuncA() override;\n"
        "};\n"
    );

    // Check: Are access specifiers considered
    QTest::newRow("Access")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "protected:\n"
        "    virtual int b() = 0;\n"
        "private:\n"
        "    virtual int c() = 0;\n"
        "public slots:\n"
        "    virtual int d() = 0;\n"
        "protected slots:\n"
        "    virtual int e() = 0;\n"
        "private slots:\n"
        "    virtual int f() = 0;\n"
        "signals:\n"
        "    virtual int g() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "protected:\n"
        "    virtual int b() = 0;\n"
        "private:\n"
        "    virtual int c() = 0;\n"
        "public slots:\n"
        "    virtual int d() = 0;\n"
        "protected slots:\n"
        "    virtual int e() = 0;\n"
        "private slots:\n"
        "    virtual int f() = 0;\n"
        "signals:\n"
        "    virtual int g() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n\n"
        "protected:\n"
        "    virtual int b();\n\n"
        "private:\n"
        "    virtual int c();\n\n"
        "public slots:\n"
        "    virtual int d();\n\n"
        "protected slots:\n"
        "    virtual int e();\n\n"
        "private slots:\n"
        "    virtual int f();\n\n"
        "signals:\n"
        "    virtual int g();\n"
        "};\n"
    );

    // Check: Is a base class of a base class considered.
    QTest::newRow("Superclass")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n"
        "\n"
        "    // BaseB interface\n"
        "public:\n"
        "    virtual int b();\n"
        "};\n"
    );


    // Check: Do not insert reimplemented functions twice.
    QTest::newRow("SuperclassOverride")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n"
        "};\n"
    );

    // Check: Insert only declarations for pure virtual function
    QTest::newRow("PureVirtualOnlyDecl")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA();\n"
        "};\n"
    );

    // Check: Insert pure virtual functions inside class
    QTest::newRow("PureVirtualInside")
        << InsertVirtualMethodsDialog::ModeInsideClass << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA()\n"
        "    {\n"
        "    }\n"
        "};\n"
    );

    // Check: Overloads
    QTest::newRow("Overloads")
        << InsertVirtualMethodsDialog::ModeInsideClass << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virt(int i) = 0;\n"
        "    virtual int virt(double d) = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virt(int i) = 0;\n"
        "    virtual int virt(double d) = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virt(int i)\n"
        "    {\n"
        "    }\n"
        "    virtual int virt(double d)\n"
        "    {\n"
        "    }\n"
        "};\n"
    );

    // Check: Insert inside class
    QTest::newRow("inside")
        << InsertVirtualMethodsDialog::ModeInsideClass << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA()\n"
        "    {\n"
        "    }\n"
        "};\n"
    );

    // Check: Insert outside class
    QTest::newRow("outside")
        << InsertVirtualMethodsDialog::ModeOutsideClass << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA();\n"
        "};\n\n"
        "int Derived::virtualFuncA()\n"
        "{\n"
        "}\n"
    );

    // Check: No trigger: all implemented
    QTest::newRow("notrigger_allImplemented")
        << InsertVirtualMethodsDialog::ModeOutsideClass << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA();\n"
        "    virtual operator==(const BaseA &);\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "    virtual operator==(const BaseA &);\n"
        "};\n"
        ) << _();

    // Check: One pure, one not
    QTest::newRow("Some_Pure")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "    virtual int virtualFuncB();\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int virtualFuncA() = 0;\n"
        "    virtual int virtualFuncB();\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int virtualFuncA();\n"
        "};\n"
    );

    // Check: Pure function in derived class
    QTest::newRow("Pure_in_Derived")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n"
        "};\n"
    );

    // Check: One pure function in base class, one in derived
    QTest::newRow("Pure_in_Base_And_Derived")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n"
        "    virtual int b();\n"
        "};\n"
    );

    // Check: One pure function in base class, two in derived
    QTest::newRow("Pure_in_Base_And_Derived_2")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "    virtual int c() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b();\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int b() = 0;\n"
        "    virtual int c() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a();\n"
        "    virtual int b();\n"
        "\n"
        "    // BaseB interface\n"
        "public:\n"
        "    virtual int c();\n"
        "};\n"
    );

    // Check: Remove final function
    QTest::newRow("final_function_removed")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() final = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() final = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseB {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int b();\n"
        "};\n"
    );

    // Check: Remove multiple final functions
    QTest::newRow("multiple_final_functions_removed")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << true << false << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int c() = 0;\n"
        "};\n\n"
        "class BaseC : public BaseB {\n"
        "public:\n"
        "    virtual int a() final = 0;\n"
        "    virtual int d() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseC {\n"
        "};\n"
        ) << _(
        "class BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int b() = 0;\n"
        "};\n\n"
        "class BaseB : public BaseA {\n"
        "public:\n"
        "    virtual int a() = 0;\n"
        "    virtual int c() = 0;\n"
        "};\n\n"
        "class BaseC : public BaseB {\n"
        "public:\n"
        "    virtual int a() final = 0;\n"
        "    virtual int d() = 0;\n"
        "};\n\n"
        "class Der@ived : public BaseC {\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int b();\n"
        "\n"
        "    // BaseB interface\n"
        "public:\n"
        "    virtual int c();\n"
        "\n"
        "    // BaseC interface\n"
        "public:\n"
        "    virtual int d();\n"
        "};\n"
    );

    // Check: Insert multiply-inherited virtual function only once.
    QTest::newRow("multiple_inheritance_insert")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << false << true << _(
        "struct Base1 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Base2 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct @Derived : Base1, Base2 {\n"
        "};\n") << _(
        "struct Base1 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Base2 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Derived : Base1, Base2 {\n\n"
        "    // Base2 interface\n"
        "public:\n"
        "    void virt() override;\n"
        "};\n");

    // Check: Do not insert multiply-inherited virtual function that has been re-implemented
    //        along the way.
    QTest::newRow("multiple_inheritance_no_insert")
        << InsertVirtualMethodsDialog::ModeOnlyDeclarations << false << true << _(
        "struct Base1 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Base2 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Derived1 : Base1, Base2 {\n"
        "    void virt() override;\n"
        "};\n\n"
        "struct @Derived2 : Derived1\n"
        "};\n") << _(
        "struct Base1 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Base2 {\n"
        "    virtual void virt() = 0;\n"
        "};\n\n"
        "struct Derived1 : Base1, Base2 {\n"
        "    void virt() override;\n"
        "};\n\n"
        "struct Derived2 : Derived1\n"
        "};\n");
}

void InsertVirtualMethodsTest::test()
{
    QFETCH(InsertVirtualMethodsDialog::ImplementationMode, implementationMode);
    QFETCH(bool, insertVirtualKeyword);
    QFETCH(bool, insertOverrideKeyword);
    QFETCH(QByteArray, original);
    QFETCH(QByteArray, expected);

    InsertVirtualMethods factory(
                new Tests::InsertVirtualMethodsDialogTest(implementationMode,
                                                          insertVirtualKeyword,
                                                          insertOverrideKeyword));
    Tests::QuickFixOperationTest(Tests::singleDocument(original, expected), &factory);
}

/// Check: Insert in implementation file
void InsertVirtualMethodsTest::testImplementationFile()
{
    QList<Tests::TestDocumentPtr> testFiles;
    QByteArray original;
    QByteArray expected;

    // Header File
    original =
        "class BaseA {\n"
        "public:\n"
        "    virtual int a(const std::vector<int> &v) = 0;\n"
        "};\n\n"
        "class Derived : public Bas@eA {\n"
        "public:\n"
        "    Derived();\n"
        "};\n";
    expected =
        "class BaseA {\n"
        "public:\n"
        "    virtual int a(const std::vector<int> &v) = 0;\n"
        "};\n\n"
        "class Derived : public BaseA {\n"
        "public:\n"
        "    Derived();\n"
        "\n"
        "    // BaseA interface\n"
        "public:\n"
        "    virtual int a(const std::vector<int> &v);\n"
        "};\n";
    testFiles << Tests::CppTestDocument::create("file.hpp", original, expected);

    // Source File
    original = "#include \"file.h\"\n";
    expected =
        "#include \"file.h\"\n"
        "\n\n"
        "int Derived::a(const std::vector<int> &v)\n"
        "{\n}";
    testFiles << Tests::CppTestDocument::create("file.cpp", original, expected);

    InsertVirtualMethods factory(new Tests::InsertVirtualMethodsDialogTest(
                                     InsertVirtualMethodsDialog::ModeImplementationFile,
                                     true,
                                     false));
    Tests::QuickFixOperationTest(testFiles, &factory);
}

/// Check: Qualified names.
void InsertVirtualMethodsTest::testBaseClassInNamespace()
{
    QList<Tests::TestDocumentPtr> testFiles;
    QByteArray original;
    QByteArray expected;

    // Header File
    original =
        "namespace BaseNS {enum BaseEnum {EnumA = 1};}\n"
        "namespace BaseNS {\n"
        "class Base {\n"
        "public:\n"
        "    virtual BaseEnum a(BaseEnum e) = 0;\n"
        "};\n"
        "}\n"
        "class Deri@ved : public BaseNS::Base {\n"
        "public:\n"
        "    Derived();\n"
        "};\n";
    expected =
        "namespace BaseNS {enum BaseEnum {EnumA = 1};}\n"
        "namespace BaseNS {\n"
        "class Base {\n"
        "public:\n"
        "    virtual BaseEnum a(BaseEnum e) = 0;\n"
        "};\n"
        "}\n"
        "class Deri@ved : public BaseNS::Base {\n"
        "public:\n"
        "    Derived();\n"
        "\n"
        "    // Base interface\n"
        "public:\n"
        "    virtual BaseNS::BaseEnum a(BaseNS::BaseEnum e);\n"
        "};\n";
    testFiles << Tests::CppTestDocument::create("file.hpp", original, expected);

    // Source File
    original = "#include \"file.h\"\n";
    expected =
        "#include \"file.h\"\n"
        "\n\n"
        "BaseNS::BaseEnum Derived::a(BaseNS::BaseEnum e)\n"
        "{\n}";
    testFiles << Tests::CppTestDocument::create("file.cpp", original, expected);

    InsertVirtualMethods factory(new Tests::InsertVirtualMethodsDialogTest(
                                     InsertVirtualMethodsDialog::ModeImplementationFile,
                                     true,
                                     false));
    Tests::QuickFixOperationTest(testFiles, &factory);
}

} // namespace Tests

InsertVirtualMethods *InsertVirtualMethods::createTestFactory()
{
    return new InsertVirtualMethods(new Tests::InsertVirtualMethodsDialogTest(
                                        InsertVirtualMethodsDialog::ModeOutsideClass, true, false));
}

#endif // WITH_TESTS

} // namespace Internal
} // namespace CppEditor

#include "cppinsertvirtualmethods.moc"