// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "environmentaspect.hpp"
#include "runconfiguration.hpp"

#include <utils/environment.hpp>

#include <QList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

namespace Utils {
class DetailsWidget;
}

namespace ProjectExplorer {

class EnvironmentWidget;

class PROJECTEXPLORER_EXPORT EnvironmentAspectWidget : public QWidget {
  Q_OBJECT

public:
  explicit EnvironmentAspectWidget(EnvironmentAspect *aspect, QWidget *additionalWidget = nullptr);
  virtual auto aspect() const -> EnvironmentAspect*;
  auto envWidget() const -> EnvironmentWidget* { return m_environmentWidget; }
  auto additionalWidget() const -> QWidget*;

private:
  auto baseEnvironmentSelected(int idx) -> void;
  auto changeBaseEnvironment() -> void;
  auto userChangesEdited() -> void;
  auto changeUserChanges(Utils::EnvironmentItems changes) -> void;
  auto environmentChanged() -> void;

  EnvironmentAspect *m_aspect;
  bool m_ignoreChange = false;
  QWidget *m_additionalWidget = nullptr;
  QComboBox *m_baseEnvironmentComboBox = nullptr;
  EnvironmentWidget *m_environmentWidget = nullptr;
};

} // namespace ProjectExplorer
