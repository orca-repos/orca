// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppindexingsupport.hpp"
#include "cppmodelmanager.hpp"

#include <utils/futuresynchronizer.hpp>

namespace CppEditor::Internal {

class BuiltinIndexingSupport : public CppIndexingSupport {
public:
  BuiltinIndexingSupport();
  ~BuiltinIndexingSupport() override;

  auto refreshSourceFiles(const QSet<QString> &sourceFiles, CppModelManager::ProgressNotificationMode mode) -> QFuture<void> override;
  auto createSymbolSearcher(const SymbolSearcher::Parameters &parameters, const QSet<QString> &fileNames) -> SymbolSearcher* override;
  static auto isFindErrorsIndexingActive() -> bool;

private:
  Utils::FutureSynchronizer m_synchronizer;
};

} // namespace CppEditor::Internal
