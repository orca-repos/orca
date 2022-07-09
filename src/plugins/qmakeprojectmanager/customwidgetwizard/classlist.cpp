// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classlist.hpp"

#include <utils/qtcassert.hpp>

#include <QKeyEvent>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>

#include <QDebug>
#include <QRegularExpression>

namespace QmakeProjectManager {
namespace Internal {

// ClassModel: Validates the class name in setData() and
// refuses placeholders and invalid characters.
class ClassModel : public QStandardItemModel {
public:
  explicit ClassModel(QObject *parent = nullptr);

  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto appendPlaceHolder() -> void { appendClass(m_newClassPlaceHolder); }
  auto placeHolderIndex() const -> QModelIndex;
  auto newClassPlaceHolder() const -> QString { return m_newClassPlaceHolder; }

private:
  auto appendClass(const QString &) -> void;

  QRegularExpression m_validator;
  const QString m_newClassPlaceHolder;
};

ClassModel::ClassModel(QObject *parent) : QStandardItemModel(0, 1, parent), m_validator(QLatin1String("^[a-zA-Z][a-zA-Z0-9_]*$")), m_newClassPlaceHolder(ClassList::tr("<New class>"))
{
  QTC_ASSERT(m_validator.isValid(), return);
  appendPlaceHolder();
}

auto ClassModel::appendClass(const QString &c) -> void
{
  auto *item = new QStandardItem(c);
  item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
  appendRow(item);
}

auto ClassModel::setData(const QModelIndex &index, const QVariant &value, int role) -> bool
{
  if (role == Qt::EditRole && !m_validator.match(value.toString()).hasMatch())
    return false;
  return QStandardItemModel::setData(index, value, role);
}

auto ClassModel::placeHolderIndex() const -> QModelIndex
{
  return index(rowCount() - 1, 0);
}

// --------------- ClassList
ClassList::ClassList(QWidget *parent) : QListView(parent), m_model(new ClassModel)
{
  setModel(m_model);
  connect(itemDelegate(), &QAbstractItemDelegate::closeEditor, this, &ClassList::classEdited);
  connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ClassList::slotCurrentRowChanged);
}

auto ClassList::startEditingNewClassItem() -> void
{
  // Start editing the 'new class' item.
  setFocus();

  const auto index = m_model->placeHolderIndex();
  setCurrentIndex(index);
  edit(index);
}

auto ClassList::className(int row) const -> QString
{
  return m_model->item(row, 0)->text();
}

auto ClassList::classEdited() -> void
{
  const auto index = currentIndex();
  QTC_ASSERT(index.isValid(), return);

  const auto name = className(index.row());
  if (index == m_model->placeHolderIndex()) {
    // Real name class entered.
    if (name != m_model->newClassPlaceHolder()) {
      emit classAdded(name);
      m_model->appendPlaceHolder();
    }
  } else {
    emit classRenamed(index.row(), name);
  }
}

auto ClassList::removeCurrentClass() -> void
{
  const auto index = currentIndex();
  if (!index.isValid() || index == m_model->placeHolderIndex())
    return;
  if (QMessageBox::question(this, tr("Confirm Delete"), tr("Delete class %1 from list?").arg(className(index.row())), QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
    return;
  // Delete row and set current on same item.
  m_model->removeRows(index.row(), 1);
  emit classDeleted(index.row());
  setCurrentIndex(m_model->indexFromItem(m_model->item(index.row(), 0)));
}

auto ClassList::keyPressEvent(QKeyEvent *event) -> void
{
  switch (event->key()) {
  case Qt::Key_Backspace:
  case Qt::Key_Delete:
    removeCurrentClass();
    break;
  case Qt::Key_Insert:
    startEditingNewClassItem();
    break;
  default:
    QListView::keyPressEvent(event);
    break;
  }
}

auto ClassList::slotCurrentRowChanged(const QModelIndex &current, const QModelIndex &) -> void
{
  emit currentRowChanged(current.row());
}

} // namespace Internal
} // namespace QmakeProjectManager
