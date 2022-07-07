// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "documentmodel.hpp"
#include "editorview.hpp"

#include <QIcon>
#include <QTreeWidget>

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
QT_END_NAMESPACE

namespace Core {

class IEditor;

namespace Internal {

class OpenEditorsTreeWidget final : public QTreeWidget {
  Q_DISABLE_COPY_MOVE(OpenEditorsTreeWidget)

public:
  explicit OpenEditorsTreeWidget(QWidget *parent = nullptr) : QTreeWidget(parent) {}
  ~OpenEditorsTreeWidget() override = default;

  auto sizeHint() const -> QSize override;
};

class OpenEditorsWindow : public QFrame {
  Q_OBJECT

public:
  enum mode {
    list_mode,
    history_mode
  };

  explicit OpenEditorsWindow(QWidget *parent = nullptr);

  auto setEditors(const QList<EditLocation> &global_history, EditorView *view) -> void;
  auto eventFilter(QObject *obj, QEvent *e) -> bool override;
  auto focusInEvent(QFocusEvent *) -> void override;
  auto setVisible(bool visible) -> void override;
  auto selectNextEditor() -> void;
  auto selectPreviousEditor() -> void;
  auto sizeHint() const -> QSize override;

public slots:
  auto selectAndHide() -> void;

private:
  auto editorClicked(const QTreeWidgetItem *item) -> void;
  static auto selectEditor(const QTreeWidgetItem *item) -> void;
  auto addHistoryItems(const QList<EditLocation> &history, EditorView *view, QSet<const DocumentModel::Entry*> &entries_done) const -> void;
  auto addRemainingItems(EditorView *view, QSet<const DocumentModel::Entry*> &entriesDone) const -> void;
  auto addItem(DocumentModel::Entry *entry, QSet<const DocumentModel::Entry*> &entriesDone, EditorView *view) const -> void;
  auto ensureCurrentVisible() const -> void;
  auto selectUpDown(bool up) -> void;
  auto isSameFile(IEditor *editor_a, IEditor *editor_b) const -> bool;

  const QIcon m_empty_icon;
  OpenEditorsTreeWidget *m_editor_list;
};

} // namespace Internal
} // namespace Core
