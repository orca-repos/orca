// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.h"
#include "theme/theme.h"
#include "utils_global.h"

#include <QIcon>
#include <QPair>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QColor)
QT_FORWARD_DECLARE_CLASS(QPixmap)
QT_FORWARD_DECLARE_CLASS(QString)

namespace Utils {

using IconMaskAndColor = QPair<FilePath, Theme::Color>;

// Returns a recolored icon with shadow and custom disabled state for a
// series of grayscalemask|Theme::Color mask pairs
class ORCA_UTILS_EXPORT Icon : public QVector<IconMaskAndColor> {
public:
  enum IconStyleOption {
    None = 0,
    Tint = 1,
    DropShadow = 2,
    PunchEdges = 4,

    ToolBarStyle = Tint | DropShadow | PunchEdges,
    MenuTintedStyle = Tint | PunchEdges
  };

  Q_DECLARE_FLAGS(IconStyleOptions, IconStyleOption)

  Icon();
  Icon(std::initializer_list<IconMaskAndColor> args, IconStyleOptions style = ToolBarStyle);
  Icon(const FilePath &imageFileName);
  Icon(const Icon &other) = default;

  auto icon() const -> QIcon;
  // Same as icon() but without disabled state.
  auto pixmap(QIcon::Mode iconMode = QIcon::Normal) const -> QPixmap;

  // Try to avoid it. it is just there for special API cases in Orca
  // where icons are still defined as filename.
  auto imageFilePath() const -> FilePath;

  // Returns either the classic or a themed icon depending on
  // the current Theme::FlatModeIcons flag.
  static auto sideBarIcon(const Icon &classic, const Icon &flat) -> QIcon;
  // Like sideBarIcon plus added action mode for the flat icon
  static auto modeIcon(const Icon &classic, const Icon &flat, const Icon &flatActive) -> QIcon;

  // Combined icon pixmaps in Normal and Disabled states from several Icons
  static auto combinedIcon(const QList<QIcon> &icons) -> QIcon;
  static auto combinedIcon(const QList<Icon> &icons) -> QIcon;

private:
  IconStyleOptions m_style = None;
};

} // namespace Utils

Q_DECLARE_OPERATORS_FOR_FLAGS(Utils::Icon::IconStyleOptions)
