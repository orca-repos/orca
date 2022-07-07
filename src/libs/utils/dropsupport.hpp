// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "filepath.hpp"

#include <QObject>
#include <QMimeData>
#include <QPoint>

#include <functional>

QT_BEGIN_NAMESPACE
class QDropEvent;
class QWidget;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT DropSupport : public QObject {
  Q_OBJECT

public:
  struct FileSpec {
    FileSpec(const FilePath &path, int r = -1, int c = -1) : filePath(path), line(r), column(c) {}
    FilePath filePath;
    int line;
    int column;
  };

  // returns true if the event should be accepted
  using DropFilterFunction = std::function<bool(QDropEvent *, DropSupport *)>;

  DropSupport(QWidget *parentWidget, const DropFilterFunction &filterFunction = DropFilterFunction());
  static auto mimeTypesForFilePaths() -> QStringList;

signals:
  auto filesDropped(const QList<Utils::DropSupport::FileSpec> &files, const QPoint &dropPos) -> void;
  auto valuesDropped(const QList<QVariant> &values, const QPoint &dropPos) -> void;

public:
  static auto isFileDrop(QDropEvent *event) -> bool;
  static auto isValueDrop(QDropEvent *event) -> bool;

protected:
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  auto emitFilesDropped() -> void;
  auto emitValuesDropped() -> void;

  DropFilterFunction m_filterFunction;
  QList<FileSpec> m_files;
  QList<QVariant> m_values;
  QPoint m_dropPos;

};

class ORCA_UTILS_EXPORT DropMimeData : public QMimeData {
  Q_OBJECT

public:
  DropMimeData();

  auto setOverrideFileDropAction(Qt::DropAction action) -> void;
  auto overrideFileDropAction() const -> Qt::DropAction;
  auto isOverridingFileDropAction() const -> bool;
  auto addFile(const FilePath &filePath, int line = -1, int column = -1) -> void;
  auto files() const -> QList<DropSupport::FileSpec>;
  auto addValue(const QVariant &value) -> void;
  auto values() const -> QList<QVariant>;

private:
  QList<DropSupport::FileSpec> m_files;
  QList<QVariant> m_values;
  Qt::DropAction m_overrideDropAction;
  bool m_isOverridingDropAction;
};

} // namespace Utils
