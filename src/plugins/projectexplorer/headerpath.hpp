// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>

#include <QString>
#include <QVector>

namespace ProjectExplorer {

enum class HeaderPathType {
  User,
  BuiltIn,
  System,
  Framework,
};

class HeaderPath {
public:
  HeaderPath() = default;
  HeaderPath(const QString &path, HeaderPathType type) : path(path), type(type) { }
  HeaderPath(const char *path, HeaderPathType type) : HeaderPath(QLatin1String(path), type) {}
  HeaderPath(const Utils::FilePath &path, HeaderPathType type) : HeaderPath(path.toString(), type) { }

  auto operator==(const HeaderPath &other) const -> bool
  {
    return type == other.type && path == other.path;
  }

  auto operator!=(const HeaderPath &other) const -> bool
  {
    return !(*this == other);
  }

  template <typename F>
  static auto makeUser(const F &fp) -> HeaderPath
  {
    return {fp, HeaderPathType::User};
  }

  template <typename F>
  static auto makeBuiltIn(const F &fp) -> HeaderPath
  {
    return {fp, HeaderPathType::BuiltIn};
  }

  template <typename F>
  static auto makeSystem(const F &fp) -> HeaderPath
  {
    return {fp, HeaderPathType::System};
  }

  template <typename F>
  static auto makeFramework(const F &fp) -> HeaderPath
  {
    return {fp, HeaderPathType::Framework};
  }

  friend auto qHash(const HeaderPath &key, uint seed = 0)
  {
    return ((qHash(key.path) << 2) | uint(key.type)) ^ seed;
  }

  QString path;
  HeaderPathType type = HeaderPathType::User;
};

using HeaderPaths = QVector<HeaderPath>;

template <typename C>
auto toHeaderPaths(const C &list, HeaderPathType type) -> HeaderPaths
{
  return Utils::transform<HeaderPaths>(list, [type](const auto &fp) {
    return HeaderPath(fp, type);
  });
}

template <typename C>
auto toUserHeaderPaths(const C &list) -> HeaderPaths
{
  return toHeaderPaths(list, HeaderPathType::User);
}

template <typename C>
auto toBuiltInHeaderPaths(const C &list) -> HeaderPaths
{
  return toHeaderPaths(list, HeaderPathType::BuiltIn);
}

template <typename C>
auto toFrameworkHeaderPaths(const C &list) -> HeaderPaths
{
  return toHeaderPaths(list, HeaderPathType::Framework);
}

} // namespace ProjectExplorer
