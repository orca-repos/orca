// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-icons.hpp"

using namespace Utils;

namespace Orca::Plugin::Core {

const Icon ORCALOGO_BIG(":/core/images/orcalogo-big.png");
const Icon ORCALOGO(":/core/images/orcalogo.png");
const Icon FIND_CASE_INSENSITIVELY(":/find/images/casesensitively.png");
const Icon FIND_WHOLE_WORD(":/find/images/wholewords.png");
const Icon FIND_REGEXP(":/find/images/regexp.png");
const Icon FIND_PRESERVE_CASE(":/find/images/preservecase.png");
const Icon MODE_EDIT_CLASSIC(":/fancyactionbar/images/mode_Edit.png");
const Icon MODE_EDIT_FLAT({{":/fancyactionbar/images/mode_edit_mask.png", Theme::IconsBaseColor}});
const Icon MODE_EDIT_FLAT_ACTIVE({{":/fancyactionbar/images/mode_edit_mask.png", Theme::IconsModeEditActiveColor}});
const Icon MODE_DESIGN_CLASSIC(":/fancyactionbar/images/mode_Design.png");
const Icon MODE_DESIGN_FLAT({{":/fancyactionbar/images/mode_design_mask.png", Theme::IconsBaseColor}});
const Icon MODE_DESIGN_FLAT_ACTIVE({{":/fancyactionbar/images/mode_design_mask.png", Theme::IconsModeDesignActiveColor}});
const Icon DESKTOP_DEVICE_SMALL({{":/utils/images/desktopdevicesmall.png", Theme::PanelTextColorDark}}, Icon::Tint);

} // namespace Orca::Plugin::Core