// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "locator.hpp"

#include <utils/optional.hpp>

#include <extensionsystem/iplugin.hpp>

#include <QFutureWatcher>
#include <QPointer>
#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QAction;
class QMenu;
QT_END_NAMESPACE

namespace Utils {
class FancyLineEdit;
}

namespace Core {
namespace Internal {

class LocatorModel;
class CompletionList;

class LocatorWidget final : public QWidget {
  Q_OBJECT

public:
  explicit LocatorWidget(Locator *locator);
  ~LocatorWidget() override;

  auto showText(const QString &text, int selection_start = -1, int selection_length = 0) -> void;
  auto currentText() const -> QString;
  auto model() const -> QAbstractItemModel*;
  auto updatePlaceholderText(const Command *command) const -> void;
  auto scheduleAcceptEntry(const QModelIndex &index) -> void;
  static auto aboutToShutdown(const std::function<void()> &emit_asynchronous_shutdown_finished) -> ExtensionSystem::IPlugin::ShutdownFlag;

signals:
  auto showCurrentItemToolTip() -> void;
  auto lostFocus() -> void;
  auto hidePopup() -> void;
  auto selectRow(int row) -> void;
  auto handleKey(QKeyEvent *key_event) -> void; // only use with DirectConnection, event is deleted
  auto parentChanged() -> void;
  auto showPopup() -> void;

private:
  auto showPopupDelayed() -> void;
  auto showPopupNow() -> void;
  auto acceptEntry(int row) -> void;
  static auto showConfigureDialog() -> void;
  auto addSearchResults(int first_index, int end_index) -> void;
  auto handleSearchFinished() -> void;
  auto updateFilterList() const -> void;
  auto isInMainWindow() const -> bool;
  auto updatePreviousFocusWidget(QWidget *previous, const QWidget *current) -> void;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;
  auto updateCompletionList(const QString &text) -> void;
  static auto filtersFor(const QString &text, QString &search_text) -> QList<ILocatorFilter*>;
  auto setProgressIndicatorVisible(bool visible) const -> void;

  LocatorModel *m_locator_model = nullptr;
  static bool m_shutting_down;
  static QFuture<void> m_shared_future;
  static LocatorWidget *m_shared_future_origin;
  QMenu *m_filter_menu = nullptr;
  QAction *m_refresh_action = nullptr;
  QAction *m_configure_action = nullptr;
  Utils::FancyLineEdit *m_file_line_edit = nullptr;
  QTimer m_show_popup_timer;
  QFutureWatcher<LocatorFilterEntry> *m_entries_watcher = nullptr;
  QString m_requested_completion_text;
  bool m_needs_clear_result = true;
  bool m_update_requested = false;
  bool m_rerun_after_finished = false;
  bool m_possible_tool_tip_request = false;
  QWidget *m_progress_indicator = nullptr;
  QTimer m_show_progress_timer;
  Utils::optional<int> m_row_requested_for_accept;
  QPointer<QWidget> m_previous_focus_widget;
};

class LocatorPopup : public QWidget {
public:
  explicit LocatorPopup(LocatorWidget *locator_widget, QWidget *parent = nullptr);

  auto completionList() const -> CompletionList*;
  auto inputWidget() const -> LocatorWidget*;
  auto focusOutEvent(QFocusEvent *event) -> void override;
  auto event(QEvent *event) -> bool override;
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

protected:
  auto preferredSize() const -> QSize;
  virtual auto doUpdateGeometry() -> void;
  virtual auto inputLostFocus() -> void;

  QPointer<QWidget> m_window;
  CompletionList *m_tree = nullptr;

private:
  auto updateWindow() -> void;

  LocatorWidget *m_input_widget = nullptr;
};

auto createStaticLocatorWidget(Locator *locator) -> LocatorWidget*;
auto createLocatorPopup(Locator *locator, QWidget *parent) -> LocatorPopup*;

} // namespace Internal
} // namespace Core
