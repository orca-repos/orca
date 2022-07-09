// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace TextEditor { class Keywords; }
namespace QmakeProjectManager {
namespace Internal {

auto qmakeKeywords() -> const TextEditor::Keywords&;

} // namespace Internal
} // namespace QmakeProjectManager
