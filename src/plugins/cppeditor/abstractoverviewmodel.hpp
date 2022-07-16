// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <utils/dropsupport.hpp>
#include <utils/treemodel.hpp>

#include <QSharedPointer>

#include <utility>

namespace CPlusPlus {
class Document;
}

namespace Utils {
class LineColumn;
class Link;
}

namespace CppEditor {

class CPPEDITOR_EXPORT AbstractOverviewModel : public Utils::TreeModel<> {
  Q_OBJECT

public:
  enum Role {
    FileNameRole = Qt::UserRole + 1,
    LineNumberRole
  };

  virtual auto rebuild(QSharedPointer<CPlusPlus::Document>) -> void {}
  virtual auto rebuild(const QString &) -> bool { return false; }

  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override
  {
    if (!index.isValid())
      return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
  }

  auto supportedDragActions() const -> Qt::DropActions override
  {
    return Qt::MoveAction;
  }

  auto mimeTypes() const -> QStringList override
  {
    return Utils::DropSupport::mimeTypesForFilePaths();
  }

  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override
  {
    auto mimeData = new Utils::DropMimeData;
    foreach(const QModelIndex &index, indexes) {
      const auto fileName = data(index, FileNameRole);
      if (!fileName.canConvert<QString>())
        continue;
      const auto lineNumber = data(index, LineNumberRole);
      if (!lineNumber.canConvert<unsigned>())
        continue;
      mimeData->addFile(Utils::FilePath::fromVariant(fileName), static_cast<int>(lineNumber.value<unsigned>()));
    }
    return mimeData;
  }

  virtual auto isGenerated(const QModelIndex &) const -> bool { return false; }
  virtual auto linkFromIndex(const QModelIndex &) const -> Utils::Link = 0;
  virtual auto lineColumnFromIndex(const QModelIndex &) const -> Utils::LineColumn = 0;

  using Range = std::pair<Utils::LineColumn, Utils::LineColumn>;
  virtual auto rangeFromIndex(const QModelIndex &) const -> Range = 0;

signals:
  auto needsUpdate() -> void;
};

} // namespace CppEditor
