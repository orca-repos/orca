// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/basehoverhandler.hpp>
#include <texteditor/codeassist/keywordscompletionassist.hpp>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

namespace QmakeProjectManager {
namespace Internal {

class ProFileHoverHandler : public TextEditor::BaseHoverHandler {
public:
  ProFileHoverHandler();

private:
  auto identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void override;
  auto identifyQMakeKeyword(const QString &text, int pos) -> void;

  enum ManualKind {
    VariableManual,
    FunctionManual,
    UnknownManual
  };

  auto manualName() const -> QString;
  auto identifyDocFragment(ManualKind manualKind, const QString &keyword) -> void;

  QString m_docFragment;
  ManualKind m_manualKind = UnknownManual;
  const TextEditor::Keywords m_keywords;
};

} // namespace Internal
} // namespace QmakeProjectManager
