// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonkitspage.hpp"
#include "jsonwizard.hpp"

#include "../kit.hpp"
#include "../project.hpp"
#include "../projectexplorer.hpp"
#include "../projectmanager.hpp"

#include <core/core-feature-provider.hpp>

#include <utils/algorithm.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {

constexpr char KEY_FEATURE[] = "feature";
constexpr char KEY_CONDITION[] = "condition";

JsonKitsPage::JsonKitsPage(QWidget *parent) : TargetSetupPage(parent) { }

auto JsonKitsPage::initializePage() -> void
{
  const auto wiz = qobject_cast<JsonWizard*>(wizard());
  QTC_ASSERT(wiz, return);

  connect(wiz, &JsonWizard::filesPolished, this, &JsonKitsPage::setupProjectFiles);

  const auto platform = Id::fromString(wiz->stringValue(QLatin1String("Platform")));
  const auto preferred = evaluate(m_preferredFeatures, wiz->value(QLatin1String("PreferredFeatures")), wiz);
  const auto required = evaluate(m_requiredFeatures, wiz->value(QLatin1String("RequiredFeatures")), wiz);

  setTasksGenerator([required, preferred, platform](const Kit *k) -> Tasks {
    if (!k->hasFeatures(required))
      return {CompileTask(Task::Error, tr("At least one required feature is not present."))};
    if (!k->supportedPlatforms().contains(platform))
      return {CompileTask(Task::Unknown, tr("Platform is not supported."))};
    if (!k->hasFeatures(preferred))
      return {CompileTask(Task::Unknown, tr("At least one preferred feature is not present."))};
    return {};
  });
  setProjectPath(wiz->expander()->expand(FilePath::fromString(unexpandedProjectPath())));

  TargetSetupPage::initializePage();
}

auto JsonKitsPage::cleanupPage() -> void
{
  const auto wiz = qobject_cast<JsonWizard*>(wizard());
  QTC_ASSERT(wiz, return);

  disconnect(wiz, &JsonWizard::allDone, this, nullptr);

  TargetSetupPage::cleanupPage();
}

auto JsonKitsPage::setUnexpandedProjectPath(const QString &path) -> void
{
  m_unexpandedProjectPath = path;
}

auto JsonKitsPage::unexpandedProjectPath() const -> QString
{
  return m_unexpandedProjectPath;
}

auto JsonKitsPage::setRequiredFeatures(const QVariant &data) -> void
{
  m_requiredFeatures = parseFeatures(data);
}

auto JsonKitsPage::setPreferredFeatures(const QVariant &data) -> void
{
  m_preferredFeatures = parseFeatures(data);
}

auto JsonKitsPage::setupProjectFiles(const JsonWizard::GeneratorFiles &files) -> void
{
  for (const auto &f : files) {
    if (f.file.attributes() & GeneratedFile::OpenProjectAttribute) {
      const QFileInfo fi(f.file.path());
      const auto path = fi.absoluteFilePath();
      const auto project = ProjectManager::openProject(mimeTypeForFile(fi), FilePath::fromString(path));
      if (project) {
        if (setupProject(project))
          project->saveSettings();
        delete project;
      }
    }
  }
}

auto JsonKitsPage::evaluate(const QVector<ConditionalFeature> &list, const QVariant &defaultSet, JsonWizard *wiz) -> QSet<Id>
{
  if (list.isEmpty())
    return Id::fromStringList(defaultSet.toStringList());

  QSet<Id> features;
  foreach(const ConditionalFeature &f, list) {
    if (JsonWizard::boolFromVariant(f.condition, wiz->expander()))
      features.insert(Id::fromString(wiz->expander()->expand(f.feature)));
  }
  return features;
}

auto JsonKitsPage::parseFeatures(const QVariant &data, QString *errorMessage) -> QVector<ConditionalFeature>
{
  QVector<ConditionalFeature> result;
  if (errorMessage)
    errorMessage->clear();

  if (data.isNull())
    return result;
  if (data.type() != QVariant::List) {
    if (errorMessage)
      *errorMessage = tr("Feature list is set and not of type list.");
    return result;
  }

  foreach(const QVariant &element, data.toList()) {
    if (element.type() == QVariant::String) {
      result.append({element.toString(), QVariant(true)});
    } else if (element.type() == QVariant::Map) {
      const auto obj = element.toMap();
      const auto feature = obj.value(QLatin1String(KEY_FEATURE)).toString();
      if (feature.isEmpty()) {
        if (errorMessage) {
          *errorMessage = tr("No \"%1\" key found in feature list object.").arg(QLatin1String(KEY_FEATURE));
        }
        return QVector<ConditionalFeature>();
      }

      result.append({feature, obj.value(QLatin1String(KEY_CONDITION), true)});
    } else {
      if (errorMessage)
        *errorMessage = tr("Feature list element is not a string or object.");
      return QVector<ConditionalFeature>();
    }
  }

  return result;
}

} // namespace ProjectExplorer
