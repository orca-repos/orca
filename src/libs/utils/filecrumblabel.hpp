// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"
#include "fileutils.hpp"

#include <QLabel>

namespace Utils {

class ORCA_UTILS_EXPORT FileCrumbLabel : public QLabel {
  Q_OBJECT

public:
  FileCrumbLabel(QWidget *parent = nullptr);
  auto setPath(const FilePath &path) -> void;

signals:
  auto pathClicked(const FilePath &path) -> void;
};

} // Utils
