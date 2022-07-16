// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <QVariantMap>

namespace Utils {
class FilePath;
}

namespace QtSupport {

class QtVersion;

class QTSUPPORT_EXPORT QtVersionFactory {
public:
  QtVersionFactory();
  virtual ~QtVersionFactory();

  static auto allQtVersionFactories() -> const QList<QtVersionFactory*>;
  auto canRestore(const QString &type) -> bool;
  auto restore(const QString &type, const QVariantMap &data) -> QtVersion*;

  /// factories with higher priority are asked first to identify
  /// a qtversion, the priority of the desktop factory is 0 and
  /// the desktop factory claims to handle all paths
  auto priority() const -> int { return m_priority; }

  static auto createQtVersionFromQMakePath(const Utils::FilePath &qmakePath, bool isAutoDetected = false, const QString &detectionSource = {}, QString *error = nullptr) -> QtVersion*;

protected:
  struct SetupData {
    QStringList platforms;
    QStringList config;
    bool isQnx = false; // eeks...
  };

  auto setQtVersionCreator(const std::function<QtVersion *()> &creator) -> void;
  auto setRestrictionChecker(const std::function<bool(const SetupData &)> &checker) -> void;
  auto setSupportedType(const QString &type) -> void;
  auto setPriority(int priority) -> void;

private:
  friend class QtVersion;
  auto create() const -> QtVersion*;

  std::function<QtVersion *()> m_creator;
  std::function<bool(const SetupData &)> m_restrictionChecker;
  QString m_supportedType;
  int m_priority = 0;
};

} // namespace QtSupport
