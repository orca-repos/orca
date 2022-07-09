// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"
#include "baseqtversion.hpp"

namespace QtSupport {

class QTSUPPORT_EXPORT QtVersionManager : public QObject {
  Q_OBJECT
  // for getUniqueId();
  friend class QtVersion;
  friend class QtVersionFactory;
  friend class Internal::QtOptionsPageWidget;

public:
   QtVersionManager();
  ~QtVersionManager() override;

  static auto instance() -> QtVersionManager*;
  static auto initialized() -> void;
  static auto isLoaded() -> bool;

  // This will *always* return at least one (Qt in Path), even if that is
  // unconfigured. The lists here are in load-time order! Use sortVersions(...) if you
  // need a list sorted by Qt Version number.
  //
  // Note: DO NOT STORE THESE POINTERS!
  //       The QtVersionManager may delete them at random times and you will
  //       need to get a new pointer by calling this function again!
  static auto versions(const QtVersion::Predicate &predicate = {}) -> QtVersions;
  static auto version(int id) -> QtVersion*;
  static auto version(const QtVersion::Predicate &predicate) -> QtVersion*;

  // Sorting is potentially expensive since it might require qmake --query to run for each version!
  static auto sortVersions(const QtVersions &input) -> QtVersions;
  static auto addVersion(QtVersion *version) -> void;
  static auto removeVersion(QtVersion *version) -> void;

  // Call latest in extensionsInitialized of plugin depending on QtSupport
  static auto registerExampleSet(const QString &displayName, const QString &manifestPath, const QString &examplesPath) -> void;

signals:
  // content of QtVersion objects with qmake path might have changed
  auto qtVersionsChanged(const QList<int> &addedIds, const QList<int> &removedIds, const QList<int> &changedIds) -> void;
  auto qtVersionsLoaded() -> void;

private:
  enum class DocumentationSetting {
    HighestOnly,
    All,
    None
  };

  static auto updateDocumentation(const QtVersions &added, const QtVersions &removed, const QtVersions &allNew) -> void;
  auto updateFromInstaller(bool emitSignal = true) -> void;
  auto triggerQtVersionRestore() -> void;
  // Used by QtOptionsPage
  static auto setNewQtVersions(const QtVersions &newVersions) -> void;
  static auto setDocumentationSetting(const DocumentationSetting &setting) -> void;
  static auto documentationSetting() -> DocumentationSetting;
  // Used by QtVersion
  static auto getUniqueId() -> int;
};

} // namespace QtSupport
