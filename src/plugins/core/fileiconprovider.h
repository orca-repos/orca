// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <QStyle>
#include <QFileIconProvider>

namespace Utils { class FilePath; }

namespace Core {

namespace FileIconProvider {

// Access to the single instance
CORE_EXPORT auto iconProvider() -> QFileIconProvider*;

// Access to individual items
CORE_EXPORT auto icon(const Utils::FilePath &file_path) -> QIcon;
CORE_EXPORT auto icon(QFileIconProvider::IconType type) -> QIcon;

// Register additional overlay icons
CORE_EXPORT auto overlayIcon(const QPixmap &base_icon, const QIcon &overlay_icon) -> QPixmap;
CORE_EXPORT auto overlayIcon(QStyle::StandardPixmap base_icon, const QIcon &overlay_icon, const QSize &size) -> QPixmap;
CORE_EXPORT auto registerIconOverlayForSuffix(const QString &path, const QString &suffix) -> void;
CORE_EXPORT auto registerIconOverlayForFilename(const QString &path, const QString &filename) -> void;
CORE_EXPORT auto registerIconOverlayForMimeType(const QString &path, const QString &mime_type) -> void;
CORE_EXPORT auto registerIconOverlayForMimeType(const QIcon &icon, const QString &mime_type) -> void;
CORE_EXPORT auto directoryIcon(const QString &overlay) -> QIcon;

} // namespace FileIconProvider
} // namespace Core
