// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

/*
    optional<T>
    make_optional(t)

    See std(::experimental)::optional.
*/

// std::optional from Apple's Clang supports methods that throw std::bad_optional_access only
// with deployment target >= macOS 10.14
// TODO: Use std::optional everywhere when we can require macOS 10.14
#if !defined(__apple_build_version__)

#include <optional>

namespace Utils {

using std::optional;
using std::nullopt;
using std::nullopt_t;
using std::in_place;

// make_optional is a copy, since there is no sensible way to import functions in C++
template <class T>
constexpr auto make_optional(T &&v) -> optional<std::decay_t<T>>
{
  return optional<std::decay_t<T>>(std::forward<T>(v));
}

template <class T, class... Args>
auto make_optional(Args &&... args) -> optional<T>
{
  return optional<T>(in_place, std::forward<Args>(args)...);
}

template <class T, class Up, class... Args>
constexpr auto make_optional(std::initializer_list<Up> il, Args &&... args) -> optional<T>
{
  return optional<T>(in_place, il, std::forward<Args>(args)...);
}

} // namespace Utils

#else

#include <3rdparty/optional/optional.hpp>

namespace Utils {

// --> Utils::optional
using std::experimental::optional;
// --> Utils::nullopt
using std::experimental::nullopt;
using std::experimental::nullopt_t;
// --> Utils::in_place
using std::experimental::in_place;

// TODO: make_optional is a copy, since there is no sensible way to import functions in C++
template <class T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& v)
{
  return optional<typename std::decay<T>::type>(std::experimental::constexpr_forward<T>(v));
}

template <class X>
constexpr optional<X&> make_optional(std::reference_wrapper<X> v)
{
  return optional<X&>(v.get());
}

} // Utils

#endif
