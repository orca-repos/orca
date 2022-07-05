// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environment.h"
#include "namevaluesdialog.h"
#include <thread>

namespace Utils {

class ORCA_UTILS_EXPORT EnvironmentDialog : public NameValuesDialog {
  Q_OBJECT

public:
  static auto getEnvironmentItems(QWidget *parent = nullptr, const EnvironmentItems &initial = {}, const QString &placeholderText = {}, Polisher polish = {}) -> Utils::optional<EnvironmentItems>;
};

} // namespace Utils
