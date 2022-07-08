// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QScrollArea>

QT_BEGIN_NAMESPACE
class QFormLayout;
class QGridLayout;
class QLineEdit;
class QLabel;
QT_END_NAMESPACE

namespace ProjectExplorer {

class ToolChain;

// --------------------------------------------------------------------------
// ToolChainConfigWidget
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT ToolChainConfigWidget : public QScrollArea {
  Q_OBJECT

public:
  explicit ToolChainConfigWidget(ToolChain *tc);

  auto toolChain() const -> ToolChain*;
  auto apply() -> void;
  auto discard() -> void;
  auto isDirty() const -> bool;
  auto makeReadOnly() -> void;

signals:
  auto dirty() -> void;

protected:
  auto setErrorMessage(const QString &) -> void;
  auto clearErrorMessage() -> void;
  virtual auto applyImpl() -> void = 0;
  virtual auto discardImpl() -> void = 0;
  virtual auto isDirtyImpl() const -> bool = 0;
  virtual auto makeReadOnlyImpl() -> void = 0;
  auto addErrorLabel() -> void;
  static auto splitString(const QString &s) -> QStringList;
  QFormLayout *m_mainLayout;
  QLineEdit *m_nameLineEdit;

private:
  ToolChain *m_toolChain;
  QLabel *m_errorLabel = nullptr;
};

} // namespace ProjectExplorer
