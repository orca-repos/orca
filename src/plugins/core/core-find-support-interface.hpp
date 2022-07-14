// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-text-find-constants.hpp"

#include <QObject>
#include <QString>

namespace Orca::Plugin::Core {

class CORE_EXPORT IFindSupport : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IFindSupport)

public:
  enum Result {
    Found,
    NotFound,
    NotYetFound
  };

  IFindSupport() : QObject(nullptr) {}
  ~IFindSupport() override = default;

  virtual auto supportsReplace() const -> bool = 0;
  virtual auto supportsSelectAll() const -> bool;
  virtual auto supportedFindFlags() const -> FindFlags = 0;
  virtual auto resetIncrementalSearch() -> void = 0;
  virtual auto clearHighlights() -> void = 0;
  virtual auto currentFindString() const -> QString = 0;
  virtual auto completedFindString() const -> QString = 0;
  virtual auto highlightAll(const QString &, FindFlags) -> void {}
  virtual auto findIncremental(const QString &txt, FindFlags find_flags) -> Result = 0;
  virtual auto findStep(const QString &txt, FindFlags find_flags) -> Result = 0;
  virtual auto replace(const QString &before, const QString &after, FindFlags find_flags) -> void;
  virtual auto replaceStep(const QString &before, const QString &after, FindFlags find_flags) -> bool;
  virtual auto replaceAll(const QString &before, const QString &after, FindFlags find_flags) -> int;
  virtual auto selectAll(const QString &txt, FindFlags find_flags) -> void;
  virtual auto defineFindScope() -> void {}
  virtual auto clearFindScope() -> void {}
  static auto showWrapIndicator(QWidget *parent) -> void;

signals:
  auto changed() -> void;
};

} // namespace Orca::Plugin::Core
