// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/actionmanager/commandmappings.h>
#include <core/dialogs/ioptionspage.h>

#include <QGridLayout>
#include <QKeySequence>
#include <QPointer>
#include <QPushButton>

#include <array>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QLabel;
QT_END_NAMESPACE

namespace Core {

class Command;

namespace Internal {

class ActionManagerPrivate;
class ShortcutSettingsWidget;

struct ShortcutItem {
  Command *m_cmd{};
  QList<QKeySequence> m_keys;
  QTreeWidgetItem *m_item{};
};

class ShortcutButton final : public QPushButton {
  Q_OBJECT

public:
  explicit ShortcutButton(QWidget *parent = nullptr);

  auto sizeHint() const -> QSize override;

signals:
  auto keySequenceChanged(const QKeySequence &sequence) -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *evt) -> bool override;

private:
  auto updateText() -> void;
  auto handleToggleChange(bool toggle_state) -> void;

  QString m_checked_text;
  QString m_unchecked_text;
  mutable int m_preferred_width = -1;
  std::array<int, 4> m_key;
  int m_key_num = 0;
};

class ShortcutInput final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ShortcutInput)

public:
  ShortcutInput();
  ~ShortcutInput() override;

  using conflict_checker = std::function<bool(QKeySequence)>;

  auto addToLayout(QGridLayout *layout, int row) const -> void;
  auto setKeySequence(const QKeySequence &key) const -> void;
  auto keySequence() const -> QKeySequence;
  auto setConflictChecker(const conflict_checker &fun) -> void;

signals:
  auto changed() -> void;
  auto showConflictsRequested() -> void;

private:
  conflict_checker m_conflict_checker;
  QPointer<QLabel> m_shortcut_label;
  QPointer<Utils::FancyLineEdit> m_shortcut_edit;
  QPointer<ShortcutButton> m_shortcut_button;
  QPointer<QLabel> m_warning_label;
};

class ShortcutSettings final : public IOptionsPage {
public:
  ShortcutSettings();

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;

private:
  QPointer<ShortcutSettingsWidget> m_widget;
};

} // namespace Internal
} // namespace Core
