// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_qrceditor.h"
#include "resourceview.hpp"

#include <core/minisplitter.hpp>
#include <QUndoStack>

namespace ResourceEditor {
namespace Internal {

class QrcEditor : public Core::MiniSplitter {
  Q_OBJECT public:
  QrcEditor(RelativeResourceModel *model, QWidget *parent = nullptr);
  ~QrcEditor() override;

  auto loaded(bool success) -> void;

  auto setResourceDragEnabled(bool e) -> void;
  auto resourceDragEnabled() const -> bool;

  auto commandHistory() const -> const QUndoStack* { return &m_history; }

  auto refresh() -> void;
  auto editCurrentItem() -> void;

  auto currentResourcePath() const -> QString;

  auto onUndo() -> void;
  auto onRedo() -> void;

signals:
  auto itemActivated(const QString &fileName) -> void;
  auto showContextMenu(const QPoint &globalPos, const QString &fileName) -> void;
  auto undoStackChanged(bool canUndo, bool canRedo) -> void;

private:
  auto updateCurrent() -> void;
  auto updateHistoryControls() -> void;

  auto resolveLocationIssues(QStringList &files) -> void;

  auto onAliasChanged(const QString &alias) -> void;
  auto onPrefixChanged(const QString &prefix) -> void;
  auto onLanguageChanged(const QString &language) -> void;
  auto onRemove() -> void;
  auto onRemoveNonExisting() -> void;
  auto onAddFiles() -> void;
  auto onAddPrefix() -> void;

  Ui::QrcEditor m_ui;
  QUndoStack m_history;
  ResourceView *m_treeview;

  QString m_currentAlias;
  QString m_currentPrefix;
  QString m_currentLanguage;
};

} // namespace Internal
} // namespace ResourceEditor
