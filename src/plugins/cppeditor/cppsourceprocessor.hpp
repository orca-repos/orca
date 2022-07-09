// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppmodelmanager.hpp"
#include "cppworkingcopy.hpp"

#include <cplusplus/PreprocessorEnvironment.h>
#include <cplusplus/pp-engine.h>

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include <functional>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace CppEditor::Internal {

// Documentation inside.
class CppSourceProcessor: public CPlusPlus::Client
{
    Q_DISABLE_COPY(CppSourceProcessor)

public:
    using CancelChecker = std::function<bool()>;
    using DocumentCallback = std::function<void (const CPlusPlus::Document::Ptr &)>;

    CppSourceProcessor(const CPlusPlus::Snapshot &snapshot, DocumentCallback documentFinished);
    ~CppSourceProcessor() override;

    static auto cleanPath(const QString &path) -> QString;
    auto setCancelChecker(const CancelChecker &cancelChecker) -> void;
    auto setWorkingCopy(const WorkingCopy &workingCopy) -> void;
    auto setHeaderPaths(const ProjectExplorer::HeaderPaths &headerPaths) -> void;
    auto setLanguageFeatures(CPlusPlus::LanguageFeatures languageFeatures) -> void;
    auto setFileSizeLimitInMb(int fileSizeLimitInMb) -> void;
    auto setTodo(const QSet<QString> &files) -> void;
    auto run(const QString &fileName, const QStringList &initialIncludes = QStringList()) -> void;
    auto removeFromCache(const QString &fileName) -> void;
    auto resetEnvironment() -> void;
    auto snapshot() const -> CPlusPlus::Snapshot { return m_snapshot; }
    auto todo() const -> const QSet<QString>& { return m_todo; }
    auto setGlobalSnapshot(const CPlusPlus::Snapshot &snapshot) -> void { m_globalSnapshot = snapshot; }

private:
    auto addFrameworkPath(const ProjectExplorer::HeaderPath &frameworkPath) -> void;
    auto switchCurrentDocument(CPlusPlus::Document::Ptr doc) -> CPlusPlus::Document::Ptr;
    auto getFileContents(const QString &absoluteFilePath, QByteArray *contents, unsigned *revision) const -> bool;
    auto checkFile(const QString &absoluteFilePath) const -> bool;
    auto resolveFile(const QString &fileName, IncludeType type) -> QString;
    auto resolveFile_helper(const QString &fileName, ProjectExplorer::HeaderPaths::Iterator headerPathsIt) -> QString;
    auto mergeEnvironment(CPlusPlus::Document::Ptr doc) -> void;

    // Client interface
    auto macroAdded(const CPlusPlus::Macro &macro) -> void override;
    auto passedMacroDefinitionCheck(int bytesOffset, int utf16charsOffset, int line, const CPlusPlus::Macro &macro) -> void override;
    auto failedMacroDefinitionCheck(int bytesOffset, int utf16charOffset, const CPlusPlus::ByteArrayRef &name) -> void override;
    auto notifyMacroReference(int bytesOffset, int utf16charOffset, int line, const CPlusPlus::Macro &macro) -> void override;
    auto startExpandingMacro(int bytesOffset, int utf16charOffset, int line, const CPlusPlus::Macro &macro, const QVector<CPlusPlus::MacroArgumentReference> &actuals) -> void override;
    auto stopExpandingMacro(int bytesOffset, const CPlusPlus::Macro &macro) -> void override;
    auto markAsIncludeGuard(const QByteArray &macroName) -> void override;
    auto startSkippingBlocks(int utf16charsOffset) -> void override;
    auto stopSkippingBlocks(int utf16charsOffset) -> void override;
    auto sourceNeeded(int line, const QString &fileName, IncludeType type, const QStringList &initialIncludes) -> void override;
  
    CPlusPlus::Snapshot m_snapshot;
    CPlusPlus::Snapshot m_globalSnapshot;
    DocumentCallback m_documentFinished;
    CPlusPlus::Environment m_env;
    CPlusPlus::Preprocessor m_preprocess;
    ProjectExplorer::HeaderPaths m_headerPaths;
    CPlusPlus::LanguageFeatures m_languageFeatures;
    WorkingCopy m_workingCopy;
    QSet<QString> m_included;
    CPlusPlus::Document::Ptr m_currentDoc;
    QSet<QString> m_todo;
    QSet<QString> m_processed;
    QHash<QString, QString> m_fileNameCache;
    int m_fileSizeLimitInMb = -1;
    QTextCodec *m_defaultCodec;
};

} // namespace CppEditor::Internal
