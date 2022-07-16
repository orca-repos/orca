// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "quickfix.hpp"

using namespace TextEditor;

QuickFixOperation::QuickFixOperation(int priority)
{
  setPriority(priority);
}

QuickFixOperation::~QuickFixOperation() = default;

auto QuickFixOperation::priority() const -> int
{
  return _priority;
}

auto QuickFixOperation::setPriority(int priority) -> void
{
  _priority = priority;
}

auto QuickFixOperation::description() const -> QString
{
  return _description;
}

auto QuickFixOperation::setDescription(const QString &description) -> void
{
  _description = description;
}
