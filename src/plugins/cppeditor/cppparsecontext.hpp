// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cpptoolsreuse.hpp"

#include <QAbstractListModel>
#include <QComboBox>

namespace CppEditor {
namespace Internal {

class ParseContextModel : public QAbstractListModel {
  Q_OBJECT

public:
  auto update(const ProjectPartInfo &projectPartInfo) -> void;
  auto setPreferred(int index) -> void;
  auto clearPreferred() -> void;
  auto currentIndex() const -> int;
  auto isCurrentPreferred() const -> bool;
  auto currentId() const -> QString;
  auto currentToolTip() const -> QString;
  auto areMultipleAvailable() const -> bool;

signals:
  auto updated(bool areMultipleAvailable) -> void;
  auto preferredParseContextChanged(const QString &id) -> void;

private:
  auto reset(const ProjectPartInfo &projectPartInfo) -> void;
  auto rowCount(const QModelIndex &parent) const -> int override;
  auto data(const QModelIndex &index, int role) const -> QVariant override;
  
  ProjectPartInfo::Hints m_hints;
  QList<ProjectPart::ConstPtr> m_projectParts;
  int m_currentIndex = -1;
};

class ParseContextWidget : public QComboBox {
  Q_OBJECT

public:
  ParseContextWidget(ParseContextModel &parseContextModel, QWidget *parent);

  auto syncToModel() -> void;
  auto minimumSizeHint() const -> QSize final;

private:
  ParseContextModel &m_parseContextModel;
  QAction *m_clearPreferredAction = nullptr;
};

} // namespace Internal
} // namespace CppEditor
