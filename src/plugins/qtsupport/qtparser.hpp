// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QRegularExpression>

#include "qtsupport_global.hpp"
#include <projectexplorer/ioutputparser.hpp>

namespace QtSupport {

// Parser for Qt-specific utilities like moc, uic, etc.

class QTSUPPORT_EXPORT QtParser : public ProjectExplorer::OutputTaskParser {
  Q_OBJECT

public:
  QtParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;

  QRegularExpression m_mocRegExp;
  QRegularExpression m_uicRegExp;
  QRegularExpression m_translationRegExp;
};

} // namespace QtSupport
