// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

namespace QtSupport {
namespace Internal {

class QtOptionsPage final : public Core::IOptionsPage {
public:
  QtOptionsPage();

  static auto canLinkWithQt() -> bool;
  static auto isLinkedWithQt() -> bool;
  static auto linkWithQt() -> void;
};

} // Internal
} // QtSupport
