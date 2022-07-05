// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "macroexpander.h"

#include <QWidget>

#include <functional>

namespace Utils {

namespace Internal {
class VariableChooserPrivate;
}

class ORCA_UTILS_EXPORT VariableChooser : public QWidget {
  Q_OBJECT

public:
  explicit VariableChooser(QWidget *parent = nullptr);
  ~VariableChooser() override;

  auto addMacroExpanderProvider(const Utils::MacroExpanderProvider &provider) -> void;
  auto addSupportedWidget(QWidget *textcontrol, const QByteArray &ownName = QByteArray()) -> void;

  static auto addSupportForChildWidgets(QWidget *parent, Utils::MacroExpander *expander) -> void;

protected:
  auto event(QEvent *ev) -> bool override;
  auto eventFilter(QObject *, QEvent *event) -> bool override;

private:
  Internal::VariableChooserPrivate *d;
};

} // namespace Utils
