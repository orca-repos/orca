// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QGroupBox>

namespace TextEditor {
namespace Internal {
namespace Ui {
class TabSettingsWidget;
} // namespace Ui
} // namespace Internal

class TabSettings;

class TEXTEDITOR_EXPORT TabSettingsWidget : public QGroupBox {
  Q_OBJECT

public:
  enum CodingStyleLink {
    CppLink,
    QtQuickLink
  };

  explicit TabSettingsWidget(QWidget *parent = nullptr);
  ~TabSettingsWidget() override;

  auto tabSettings() const -> TabSettings;
  auto setCodingStyleWarningVisible(bool visible) -> void;
  auto setTabSettings(const TabSettings &s) -> void;

signals:
  auto settingsChanged(const TabSettings &) -> void;
  auto codingStyleLinkClicked(CodingStyleLink link) -> void;

private:
  auto slotSettingsChanged() -> void;
  auto codingStyleLinkActivated(const QString &linkString) -> void;

  Internal::Ui::TabSettingsWidget *ui;
};

} // namespace TextEditor
