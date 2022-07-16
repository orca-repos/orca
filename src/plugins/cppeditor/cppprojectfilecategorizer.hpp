// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppprojectfile.hpp"

#include <projectexplorer/rawprojectpart.hpp>

#include <QString>
#include <QVector>

namespace CppEditor {

class CPPEDITOR_EXPORT ProjectFileCategorizer {
public:
  using FileIsActive = ProjectExplorer::RawProjectPart::FileIsActive;
  using GetMimeType = ProjectExplorer::RawProjectPart::GetMimeType;
  
  ProjectFileCategorizer(const QString &projectPartName, const QStringList &filePaths, const FileIsActive &fileIsActive = {}, const GetMimeType &getMimeType = {});

  auto hasCSources() const -> bool { return !m_cSources.isEmpty(); }
  auto hasCxxSources() const -> bool { return !m_cxxSources.isEmpty(); }
  auto hasObjcSources() const -> bool { return !m_objcSources.isEmpty(); }
  auto hasObjcxxSources() const -> bool { return !m_objcxxSources.isEmpty(); }
  auto cSources() const -> ProjectFiles { return m_cSources; }
  auto cxxSources() const -> ProjectFiles { return m_cxxSources; }
  auto objcSources() const -> ProjectFiles { return m_objcSources; }
  auto objcxxSources() const -> ProjectFiles { return m_objcxxSources; }
  auto hasMultipleParts() const -> bool { return m_partCount > 1; }
  auto hasParts() const -> bool { return m_partCount > 0; }
  auto partName(const QString &languageName) const -> QString;

private:
  auto classifyFiles(const QStringList &filePaths, const FileIsActive &fileIsActive, const GetMimeType &getMimeType) -> ProjectFiles;
  auto expandSourcesWithAmbiguousHeaders(const ProjectFiles &ambiguousHeaders) -> void;

  QString m_partName;
  ProjectFiles m_cSources;
  ProjectFiles m_cxxSources;
  ProjectFiles m_objcSources;
  ProjectFiles m_objcxxSources;
  int m_partCount;
};

} // namespace CppEditor
