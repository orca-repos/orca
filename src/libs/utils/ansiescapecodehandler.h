// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QTextCharFormat>

namespace Utils {

class ORCA_UTILS_EXPORT FormattedText {
public:
  FormattedText() = default;
  FormattedText(const FormattedText &other) = default;
  FormattedText(const QString &txt, const QTextCharFormat &fmt = QTextCharFormat()) : text(txt), format(fmt) { }

  QString text;
  QTextCharFormat format;
};

class ORCA_UTILS_EXPORT AnsiEscapeCodeHandler {
public:
  auto parseText(const FormattedText &input) -> QList<FormattedText>;
  auto endFormatScope() -> void;

private:
  auto setFormatScope(const QTextCharFormat &charFormat) -> void;

  bool m_previousFormatClosed = true;
  bool m_waitingForTerminator = false;
  QString m_alternateTerminator;
  QTextCharFormat m_previousFormat;
  QString m_pendingText;
};

} // namespace Utils
