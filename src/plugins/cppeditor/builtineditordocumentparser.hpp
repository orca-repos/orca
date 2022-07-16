// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "baseeditordocumentparser.hpp"
#include "cppeditor_global.hpp"

#include <cplusplus/CppDocument.h>

#include <QString>

namespace CppEditor {

class CPPEDITOR_EXPORT BuiltinEditorDocumentParser : public BaseEditorDocumentParser {
  Q_OBJECT

public:
  BuiltinEditorDocumentParser(const QString &filePath, int fileSizeLimitInMb = -1);

  auto releaseSourceAndAST() const -> bool;
  auto setReleaseSourceAndAST(bool release) -> void;
  auto document() const -> CPlusPlus::Document::Ptr;
  auto snapshot() const -> CPlusPlus::Snapshot;
  auto headerPaths() const -> ProjectExplorer::HeaderPaths;
  auto releaseResources() -> void;

signals:
  auto finished(CPlusPlus::Document::Ptr document, CPlusPlus::Snapshot snapshot) -> void;

public:
  using Ptr = QSharedPointer<BuiltinEditorDocumentParser>;
  static auto get(const QString &filePath) -> Ptr;

private:
  auto updateImpl(const QFutureInterface<void> &future, const UpdateParams &updateParams) -> void override;
  auto addFileAndDependencies(CPlusPlus::Snapshot *snapshot, QSet<Utils::FilePath> *toRemove, const Utils::FilePath &fileName) const -> void;

  struct ExtraState {
    QByteArray configFile;

    ProjectExplorer::HeaderPaths headerPaths;
    QString projectConfigFile;
    QStringList includedFiles;
    QStringList precompiledHeaders;

    CPlusPlus::Snapshot snapshot;
    bool forceSnapshotInvalidation = false;
  };

  auto extraState() const -> ExtraState;
  auto setExtraState(const ExtraState &extraState) -> void;

  bool m_releaseSourceAndAST = true;
  ExtraState m_extraState;
  const int m_fileSizeLimitInMb = -1;
};

} // namespace CppEditor
