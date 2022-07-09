// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "refactoringengineinterface.hpp"
#include "projectinfo.hpp"
#include "projectpart.hpp"
#include <projectexplorer/headerpath.hpp>

#include <cplusplus/cppmodelmanagerbase.h>
#include <core/find/ifindfilter.hpp>
#include <core/locator/ilocatorfilter.hpp>

#include <QFuture>
#include <QObject>
#include <QStringList>

namespace Core {
class IDocument;
class IEditor;
}

namespace CPlusPlus {
class LookupContext;
}

namespace ProjectExplorer {
class Project;
}

namespace TextEditor {
class BaseHoverHandler;
class TextDocument;
} // namespace TextEditor

namespace CppEditor {

class AbstractEditorSupport;
class AbstractOverviewModel;
class BaseEditorDocumentProcessor;
class CppCompletionAssistProvider;
class CppEditorDocumentHandle;
class CppIndexingSupport;
class CppLocatorData;
class ModelManagerSupportProvider;
class FollowSymbolInterface;
class SymbolFinder;
class WorkingCopy;

namespace Internal {
class CppSourceProcessor;
class CppEditorPluginPrivate;
class CppModelManagerPrivate;
}

namespace Tests {
class ModelManagerTestHelper;
}

enum class RefactoringEngineType : int {
  BuiltIn = 0,
  ClangCodeModel = 1,
  ClangRefactoring = 2
};

class CPPEDITOR_EXPORT CppModelManager final : public CPlusPlus::CppModelManagerBase, public RefactoringEngineInterface {
  Q_OBJECT
  friend class Internal::CppEditorPluginPrivate;

  CppModelManager();
  ~CppModelManager() override;

public:
  using Document = CPlusPlus::Document;

  static auto instance() -> CppModelManager*;
  auto registerJsExtension() -> void;

  // Documented in source file.
  enum ProgressNotificationMode {
    ForcedProgressNotification,
    ReservedProgressNotification
  };

  auto updateSourceFiles(const QSet<QString> &sourceFiles, ProgressNotificationMode mode = ReservedProgressNotification) -> QFuture<void>;
  auto updateCppEditorDocuments(bool projectsUpdated = false) const -> void;
  auto workingCopy() const -> WorkingCopy;
  auto codeModelConfiguration() const -> QByteArray;
  auto locatorData() const -> CppLocatorData*;
  auto projectInfos() const -> QList<ProjectInfo::ConstPtr>;
  auto projectInfo(ProjectExplorer::Project *project) const -> ProjectInfo::ConstPtr;
  auto updateProjectInfo(const ProjectInfo::ConstPtr &newProjectInfo, const QSet<QString> &additionalFiles = {}) -> QFuture<void>;

  /// \return The project part with the given project file
  auto projectPartForId(const QString &projectPartId) const -> ProjectPart::ConstPtr;
  /// \return All project parts that mention the given file name as one of the sources/headers.
  auto projectPart(const Utils::FilePath &fileName) const -> QList<ProjectPart::ConstPtr>;
  auto projectPart(const QString &fileName) const -> QList<ProjectPart::ConstPtr> { return projectPart(Utils::FilePath::fromString(fileName)); }
  /// This is a fall-back function: find all files that includes the file directly or indirectly,
  /// and return its \c ProjectPart list for use with this file.
  auto projectPartFromDependencies(const Utils::FilePath &fileName) const -> QList<ProjectPart::ConstPtr>;
  /// \return A synthetic \c ProjectPart which consists of all defines/includes/frameworks from
  ///         all loaded projects.
  auto fallbackProjectPart() -> ProjectPart::ConstPtr;

  auto snapshot() const -> CPlusPlus::Snapshot override;
  auto document(const QString &fileName) const -> Document::Ptr;
  auto replaceDocument(Document::Ptr newDoc) -> bool;
  auto emitDocumentUpdated(Document::Ptr doc) -> void;
  auto emitAbstractEditorSupportContentsUpdated(const QString &filePath, const QString &sourcePath, const QByteArray &contents) -> void;
  auto emitAbstractEditorSupportRemoved(const QString &filePath) -> void;
  static auto isCppEditor(Core::IEditor *editor) -> bool;
  static auto supportsOutline(const TextEditor::TextDocument *document) -> bool;
  static auto supportsLocalUses(const TextEditor::TextDocument *document) -> bool;
  auto isClangCodeModelActive() const -> bool;
  auto abstractEditorSupports() const -> QSet<AbstractEditorSupport*>;
  auto addExtraEditorSupport(AbstractEditorSupport *editorSupport) -> void;
  auto removeExtraEditorSupport(AbstractEditorSupport *editorSupport) -> void;
  auto cppEditorDocuments() const -> QList<CppEditorDocumentHandle*>;
  auto cppEditorDocument(const QString &filePath) const -> CppEditorDocumentHandle*;
  static auto cppEditorDocumentProcessor(const QString &filePath) -> BaseEditorDocumentProcessor*;
  auto registerCppEditorDocument(CppEditorDocumentHandle *cppEditorDocument) -> void;
  auto unregisterCppEditorDocument(const QString &filePath) -> void;
  auto references(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) -> QList<int>;
  auto startLocalRenaming(const CursorInEditor &data, const ProjectPart *projectPart, RenameCallback &&renameSymbolsCallback) -> void final;
  auto globalRename(const CursorInEditor &data, UsagesCallback &&renameCallback, const QString &replacement) -> void final;
  auto findUsages(const CursorInEditor &data, UsagesCallback &&showUsagesCallback) const -> void final;
  auto globalFollowSymbol(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) const -> void final;
  auto positionRequiresSignal(const QString &filePath, const QByteArray &content, int position) const -> bool;
  auto renameUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context, const QString &replacement = QString()) -> void;
  auto findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) -> void;
  auto findMacroUsages(const CPlusPlus::Macro &macro) -> void;
  auto renameMacroUsages(const CPlusPlus::Macro &macro, const QString &replacement) -> void;
  auto finishedRefreshingSourceFiles(const QSet<QString> &files) -> void;
  auto activateClangCodeModel(ModelManagerSupportProvider *modelManagerSupportProvider) -> void;
  auto completionAssistProvider() const -> CppCompletionAssistProvider*;
  auto functionHintAssistProvider() const -> CppCompletionAssistProvider*;
  auto createEditorDocumentProcessor(TextEditor::TextDocument *baseTextDocument) const -> BaseEditorDocumentProcessor*;
  auto createHoverHandler() const -> TextEditor::BaseHoverHandler*;
  auto followSymbolInterface() const -> FollowSymbolInterface&;
  auto createOverviewModel() const -> std::unique_ptr<AbstractOverviewModel>;
  auto indexingSupport() -> CppIndexingSupport*;
  auto projectFiles() -> QStringList;
  auto headerPaths() -> ProjectExplorer::HeaderPaths;

  // Use this *only* for auto tests
  auto setHeaderPaths(const ProjectExplorer::HeaderPaths &headerPaths) -> void;
  auto definedMacros() -> ProjectExplorer::Macros;
  auto enableGarbageCollector(bool enable) -> void;
  auto symbolFinder() -> SymbolFinder*;
  auto sharedThreadPool() -> QThreadPool*;

  static auto timeStampModifiedFiles(const QList<Document::Ptr> &documentsToCheck) -> QSet<QString>;
  static auto createSourceProcessor() -> Internal::CppSourceProcessor*;
  static auto configurationFileName() -> QString;
  static auto editorConfigurationFileName() -> QString;
  static auto addRefactoringEngine(RefactoringEngineType type, RefactoringEngineInterface *refactoringEngine) -> void;
  static auto removeRefactoringEngine(RefactoringEngineType type) -> void;
  static auto builtinRefactoringEngine() -> RefactoringEngineInterface*;
  static auto builtinFollowSymbol() -> FollowSymbolInterface&;

  auto setLocatorFilter(std::unique_ptr<Core::ILocatorFilter> &&filter) -> void;
  auto setClassesFilter(std::unique_ptr<Core::ILocatorFilter> &&filter) -> void;
  auto setIncludesFilter(std::unique_ptr<Core::ILocatorFilter> &&filter) -> void;
  auto setFunctionsFilter(std::unique_ptr<Core::ILocatorFilter> &&filter) -> void;
  auto setSymbolsFindFilter(std::unique_ptr<Core::IFindFilter> &&filter) -> void;
  auto setCurrentDocumentFilter(std::unique_ptr<Core::ILocatorFilter> &&filter) -> void;
  auto locatorFilter() const -> Core::ILocatorFilter*;
  auto classesFilter() const -> Core::ILocatorFilter*;
  auto includesFilter() const -> Core::ILocatorFilter*;
  auto functionsFilter() const -> Core::ILocatorFilter*;
  auto symbolsFindFilter() const -> Core::IFindFilter*;
  auto currentDocumentFilter() const -> Core::ILocatorFilter*;

  /*
   * try to find build system target that depends on the given file - if the file is no header
   * try to find the corresponding header and use this instead to find the respective target
   */
  auto dependingInternalTargets(const Utils::FilePath &file) const -> QSet<QString>;
  auto internalTargets(const Utils::FilePath &filePath) const -> QSet<QString>;
  auto renameIncludes(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> void;

  // for VcsBaseSubmitEditor
  auto symbolsInFiles(const QSet<Utils::FilePath> &files) const -> Q_INVOKABLE QSet<QString>;

signals:
  /// Project data might be locked while this is emitted.
  auto aboutToRemoveFiles(const QStringList &files) -> void;
  auto documentUpdated(CPlusPlus::Document::Ptr doc) -> void;
  auto sourceFilesRefreshed(const QSet<QString> &files) -> void;
  auto projectPartsUpdated(ProjectExplorer::Project *project) -> void;
  auto projectPartsRemoved(const QStringList &projectPartIds) -> void;
  auto globalSnapshotChanged() -> void;
  auto gcFinished() -> void; // Needed for tests.
  auto abstractEditorSupportContentsUpdated(const QString &filePath, const QString &sourcePath, const QByteArray &contents) -> void;
  auto abstractEditorSupportRemoved(const QString &filePath) -> void;
  auto fallbackProjectPartUpdated() -> void;

public slots:
  auto updateModifiedSourceFiles() -> void;
  auto GC() -> void;

private:
  // This should be executed in the GUI thread.
  friend class Tests::ModelManagerTestHelper;

  auto onAboutToLoadSession() -> void;
  auto onProjectAdded(ProjectExplorer::Project *project) -> void;
  auto onAboutToRemoveProject(ProjectExplorer::Project *project) -> void;
  auto onActiveProjectChanged(ProjectExplorer::Project *project) -> void;
  auto onSourceFilesRefreshed() const -> void;
  auto onCurrentEditorChanged(Core::IEditor *editor) -> void;
  auto onCoreAboutToClose() -> void;
  auto setupFallbackProjectPart() -> void;
  auto initializeBuiltinModelManagerSupport() -> void;
  auto delayedGC() -> void;
  auto recalculateProjectPartMappings() -> void;
  auto replaceSnapshot(const CPlusPlus::Snapshot &newSnapshot) -> void;
  auto removeFilesFromSnapshot(const QSet<QString> &removedFiles) -> void;
  auto removeProjectInfoFilesAndIncludesFromSnapshot(const ProjectInfo &projectInfo) -> void;
  auto buildWorkingCopyList() -> WorkingCopy;
  auto ensureUpdated() -> void;
  auto internalProjectFiles() const -> QStringList;
  auto internalHeaderPaths() const -> ProjectExplorer::HeaderPaths;
  auto internalDefinedMacros() const -> ProjectExplorer::Macros;
  auto dumpModelManagerConfiguration(const QString &logFileId) -> void;
  auto initCppTools() -> void;
  
  Internal::CppModelManagerPrivate *d;
};

} // namespace CppEditor
