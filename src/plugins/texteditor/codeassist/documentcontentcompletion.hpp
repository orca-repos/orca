// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "completionassistprovider.hpp"

#include <texteditor/texteditorconstants.hpp>

namespace TextEditor {

class TEXTEDITOR_EXPORT DocumentContentCompletionProvider : public CompletionAssistProvider {
  Q_OBJECT

public:
  DocumentContentCompletionProvider(const QString &snippetGroup = QString(Constants::TEXT_SNIPPET_GROUP_ID));

  auto runType() const -> RunType override;
  auto createProcessor(const AssistInterface *) const -> IAssistProcessor* override;

private:
  QString m_snippetGroup;
};

} // TextEditor
