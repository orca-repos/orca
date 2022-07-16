// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakefilecompletionassist.hpp"

#include "cmakekitinformation.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmaketool.hpp"

#include <texteditor/codeassist/assistinterface.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>

#include <QFileInfo>

using namespace CMakeProjectManager::Internal;
using namespace TextEditor;
using namespace ProjectExplorer;

// -------------------------------
// CMakeFileCompletionAssistProvider
// -------------------------------

auto CMakeFileCompletionAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new CMakeFileCompletionAssist;
}

CMakeFileCompletionAssist::CMakeFileCompletionAssist() : KeywordsCompletionAssistProcessor(Keywords())
{
  setSnippetGroup(Constants::CMAKE_SNIPPETS_GROUP_ID);
  setDynamicCompletionFunction(&TextEditor::pathComplete);
}

auto CMakeFileCompletionAssist::perform(const AssistInterface *interface) -> IAssistProposal*
{
  Keywords kw;
  const auto &filePath = interface->filePath();
  if (!filePath.isEmpty() && filePath.toFileInfo().isFile()) {
    auto p = SessionManager::projectForFile(filePath);
    if (p && p->activeTarget()) {
      auto cmake = CMakeKitAspect::cmakeTool(p->activeTarget()->kit());
      if (cmake && cmake->isValid())
        kw = cmake->keywords();
    }
  }

  setKeywords(kw);
  return KeywordsCompletionAssistProcessor::perform(interface);
}
