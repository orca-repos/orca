// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "plugininstallwizard.h"
#include "coreplugin.h"
#include "icore.h"

#include <utils/archive.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/infolabel.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/runextensions.h>
#include <utils/temporarydirectory.h>
#include <utils/wizard.h>
#include <utils/wizardpage.h>

#include <app/app_version.h>

#include <extensionsystem/pluginspec.h>

#include <QButtonGroup>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <memory>

using namespace ExtensionSystem;
using namespace Utils;

struct Data {
  FilePath source_path;
  FilePath extracted_path;
  bool install_into_application = false;
};

static auto libraryNameFilter() -> QStringList
{
  if constexpr (HostOsInfo::isWindowsHost())
    return {"*.dll"};
  else if constexpr (HostOsInfo::isLinuxHost())
    return {"*.so"};
  else return {"*.dylib"};
}

static auto hasLibSuffix(const FilePath &path) -> bool
{
  return HostOsInfo::isWindowsHost() && path.endsWith(".dll") || HostOsInfo::isLinuxHost() && path.completeSuffix().startsWith(".so") || HostOsInfo::isMacHost() && path.endsWith(".dylib");
}

static auto pluginInstallPath(const bool install_into_application) -> FilePath
{
  return FilePath::fromString(install_into_application ? Core::ICore::pluginPath() : Core::ICore::userPluginPath());
}

namespace Core {
namespace Internal {

class SourcePage final : public WizardPage {
public:
  SourcePage(Data *data, QWidget *parent) : WizardPage(parent), m_data(data)
  {
    setTitle(PluginInstallWizard::tr("Source"));
    const auto vlayout = new QVBoxLayout;
    setLayout(vlayout);
    const auto label = new QLabel("<p>" + PluginInstallWizard::tr("Choose source location. This can be a plugin library file or a zip file.") + "</p>");
    label->setWordWrap(true);
    vlayout->addWidget(label);
    auto path = new PathChooser;
    path->setExpectedKind(PathChooser::Any);
    vlayout->addWidget(path);

    connect(path, &PathChooser::pathChanged, this, [this, path] {
      m_data->source_path = path->filePath();
      updateWarnings();
    });

    m_info = new InfoLabel;
    m_info->setType(InfoLabel::Error);
    m_info->setVisible(false);
    vlayout->addWidget(m_info);
  }

  auto updateWarnings() -> void
  {
    m_info->setVisible(!isComplete());
    emit completeChanged();
  }

  auto isComplete() const -> bool final
  {
    const auto& path = m_data->source_path;

    if (!QFile::exists(path.toString())) {
      m_info->setText(PluginInstallWizard::tr("File does not exist."));
      return false;
    }

    if (hasLibSuffix(path))
      return true;

    QString error;

    if (!Archive::supportsFile(path, &error)) {
      m_info->setText(error);
      return false;
    }

    return true;
  }

  auto nextId() const -> int final
  {
    if (hasLibSuffix(m_data->source_path))
      return WizardPage::nextId() + 1; // jump over check archive
    return WizardPage::nextId();
  }

  InfoLabel *m_info = nullptr;
  Data *m_data = nullptr;
};

class CheckArchivePage final : public WizardPage {
public:
  struct ArchiveIssue {
    QString message;
    InfoLabel::InfoType type;
  };

  CheckArchivePage(Data *data, QWidget *parent) : WizardPage(parent), m_data(data)
  {
    setTitle(PluginInstallWizard::tr("Check Archive"));
    const auto vlayout = new QVBoxLayout;
    setLayout(vlayout);

    m_label = new InfoLabel;
    m_label->setElideMode(Qt::ElideNone);
    m_label->setWordWrap(true);
    m_cancel_button = new QPushButton(PluginInstallWizard::tr("Cancel"));
    m_output = new QTextEdit;
    m_output->setReadOnly(true);

    const auto hlayout = new QHBoxLayout;
    hlayout->addWidget(m_label, 1);
    hlayout->addStretch();
    hlayout->addWidget(m_cancel_button);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(m_output);
  }

  auto initializePage() -> void override
  {
    m_is_complete = false;

    emit completeChanged();

    m_canceled = false;
    m_temp_dir = std::make_unique<TemporaryDirectory>("plugininstall");
    m_data->extracted_path = m_temp_dir->path();
    m_label->setText(PluginInstallWizard::tr("Checking archive..."));
    m_label->setType(InfoLabel::None);
    m_cancel_button->setVisible(true);
    m_output->clear();
    m_archive = Archive::unarchive(m_data->source_path, m_temp_dir->path());

    if (!m_archive) {
      m_label->setType(InfoLabel::Error);
      m_label->setText(PluginInstallWizard::tr("The file is not an archive."));
      return;
    }

    connect(m_archive, &Archive::outputReceived, this, [this](const QString &output) {
      m_output->append(output);
    });

    connect(m_archive, &Archive::finished, this, [this](const bool success) {
      m_archive = nullptr; // we don't own it
      m_cancel_button->disconnect();
      if (!success) {
        // unarchiving failed
        m_cancel_button->setVisible(false);
        if (m_canceled) {
          m_label->setType(InfoLabel::Information);
          m_label->setText(PluginInstallWizard::tr("Canceled."));
        } else {
          m_label->setType(InfoLabel::Error);
          m_label->setText(PluginInstallWizard::tr("There was an error while unarchiving."));
        }
      } else {
        // unarchiving was successful, run a check
        m_archive_check = runAsync([this](QFutureInterface<ArchiveIssue> &fi) { return checkContents(fi); });
        onFinished(m_archive_check, this, [this](const QFuture<ArchiveIssue> &f) {
          m_cancel_button->setVisible(false);
          m_cancel_button->disconnect();
          const auto ok = f.resultCount() == 0 && !f.isCanceled();
          if (f.isCanceled()) {
            m_label->setType(InfoLabel::Information);
            m_label->setText(PluginInstallWizard::tr("Canceled."));
          } else if (ok) {
            m_label->setType(InfoLabel::Ok);
            m_label->setText(PluginInstallWizard::tr("Archive is OK."));
          } else {
            const auto [message, type] = f.result();
            m_label->setType(type);
            m_label->setText(message);
          }
          m_is_complete = ok;
          emit completeChanged();
        });
        connect(m_cancel_button, &QPushButton::clicked, this, [this] {
          m_archive_check.cancel();
        });
      }
    });
    connect(m_cancel_button, &QPushButton::clicked, m_archive, [this] {
      m_canceled = true;
      m_archive->cancel();
    });
  }

  // Async. Result is set if any issue was found.
  auto checkContents(QFutureInterface<ArchiveIssue> &fi) const -> void
  {
    QTC_ASSERT(m_temp_dir.get(), return);

    auto coreplugin = CorePlugin::instance()->pluginSpec();

    // look for plugin
    QDirIterator it(m_temp_dir->path().path(), libraryNameFilter(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      if (fi.isCanceled())
        return;
      it.next();
      if (const auto spec = PluginSpec::read(it.filePath())) {
        // Is a Orca plugin. Let's see if we find a Core dependency and check the
        // version
        const auto dependencies = spec->dependencies();
        if (const auto found = std::find_if(dependencies.constBegin(), dependencies.constEnd(), [coreplugin](const PluginDependency &d) {
          return d.name == coreplugin->name();
        }); found != dependencies.constEnd()) {
          if (!coreplugin->provides(found->name, found->version)) {
            fi.reportResult({PluginInstallWizard::tr("Plugin requires an incompatible version of %1 (%2).").arg(Constants::IDE_DISPLAY_NAME).arg(found->version), InfoLabel::Error});
            return;
          }
        }
        return; // successful / no error
      }
    }
    fi.reportResult({PluginInstallWizard::tr("Did not find %1 plugin.").arg(Constants::IDE_DISPLAY_NAME), InfoLabel::Error});
  }

  auto cleanupPage() -> void override
  {
    // back button pressed
    m_cancel_button->disconnect();
    if (m_archive) {
      m_archive->disconnect();
      m_archive->cancel();
      m_archive = nullptr; // we don't own it
    }
    if (m_archive_check.isRunning()) {
      m_archive_check.cancel();
      m_archive_check.waitForFinished();
    }
    m_temp_dir.reset();
  }

  auto isComplete() const -> bool override { return m_is_complete; }

  std::unique_ptr<TemporaryDirectory> m_temp_dir;
  Archive *m_archive = nullptr;
  QFuture<ArchiveIssue> m_archive_check;
  InfoLabel *m_label = nullptr;
  QPushButton *m_cancel_button = nullptr;
  QTextEdit *m_output = nullptr;
  Data *m_data = nullptr;
  bool m_is_complete = false;
  bool m_canceled = false;
};

class InstallLocationPage final : public WizardPage {
public:
  InstallLocationPage(Data *data, QWidget *parent) : WizardPage(parent), m_data(data)
  {
    setTitle(PluginInstallWizard::tr("Install Location"));

    const auto vlayout = new QVBoxLayout;
    setLayout(vlayout);

    const auto label = new QLabel("<p>" + PluginInstallWizard::tr("Choose install location.") + "</p>");
    label->setWordWrap(true);

    vlayout->addWidget(label);
    vlayout->addSpacing(10);

    const auto local_install = new QRadioButton(PluginInstallWizard::tr("User plugins"));
    local_install->setChecked(!m_data->install_into_application);

    const auto local_label = new QLabel(PluginInstallWizard::tr("The plugin will be available to all compatible %1 " "installations, but only for the current user.").arg(Constants::IDE_DISPLAY_NAME));
    local_label->setWordWrap(true);
    local_label->setAttribute(Qt::WA_MacSmallSize, true);

    vlayout->addWidget(local_install);
    vlayout->addWidget(local_label);
    vlayout->addSpacing(10);

    const auto app_install = new QRadioButton(PluginInstallWizard::tr("%1 installation").arg(Constants::IDE_DISPLAY_NAME));
    app_install->setChecked(m_data->install_into_application);

    const auto app_label = new QLabel(PluginInstallWizard::tr("The plugin will be available only to this %1 " "installation, but for all users that can access it.").arg(Constants::IDE_DISPLAY_NAME));
    app_label->setWordWrap(true);
    app_label->setAttribute(Qt::WA_MacSmallSize, true);

    vlayout->addWidget(app_install);
    vlayout->addWidget(app_label);

    const auto group = new QButtonGroup(this);
    group->addButton(local_install);
    group->addButton(app_install);

    connect(app_install, &QRadioButton::toggled, this, [this](const bool toggled) {
      m_data->install_into_application = toggled;
    });
  }

  Data *m_data = nullptr;
};

class SummaryPage final : public WizardPage {
public:
  SummaryPage(Data *data, QWidget *parent) : WizardPage(parent), m_data(data)
  {
    setTitle(PluginInstallWizard::tr("Summary"));
    const auto vlayout = new QVBoxLayout;
    setLayout(vlayout);
    m_summary_label = new QLabel(this);
    m_summary_label->setWordWrap(true);
    vlayout->addWidget(m_summary_label);
  }

  auto initializePage() -> void override
  {
    m_summary_label->setText(PluginInstallWizard::tr(R"("%1" will be installed into "%2".)").arg(m_data->source_path.toUserOutput(), pluginInstallPath(m_data->install_into_application).toUserOutput()));
  }

private:
  QLabel *m_summary_label;
  Data *m_data = nullptr;
};

static auto postCopyOperation() -> std::function<void(FilePath)>
{
  return [](const FilePath &file_path) {
    if constexpr (!HostOsInfo::isMacHost())
      return;
    // On macOS, downloaded files get a quarantine flag, remove it, otherwise it is a hassle
    // to get it loaded as a plugin in Orca.
    QtcProcess xattr;
    xattr.setTimeoutS(1);
    xattr.setCommand({"/usr/bin/xattr", {"-d", "com.apple.quarantine", file_path.absoluteFilePath().toString()}});
    xattr.runBlocking();
  };
}

static auto copyPluginFile(const FilePath &src, const FilePath &dest) -> bool
{
  const auto dest_file = dest.pathAppended(src.fileName());

  if (dest_file.exists()) {
    QMessageBox box(QMessageBox::Question, PluginInstallWizard::tr("Overwrite File"), PluginInstallWizard::tr("The file \"%1\" exists. Overwrite?").arg(dest_file.toUserOutput()), QMessageBox::Cancel, ICore::dialogParent());
    const auto accept_button = box.addButton(PluginInstallWizard::tr("Overwrite"), QMessageBox::AcceptRole);
    box.setDefaultButton(accept_button);
    box.exec();

    if (box.clickedButton() != accept_button)
      return false;

    dest_file.removeFile();
  }

  dest.parentDir().ensureWritableDir();

  if (!src.copyFile(dest_file)) {
    QMessageBox::warning(ICore::dialogParent(), PluginInstallWizard::tr("Failed to Write File"), PluginInstallWizard::tr("Failed to write file \"%1\".").arg(dest_file.toUserOutput()));
    return false;
  }

  postCopyOperation()(dest_file);
  return true;
}

auto PluginInstallWizard::exec() -> bool
{
  Wizard wizard(ICore::dialogParent());
  wizard.setWindowTitle(tr("Install Plugin"));

  Data data;

  const auto file_page = new SourcePage(&data, &wizard);
  wizard.addPage(file_page);

  const auto check_archive_page = new CheckArchivePage(&data, &wizard);
  wizard.addPage(check_archive_page);

  const auto install_location_page = new InstallLocationPage(&data, &wizard);
  wizard.addPage(install_location_page);

  const auto summary_page = new SummaryPage(&data, &wizard);
  wizard.addPage(summary_page);

  if (wizard.exec()) {
    const auto install_path = pluginInstallPath(data.install_into_application);
    if (hasLibSuffix(data.source_path)) {
      return copyPluginFile(data.source_path, install_path);
    }
    QString error;
    if (!FileUtils::copyRecursively(data.extracted_path, install_path, &error, FileUtils::CopyAskingForOverwrite(ICore::dialogParent(), postCopyOperation()))) {
      QMessageBox::warning(ICore::dialogParent(), tr("Failed to Copy Plugin Files"), error);
      return false;
    }
    return true;
  }

  return false;
}

} // namespace Internal
} // namespace Core
