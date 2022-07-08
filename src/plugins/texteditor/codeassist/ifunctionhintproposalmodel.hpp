// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposalmodel.hpp"

#include <texteditor/texteditor_global.hpp>

QT_FORWARD_DECLARE_CLASS(QString);

namespace TextEditor {

class TEXTEDITOR_EXPORT IFunctionHintProposalModel : public IAssistProposalModel {
public:
  IFunctionHintProposalModel();
  ~IFunctionHintProposalModel() override;

  virtual auto activeArgument(const QString &prefix) const -> int = 0;
  virtual auto id(int index) const -> QString;
};

using FunctionHintProposalModelPtr = QSharedPointer<IFunctionHintProposalModel>;

} // TextEditor
