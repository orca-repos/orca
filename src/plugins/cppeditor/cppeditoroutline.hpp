// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractoverviewmodel.hpp"

#include <QModelIndex>
#include <QObject>

#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QSortFilterProxyModel;
class QTimer;
QT_END_NAMESPACE

namespace TextEditor {
class TextEditorWidget;
}

namespace Utils {
class TreeViewComboBox;
}

namespace CppEditor::Internal {

class CppEditorOutline : public QObject {
  Q_OBJECT

public:
  explicit CppEditorOutline(TextEditor::TextEditorWidget *editorWidget);

  auto update() -> void;
  auto model() const -> AbstractOverviewModel*;
  auto modelIndex() -> QModelIndex;
  auto widget() const -> QWidget*; // Must be deleted by client.

signals:
  auto modelIndexChanged(const QModelIndex &index) -> void;

public slots:
  auto updateIndex() -> void;
  auto setSorted(bool sort) -> void;

private:
  auto updateNow() -> void;
  auto updateIndexNow() -> void;
  auto updateToolTip() -> void;
  auto gotoSymbolInEditor() -> void;

  CppEditorOutline();

  auto isSorted() const -> bool;
  auto indexForPosition(int line, int column, const QModelIndex &rootIndex = QModelIndex()) const -> QModelIndex;

  QSharedPointer<CPlusPlus::Document> m_document;
  std::unique_ptr<AbstractOverviewModel> m_model;
  TextEditor::TextEditorWidget *m_editorWidget;
  Utils::TreeViewComboBox *m_combo; // Not owned
  QSortFilterProxyModel *m_proxyModel;
  QModelIndex m_modelIndex;
  QAction *m_sortAction;
  QTimer *m_updateTimer;
  QTimer *m_updateIndexTimer;
};

} // namespace CppEditor::Internal
