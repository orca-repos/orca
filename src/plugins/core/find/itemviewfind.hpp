// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ifindsupport.hpp"

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QFrame;
class QModelIndex;
QT_END_NAMESPACE

namespace Core {
class ItemModelFindPrivate;

class CORE_EXPORT ItemViewFind final : public IFindSupport {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ItemViewFind)

public:
  enum FetchOption {
    DoNotFetchMoreWhileSearching,
    FetchMoreWhileSearching
  };

  enum ColorOption {
    DarkColored = 0,
    LightColored = 1
  };

  explicit ItemViewFind(QAbstractItemView *view, int role = Qt::DisplayRole, FetchOption option = DoNotFetchMoreWhileSearching);
  ~ItemViewFind() override;

  auto supportsReplace() const -> bool override;
  auto supportedFindFlags() const -> FindFlags override;
  auto resetIncrementalSearch() -> void override;
  auto clearHighlights() -> void override;
  auto currentFindString() const -> QString override;
  auto completedFindString() const -> QString override;
  auto highlightAll(const QString &txt, FindFlags find_flags) -> void override;
  auto findIncremental(const QString &txt, FindFlags find_flags) -> Result override;
  auto findStep(const QString &txt, FindFlags find_flags) -> Result override;
  static auto createSearchableWrapper(QAbstractItemView *tree_view, ColorOption color_option = DarkColored, FetchOption option = DoNotFetchMoreWhileSearching) -> QFrame*;
  static auto createSearchableWrapper(ItemViewFind *finder, ColorOption color_option = DarkColored) -> QFrame*;

private:
  auto find(const QString &search_txt, FindFlags find_flags, bool start_from_current_index, bool *wrapped) const -> Result;
  auto nextIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex;
  auto prevIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex;
  auto followingIndex(const QModelIndex &idx, bool backward, bool *wrapped) const -> QModelIndex;

  ItemModelFindPrivate *d;
};

} // namespace Core
