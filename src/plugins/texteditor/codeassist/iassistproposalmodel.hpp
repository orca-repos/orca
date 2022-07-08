// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <QSharedPointer>

QT_FORWARD_DECLARE_CLASS(QString)

namespace TextEditor {

class TEXTEDITOR_EXPORT IAssistProposalModel {
public:
  IAssistProposalModel();
  virtual ~IAssistProposalModel();

  virtual auto reset() -> void = 0;
  virtual auto size() const -> int = 0;
  virtual auto text(int index) const -> QString = 0;
};

using ProposalModelPtr = QSharedPointer<IAssistProposalModel>;

} // TextEditor
