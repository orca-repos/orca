// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QString;
class QVariant;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT SettingsSelector : public QWidget {
  Q_OBJECT

public:
  explicit SettingsSelector(QWidget *parent = nullptr);
  ~SettingsSelector() override;

  auto setConfigurationModel(QAbstractItemModel *model) -> void;
  auto configurationModel() const -> QAbstractItemModel*;
  auto setLabelText(const QString &text) -> void;
  auto labelText() const -> QString;
  auto setCurrentIndex(int index) -> void;
  auto setAddMenu(QMenu *) -> void;
  auto addMenu() const -> QMenu*;
  auto currentIndex() const -> int;

signals:
  auto add() -> void;
  auto remove(int index) -> void;
  auto rename(int index, const QString &newName) -> void;
  auto currentChanged(int index) -> void;

private:
  auto removeButtonClicked() -> void;
  auto renameButtonClicked() -> void;
  auto updateButtonState() -> void;

  QLabel *m_label;
  QComboBox *m_configurationCombo;
  QPushButton *m_addButton;
  QPushButton *m_removeButton;
  QPushButton *m_renameButton;
};

} // namespace Utils
