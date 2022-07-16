// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildstep.hpp"
#include "namedwidget.hpp"
#include <utils/detailsbutton.hpp>

QT_BEGIN_NAMESPACE
class QPushButton;
class QToolButton;
class QLabel;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Utils {
class DetailsWidget;
}

namespace ProjectExplorer {
namespace Internal {

class ToolWidget : public Utils::FadingPanel {
  Q_OBJECT

public:
  explicit ToolWidget(QWidget *parent = nullptr);

  auto fadeTo(qreal value) -> void override;
  auto setOpacity(qreal value) -> void override;
  auto setBuildStepEnabled(bool b) -> void;
  auto setUpEnabled(bool b) -> void;
  auto setDownEnabled(bool b) -> void;
  auto setRemoveEnabled(bool b) -> void;
  auto setUpVisible(bool b) -> void;
  auto setDownVisible(bool b) -> void;

signals:
  auto disabledClicked() -> void;
  auto upClicked() -> void;
  auto downClicked() -> void;
  auto removeClicked() -> void;

private:
  QToolButton *m_disableButton;
  QToolButton *m_upButton;
  QToolButton *m_downButton;
  QToolButton *m_removeButton;
  bool m_buildStepEnabled = true;
  Utils::FadingWidget *m_firstWidget;
  Utils::FadingWidget *m_secondWidget;
  qreal m_targetOpacity = .999;
};

class BuildStepsWidgetData {
public:
  BuildStepsWidgetData(BuildStep *s);
  ~BuildStepsWidgetData();

  BuildStep *step;
  QWidget *widget;
  Utils::DetailsWidget *detailsWidget;
  ToolWidget *toolWidget;
};

class BuildStepListWidget : public NamedWidget {
  Q_OBJECT

public:
  explicit BuildStepListWidget(BuildStepList *bsl);
  ~BuildStepListWidget() override;

private:
  auto updateAddBuildStepMenu() -> void;
  auto addBuildStep(int pos) -> void;
  auto stepMoved(int from, int to) -> void;
  auto removeBuildStep(int pos) -> void;
  auto setupUi() -> void;
  auto updateBuildStepButtonsState() -> void;

  BuildStepList *m_buildStepList = nullptr;
  QList<BuildStepsWidgetData*> m_buildStepsData;
  QVBoxLayout *m_vbox = nullptr;
  QLabel *m_noStepsLabel = nullptr;
  QPushButton *m_addButton = nullptr;
};

} // Internal
} // ProjectExplorer
