// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_finddialog.h"
#include "findplugin.hpp"

#include <QList>

QT_FORWARD_DECLARE_CLASS(QCompleter)

namespace Core {
class IFindFilter;

namespace Internal {

class FindToolWindow final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(FindToolWindow)

public:
  explicit FindToolWindow(QWidget *parent = nullptr);
  ~FindToolWindow() override;

  static auto instance() -> FindToolWindow*;
  auto setFindFilters(const QList<IFindFilter*> &filters) -> void;
  auto findFilters() const -> QList<IFindFilter*>;
  auto setFindText(const QString &text) const -> void;
  auto setCurrentFilter(IFindFilter *filter) -> void;
  auto readSettings() -> void;
  auto writeSettings() -> void;

protected:
  auto event(QEvent *event) -> bool override;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  auto search() const -> void;
  auto replace() const -> void;
  auto setCurrentFilter(int index) -> void;
  auto updateButtonStates() const -> void;
  auto updateFindFlags() const -> void;
  auto updateFindFilterName(IFindFilter *filter) const -> void;
  static auto findCompleterActivated(const QModelIndex &index) -> void;
  auto acceptAndGetParameters(QString *term, IFindFilter **filter) const -> void;

  Ui::FindDialog m_ui{};
  QList<IFindFilter*> m_filters;
  QCompleter *m_find_completer;
  QWidgetList m_config_widgets;
  IFindFilter *m_current_filter;
  QWidget *m_config_widget;
};

} // namespace Internal
} // namespace Core
