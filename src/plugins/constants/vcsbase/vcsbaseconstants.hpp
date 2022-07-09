// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace VcsBase {
namespace Constants {

constexpr char VCS_SETTINGS_CATEGORY[] = "V.Version Control";
constexpr char VCS_COMMON_SETTINGS_ID[] = "A.VCS.General";
constexpr char VCS_COMMON_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("VcsBase", "General");

// Ids for sort order (wizards and preferences)
constexpr char VCS_ID_BAZAAR[] = "B.Bazaar";
constexpr char VCS_ID_GIT[] = "G.Git";
constexpr char VCS_ID_MERCURIAL[] = "H.Mercurial";
constexpr char VCS_ID_SUBVERSION[] = "J.Subversion";
constexpr char VCS_ID_PERFORCE[] = "P.Perforce";
constexpr char VCS_ID_CVS[] = "Z.CVS";
constexpr char VAR_VCS_NAME[] = "CurrentDocument:Project:VcsName";
constexpr char VAR_VCS_TOPIC[] = "CurrentDocument:Project:VcsTopic";
constexpr char VAR_VCS_TOPLEVELPATH[] = "CurrentDocument:Project:VcsTopLevelPath";

} // namespace Constants
} // namespace VcsBase
