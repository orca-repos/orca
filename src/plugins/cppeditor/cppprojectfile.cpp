// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppprojectfile.hpp"

#include "cppeditorconstants.hpp"

#include <core/icore.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QDebug>

namespace CppEditor {

ProjectFile::ProjectFile(const QString &filePath, Kind kind, bool active) : path(filePath), kind(kind), active(active) {}

auto ProjectFile::operator==(const ProjectFile &other) const -> bool
{
  return active == other.active && kind == other.kind && path == other.path;
}

auto ProjectFile::classifyByMimeType(const QString &mt) -> ProjectFile::Kind
{
  if (mt == CppEditor::Constants::C_SOURCE_MIMETYPE)
    return CSource;
  if (mt == CppEditor::Constants::C_HEADER_MIMETYPE)
    return CHeader;
  if (mt == CppEditor::Constants::CPP_SOURCE_MIMETYPE)
    return CXXSource;
  if (mt == CppEditor::Constants::CPP_HEADER_MIMETYPE)
    return CXXHeader;
  if (mt == CppEditor::Constants::OBJECTIVE_C_SOURCE_MIMETYPE)
    return ObjCSource;
  if (mt == CppEditor::Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE)
    return ObjCXXSource;
  if (mt == CppEditor::Constants::QDOC_MIMETYPE)
    return CXXSource;
  if (mt == CppEditor::Constants::MOC_MIMETYPE)
    return CXXSource;
  if (mt == CppEditor::Constants::CUDA_SOURCE_MIMETYPE)
    return CudaSource;
  if (mt == CppEditor::Constants::AMBIGUOUS_HEADER_MIMETYPE)
    return AmbiguousHeader;
  return Unsupported;
}

auto ProjectFile::classify(const QString &filePath) -> ProjectFile::Kind
{
  if (isAmbiguousHeader(filePath))
    return AmbiguousHeader;

  const auto mimeType = Utils::mimeTypeForFile(filePath);
  return classifyByMimeType(mimeType.name());
}

auto ProjectFile::isAmbiguousHeader(const QString &filePath) -> bool
{
  return filePath.endsWith(".hpp");
}

auto ProjectFile::isObjC(const QString &filePath) -> bool
{
  const auto kind = classify(filePath);
  switch (kind) {
  case CppEditor::ProjectFile::ObjCHeader:
  case CppEditor::ProjectFile::ObjCXXHeader:
  case CppEditor::ProjectFile::ObjCSource:
  case CppEditor::ProjectFile::ObjCXXSource:
    return true;
  default:
    return false;
  }
}

auto ProjectFile::sourceForHeaderKind(ProjectFile::Kind kind) -> ProjectFile::Kind
{
  ProjectFile::Kind sourceKind;
  switch (kind) {
  case ProjectFile::CHeader:
    sourceKind = ProjectFile::CSource;
    break;
  case ProjectFile::ObjCHeader:
    sourceKind = ProjectFile::ObjCSource;
    break;
  case ProjectFile::ObjCXXHeader:
    sourceKind = ProjectFile::ObjCXXSource;
    break;
  case ProjectFile::Unsupported: // no file extension, e.g. stl headers
  case ProjectFile::AmbiguousHeader:
  case ProjectFile::CXXHeader: default:
    sourceKind = ProjectFile::CXXSource;
  }

  return sourceKind;
}

auto ProjectFile::sourceKind(Kind kind) -> ProjectFile::Kind
{
  auto sourceKind = kind;
  if (ProjectFile::isHeader(kind))
    sourceKind = ProjectFile::sourceForHeaderKind(kind);
  return sourceKind;
}

auto ProjectFile::isHeader(ProjectFile::Kind kind) -> bool
{
  switch (kind) {
  case ProjectFile::CHeader:
  case ProjectFile::CXXHeader:
  case ProjectFile::ObjCHeader:
  case ProjectFile::ObjCXXHeader:
  case ProjectFile::Unsupported: // no file extension, e.g. stl headers
  case ProjectFile::AmbiguousHeader:
    return true;
  default:
    return false;
  }
}

auto ProjectFile::isSource(ProjectFile::Kind kind) -> bool
{
  switch (kind) {
  case ProjectFile::CSource:
  case ProjectFile::CXXSource:
  case ProjectFile::ObjCSource:
  case ProjectFile::ObjCXXSource:
  case ProjectFile::CudaSource:
  case ProjectFile::OpenCLSource:
    return true;
  default:
    return false;
  }
}

auto ProjectFile::isHeader() const -> bool
{
  return isHeader(kind);
}

auto ProjectFile::isSource() const -> bool
{
  return isSource(kind);
}

auto ProjectFile::isC(ProjectFile::Kind kind) -> bool
{
  switch (kind) {
  case ProjectFile::CHeader:
  case ProjectFile::CSource:
  case ProjectFile::ObjCHeader:
  case ProjectFile::ObjCSource:
    return true;
  default:
    return false;
  }
}

auto ProjectFile::isCxx(ProjectFile::Kind kind) -> bool
{
  switch (kind) {
  case ProjectFile::CXXHeader:
  case ProjectFile::CXXSource:
  case ProjectFile::ObjCXXHeader:
  case ProjectFile::ObjCXXSource:
  case ProjectFile::CudaSource:
    return true;
  default:
    return false;
  }
}

auto ProjectFile::isC() const -> bool
{
  return isC(kind);
}

auto ProjectFile::isCxx() const -> bool
{
  return isCxx(kind);
}

#define RETURN_TEXT_FOR_CASE(enumValue) case ProjectFile::enumValue: return #enumValue

auto projectFileKindToText(ProjectFile::Kind kind) -> const char*
{
  switch (kind) {
  RETURN_TEXT_FOR_CASE(Unclassified);
  RETURN_TEXT_FOR_CASE(Unsupported);
  RETURN_TEXT_FOR_CASE(AmbiguousHeader);
  RETURN_TEXT_FOR_CASE(CHeader);
  RETURN_TEXT_FOR_CASE(CSource);
  RETURN_TEXT_FOR_CASE(CXXHeader);
  RETURN_TEXT_FOR_CASE(CXXSource);
  RETURN_TEXT_FOR_CASE(ObjCHeader);
  RETURN_TEXT_FOR_CASE(ObjCSource);
  RETURN_TEXT_FOR_CASE(ObjCXXHeader);
  RETURN_TEXT_FOR_CASE(ObjCXXSource);
  RETURN_TEXT_FOR_CASE(CudaSource);
  RETURN_TEXT_FOR_CASE(OpenCLSource);
  }

  return "UnhandledProjectFileKind";
}
#undef RETURN_TEXT_FOR_CASE

auto operator<<(QDebug stream, const ProjectFile &projectFile) -> QDebug
{
  stream << projectFile.path << QLatin1String(", ") << projectFileKindToText(projectFile.kind);
  return stream;
}

} // namespace CppEditor
