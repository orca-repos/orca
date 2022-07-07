// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QEvent;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

class VersionDialog final : public QDialog {
  Q_OBJECT

public:
  explicit VersionDialog(QWidget *parent);

  auto event(QEvent *event) -> bool override;
};

} // namespace Internal
} // namespace Core
