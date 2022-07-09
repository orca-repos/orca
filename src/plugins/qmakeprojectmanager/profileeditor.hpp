// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor.hpp>

namespace QmakeProjectManager {
namespace Internal {

class ProFileEditorFactory : public TextEditor::TextEditorFactory {
public:
  ProFileEditorFactory();
};

} // namespace Internal
} // namespace QmakeProjectManager
