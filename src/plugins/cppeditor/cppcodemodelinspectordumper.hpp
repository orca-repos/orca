// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectpart.hpp"
#include "projectinfo.hpp"

#include <cplusplus/CppDocument.h>

#include <QFile>
#include <QTextStream>

namespace CppEditor {

class WorkingCopy;

namespace CppCodeModelInspector {

struct Utils {
  static auto toString(bool value) -> QString;
  static auto toString(int value) -> QString;
  static auto toString(unsigned value) -> QString;
  static auto toString(const QDateTime &dateTime) -> QString;
  static auto toString(CPlusPlus::Document::CheckMode checkMode) -> QString;
  static auto toString(CPlusPlus::Document::DiagnosticMessage::Level level) -> QString;
  static auto toString(ProjectExplorer::HeaderPathType type) -> QString;
  static auto toString(::Utils::LanguageVersion languageVersion) -> QString;
  static auto toString(::Utils::LanguageExtensions languageExtension) -> QString;
  static auto toString(::Utils::QtMajorVersion qtVersion) -> QString;
  static auto toString(ProjectExplorer::BuildTargetType buildTargetType) -> QString;
  static auto toString(const QVector<ProjectFile> &projectFiles) -> QString;
  static auto toString(ProjectFile::Kind kind) -> QString;
  static auto toString(CPlusPlus::Kind kind) -> QString;
  static auto toString(ProjectPart::ToolChainWordWidth width) -> QString;
  static auto partsForFile(const QString &fileName) -> QString;
  static auto unresolvedFileNameWithDelimiters(const CPlusPlus::Document::Include &include) -> QString;
  static auto pathListToString(const QStringList &pathList) -> QString;
  static auto pathListToString(const ProjectExplorer::HeaderPaths &pathList) -> QString;
  static auto snapshotToList(const CPlusPlus::Snapshot &snapshot) -> QList<CPlusPlus::Document::Ptr>;
};

class Dumper {
public:
  explicit Dumper(const CPlusPlus::Snapshot &globalSnapshot, const QString &logFileId = QString());
  ~Dumper();

  auto dumpProjectInfos(const QList<ProjectInfo::ConstPtr> &projectInfos) -> void;
  auto dumpSnapshot(const CPlusPlus::Snapshot &snapshot, const QString &title, bool isGlobalSnapshot = false) -> void;
  auto dumpWorkingCopy(const WorkingCopy &workingCopy) -> void;
  auto dumpMergedEntities(const ProjectExplorer::HeaderPaths &mergedHeaderPaths, const QByteArray &mergedMacros) -> void;

private:
  auto dumpStringList(const QStringList &list, const QByteArray &indent) -> void;
  auto dumpDocuments(const QList<CPlusPlus::Document::Ptr> &documents, bool skipDetails = false) -> void;
  static auto indent(int level) -> QByteArray;

  CPlusPlus::Snapshot m_globalSnapshot;
  QFile m_logFile;
  QTextStream m_out;
};

} // namespace CppCodeModelInspector
} // namespace CppEditor
