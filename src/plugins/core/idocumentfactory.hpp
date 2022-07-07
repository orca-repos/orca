// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/id.hpp>

#include <functional>

namespace Utils {
class FilePath;
}

namespace Core {
class IDocument;

class CORE_EXPORT IDocumentFactory {
public:
  IDocumentFactory();
  ~IDocumentFactory();

  using Opener = std::function<IDocument* (const Utils::FilePath& file_path)>;

  static auto allDocumentFactories() -> QList<IDocumentFactory*>;
  auto open(const Utils::FilePath &file_path) const -> IDocument*;
  auto mimeTypes() const -> QStringList { return m_mime_types; }
  auto setOpener(const Opener &opener) -> void { m_opener = opener; }
  auto setMimeTypes(const QStringList &mime_types) -> void { m_mime_types = mime_types; }
  auto addMimeType(const char *mime_type) -> void { m_mime_types.append(QLatin1String(mime_type)); }
  auto addMimeType(const QString &mime_type) -> void { m_mime_types.append(mime_type); }

private:
  Opener m_opener;
  QStringList m_mime_types;
  QString m_display_name;
};

} // namespace Core
