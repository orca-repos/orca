// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/environmentfwd.hpp>
#include <utils/namevalueitem.hpp>

#include <QWidget>

#include <functional>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QModelIndex)

namespace ProjectExplorer {

class EnvironmentWidgetPrivate;

class PROJECTEXPLORER_EXPORT EnvironmentWidget : public QWidget {
  Q_OBJECT

public:
  enum Type {
    TypeLocal,
    TypeRemote
  };

  explicit EnvironmentWidget(QWidget *parent, Type type, QWidget *additionalDetailsWidget = nullptr);
  ~EnvironmentWidget() override;

  using OpenTerminalFunc = std::function<void(const Utils::Environment &env)>;

  auto setBaseEnvironmentText(const QString &text) -> void;
  auto setBaseEnvironment(const Utils::Environment &env) -> void;
  auto userChanges() const -> Utils::EnvironmentItems;
  auto setUserChanges(const Utils::EnvironmentItems &list) -> void;
  auto setOpenTerminalFunc(const OpenTerminalFunc &func) -> void;
  auto expand() -> void;

signals:
  auto userChangesChanged() -> void;
  auto detailsVisibleChanged(bool visible) -> void;

private:
  using PathListModifier = std::function<QString(const QString &oldList, const QString &newDir)>;

  auto editEnvironmentButtonClicked() -> void;
  auto addEnvironmentButtonClicked() -> void;
  auto removeEnvironmentButtonClicked() -> void;
  auto unsetEnvironmentButtonClicked() -> void;
  auto appendPathButtonClicked() -> void;
  auto prependPathButtonClicked() -> void;
  auto batchEditEnvironmentButtonClicked() -> void;
  auto environmentCurrentIndexChanged(const QModelIndex &current) -> void;
  auto invalidateCurrentIndex() -> void;
  auto updateSummaryText() -> void;
  auto focusIndex(const QModelIndex &index) -> void;
  auto updateButtons() -> void;
  auto linkActivated(const QString &link) -> void;
  auto amendPathList(Utils::NameValueItem::Operation op) -> void;

  const std::unique_ptr<EnvironmentWidgetPrivate> d;
};

} // namespace ProjectExplorer
