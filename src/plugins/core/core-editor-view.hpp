// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/dropsupport.hpp>
#include <utils/fileutils.hpp>
#include <utils/id.hpp>

#include <QList>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QFrame;
class QLabel;
class QMenu;
class QSplitter;
class QStackedLayout;
class QStackedWidget;
class QToolButton;
QT_END_NAMESPACE

namespace Utils {
class InfoBarDisplay;
}

namespace Orca::Plugin::Core {

class IDocument;
class IEditor;
class EditorToolBar;
class SplitterOrView;

struct EditLocation {
  QPointer<IDocument> document;
  Utils::FilePath file_path;
  Utils::Id id;
  QVariant state;
};

class EditorView final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(EditorView)

public:
  explicit EditorView(SplitterOrView *parent_splitter_or_view, QWidget *parent = nullptr);
  ~EditorView() override;

  auto parentSplitterOrView() const -> SplitterOrView*;
  auto findNextView() const -> EditorView*;
  auto findPreviousView() const -> EditorView*;
  auto editorCount() const -> int;
  auto addEditor(IEditor *editor) -> void;
  auto removeEditor(IEditor *editor) -> void;
  auto currentEditor() const -> IEditor*;
  auto setCurrentEditor(IEditor *editor) -> void;
  auto hasEditor(IEditor *editor) const -> bool;
  auto editors() const -> QList<IEditor*>;
  auto editorForDocument(const IDocument *document) const -> IEditor*;
  auto showEditorStatusBar(const QString &id, const QString &info_text, const QString &button_text, const QObject *object, const std::function<void()> &function) -> void;
  auto hideEditorStatusBar(const QString &id) const -> void;
  auto setCloseSplitEnabled(bool enable) const -> void;
  auto setCloseSplitIcon(const QIcon &icon) const -> void;
  static auto updateEditorHistory(const IEditor *editor, QList<EditLocation> &history) -> void;

signals:
  auto currentEditorChanged(Core::IEditor *editor) -> void;

protected:
  auto paintEvent(QPaintEvent *) -> void override;
  auto mousePressEvent(QMouseEvent *e) -> void override;
  auto focusInEvent(QFocusEvent *) -> void override;

private:
  friend class SplitterOrView; // for setParentSplitterOrView

  auto closeCurrentEditor() const -> void;
  auto listSelectionActivated(int index) -> void;
  auto splitHorizontally() const -> void;
  auto splitVertically() const -> void;
  auto splitNewWindow() const -> void;
  auto closeSplit() -> void;
  auto openDroppedFiles(const QList<Utils::DropSupport::FileSpec> &files) -> void;
  auto setParentSplitterOrView(SplitterOrView *splitter_or_view) -> void;
  auto fillListContextMenu(QMenu *menu) const -> void;
  auto updateNavigatorActions() const -> void;
  auto updateToolBar(IEditor *editor) -> void;
  auto checkProjectLoaded(IEditor *editor) -> void;
  auto updateCurrentPositionInNavigationHistory() -> void;

  SplitterOrView *m_parent_splitter_or_view;
  EditorToolBar *m_tool_bar;
  QStackedWidget *m_container;
  Utils::InfoBarDisplay *m_info_bar_display;
  QString m_status_widget_id;
  QFrame *m_status_h_line;
  QFrame *m_status_widget;
  QLabel *m_status_widget_label;
  QToolButton *m_status_widget_button;
  QList<IEditor*> m_editors;
  QMap<QWidget*, IEditor*> m_widget_editor_map;
  QLabel *m_empty_view_label;
  QList<EditLocation> m_navigation_history;
  QList<EditLocation> m_editor_history;
  int m_current_navigation_history_position = 0;

public:
  auto canGoForward() const -> bool { return m_current_navigation_history_position < m_navigation_history.size() - 1; }
  auto canGoBack() const -> bool { return m_current_navigation_history_position > 0; }

public slots:
  auto goBackInNavigationHistory() -> void;
  auto goForwardInNavigationHistory() -> void;

public:
  auto goToEditLocation(const EditLocation &location) -> void;
  auto addCurrentPositionToNavigationHistory(const QByteArray &save_state = QByteArray()) -> void;
  auto cutForwardNavigationHistory() -> void;
  auto editorHistory() const -> QList<EditLocation> { return m_editor_history; }
  auto copyNavigationHistoryFrom(const EditorView *other) -> void;
  auto updateEditorHistory(const IEditor *editor) -> void;
};

class SplitterOrView : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SplitterOrView)

public:
  explicit SplitterOrView(IEditor *editor = nullptr);
  explicit SplitterOrView(EditorView *view);
  ~SplitterOrView() override;

  auto split(Qt::Orientation orientation, bool activate_view = true) -> void;
  auto unsplit() -> void;
  auto isView() const -> bool { return m_view != nullptr; }
  auto isSplitter() const -> bool { return m_splitter != nullptr; }
  auto editor() const -> IEditor* { return m_view ? m_view->currentEditor() : nullptr; }
  auto editors() const -> QList<IEditor*> { return m_view ? m_view->editors() : QList<IEditor*>(); }
  auto hasEditor(IEditor *editor) const -> bool { return m_view && m_view->hasEditor(editor); }
  auto hasEditors() const -> bool { return m_view && m_view->editorCount() != 0; }
  auto view() const -> EditorView* { return m_view; }
  auto splitter() const -> QSplitter* { return m_splitter; }
  auto takeSplitter() -> QSplitter*;
  auto takeView() -> EditorView*;
  auto saveState() const -> QByteArray;
  auto restoreState(const QByteArray &) -> void;
  auto findFirstView() const -> EditorView*;
  auto findLastView() const -> EditorView*;
  auto findParentSplitter() const -> SplitterOrView*;
  auto sizeHint() const -> QSize override { return minimumSizeHint(); }
  auto minimumSizeHint() const -> QSize override;
  auto unsplitAll() -> void;

signals:
  auto splitStateChanged() -> void;

private:
  auto unsplitAllHelper() const -> QList<IEditor*>;

  QStackedLayout *m_layout;
  EditorView *m_view;
  QSplitter *m_splitter;
};

} // namespace Orca::Plugin::Core
