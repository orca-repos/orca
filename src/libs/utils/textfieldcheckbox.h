// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QCheckBox>

namespace Utils {

// Documentation inside.
class ORCA_UTILS_EXPORT TextFieldCheckBox : public QCheckBox {
  Q_PROPERTY(QString compareText READ text WRITE setText)
  Q_PROPERTY(QString trueText READ trueText WRITE setTrueText)
  Q_PROPERTY(QString falseText READ falseText WRITE setFalseText)
  Q_OBJECT

public:
  explicit TextFieldCheckBox(const QString &text, QWidget *parent = nullptr);

  auto text() const -> QString;
  auto setText(const QString &s) -> void;
  auto setTrueText(const QString &t) -> void { m_trueText = t; }
  auto trueText() const -> QString { return m_trueText; }
  auto setFalseText(const QString &t) -> void { m_falseText = t; }
  auto falseText() const -> QString { return m_falseText; }

signals:
  auto textChanged(const QString &) -> void;

private:
  auto slotStateChanged(int) -> void;

  QString m_trueText;
  QString m_falseText;
};

} // namespace Utils
