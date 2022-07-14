// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-file-icon-provider.hpp"

#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>
#include <utils/variant.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QApplication>
#include <QDebug>
#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QStyle>

using namespace Utils;

/*!
  \namespace Core::FileIconProvider
  \inmodule Orca
  \brief Provides functions for registering custom overlay icons for system
  icons.

  Provides icons based on file suffixes with the ability to overwrite system
  icons for specific subtypes. The underlying QFileIconProvider
  can be used for QFileSystemModel.

  \note Registering overlay icons currently completely replaces the system
        icon and is therefore not recommended on platforms that have their
        own overlay icon handling (\macOS and Windows).

  Plugins can register custom overlay icons via registerIconOverlayForSuffix(), and
  retrieve icons via the icon() function.
  */

using item = variant<QIcon, QString>; // icon or filename for the icon

namespace Orca::Plugin::Core {

enum {
  debug = 0
};

static auto getIcon(QHash<QString, item> &cache, const QString &key) -> optional<QIcon>
{
  const auto it = cache.constFind(key);

  if (it == cache.constEnd())
    return {};

  if (const auto icon = Utils::get_if<QIcon>(&*it))
    return *icon;

  // need to create icon from file name first
  const auto file_name = Utils::get_if<QString>(&*it);
  QTC_ASSERT(file_name, return {});
  const auto icon = QIcon(overlayIcon(QStyle::SP_FileIcon, QIcon(*file_name), QSize(16, 16)));
  cache.insert(key, icon);

  return icon;
}

class FileIconProviderImplementation final : public QFileIconProvider {
public:
  FileIconProviderImplementation() = default;

  auto icon(const FilePath &file_path) const -> QIcon;
  using QFileIconProvider::icon;

  auto registerIconOverlayForFilename(const QString &icon_file_path, const QString &filename) const -> void
  {
    m_filename_cache.insert(filename, icon_file_path);
  }

  auto registerIconOverlayForSuffix(const QString &icon_file_path, const QString &suffix) const -> void
  {
    m_suffix_cache.insert(suffix, icon_file_path);
  }

  auto registerIconOverlayForMimeType(const QIcon &icon, const MimeType &mime_type) const -> void
  {
    for (const auto suffixes = mime_type.suffixes(); const auto &suffix : suffixes) {
      QTC_ASSERT(!icon.isNull() && !suffix.isEmpty(), return);

      const auto file_icon_pixmap = overlayIcon(QStyle::SP_FileIcon, icon, QSize(16, 16));
      // replace old icon, if it exists
      m_suffix_cache.insert(suffix, file_icon_pixmap);
    }
  }

  auto registerIconOverlayForMimeType(const QString &icon_file_path, const MimeType &mime_type) const -> void
  {
    for(const auto &suffix: mime_type.suffixes())
      registerIconOverlayForSuffix(icon_file_path, suffix);
  }

  // Mapping of file suffix to icon.
  mutable QHash<QString, item> m_suffix_cache;
  mutable QHash<QString, item> m_filename_cache;
};

auto instance() -> FileIconProviderImplementation*
{
  static FileIconProviderImplementation the_instance;
  return &the_instance;
}

auto iconProvider() -> QFileIconProvider*
{
  return instance();
}

static auto unknownFileIcon() -> const QIcon&
{
  static const auto icon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
  return icon;
}

static auto dirIcon() -> const QIcon&
{
  static const auto icon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
  return icon;
}

auto FileIconProviderImplementation::icon(const FilePath &file_path) const -> QIcon
{
  if constexpr (debug)
    qDebug() << "FileIconProvider::icon" << file_path.absoluteFilePath();

  const auto is_dir = file_path.isDir();

  if (file_path.needsDevice())
    return is_dir ? dirIcon() : unknownFileIcon();

  // Check for cached overlay icons by file suffix.
  if (const auto filename = !is_dir ? file_path.fileName() : QString(); !filename.isEmpty()) {
    if (const auto icon = getIcon(m_filename_cache, filename))
      return *icon;
  }

  const auto suffix = !is_dir ? file_path.suffix() : QString();

  if (!suffix.isEmpty()) {
    if (const auto icon = getIcon(m_suffix_cache, suffix))
      return *icon;
  }

  // Get icon from OS (and cache it based on suffix!)
  QIcon icon;

  if constexpr (HostOsInfo::isWindowsHost() || HostOsInfo::isMacHost())
    icon = QFileIconProvider::icon(file_path.toFileInfo());
  else // File icons are unknown on linux systems.
    icon = is_dir ? QFileIconProvider::icon(file_path.toFileInfo()) : unknownFileIcon();

  if (!is_dir && !suffix.isEmpty())
    m_suffix_cache.insert(suffix, icon);

  return icon;
}

/*!
  Returns the icon associated with the file suffix in \a filePath. If there is none,
  the default icon of the operating system is returned.
  */

auto icon(const FilePath &file_path) -> QIcon
{
  return instance()->icon(file_path);
}

/*!
 * \overload
 */
auto icon(const QFileIconProvider::IconType type) -> QIcon
{
  return instance()->icon(type);
}

/*!
  Creates a pixmap with \a baseIcon and lays \a overlayIcon over it.
  */
auto overlayIcon(const QPixmap &base_icon, const QIcon &overlay_icon) -> QPixmap
{
  auto result = base_icon;
  QPainter painter(&result);
  overlay_icon.paint(&painter, QRect(QPoint(), result.size() / result.devicePixelRatio()));
  return result;
}

/*!
  Creates a pixmap with \a baseIcon at \a size and \a overlay.
  */
auto overlayIcon(const QStyle::StandardPixmap base_icon, const QIcon &overlay_icon, const QSize &size) -> QPixmap
{
  return overlayIcon(QApplication::style()->standardIcon(base_icon).pixmap(size), overlay_icon);
}

/*!
  Registers an icon at \a path for a given \a suffix, overlaying the system
  file icon.
 */
auto registerIconOverlayForSuffix(const QString &path, const QString &suffix) -> void
{
  instance()->registerIconOverlayForSuffix(path, suffix);
}

/*!
  Registers \a icon for all the suffixes of a the mime type \a mimeType,
  overlaying the system file icon.
  */
auto registerIconOverlayForMimeType(const QIcon &icon, const QString &mime_type) -> void
{
  instance()->registerIconOverlayForMimeType(icon, mimeTypeForName(mime_type));
}

/*!
 * \overload
 */
auto registerIconOverlayForMimeType(const QString &path, const QString &mime_type) -> void
{
  instance()->registerIconOverlayForMimeType(path, mimeTypeForName(mime_type));
}

auto registerIconOverlayForFilename(const QString &path, const QString &filename) -> void
{
  instance()->registerIconOverlayForFilename(path, filename);
}

// Return a standard directory icon with the specified overlay:
auto directoryIcon(const QString &overlay) -> QIcon
{
  // Overlay the SP_DirIcon with the custom icons
  constexpr auto desired_size = QSize(16, 16);
  const auto dir_pixmap = QApplication::style()->standardIcon(QStyle::SP_DirIcon).pixmap(desired_size);
  const QIcon overlay_icon(overlay);

  QIcon result;
  result.addPixmap(overlayIcon(dir_pixmap, overlay_icon));

  return result;
}

} // namespace Orca::Plugin::Core
