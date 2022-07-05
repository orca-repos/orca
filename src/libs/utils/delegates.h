// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "pathchooser.h"

#include <QStyledItemDelegate>

namespace Utils {

class ORCA_UTILS_EXPORT AnnotatedItemDelegate : public QStyledItemDelegate {
public:
  AnnotatedItemDelegate(QObject *parent = nullptr);
  ~AnnotatedItemDelegate() override;

  auto setAnnotationRole(int role) -> void;
  auto annotationRole() const -> int;
  auto setDelimiter(const QString &delimiter) -> void;
  auto delimiter() const -> const QString&;

protected:
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override;

private:
  int m_annotationRole;
  QString m_delimiter;
};

class ORCA_UTILS_EXPORT PathChooserDelegate : public QStyledItemDelegate {
public:
  explicit PathChooserDelegate(QObject *parent = nullptr);

  auto setExpectedKind(PathChooser::Kind kind) -> void;
  auto setPromptDialogFilter(const QString &filter) -> void;
  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* override;
  auto setEditorData(QWidget *editor, const QModelIndex &index) const -> void override;
  auto setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void override;
  auto updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
  auto setHistoryCompleter(const QString &key) -> void;

private:
  PathChooser::Kind m_kind = PathChooser::ExistingDirectory;
  QString m_filter;
  QString m_historyKey;
};

class ORCA_UTILS_EXPORT CompleterDelegate : public QStyledItemDelegate {
public:
  CompleterDelegate(const QStringList &candidates, QObject *parent = nullptr);
  CompleterDelegate(QAbstractItemModel *model, QObject *parent = nullptr);
  CompleterDelegate(QCompleter *completer, QObject *parent = nullptr);
  ~CompleterDelegate() override;

  CompleterDelegate(const CompleterDelegate &other) = delete;
  CompleterDelegate(CompleterDelegate &&other) = delete;

  auto operator=(const CompleterDelegate &other) -> CompleterDelegate& = delete;
  auto operator=(CompleterDelegate &&other) -> CompleterDelegate& = delete;
  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* override;
  auto setEditorData(QWidget *editor, const QModelIndex &index) const -> void override;
  auto setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void override;
  auto updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;

private:
  QCompleter *m_completer = nullptr;
};

} // Utils
