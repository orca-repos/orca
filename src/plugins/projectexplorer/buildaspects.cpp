// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildaspects.hpp"

#include "buildconfiguration.hpp"
#include "buildpropertiessettings.hpp"
#include "projectexplorer.hpp"

#include <core/fileutils.hpp>

#include <utils/fileutils.hpp>
#include <utils/infolabel.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/pathchooser.hpp>

#include <QLayout>

using namespace Utils;

namespace ProjectExplorer {

class BuildDirectoryAspect::Private {
public:
  FilePath sourceDir;
  FilePath savedShadowBuildDir;
  QString problem;
  QPointer<InfoLabel> problemLabel;
};

BuildDirectoryAspect::BuildDirectoryAspect(const BuildConfiguration *bc) : d(new Private)
{
  setSettingsKey("ProjectExplorer.BuildConfiguration.BuildDirectory");
  setLabelText(tr("Build directory:"));
  setDisplayStyle(PathChooserDisplay);
  setExpectedKind(PathChooser::Directory);
  setValidationFunction([this](FancyLineEdit *edit, QString *error) {
    const auto fixedDir = fixupDir(FilePath::fromUserInput(edit->text()));
    if (!fixedDir.isEmpty())
      edit->setText(fixedDir.toUserOutput());
    return pathChooser() ? pathChooser()->defaultValidationFunction()(edit, error) : true;
  });
  setOpenTerminalHandler([this, bc] {
    Core::FileUtils::openTerminal(FilePath::fromString(value()), bc->environment());
  });
}

BuildDirectoryAspect::~BuildDirectoryAspect()
{
  delete d;
}

auto BuildDirectoryAspect::allowInSourceBuilds(const FilePath &sourceDir) -> void
{
  d->sourceDir = sourceDir;
  makeCheckable(CheckBoxPlacement::Top, tr("Shadow build:"), QString());
  setChecked(d->sourceDir != filePath());
}

auto BuildDirectoryAspect::isShadowBuild() const -> bool
{
  return !d->sourceDir.isEmpty() && d->sourceDir != filePath();
}

auto BuildDirectoryAspect::setProblem(const QString &description) -> void
{
  d->problem = description;
  updateProblemLabel();
}

auto BuildDirectoryAspect::toMap(QVariantMap &map) const -> void
{
  StringAspect::toMap(map);
  if (!d->sourceDir.isEmpty()) {
    const auto shadowDir = isChecked() ? filePath() : d->savedShadowBuildDir;
    saveToMap(map, shadowDir.toString(), QString(), settingsKey() + ".shadowDir");
  }
}

auto BuildDirectoryAspect::fromMap(const QVariantMap &map) -> void
{
  StringAspect::fromMap(map);
  if (!d->sourceDir.isEmpty()) {
    d->savedShadowBuildDir = FilePath::fromString(map.value(settingsKey() + ".shadowDir").toString());
    if (d->savedShadowBuildDir.isEmpty())
      setFilePath(d->sourceDir);
    setChecked(d->sourceDir != filePath());
  }
}

auto BuildDirectoryAspect::addToLayout(LayoutBuilder &builder) -> void
{
  StringAspect::addToLayout(builder);
  d->problemLabel = new InfoLabel({}, InfoLabel::Warning);
  d->problemLabel->setElideMode(Qt::ElideNone);
  builder.addRow({{}, d->problemLabel.data()});
  updateProblemLabel();
  if (!d->sourceDir.isEmpty()) {
    connect(this, &StringAspect::checkedChanged, this, [this] {
      if (isChecked()) {
        setFilePath(d->savedShadowBuildDir.isEmpty() ? d->sourceDir : d->savedShadowBuildDir);
      } else {
        d->savedShadowBuildDir = filePath();
        setFilePath(d->sourceDir);
      }
    });
  }
}

auto BuildDirectoryAspect::fixupDir(const FilePath &dir) -> FilePath
{
  if (!dir.startsWithDriveLetter())
    return {};
  const auto dirString = dir.toString().toLower();
  const auto drives = transform(QDir::drives(), [](const QFileInfo &fi) {
    return fi.absoluteFilePath().toLower().chopped(1);
  });
  if (!contains(drives, [&dirString](const QString &drive) {
    return dirString.startsWith(drive);
  }) && !drives.isEmpty()) {
    auto newDir = dir.path();
    newDir.replace(0, 2, drives.first());
    return dir.withNewPath(newDir);
  }
  return {};
}

auto BuildDirectoryAspect::updateProblemLabel() -> void
{
  if (!d->problemLabel)
    return;

  d->problemLabel->setText(d->problem);
  d->problemLabel->setVisible(!d->problem.isEmpty());
}

SeparateDebugInfoAspect::SeparateDebugInfoAspect()
{
  setDisplayName(tr("Separate debug info:"));
  setSettingsKey("SeparateDebugInfo");
  setValue(ProjectExplorerPlugin::buildPropertiesSettings().separateDebugInfo.value());
}

} // namespace ProjectExplorer
