// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "targetsetupwidget.hpp"

#include "buildconfiguration.hpp"
#include "buildinfo.hpp"
#include "projectexplorerconstants.hpp"
#include "kitmanager.hpp"
#include "kitoptionspage.hpp"

#include <core/core-interface.hpp>

#include <utils/algorithm.hpp>
#include <utils/detailsbutton.hpp>
#include <utils/detailswidget.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

// -------------------------------------------------------------------------
// TargetSetupWidget
// -------------------------------------------------------------------------

TargetSetupWidget::TargetSetupWidget(Kit *k, const FilePath &projectPath) : m_kit(k)
{
  Q_ASSERT(m_kit);

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  const auto vboxLayout = new QVBoxLayout();
  setLayout(vboxLayout);
  vboxLayout->setContentsMargins(0, 0, 0, 0);
  m_detailsWidget = new DetailsWidget(this);
  m_detailsWidget->setUseCheckBox(true);
  m_detailsWidget->setChecked(false);
  m_detailsWidget->setSummaryFontBold(true);
  vboxLayout->addWidget(m_detailsWidget);

  const auto panel = new FadingWidget(m_detailsWidget);
  const auto panelLayout = new QHBoxLayout(panel);
  m_manageButton = new QPushButton(KitAspectWidget::msgManage());
  panelLayout->addWidget(m_manageButton);
  m_detailsWidget->setToolWidget(panel);

  const auto widget = new QWidget;
  const auto layout = new QVBoxLayout;
  widget->setLayout(layout);
  layout->setContentsMargins(0, 0, 0, 0);

  const auto w = new QWidget;
  m_newBuildsLayout = new QGridLayout;
  m_newBuildsLayout->setContentsMargins(0, 0, 0, 0);
  if (HostOsInfo::isMacHost())
    m_newBuildsLayout->setSpacing(0);
  w->setLayout(m_newBuildsLayout);
  layout->addWidget(w);

  widget->setEnabled(false);
  m_detailsWidget->setWidget(widget);

  setProjectPath(projectPath);

  connect(m_detailsWidget, &DetailsWidget::checked, this, &TargetSetupWidget::targetCheckBoxToggled);

  connect(m_manageButton, &QAbstractButton::clicked, this, &TargetSetupWidget::manageKit);
}

auto TargetSetupWidget::kit() const -> Kit*
{
  return m_kit;
}

auto TargetSetupWidget::clearKit() -> void
{
  m_kit = nullptr;
}

auto TargetSetupWidget::isKitSelected() const -> bool
{
  if (!m_kit || !m_detailsWidget->isChecked())
    return false;

  return !selectedBuildInfoList().isEmpty();
}

auto TargetSetupWidget::setKitSelected(bool b) -> void
{
  // Only check target if there are build configurations possible
  b &= hasSelectedBuildConfigurations();
  m_ignoreChange = true;
  m_detailsWidget->setChecked(b);
  m_detailsWidget->widget()->setEnabled(b);
  m_ignoreChange = false;
}

auto TargetSetupWidget::addBuildInfo(const BuildInfo &info, bool isImport) -> void
{
  QTC_ASSERT(info.kitId == m_kit->id(), return);

  if (isImport && !m_haveImported) {
    // disable everything on first import
    for (auto &store : m_infoStore) {
      store.isEnabled = false;
      store.checkbox->setChecked(false);
    }
    m_selected = 0;

    m_haveImported = true;
  }

  const auto pos = static_cast<int>(m_infoStore.size());

  BuildInfoStore store;
  store.buildInfo = info;
  store.isEnabled = true;
  ++m_selected;

  if (info.factory) {
    store.checkbox = new QCheckBox;
    store.checkbox->setText(info.displayName);
    store.checkbox->setChecked(store.isEnabled);
    store.checkbox->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    m_newBuildsLayout->addWidget(store.checkbox, pos * 2, 0);

    store.pathChooser = new PathChooser();
    store.pathChooser->setExpectedKind(PathChooser::Directory);
    store.pathChooser->setFilePath(info.buildDirectory);
    store.pathChooser->setHistoryCompleter(QLatin1String("TargetSetup.BuildDir.History"));
    store.pathChooser->setReadOnly(isImport);
    m_newBuildsLayout->addWidget(store.pathChooser, pos * 2, 1);

    store.issuesLabel = new QLabel;
    store.issuesLabel->setIndent(32);
    m_newBuildsLayout->addWidget(store.issuesLabel, pos * 2 + 1, 0, 1, 2);
    store.issuesLabel->setVisible(false);

    connect(store.checkbox, &QAbstractButton::toggled, this, &TargetSetupWidget::checkBoxToggled);
    connect(store.pathChooser, &PathChooser::rawPathChanged, this, &TargetSetupWidget::pathChanged);
  }

  store.hasIssues = false;
  m_infoStore.emplace_back(std::move(store));

  reportIssues(pos);

  emit selectedToggled();
}

auto TargetSetupWidget::targetCheckBoxToggled(bool b) -> void
{
  if (m_ignoreChange)
    return;
  m_detailsWidget->widget()->setEnabled(b);
  if (b && (contains(m_infoStore, &BuildInfoStore::hasIssues) || !contains(m_infoStore, &BuildInfoStore::isEnabled))) {
    m_detailsWidget->setState(DetailsWidget::Expanded);
  } else if (!b) {
    m_detailsWidget->setState(DetailsWidget::Collapsed);
  }
  emit selectedToggled();
}

auto TargetSetupWidget::manageKit() -> void
{
  if (!m_kit)
    return;

  if (const auto kitPage = KitOptionsPage::instance()) {
    kitPage->showKit(m_kit);
    Orca::Plugin::Core::ICore::showOptionsDialog(Constants::KITS_SETTINGS_PAGE_ID, parentWidget());
  }
}

auto TargetSetupWidget::setProjectPath(const FilePath &projectPath) -> void
{
  if (!m_kit)
    return;

  m_projectPath = projectPath;
  clear();

  for (const auto &info : buildInfoList(m_kit, projectPath))
    addBuildInfo(info, false);
}

auto TargetSetupWidget::expandWidget() -> void
{
  m_detailsWidget->setState(DetailsWidget::Expanded);
}

auto TargetSetupWidget::update(const TasksGenerator &generator) -> void
{
  const auto tasks = generator(kit());

  m_detailsWidget->setSummaryText(kit()->displayName());
  m_detailsWidget->setIcon(kit()->isValid() ? kit()->icon() : Icons::CRITICAL.icon());

  const auto errorTask = findOrDefault(tasks, equal(&Task::type, Task::Error));

  // Kits that where the taskGenarator reports an error are not selectable, because we cannot
  // guarantee that we can handle the project sensibly (e.g. qmake project without Qt).
  if (!errorTask.isNull()) {
    toggleEnabled(false);
    m_detailsWidget->setToolTip(kit()->toHtml(tasks, ""));
    m_infoStore.clear();
    return;
  }

  toggleEnabled(true);
  updateDefaultBuildDirectories();
}

auto TargetSetupWidget::buildInfoList(const Kit *k, const FilePath &projectPath) -> const QList<BuildInfo>
{
  if (const auto factory = BuildConfigurationFactory::find(k, projectPath))
    return factory->allAvailableSetups(k, projectPath);

  BuildInfo info;
  info.kitId = k->id();
  return {info};
}

auto TargetSetupWidget::hasSelectedBuildConfigurations() const -> bool
{
  return !selectedBuildInfoList().isEmpty();
}

auto TargetSetupWidget::toggleEnabled(bool enabled) -> void
{
  m_detailsWidget->widget()->setEnabled(enabled && hasSelectedBuildConfigurations());
  m_detailsWidget->setCheckable(enabled);
  m_detailsWidget->setExpandable(enabled);
  if (!enabled) {
    m_detailsWidget->setState(DetailsWidget::Collapsed);
    m_detailsWidget->setChecked(false);
  }
}

auto TargetSetupWidget::selectedBuildInfoList() const -> const QList<BuildInfo>
{
  QList<BuildInfo> result;
  for (const auto &store : m_infoStore) {
    if (store.isEnabled)
      result.append(store.buildInfo);
  }
  return result;
}

auto TargetSetupWidget::clear() -> void
{
  m_infoStore.clear();

  m_selected = 0;
  m_haveImported = false;

  emit selectedToggled();
}

auto TargetSetupWidget::updateDefaultBuildDirectories() -> void
{
  for (const auto &buildInfo : buildInfoList(m_kit, m_projectPath)) {
    if (!buildInfo.factory)
      continue;
    auto found = false;
    for (auto &buildInfoStore : m_infoStore) {
      if (buildInfoStore.buildInfo.typeName == buildInfo.typeName) {
        if (!buildInfoStore.customBuildDir) {
          m_ignoreChange = true;
          buildInfoStore.pathChooser->setFilePath(buildInfo.buildDirectory);
          m_ignoreChange = false;
        }
        found = true;
        break;
      }
    }
    if (!found) // the change of the kit may have produced more build information than before
      addBuildInfo(buildInfo, false);
  }
}

auto TargetSetupWidget::checkBoxToggled(bool b) -> void
{
  auto box = qobject_cast<QCheckBox*>(sender());
  if (!box)
    return;
  const auto it = std::find_if(m_infoStore.begin(), m_infoStore.end(), [box](const BuildInfoStore &store) { return store.checkbox == box; });
  QTC_ASSERT(it != m_infoStore.end(), return);
  if (it->isEnabled == b)
    return;
  m_selected += b ? 1 : -1;
  it->isEnabled = b;
  if ((m_selected == 0 && !b) || (m_selected == 1 && b)) {
    emit selectedToggled();
    m_detailsWidget->setChecked(b);
  }
}

auto TargetSetupWidget::pathChanged() -> void
{
  if (m_ignoreChange)
    return;
  auto pathChooser = qobject_cast<PathChooser*>(sender());
  QTC_ASSERT(pathChooser, return);

  const auto it = std::find_if(m_infoStore.begin(), m_infoStore.end(), [pathChooser](const BuildInfoStore &store) {
    return store.pathChooser == pathChooser;
  });
  QTC_ASSERT(it != m_infoStore.end(), return);
  it->buildInfo.buildDirectory = pathChooser->filePath();
  it->customBuildDir = true;
  reportIssues(static_cast<int>(std::distance(m_infoStore.begin(), it)));
}

auto TargetSetupWidget::reportIssues(int index) -> void
{
  const auto size = static_cast<int>(m_infoStore.size());
  QTC_ASSERT(index >= 0 && index < size, return);

  auto &store = m_infoStore[static_cast<size_t>(index)];
  if (store.issuesLabel) {
    const auto issues = findIssues(store.buildInfo);
    store.issuesLabel->setText(issues.second);
    store.hasIssues = issues.first != Task::Unknown;
    store.issuesLabel->setVisible(store.hasIssues);
  }
}

auto TargetSetupWidget::findIssues(const BuildInfo &info) -> QPair<Task::TaskType, QString>
{
  if (m_projectPath.isEmpty() || !info.factory)
    return qMakePair(Task::Unknown, QString());

  const auto buildDir = info.buildDirectory.toString();
  Tasks issues;
  if (info.factory)
    issues = info.factory->reportIssues(m_kit, m_projectPath.toString(), buildDir);

  QString text;
  auto highestType = Task::Unknown;
  foreach(const Task &t, issues) {
    if (!text.isEmpty())
      text.append(QLatin1String("<br>"));
    // set severity:
    QString severity;
    if (t.type == Task::Error) {
      highestType = Task::Error;
      severity = tr("<b>Error:</b> ", "Severity is Task::Error");
    } else if (t.type == Task::Warning) {
      if (highestType == Task::Unknown)
        highestType = Task::Warning;
      severity = tr("<b>Warning:</b> ", "Severity is Task::Warning");
    }
    text.append(severity + t.description());
  }
  if (!text.isEmpty())
    text = QLatin1String("<nobr>") + text;
  return qMakePair(highestType, text);
}

TargetSetupWidget::BuildInfoStore::~BuildInfoStore()
{
  delete checkbox;
  delete label;
  delete issuesLabel;
  delete pathChooser;
}

TargetSetupWidget::BuildInfoStore::BuildInfoStore(BuildInfoStore &&other)
{
  std::swap(other.buildInfo, buildInfo);
  std::swap(other.checkbox, checkbox);
  std::swap(other.label, label);
  std::swap(other.issuesLabel, issuesLabel);
  std::swap(other.pathChooser, pathChooser);
  std::swap(other.isEnabled, isEnabled);
  std::swap(other.hasIssues, hasIssues);
}

} // namespace Internal
} // namespace ProjectExplorer
