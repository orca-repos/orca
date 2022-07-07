// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <core/progressmanager/futureprogress.hpp>

#include <utils/shellcommand.hpp>

#include <QPointer>

namespace Core {

class CORE_EXPORT ShellCommand final : public Utils::ShellCommand {
  Q_OBJECT

public:
  ShellCommand(const Utils::FilePath &working_directory, const Utils::Environment &environment);

  auto futureProgress() const -> FutureProgress*;

protected:
  auto addTask(QFuture<void> &future) -> void override;
  auto coreAboutToClose() -> void;

private:
  QPointer<FutureProgress> m_progress;
};

} // namespace Core
