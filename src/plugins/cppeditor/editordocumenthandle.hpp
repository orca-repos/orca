// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "senddocumenttracker.hpp"

#include <QString>

namespace CppEditor {

class BaseEditorDocumentProcessor;

class CPPEDITOR_EXPORT CppEditorDocumentHandle {
public:
  virtual ~CppEditorDocumentHandle();

  enum RefreshReason {
    None,
    ProjectUpdate,
    Other,
  };

  auto refreshReason() const -> RefreshReason;
  auto setRefreshReason(const RefreshReason &refreshReason) -> void;

  // For the Working Copy
  virtual auto filePath() const -> QString = 0;
  virtual auto contents() const -> QByteArray = 0;
  virtual auto revision() const -> unsigned = 0;

  // For updating if new project info is set
  virtual auto processor() const -> BaseEditorDocumentProcessor* = 0;
  virtual auto resetProcessor() -> void = 0;
  auto sendTracker() -> SendDocumentTracker&;

private:
  SendDocumentTracker m_sendTracker;
  RefreshReason m_refreshReason = None;
};

} // namespace CppEditor
