// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

/*
    See std(::experimental)::variant.
*/

// std::variant from Apple's Clang supports methods that throw std::bad_optional_access only
// with deployment target >= macOS 10.14
// TODO: Use std::variant everywhere when we can require macOS 10.14
#if !defined(__apple_build_version__)
#include <variant>

namespace Utils {
using std::get;
using std::get_if;
using std::holds_alternative;
using std::monostate;
using std::variant;
using std::variant_alternative_t;
using std::visit;
} // namespace Utils

#else
#include <3rdparty/variant/variant.hpp>

namespace Utils {
using mpark::get;
using mpark::get_if;
using mpark::holds_alternative;
using mpark::monostate;
using mpark::variant;
using mpark::variant_alternative_t;
using mpark::visit;
} // namespace Utils

#endif
