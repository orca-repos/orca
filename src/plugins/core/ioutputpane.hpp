// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/fancylineedit.hpp>
#include <utils/id.hpp>

#include <QObject>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
class QAction;
class QWidget;
QT_END_NAMESPACE

namespace Core {
class CommandButton;
class IContext;
class OutputWindow;

class CORE_EXPORT IOutputPane : public QObject {
  Q_OBJECT

public:
  explicit IOutputPane(QObject *parent = nullptr);
  ~IOutputPane() override;

  virtual auto outputWidget(QWidget *parent) -> QWidget* = 0;
  virtual auto toolBarWidgets() const -> QList<QWidget*>;
  virtual auto displayName() const -> QString = 0;
  virtual auto outputWindows() const -> QList<OutputWindow*> { return {}; }
  virtual auto ensureWindowVisible(OutputWindow *) -> void { }
  virtual auto priorityInStatusBar() const -> int = 0;
  virtual auto clearContents() -> void = 0;
  virtual auto visibilityChanged(bool visible) -> void;
  virtual auto setFocus() -> void = 0;
  virtual auto hasFocus() const -> bool = 0;
  virtual auto canFocus() const -> bool = 0;
  virtual auto canNavigate() const -> bool = 0;
  virtual auto canNext() const -> bool = 0;
  virtual auto canPrevious() const -> bool = 0;
  virtual auto goToNext() -> void = 0;
  virtual auto goToPrev() -> void = 0;
  auto setFont(const QFont &font) -> void;
  auto setWheelZoomEnabled(bool enabled) -> void;

  enum Flag {
    NoModeSwitch = 0,
    ModeSwitch = 1,
    WithFocus = 2,
    EnsureSizeHint = 4
  };

  Q_DECLARE_FLAGS(Flags, Flag)

public slots:
  auto popup(const int flags) -> void
  {
    emit showPage(flags);
  }

  auto hide() -> void
  {
    emit hidePage();
  }

  auto toggle(const int flags) -> void
  {
    emit togglePage(flags);
  }

  auto navigateStateChanged() -> void
  {
    emit navigateStateUpdate();
  }

  auto flash() -> void
  {
    emit flashButton();
  }

  auto setIconBadgeNumber(const int number) -> void
  {
    emit setBadgeNumber(number);
  }

signals:
  auto showPage(int flags) -> void;
  auto hidePage() -> void;
  auto togglePage(int flags) -> void;
  auto navigateStateUpdate() -> void;
  auto flashButton() -> void;
  auto setBadgeNumber(int number) -> void;
  auto zoomInRequested(int range) -> void;
  auto zoomOutRequested(int range) -> void;
  auto resetZoomRequested() -> void;
  auto wheelZoomEnabledChanged(bool enabled) -> void;
  auto fontChanged(const QFont &font) -> void;

protected:
  auto setupFilterUi(const QString &history_key) -> void;
  auto filterText() const -> QString;
  auto filterUsesRegexp() const -> bool { return m_filter_regexp; }
  auto filterIsInverted() const -> bool { return m_invert_filter; }
  auto filterCaseSensitivity() const -> Qt::CaseSensitivity { return m_filter_case_sensitivity; }
  auto setFilteringEnabled(bool enable) const -> void;
  auto filterWidget() const -> QWidget* { return m_filter_output_line_edit; }
  auto setupContext(const char *context, QWidget *widget) -> void;
  auto setZoomButtonsEnabled(bool enabled) const -> void;

private:
  virtual auto updateFilter() -> void;

  auto filterOutputButtonClicked() const -> void;
  auto setCaseSensitive(bool case_sensitive) -> void;
  auto setRegularExpressions(bool regular_expressions) -> void;
  auto filterRegexpActionId() const -> Utils::Id;
  auto filterCaseSensitivityActionId() const -> Utils::Id;
  auto filterInvertedActionId() const -> Utils::Id;

  Core::CommandButton *const m_zoom_in_button;
  Core::CommandButton *const m_zoom_out_button;
  QAction *m_filter_action_regexp = nullptr;
  QAction *m_filter_action_case_sensitive = nullptr;
  QAction *m_invert_filter_action = nullptr;
  Utils::FancyLineEdit *m_filter_output_line_edit = nullptr;
  IContext *m_context = nullptr;
  bool m_filter_regexp = false;
  bool m_invert_filter = false;
  Qt::CaseSensitivity m_filter_case_sensitivity = Qt::CaseInsensitive;
};

} // namespace Core

 Q_DECLARE_OPERATORS_FOR_FLAGS(Core::IOutputPane::Flags)
