// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QPushButton;
QT_END_NAMESPACE

namespace Utils {

struct PathListEditorPrivate;

class ORCA_UTILS_EXPORT PathListEditor : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QStringList pathList READ pathList WRITE setPathList DESIGNABLE true)
  Q_PROPERTY(QString fileDialogTitle READ fileDialogTitle WRITE setFileDialogTitle DESIGNABLE true)

public:
  explicit PathListEditor(QWidget *parent = nullptr);
  ~PathListEditor() override;

  auto pathListString() const -> QString;
  auto pathList() const -> QStringList;
  auto fileDialogTitle() const -> QString;
  auto clear() -> void;
  auto setPathList(const QStringList &l) -> void;
  auto setPathList(const QString &pathString) -> void;
  auto setFileDialogTitle(const QString &l) -> void;

signals:
  auto changed() -> void;

protected:
  // Index after which to insert further "Add" buttons
  static const int lastInsertButtonIndex;

  auto addButton(const QString &text, QObject *parent, std::function<void()> slotFunc) -> QPushButton*;
  auto insertButton(int index /* -1 */, const QString &text, QObject *parent, std::function<void()> slotFunc) -> QPushButton*;
  auto text() const -> QString;
  auto setText(const QString &) -> void;
  auto insertPathAtCursor(const QString &) -> void;
  auto deletePathAtCursor() -> void;

private:
  PathListEditorPrivate *d;
};

} // namespace Utils
