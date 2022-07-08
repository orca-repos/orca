// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "command.hpp"

namespace TextEditor {

auto Command::isValid() const -> bool
{
  return !m_executable.isEmpty();
}

auto Command::executable() const -> QString
{
  return m_executable;
}

auto Command::setExecutable(const QString &executable) -> void
{
  m_executable = executable;
}

auto Command::options() const -> QStringList
{
  return m_options;
}

auto Command::addOption(const QString &option) -> void
{
  m_options << option;
}

auto Command::processing() const -> Processing
{
  return m_processing;
}

auto Command::setProcessing(const Processing &processing) -> void
{
  m_processing = processing;
}

auto Command::pipeAddsNewline() const -> bool
{
  return m_pipeAddsNewline;
}

auto Command::setPipeAddsNewline(bool pipeAddsNewline) -> void
{
  m_pipeAddsNewline = pipeAddsNewline;
}

auto Command::returnsCRLF() const -> bool
{
  return m_returnsCRLF;
}

auto Command::setReturnsCRLF(bool returnsCRLF) -> void
{
  m_returnsCRLF = returnsCRLF;
}

} // namespace TextEditor
