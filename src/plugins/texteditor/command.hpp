// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QString>
#include <QStringList>

namespace TextEditor {

class TEXTEDITOR_EXPORT Command {
public:
  enum Processing {
    FileProcessing,
    PipeProcessing
  };

  auto isValid() const -> bool;
  auto executable() const -> QString;
  auto setExecutable(const QString &executable) -> void;
  auto options() const -> QStringList;
  auto addOption(const QString &option) -> void;
  auto processing() const -> Processing;
  auto setProcessing(const Processing &processing) -> void;
  auto pipeAddsNewline() const -> bool;
  auto setPipeAddsNewline(bool pipeAddsNewline) -> void;
  auto returnsCRLF() const -> bool;
  auto setReturnsCRLF(bool returnsCRLF) -> void;

private:
  QString m_executable;
  QStringList m_options;
  Processing m_processing = FileProcessing;
  bool m_pipeAddsNewline = false;
  bool m_returnsCRLF = false;
};

} // namespace TextEditor
