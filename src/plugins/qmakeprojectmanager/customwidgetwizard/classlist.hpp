// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QListView>

QT_FORWARD_DECLARE_CLASS(QModelIndex)

namespace QmakeProjectManager {
namespace Internal {

class ClassModel;

// Class list for new Custom widget classes. Provides
// editable '<new class>' field and Delete/Insert key handling.
class ClassList : public QListView {
  Q_OBJECT

public:
  explicit ClassList(QWidget *parent = nullptr);

  auto className(int row) const -> QString;

signals:
  auto classAdded(const QString &name) -> void;
  auto classRenamed(int index, const QString &newName) -> void;
  auto classDeleted(int index) -> void;
  auto currentRowChanged(int) -> void;

public:
  auto removeCurrentClass() -> void;
  auto startEditingNewClassItem() -> void;

private:
  auto classEdited() -> void;
  auto slotCurrentRowChanged(const QModelIndex &, const QModelIndex &) -> void;

protected:
  auto keyPressEvent(QKeyEvent *event) -> void override;

private:
  ClassModel *m_model;
};

} // namespace Internal
} // namespace QmakeProjectManager
