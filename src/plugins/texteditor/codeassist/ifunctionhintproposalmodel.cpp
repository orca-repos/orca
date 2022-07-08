// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ifunctionhintproposalmodel.hpp"

#include <QString>

using namespace TextEditor;

IFunctionHintProposalModel::IFunctionHintProposalModel() = default;
IFunctionHintProposalModel::~IFunctionHintProposalModel() = default;

auto IFunctionHintProposalModel::id(int /*index*/) const -> QString
{
  return QString();
}
