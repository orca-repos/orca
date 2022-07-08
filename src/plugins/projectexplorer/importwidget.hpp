// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

namespace Utils {
class PathChooser;
class FilePath;
} // namespace Utils

namespace ProjectExplorer {
namespace Internal {

class ImportWidget : public QWidget {
  Q_OBJECT

public:
  explicit ImportWidget(QWidget *parent = nullptr);

  auto setCurrentDirectory(const Utils::FilePath &dir) -> void;
  auto ownsReturnKey() const -> bool;

signals:
  auto importFrom(const Utils::FilePath &dir) -> void;

private:
  auto handleImportRequest() -> void;

  Utils::PathChooser *m_pathChooser;
  bool m_ownsReturnKey = false;
};

} // namespace Internal
} // namespace ProjectExplorer
