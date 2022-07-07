// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QComboBox>

namespace Utils {

// Documentation inside.
class ORCA_UTILS_EXPORT TextFieldComboBox : public QComboBox {
  Q_PROPERTY(QString indexText READ text WRITE setText)
  Q_OBJECT

public:
  explicit TextFieldComboBox(QWidget *parent = nullptr);

  auto text() const -> QString;
  auto setText(const QString &s) -> void;
  auto setItems(const QStringList &displayTexts, const QStringList &values) -> void;

signals:
  auto text4Changed(const QString &) -> void; // Do not conflict with Qt 3 compat signal.

private:
  auto slotCurrentIndexChanged(int) -> void;

  inline auto valueAt(int) const -> QString;
};

} // namespace Utils
