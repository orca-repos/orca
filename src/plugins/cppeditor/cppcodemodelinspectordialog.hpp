// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppmodelmanager.hpp"

#include <cplusplus/CppDocument.h>

#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
class QSortFilterProxyModel;
class QModelIndex;

namespace Ui {
class CppCodeModelInspectorDialog;
}
QT_END_NAMESPACE

namespace CppEditor {
namespace Internal {

class FilterableView;
class SnapshotInfo;

class DiagnosticMessagesModel;
class IncludesModel;
class KeyValueModel;
class MacrosModel;
class ProjectPartsModel;
class ProjectFilesModel;
class ProjectHeaderPathsModel;
class SnapshotModel;
class SymbolsModel;
class TokensModel;
class WorkingCopyModel;

//
// This dialog is for DEBUGGING PURPOSES and thus NOT TRANSLATED.
//

class CppCodeModelInspectorDialog : public QDialog {
  Q_OBJECT

public:
  explicit CppCodeModelInspectorDialog(QWidget *parent = nullptr);
  ~CppCodeModelInspectorDialog() override;

private:
  auto onRefreshRequested() -> void;
  auto onSnapshotFilterChanged(const QString &pattern) -> void;
  auto onSnapshotSelected(int row) -> void;
  auto onDocumentSelected(const QModelIndex &current, const QModelIndex &) -> void;
  auto onSymbolsViewExpandedOrCollapsed(const QModelIndex &) -> void;
  auto onProjectPartFilterChanged(const QString &pattern) -> void;
  auto onProjectPartSelected(const QModelIndex &current, const QModelIndex &) -> void;
  auto onWorkingCopyFilterChanged(const QString &pattern) -> void;
  auto onWorkingCopyDocumentSelected(const QModelIndex &current, const QModelIndex &) -> void;
  auto refresh() -> void;
  auto clearDocumentData() -> void;
  auto updateDocumentData(const CPlusPlus::Document::Ptr &document) -> void;
  auto clearProjectPartData() -> void;
  auto updateProjectPartData(const ProjectPart::ConstPtr &part) -> void;
  auto event(QEvent *e) -> bool override;
  
  QT_PREPEND_NAMESPACE(Ui)::CppCodeModelInspectorDialog *m_ui;

  // Snapshots and Documents
  QList<SnapshotInfo> *m_snapshotInfos;
  FilterableView *m_snapshotView;
  SnapshotModel *m_snapshotModel;
  QSortFilterProxyModel *m_proxySnapshotModel;
  KeyValueModel *m_docGenericInfoModel;
  IncludesModel *m_docIncludesModel;
  DiagnosticMessagesModel *m_docDiagnosticMessagesModel;
  MacrosModel *m_docMacrosModel;
  SymbolsModel *m_docSymbolsModel;
  TokensModel *m_docTokensModel;

  // Project Parts
  FilterableView *m_projectPartsView;
  ProjectPartsModel *m_projectPartsModel;
  QSortFilterProxyModel *m_proxyProjectPartsModel;
  KeyValueModel *m_partGenericInfoModel;
  ProjectFilesModel *m_projectFilesModel;
  ProjectHeaderPathsModel *m_projectHeaderPathsModel;

  // Working Copy
  FilterableView *m_workingCopyView;
  WorkingCopyModel *m_workingCopyModel;
  QSortFilterProxyModel *m_proxyWorkingCopyModel;
};

} // namespace Internal
} // namespace CppEditor
