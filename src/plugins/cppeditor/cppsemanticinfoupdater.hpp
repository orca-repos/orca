// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppsemanticinfo.hpp"

#include <QObject>
#include <QScopedPointer>

namespace CppEditor {

class SemanticInfoUpdaterPrivate;

class SemanticInfoUpdater : public QObject {
  Q_OBJECT

public:
  explicit SemanticInfoUpdater();
  ~SemanticInfoUpdater() override;

  auto semanticInfo() const -> SemanticInfo;
  auto update(const SemanticInfo::Source &source) -> SemanticInfo;
  auto updateDetached(const SemanticInfo::Source &source) -> void;

signals:
  auto updated(const SemanticInfo &semanticInfo) -> void;

private:
  QScopedPointer<SemanticInfoUpdaterPrivate> d;
};

} // namespace CppEditor
