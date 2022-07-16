// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtbuildaspects.hpp"

#include "baseqtversion.hpp"

#include <projectexplorer/buildpropertiessettings.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/kitmanager.hpp>

#include <utils/infolabel.hpp>
#include <utils/layoutbuilder.hpp>

#include <QCheckBox>
#include <QLayout>

using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {

QmlDebuggingAspect::QmlDebuggingAspect()
{
  setSettingsKey("EnableQmlDebugging");
  setDisplayName(tr("QML debugging and profiling:"));
  setValue(ProjectExplorerPlugin::buildPropertiesSettings().qmlDebugging.value());
}

auto QmlDebuggingAspect::addToLayout(LayoutBuilder &builder) -> void
{
  SelectionAspect::addToLayout(builder);
  const auto warningLabel = createSubWidget<InfoLabel>(QString(), InfoLabel::Warning);
  warningLabel->setElideMode(Qt::ElideNone);
  warningLabel->setVisible(false);
  builder.addRow({{}, warningLabel});
  const auto changeHandler = [this, warningLabel] {
    QString warningText;
    const auto supported = m_kit && QtVersion::isQmlDebuggingSupported(m_kit, &warningText);
    if (!supported) {
      setValue(TriState::Default);
    } else if (value() == TriState::Enabled) {
      warningText = tr("Might make your application vulnerable.<br/>" "Only use in a safe environment.");
    }
    warningLabel->setText(warningText);
    setVisible(supported);
    const auto warningLabelsVisible = supported && !warningText.isEmpty();
    if (warningLabel->parentWidget())
      warningLabel->setVisible(warningLabelsVisible);
  };
  connect(KitManager::instance(), &KitManager::kitsChanged, warningLabel, changeHandler);
  connect(this, &QmlDebuggingAspect::changed, warningLabel, changeHandler);
  changeHandler();
}

QtQuickCompilerAspect::QtQuickCompilerAspect()
{
  setSettingsKey("QtQuickCompiler");
  setDisplayName(tr("Qt Quick Compiler:"));
  setValue(ProjectExplorerPlugin::buildPropertiesSettings().qtQuickCompiler.value());
}

auto QtQuickCompilerAspect::addToLayout(LayoutBuilder &builder) -> void
{
  SelectionAspect::addToLayout(builder);
  const auto warningLabel = createSubWidget<InfoLabel>(QString(), InfoLabel::Warning);
  warningLabel->setElideMode(Qt::ElideNone);
  warningLabel->setVisible(false);
  builder.addRow({{}, warningLabel});
  const auto changeHandler = [this, warningLabel] {
    QString warningText;
    const auto supported = m_kit && QtVersion::isQtQuickCompilerSupported(m_kit, &warningText);
    if (!supported)
      setValue(TriState::Default);
    if (value() == TriState::Enabled && m_qmlDebuggingAspect && m_qmlDebuggingAspect->value() == TriState::Enabled) {
      warningText = tr("Disables QML debugging. QML profiling will still work.");
    }
    warningLabel->setText(warningText);
    setVisible(supported);
    const auto warningLabelsVisible = supported && !warningText.isEmpty();
    if (warningLabel->parentWidget())
      warningLabel->setVisible(warningLabelsVisible);
  };
  connect(KitManager::instance(), &KitManager::kitsChanged, warningLabel, changeHandler);
  connect(this, &QmlDebuggingAspect::changed, warningLabel, changeHandler);
  connect(this, &QtQuickCompilerAspect::changed, warningLabel, changeHandler);
  if (m_qmlDebuggingAspect)
    connect(m_qmlDebuggingAspect, &QmlDebuggingAspect::changed, warningLabel, changeHandler);
  changeHandler();
}

auto QtQuickCompilerAspect::acquaintSiblings(const AspectContainer &siblings) -> void
{
  m_qmlDebuggingAspect = siblings.aspect<QmlDebuggingAspect>();
}

} // namespace QtSupport
