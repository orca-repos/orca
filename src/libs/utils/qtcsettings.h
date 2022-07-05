// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QSettings>

namespace Utils {

class ORCA_UTILS_EXPORT QtcSettings : public QSettings {
public:
  using QSettings::QSettings;

  template <typename T>
  auto setValueWithDefault(const QString &key, const T &val, const T &defaultValue) -> void;
  template <typename T>
  auto setValueWithDefault(const QString &key, const T &val) -> void;

  template <typename T>
  static auto setValueWithDefault(QSettings *settings, const QString &key, const T &val, const T &defaultValue) -> void;
  template <typename T>
  static auto setValueWithDefault(QSettings *settings, const QString &key, const T &val) -> void;
};

template <typename T>
auto QtcSettings::setValueWithDefault(const QString &key, const T &val, const T &defaultValue) -> void
{
  setValueWithDefault(this, key, val, defaultValue);
}

template <typename T>
auto QtcSettings::setValueWithDefault(const QString &key, const T &val) -> void
{
  setValueWithDefault(this, key, val);
}

template <typename T>
auto QtcSettings::setValueWithDefault(QSettings *settings, const QString &key, const T &val, const T &defaultValue) -> void
{
  if (val == defaultValue)
    settings->remove(key);
  else
    settings->setValue(key, QVariant::fromValue(val));
}

template <typename T>
auto QtcSettings::setValueWithDefault(QSettings *settings, const QString &key, const T &val) -> void
{
  if (val == T())
    settings->remove(key);
  else
    settings->setValue(key, QVariant::fromValue(val));
}

} // namespace Utils
