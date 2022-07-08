// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <codeassist/iassistprovider.hpp>

namespace TextEditor {
namespace Internal {

class ClipboardAssistProvider: public IAssistProvider
{
public:
    ClipboardAssistProvider(QObject *parent = nullptr) : IAssistProvider(parent) {}
    auto runType() const -> RunType override;
    auto createProcessor(const AssistInterface *) const -> IAssistProcessor* override;
};

} // namespace Internal
} // namespace TextEditor
