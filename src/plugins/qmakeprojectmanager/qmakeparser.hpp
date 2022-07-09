// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"

#include <projectexplorer/ioutputparser.hpp>

#include <QRegularExpression>

namespace QmakeProjectManager {

class QMAKEPROJECTMANAGER_EXPORT QMakeParser : public ProjectExplorer::OutputTaskParser {
  Q_OBJECT

public:
  QMakeParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;

  const QRegularExpression m_error;
};

} // namespace QmakeProjectManager
