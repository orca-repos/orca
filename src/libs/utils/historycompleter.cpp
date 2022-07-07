// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "historycompleter.hpp"

#include "fancylineedit.hpp"
#include "qtcassert.hpp"
#include "qtcsettings.hpp"
#include "theme/theme.hpp"
#include "utilsicons.hpp"

#include <QItemDelegate>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QWindow>

namespace Utils {
namespace Internal {

static QtcSettings *theSettings = nullptr;
const bool isLastItemEmptyDefault = false;

class HistoryCompleterPrivate : public QAbstractListModel {
public:
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) -> bool override;
  auto clearHistory() -> void;
  auto addEntry(const QString &str) -> void;

  QStringList list;
  QString historyKey;
  QString historyKeyIsLastItemEmpty;
  int maxLines = 6;
  bool isLastItemEmpty = isLastItemEmptyDefault;
};

class HistoryLineDelegate : public QItemDelegate {
public:
  HistoryLineDelegate(QAbstractItemView *parent) : QItemDelegate(parent), view(parent), icon(Icons::EDIT_CLEAR.icon()) {}

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override
  {
    // from QHistoryCompleter
    QStyleOptionViewItem optCopy = option;
    optCopy.showDecorationSelected = true;
    if (view->currentIndex() == index)
      optCopy.state |= QStyle::State_HasFocus;
    QItemDelegate::paint(painter, option, index);
    // add remove button
    QWindow *window = view->window()->windowHandle();
    const QPixmap iconPixmap = icon.pixmap(window, option.rect.size());
    QRect pixmapRect = QStyle::alignedRect(option.direction, Qt::AlignRight | Qt::AlignVCenter, iconPixmap.size() / window->devicePixelRatio(), option.rect);
    if (!clearIconSize.isValid())
      clearIconSize = pixmapRect.size();
    painter->drawPixmap(pixmapRect, iconPixmap);
  }

  QAbstractItemView *view;
  QIcon icon;
  mutable QSize clearIconSize;
};

class HistoryLineView : public QListView {
public:
  HistoryLineView(HistoryCompleterPrivate *model_) : model(model_) { }

  auto installDelegate() -> void
  {
    delegate = new HistoryLineDelegate(this);
    setItemDelegate(delegate);
  }

private:
  auto mousePressEvent(QMouseEvent *event) -> void override
  {
    const QSize clearButtonSize = delegate->clearIconSize;
    if (clearButtonSize.isValid()) {
      int rr = event->x();
      if (layoutDirection() == Qt::LeftToRight)
        rr = viewport()->width() - event->x();
      if (rr < clearButtonSize.width()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
          model->removeRow(indexAt(event->pos()).row());
          return;
        }
      }
    }
    QListView::mousePressEvent(event);
  }

  HistoryCompleterPrivate *model;
  HistoryLineDelegate *delegate;
};

} // namespace Internal

using namespace Internal;

auto HistoryCompleterPrivate::rowCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : list.count();
}

auto HistoryCompleterPrivate::data(const QModelIndex &index, int role) const -> QVariant
{
  if (index.row() >= list.count() || index.column() != 0)
    return QVariant();
  if (role == Qt::DisplayRole || role == Qt::EditRole)
    return list.at(index.row());
  return QVariant();
}

auto HistoryCompleterPrivate::removeRows(int row, int count, const QModelIndex &parent) -> bool
{
  QTC_ASSERT(theSettings, return false);
  if (row + count > list.count())
    return false;
  beginRemoveRows(parent, row, row + count - 1);
  for (int i = 0; i < count; ++i)
    list.removeAt(row);
  theSettings->setValueWithDefault(historyKey, list);
  endRemoveRows();
  return true;
}

auto HistoryCompleterPrivate::clearHistory() -> void
{
  beginResetModel();
  list.clear();
  endResetModel();
}

auto HistoryCompleterPrivate::addEntry(const QString &str) -> void
{
  QTC_ASSERT(theSettings, return);
  const QString entry = str.trimmed();
  if (entry.isEmpty()) {
    isLastItemEmpty = true;
    theSettings->setValueWithDefault(historyKeyIsLastItemEmpty, isLastItemEmpty, isLastItemEmptyDefault);
    return;
  }
  int removeIndex = list.indexOf(entry);
  beginResetModel();
  if (removeIndex != -1)
    list.removeAt(removeIndex);
  list.prepend(entry);
  list = list.mid(0, maxLines - 1);
  endResetModel();
  theSettings->setValueWithDefault(historyKey, list);
  isLastItemEmpty = false;
  theSettings->setValueWithDefault(historyKeyIsLastItemEmpty, isLastItemEmpty, isLastItemEmptyDefault);
}

HistoryCompleter::HistoryCompleter(const QString &historyKey, QObject *parent) : QCompleter(parent), d(new HistoryCompleterPrivate)
{
  QTC_ASSERT(!historyKey.isEmpty(), return);
  QTC_ASSERT(theSettings, return);

  d->historyKey = QLatin1String("CompleterHistory/") + historyKey;
  d->list = theSettings->value(d->historyKey).toStringList();
  d->historyKeyIsLastItemEmpty = QLatin1String("CompleterHistory/") + historyKey + QLatin1String(".IsLastItemEmpty");
  d->isLastItemEmpty = theSettings->value(d->historyKeyIsLastItemEmpty, isLastItemEmptyDefault).toBool();

  setModel(d);
  auto popup = new HistoryLineView(d);
  setPopup(popup);
  // setPopup unconditionally sets a delegate on the popup,
  // so we need to set our delegate afterwards
  popup->installDelegate();
}

auto HistoryCompleter::removeHistoryItem(int index) -> bool
{
  return d->removeRow(index);
}

auto HistoryCompleter::historyItem() const -> QString
{
  if (historySize() == 0 || d->isLastItemEmpty)
    return QString();
  return d->list.at(0);
}

auto HistoryCompleter::historyExistsFor(const QString &historyKey) -> bool
{
  QTC_ASSERT(theSettings, return false);
  const QString fullKey = QLatin1String("CompleterHistory/") + historyKey;
  return theSettings->value(fullKey).isValid();
}

HistoryCompleter::~HistoryCompleter()
{
  delete d;
}

auto HistoryCompleter::historySize() const -> int
{
  return d->rowCount();
}

auto HistoryCompleter::maximalHistorySize() const -> int
{
  return d->maxLines;
}

auto HistoryCompleter::setMaximalHistorySize(int numberOfEntries) -> void
{
  d->maxLines = numberOfEntries;
}

auto HistoryCompleter::clearHistory() -> void
{
  d->clearHistory();
}

auto HistoryCompleter::addEntry(const QString &str) -> void
{
  d->addEntry(str);
}

auto HistoryCompleter::setSettings(QtcSettings *settings) -> void
{
  Internal::theSettings = settings;
}

} // namespace Utils
