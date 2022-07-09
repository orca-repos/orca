// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>

#include "classviewparsertreeitem.hpp"

#include <cplusplus/CppDocument.h>

// might be changed to forward declaration - is not done to be less dependent
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/project.hpp>

#include <QList>
#include <QSharedPointer>
#include <QStandardItem>

namespace ClassView {
namespace Internal {

class ParserPrivate;

class Parser : public QObject
{
    Q_OBJECT

public:
    explicit Parser(QObject *parent = nullptr);
    ~Parser() override;

    auto requestCurrentState() -> void;
    auto removeFiles(const QStringList &fileList) -> void;
    auto resetData(const QHash<Utils::FilePath, QPair<QString, Utils::FilePaths>> &projects) -> void;
    auto addProject(const Utils::FilePath &projectPath, const QString &projectName, const Utils::FilePaths &filesInProject) -> void;
    auto removeProject(const Utils::FilePath &projectPath) -> void;
    auto setFlatMode(bool flat) -> void;
    auto updateDocuments(const QSet<Utils::FilePath> &documentPaths) -> void;

signals:
    auto treeRegenerated(const ParserTreeItem::ConstPtr &root) -> void;

private:
    auto updateDocumentsFromSnapshot(const QSet<Utils::FilePath> &documentPaths, const CPlusPlus::Snapshot &snapshot) -> void;
    auto getParseDocumentTree(const CPlusPlus::Document::Ptr &doc) -> ParserTreeItem::ConstPtr;
    auto getCachedOrParseDocumentTree(const CPlusPlus::Document::Ptr &doc) -> ParserTreeItem::ConstPtr;
    auto getParseProjectTree(const Utils::FilePath &projectPath, const QSet<Utils::FilePath> &filesInProject) -> ParserTreeItem::ConstPtr;
    auto getCachedOrParseProjectTree(const Utils::FilePath &projectPath, const QSet<Utils::FilePath> &filesInProject) -> ParserTreeItem::ConstPtr;
    auto parse() -> ParserTreeItem::ConstPtr;

    //! Private class data pointer
    ParserPrivate *d;
};

} // namespace Internal
} // namespace ClassView
