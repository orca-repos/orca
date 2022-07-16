// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppminimizableinfobars.hpp"

#include "cppeditorconstants.hpp"
#include "cpptoolssettings.hpp"

#include <QToolButton>

#include <utils/infobar.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

using namespace Utils;

namespace CppEditor {
namespace Internal {

static auto settings() -> CppToolsSettings*
{
  return CppToolsSettings::instance();
}

MinimizableInfoBars::MinimizableInfoBars(InfoBar &infoBar, QObject *parent) : QObject(parent), m_infoBar(infoBar)
{
  connect(settings(), &CppToolsSettings::showHeaderErrorInfoBarChanged, this, &MinimizableInfoBars::updateHeaderErrors);
  connect(settings(), &CppToolsSettings::showNoProjectInfoBarChanged, this, &MinimizableInfoBars::updateNoProjectConfiguration);
}

auto MinimizableInfoBars::createShowInfoBarActions(const ActionCreator &actionCreator) -> MinimizableInfoBars::Actions
{
  Actions result;
  QTC_ASSERT(actionCreator, return result);

  // No project configuration available
  auto *button = new QToolButton();
  button->setToolTip(tr("File is not part of any project."));
  button->setIcon(Utils::Icons::WARNING_TOOLBAR.pixmap());
  connect(button, &QAbstractButton::clicked, []() {
    settings()->setShowNoProjectInfoBar(true);
  });
  auto action = actionCreator(button);
  action->setVisible(!settings()->showNoProjectInfoBar());
  result.insert(Constants::NO_PROJECT_CONFIGURATION, action);

  // Errors in included files
  button = new QToolButton();
  button->setToolTip(tr("File contains errors in included files."));
  button->setIcon(Utils::Icons::WARNING_TOOLBAR.pixmap());
  connect(button, &QAbstractButton::clicked, []() {
    settings()->setShowHeaderErrorInfoBar(true);
  });
  action = actionCreator(button);
  action->setVisible(!settings()->showHeaderErrorInfoBar());
  result.insert(Constants::ERRORS_IN_HEADER_FILES, action);

  return result;
}

auto MinimizableInfoBars::processHeaderDiagnostics(const DiagnosticWidgetCreator &diagnosticWidgetCreator) -> void
{
  m_diagnosticWidgetCreator = diagnosticWidgetCreator;
  updateHeaderErrors();
}

auto MinimizableInfoBars::processHasProjectPart(bool hasProjectPart) -> void
{
  m_hasProjectPart = hasProjectPart;
  updateNoProjectConfiguration();
}

auto MinimizableInfoBars::updateHeaderErrors() -> void
{
  const Id id(Constants::ERRORS_IN_HEADER_FILES);
  m_infoBar.removeInfo(id);

  auto show = false;
  // Show the info entry only if there is a project configuration.
  if (m_hasProjectPart && m_diagnosticWidgetCreator) {
    if (settings()->showHeaderErrorInfoBar())
      addHeaderErrorEntry(id, m_diagnosticWidgetCreator);
    else
      show = true;
  }

  emit showAction(id, show);
}

auto MinimizableInfoBars::updateNoProjectConfiguration() -> void
{
  const Id id(Constants::NO_PROJECT_CONFIGURATION);
  m_infoBar.removeInfo(id);

  auto show = false;
  if (!m_hasProjectPart) {
    if (settings()->showNoProjectInfoBar())
      addNoProjectConfigurationEntry(id);
    else
      show = true;
  }

  emit showAction(id, show);
}

static auto createMinimizableInfo(const Id &id, const QString &text, std::function<void()> minimizer) -> InfoBarEntry
{
  QTC_CHECK(minimizer);

  InfoBarEntry info(id, text);
  info.removeCancelButton();
  // The minimizer() might delete the "Minimize" button immediately and as
  // result invalid reads will happen in QToolButton::mouseReleaseEvent().
  // Avoid this by running the minimizer in the next event loop iteration.
  info.addCustomButton(MinimizableInfoBars::tr("Minimize"), [minimizer] {
    QMetaObject::invokeMethod(settings(), [minimizer] { minimizer(); }, Qt::QueuedConnection);
  });

  return info;
}

auto MinimizableInfoBars::addNoProjectConfigurationEntry(const Id &id) -> void
{
  const auto text = tr("<b>Warning</b>: This file is not part of any project. " "The code model might have issues parsing this file properly.");

  m_infoBar.addInfo(createMinimizableInfo(id, text, []() {
    settings()->setShowNoProjectInfoBar(false);
  }));
}

auto MinimizableInfoBars::addHeaderErrorEntry(const Id &id, const DiagnosticWidgetCreator &diagnosticWidgetCreator) -> void
{
  const auto text = tr("<b>Warning</b>: The code model could not parse an included file, " "which might lead to incorrect code completion and " "highlighting, for example.");

  auto info = createMinimizableInfo(id, text, []() {
    settings()->setShowHeaderErrorInfoBar(false);
  });
  info.setDetailsWidgetCreator(diagnosticWidgetCreator);

  m_infoBar.addInfo(info);
}

} // namespace Internal
} // namespace CppEditor
