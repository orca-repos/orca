// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ilocatorfilter.h"

#include <utils/fileutils.h>

#include <QSharedPointer>

namespace Core {

namespace Internal {
class BaseFileFilterPrivate;
}

class CORE_EXPORT BaseFileFilter : public ILocatorFilter {
  Q_OBJECT

public:
  class CORE_EXPORT Iterator {

  public:
    virtual ~Iterator();
    virtual auto toFront() -> void = 0;
    virtual auto hasNext() const -> bool = 0;
    virtual auto next() -> Utils::FilePath = 0;
    virtual auto filePath() const -> Utils::FilePath = 0;
  };

  class CORE_EXPORT ListIterator final : public Iterator {
  public:
    explicit ListIterator(const Utils::FilePaths &file_paths);

    auto toFront() -> void override;
    auto hasNext() const -> bool override;
    auto next() -> Utils::FilePath override;
    auto filePath() const -> Utils::FilePath override;

  private:
    Utils::FilePaths m_file_paths;
    Utils::FilePaths::const_iterator m_path_position;
  };

  BaseFileFilter();
  ~BaseFileFilter() override;

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  static auto openEditorAt(const LocatorFilterEntry &selection) -> void;

protected:
  auto setFileIterator(Iterator *iterator) const -> void;
  auto fileIterator() const -> QSharedPointer<Iterator>;

private:
  static auto matchLevelFor(const QRegularExpressionMatch &match, const QString &match_text) -> MatchLevel;
  auto updatePreviousResultData() const -> void;

  Internal::BaseFileFilterPrivate *d = nullptr;
};

} // namespace Core
