// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "builtineditordocumentparser.hpp"

#include "cppsourceprocessor.hpp"

#include <projectexplorer/projectmacro.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace CPlusPlus;

namespace CppEditor {

static auto overwrittenToolchainDefines(const ProjectPart &projectPart) -> QByteArray
{
  QByteArray defines;

  // MSVC's predefined macros like __FUNCSIG__ expand to itself.
  // We can't parse this, so redefine to the empty string literal.
  if (projectPart.toolchainType == ProjectExplorer::Constants::MSVC_TOOLCHAIN_TYPEID) {
    defines += "#define __FUNCSIG__ \"void __cdecl someLegalAndLongishFunctionNameThatWorksAroundQTCREATORBUG-24580(void)\"\n" "#define __FUNCDNAME__ \"?someLegalAndLongishFunctionNameThatWorksAroundQTCREATORBUG-24580@@YAXXZ\"\n" "#define __FUNCTION__ \"someLegalAndLongishFunctionNameThatWorksAroundQTCREATORBUG-24580\"\n";
  }

  return defines;
}

BuiltinEditorDocumentParser::BuiltinEditorDocumentParser(const QString &filePath, int fileSizeLimitInMb) : BaseEditorDocumentParser(filePath), m_fileSizeLimitInMb(fileSizeLimitInMb)
{
  qRegisterMetaType<CPlusPlus::Snapshot>("CPlusPlus::Snapshot");
}

auto BuiltinEditorDocumentParser::updateImpl(const QFutureInterface<void> &future, const UpdateParams &updateParams) -> void
{
  if (filePath().isEmpty())
    return;

  const auto baseConfig = configuration();
  const auto releaseSourceAndAST_ = releaseSourceAndAST();

  auto baseState = state();
  auto state = extraState();
  auto workingCopy = updateParams.workingCopy;

  auto invalidateSnapshot = false, invalidateConfig = false, editorDefinesChanged_ = false;

  auto modelManager = CppModelManager::instance();
  auto configFile = modelManager->codeModelConfiguration();
  ProjectExplorer::HeaderPaths headerPaths;
  QStringList includedFiles;
  QStringList precompiledHeaders;
  QString projectConfigFile;
  LanguageFeatures features = LanguageFeatures::defaultFeatures();

  baseState.projectPartInfo = determineProjectPart(filePath(), baseConfig.preferredProjectPartId, baseState.projectPartInfo, updateParams.activeProject, updateParams.languagePreference, updateParams.projectsUpdated);
  emit projectPartInfoUpdated(baseState.projectPartInfo);

  if (state.forceSnapshotInvalidation) {
    invalidateSnapshot = true;
    state.forceSnapshotInvalidation = false;
  }

  if (const auto part = baseState.projectPartInfo.projectPart) {
    configFile += ProjectExplorer::Macro::toByteArray(part->toolChainMacros);
    configFile += overwrittenToolchainDefines(*part.data());
    configFile += ProjectExplorer::Macro::toByteArray(part->projectMacros);
    if (!part->projectConfigFile.isEmpty())
      configFile += ProjectPart::readProjectConfigFile(part->projectConfigFile);
    headerPaths = part->headerPaths;
    projectConfigFile = part->projectConfigFile;
    includedFiles = part->includedFiles;
    if (baseConfig.usePrecompiledHeaders)
      precompiledHeaders = part->precompiledHeaders;
    features = part->languageFeatures;
  }

  if (configFile != state.configFile) {
    state.configFile = configFile;
    invalidateSnapshot = true;
    invalidateConfig = true;
  }

  if (baseConfig.editorDefines != baseState.editorDefines) {
    baseState.editorDefines = baseConfig.editorDefines;
    invalidateSnapshot = true;
    editorDefinesChanged_ = true;
  }

  if (headerPaths != state.headerPaths) {
    state.headerPaths = headerPaths;
    invalidateSnapshot = true;
  }

  if (projectConfigFile != state.projectConfigFile) {
    state.projectConfigFile = projectConfigFile;
    invalidateSnapshot = true;
  }

  if (includedFiles != state.includedFiles) {
    state.includedFiles = includedFiles;
    invalidateSnapshot = true;
  }

  if (precompiledHeaders != state.precompiledHeaders) {
    state.precompiledHeaders = precompiledHeaders;
    invalidateSnapshot = true;
  }

  unsigned rev = 0;
  if (Document::Ptr doc = state.snapshot.document(filePath()))
    rev = doc->revision();
  else
    invalidateSnapshot = true;

  Snapshot globalSnapshot = modelManager->snapshot();

  if (invalidateSnapshot) {
    state.snapshot = Snapshot();
  } else {
    // Remove changed files from the snapshot
    QSet<Utils::FilePath> toRemove;
    foreach(const Document::Ptr &doc, state.snapshot) {
      const Utils::FilePath fileName = Utils::FilePath::fromString(doc->fileName());
      if (workingCopy.contains(fileName)) {
        if (workingCopy.get(fileName).second != doc->editorRevision())
          addFileAndDependencies(&state.snapshot, &toRemove, fileName);
        continue;
      }
      Document::Ptr otherDoc = globalSnapshot.document(fileName);
      if (!otherDoc.isNull() && otherDoc->revision() != doc->revision())
        addFileAndDependencies(&state.snapshot, &toRemove, fileName);
    }

    if (!toRemove.isEmpty()) {
      invalidateSnapshot = true;
      foreach(const Utils::FilePath &fileName, toRemove)
        state.snapshot.remove(fileName);
    }
  }

  // Update the snapshot
  if (invalidateSnapshot) {
    const auto configurationFileName = CppModelManager::configurationFileName();
    if (invalidateConfig)
      state.snapshot.remove(configurationFileName);
    if (!state.snapshot.contains(configurationFileName))
      workingCopy.insert(configurationFileName, state.configFile);
    state.snapshot.remove(filePath());

    static const auto editorDefinesFileName = CppModelManager::editorConfigurationFileName();
    if (editorDefinesChanged_) {
      state.snapshot.remove(editorDefinesFileName);
      workingCopy.insert(editorDefinesFileName, baseState.editorDefines);
    }

    Internal::CppSourceProcessor sourceProcessor(state.snapshot, [&](const Document::Ptr &doc) {
      const QString fileName = doc->fileName();
      const bool isInEditor = fileName == filePath();
      Document::Ptr otherDoc = modelManager->document(fileName);
      unsigned newRev = otherDoc.isNull() ? 1U : otherDoc->revision() + 1;
      if (isInEditor)
        newRev = qMax(rev + 1, newRev);
      doc->setRevision(newRev);
      modelManager->emitDocumentUpdated(doc);
      if (releaseSourceAndAST_)
        doc->releaseSourceAndAST();
    });
    sourceProcessor.setFileSizeLimitInMb(m_fileSizeLimitInMb);
    sourceProcessor.setCancelChecker([future]() {
      return future.isCanceled();
    });

    Snapshot globalSnapshot = modelManager->snapshot();
    globalSnapshot.remove(filePath());
    sourceProcessor.setGlobalSnapshot(globalSnapshot);
    sourceProcessor.setWorkingCopy(workingCopy);
    sourceProcessor.setHeaderPaths(state.headerPaths);
    sourceProcessor.setLanguageFeatures(features);
    sourceProcessor.run(configurationFileName);
    if (baseConfig.usePrecompiledHeaders) {
      foreach(const QString &precompiledHeader, state.precompiledHeaders)
        sourceProcessor.run(precompiledHeader);
    }
    if (!baseState.editorDefines.isEmpty())
      sourceProcessor.run(editorDefinesFileName);
    auto includedFiles = state.includedFiles;
    if (baseConfig.usePrecompiledHeaders)
      includedFiles << state.precompiledHeaders;
    includedFiles.removeDuplicates();
    sourceProcessor.run(filePath(), includedFiles);
    state.snapshot = sourceProcessor.snapshot();
    Snapshot newSnapshot = state.snapshot.simplified(state.snapshot.document(filePath()));
    for (Snapshot::const_iterator i = state.snapshot.begin(), ei = state.snapshot.end(); i != ei; ++i) {
      if (Client::isInjectedFile(i.key().toString()))
        newSnapshot.insert(i.value());
    }
    state.snapshot = newSnapshot;
    state.snapshot.updateDependencyTable();
  }

  setState(baseState);
  setExtraState(state);

  if (invalidateSnapshot) emit finished(state.snapshot.document(filePath()), state.snapshot);
}

auto BuiltinEditorDocumentParser::releaseResources() -> void
{
  auto s = extraState();
  s.snapshot = Snapshot();
  s.forceSnapshotInvalidation = true;
  setExtraState(s);
}

auto BuiltinEditorDocumentParser::document() const -> Document::Ptr
{
  return extraState().snapshot.document(filePath());
}

auto BuiltinEditorDocumentParser::snapshot() const -> Snapshot
{
  return extraState().snapshot;
}

auto BuiltinEditorDocumentParser::headerPaths() const -> ProjectExplorer::HeaderPaths
{
  return extraState().headerPaths;
}

auto BuiltinEditorDocumentParser::get(const QString &filePath) -> BuiltinEditorDocumentParser::Ptr
{
  if (auto b = BaseEditorDocumentParser::get(filePath))
    return b.objectCast<BuiltinEditorDocumentParser>();
  return BuiltinEditorDocumentParser::Ptr();
}

auto BuiltinEditorDocumentParser::addFileAndDependencies(Snapshot *snapshot, QSet<Utils::FilePath> *toRemove, const Utils::FilePath &fileName) const -> void
{
  QTC_ASSERT(snapshot, return);

  toRemove->insert(fileName);
  if (fileName != Utils::FilePath::fromString(filePath())) {
    Utils::FilePaths deps = snapshot->filesDependingOn(fileName);
    toRemove->unite(Utils::toSet(deps));
  }
}

auto BuiltinEditorDocumentParser::extraState() const -> BuiltinEditorDocumentParser::ExtraState
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  return m_extraState;
}

auto BuiltinEditorDocumentParser::setExtraState(const ExtraState &extraState) -> void
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  m_extraState = extraState;
}

auto BuiltinEditorDocumentParser::releaseSourceAndAST() const -> bool
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  return m_releaseSourceAndAST;
}

auto BuiltinEditorDocumentParser::setReleaseSourceAndAST(bool release) -> void
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  m_releaseSourceAndAST = release;
}

} // namespace CppEditor
