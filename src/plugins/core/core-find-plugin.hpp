// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-text-find-constants.hpp"

#include <QObject>

QT_BEGIN_NAMESPACE
class QAbstractListModel;
class QStringListModel;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class IFindFilter;
class CorePlugin;

class CORE_EXPORT Find final : public QObject {
  Q_OBJECT

public:
  static auto instance() -> Find*;

  enum find_direction {
    find_forward_direction,
    find_backward_direction
  };

  enum {
    completion_model_find_flags_role = Qt::UserRole + 1
  };

  static auto findFlags() -> FindFlags;
  static auto hasFindFlag(FindFlag flag) -> bool;
  static auto updateFindCompletion(const QString &text, FindFlags flags = {}) -> void;
  static auto updateReplaceCompletion(const QString &text) -> void;
  static auto findCompletionModel() -> QAbstractListModel*;
  static auto replaceCompletionModel() -> QStringListModel*;
  static auto setUseFakeVim(bool on) -> void;
  static auto openFindToolBar(find_direction direction) -> void;
  static auto openFindDialog(IFindFilter *filter) -> void;
  static auto setCaseSensitive(bool sensitive) -> void;
  static auto setWholeWord(bool whole_only) -> void;
  static auto setBackward(bool backward) -> void;
  static auto setRegularExpression(bool reg_exp) -> void;
  static auto setPreserveCase(bool preserve_case) -> void;

signals:
  auto findFlagsChanged() -> void;

private:
  static auto initialize() -> void;
  static auto extensionsInitialized() -> void;
  static auto aboutToShutdown() -> void;
  static auto destroy() -> void;

  friend class CorePlugin;
};

} // namespace Orca::Plugin::Core
