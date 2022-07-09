// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QString>

namespace CppEditor {

class CPPEDITOR_EXPORT ProjectFile {
public:
  enum Kind {
    Unclassified,
    Unsupported,
    AmbiguousHeader,
    CHeader,
    CSource,
    CXXHeader,
    CXXSource,
    ObjCHeader,
    ObjCSource,
    ObjCXXHeader,
    ObjCXXSource,
    CudaSource,
    OpenCLSource,
  };

  ProjectFile() = default;
  ProjectFile(const QString &filePath, Kind kind, bool active = true);

  static auto classifyByMimeType(const QString &mt) -> Kind;
  static auto classify(const QString &filePath) -> Kind;
  static auto sourceForHeaderKind(Kind kind) -> Kind;
  static auto sourceKind(Kind kind) -> Kind;
  static auto isSource(Kind kind) -> bool;
  static auto isHeader(Kind kind) -> bool;
  static auto isC(Kind kind) -> bool;
  static auto isCxx(Kind kind) -> bool;
  static auto isAmbiguousHeader(const QString &filePath) -> bool;
  static auto isObjC(const QString &filePath) -> bool;

  auto isHeader() const -> bool;
  auto isSource() const -> bool;
  auto isC() const -> bool;
  auto isCxx() const -> bool;

  auto operator==(const ProjectFile &other) const -> bool;

  friend auto operator<<(QDebug stream, const CppEditor::ProjectFile &projectFile) -> QDebug;

  QString path;
  Kind kind = Unclassified;
  bool active = true;
};

using ProjectFiles = QVector<ProjectFile>;

auto projectFileKindToText(ProjectFile::Kind kind) -> const char*;

} // namespace CppEditor
