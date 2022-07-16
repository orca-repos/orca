// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-navigation-widget-factory-interface.hpp>
#include <utils/futuresynchronizer.hpp>

#include <QFuture>
#include <QFutureWatcher>
#include <QList>
#include <QSharedPointer>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QModelIndex;
class QStackedLayout;
class QStandardItem;
QT_END_NAMESPACE

namespace TextEditor {
class TextEditorLinkLabel;
}

namespace Utils {
class AnnotatedItemDelegate;
class NavigationTreeView;
class ProgressIndicator;
}

namespace CppEditor {

class CppEditorWidget;

namespace Internal {

class CppClass;
class CppElement;

class CppTypeHierarchyModel : public QStandardItemModel {
  Q_OBJECT public:
  CppTypeHierarchyModel(QObject *parent);

  auto supportedDragActions() const -> Qt::DropActions override;
  auto mimeTypes() const -> QStringList override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
};

class CppTypeHierarchyWidget : public QWidget {
  Q_OBJECT

public:
  CppTypeHierarchyWidget();

  auto perform() -> void;

private slots:
  auto displayHierarchy() -> void;

private:
  using HierarchyMember = QList<CppClass>CppClass::*;
  auto performFromExpression(const QString &expression, const QString &fileName) -> void;
  auto buildHierarchy(const CppClass &cppClass, QStandardItem *parent, bool isRoot, HierarchyMember member) -> QStandardItem*;
  auto showNoTypeHierarchyLabel() -> void;
  auto showTypeHierarchy() -> void;
  auto showProgress() -> void;
  auto hideProgress() -> void;
  auto clearTypeHierarchy() -> void;
  auto onItemActivated(const QModelIndex &index) -> void;
  auto onItemDoubleClicked(const QModelIndex &index) -> void;

  CppEditorWidget *m_cppEditor = nullptr;
  Utils::NavigationTreeView *m_treeView = nullptr;
  QWidget *m_hierarchyWidget = nullptr;
  QStackedLayout *m_stackLayout = nullptr;
  QStandardItemModel *m_model = nullptr;
  Utils::AnnotatedItemDelegate *m_delegate = nullptr;
  TextEditor::TextEditorLinkLabel *m_inspectedClass = nullptr;
  QLabel *m_infoLabel = nullptr;
  QFuture<QSharedPointer<CppElement>> m_future;
  QFutureWatcher<void> m_futureWatcher;
  Utils::FutureSynchronizer m_synchronizer;
  Utils::ProgressIndicator *m_progressIndicator = nullptr;
  QString m_oldClass;
  bool m_showOldClass = false;
};

class CppTypeHierarchyFactory : public Orca::Plugin::Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  CppTypeHierarchyFactory();

  auto createWidget() -> Orca::Plugin::Core::NavigationView override;
};

} // namespace Internal
} // namespace CppEditor
