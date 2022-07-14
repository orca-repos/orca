// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.hpp>

#include <QObject>
#include <QString>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QKeySequence)

namespace Orca::Plugin::Core {

struct ShortcutItem;

class CommandsFile final : public QObject {
  Q_OBJECT

public:
  explicit CommandsFile(Utils::FilePath filename);

  auto importCommands() const -> QMap<QString, QList<QKeySequence>>;
  auto exportCommands(const QList<ShortcutItem*> &items) const -> bool;

private:
  Utils::FilePath m_file_path;
};

} // namespace Orca::Plugin::Core
