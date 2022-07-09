// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "gettingstartedwelcomepage.hpp"

#include "exampleslistmodel.hpp"
#include "screenshotcropper.hpp"

#include <utils/fileutils.hpp>
#include <utils/pathchooser.hpp>
#include <utils/theme/theme.hpp>
#include <utils/winutils.hpp>

#include <core/coreconstants.hpp>
#include <core/documentmanager.hpp>
#include <core/icore.hpp>
#include <core/helpmanager.hpp>
#include <core/modemanager.hpp>
#include <core/welcomepagehelper.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/project.hpp>

#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>

using namespace Core;
using namespace Utils;

namespace QtSupport {
namespace Internal {

constexpr char C_FALLBACK_ROOT[] = "ProjectsFallbackRoot";

ExamplesWelcomePage::ExamplesWelcomePage(bool showExamples) : m_showExamples(showExamples) {}

auto ExamplesWelcomePage::title() const -> QString
{
  return m_showExamples ? tr("Examples") : tr("Tutorials");
}

auto ExamplesWelcomePage::priority() const -> int
{
  return m_showExamples ? 30 : 40;
}

auto ExamplesWelcomePage::id() const -> Id
{
  return m_showExamples ? "Examples" : "Tutorials";
}

auto ExamplesWelcomePage::copyToAlternativeLocation(const QFileInfo &proFileInfo, QStringList &filesToOpen, const QStringList &dependencies) -> QString
{
  const auto projectDir = proFileInfo.canonicalPath();
  QDialog d(ICore::dialogParent());
  const auto lay = new QGridLayout(&d);
  const auto descrLbl = new QLabel;
  d.setWindowTitle(tr("Copy Project to writable Location?"));
  descrLbl->setTextFormat(Qt::RichText);
  descrLbl->setWordWrap(false);
  const auto nativeProjectDir = QDir::toNativeSeparators(projectDir);
  descrLbl->setText(QString::fromLatin1("<blockquote>%1</blockquote>").arg(nativeProjectDir));
  descrLbl->setMinimumWidth(descrLbl->sizeHint().width());
  descrLbl->setWordWrap(true);
  descrLbl->setText(tr("<p>The project you are about to open is located in the " "write-protected location:</p><blockquote>%1</blockquote>" "<p>Please select a writable location below and click \"Copy Project and Open\" " "to open a modifiable copy of the project or click \"Keep Project and Open\" " "to open the project in location.</p><p><b>Note:</b> You will not " "be able to alter or compile your project in the current location.</p>").arg(nativeProjectDir));
  lay->addWidget(descrLbl, 0, 0, 1, 2);
  const auto txt = new QLabel(tr("&Location:"));
  const auto chooser = new PathChooser;
  txt->setBuddy(chooser);
  chooser->setExpectedKind(PathChooser::ExistingDirectory);
  chooser->setHistoryCompleter(QLatin1String("Qt.WritableExamplesDir.History"));
  const auto defaultRootDirectory = DocumentManager::projectsDirectory().toString();
  const auto settings = ICore::settings();
  chooser->setFilePath(FilePath::fromVariant(settings->value(C_FALLBACK_ROOT, defaultRootDirectory)));
  lay->addWidget(txt, 1, 0);
  lay->addWidget(chooser, 1, 1);
  enum {
    Copy = QDialog::Accepted + 1,
    Keep = QDialog::Accepted + 2
  };
  const auto bb = new QDialogButtonBox;
  const auto copyBtn = bb->addButton(tr("&Copy Project and Open"), QDialogButtonBox::AcceptRole);
  connect(copyBtn, &QAbstractButton::released, &d, [&d] { d.done(Copy); });
  copyBtn->setDefault(true);
  const auto keepBtn = bb->addButton(tr("&Keep Project and Open"), QDialogButtonBox::RejectRole);
  connect(keepBtn, &QAbstractButton::released, &d, [&d] { d.done(Keep); });
  lay->addWidget(bb, 2, 0, 1, 2);
  connect(chooser, &PathChooser::validChanged, copyBtn, &QWidget::setEnabled);
  const auto code = d.exec();
  if (code == Copy) {
    const auto exampleDirName = proFileInfo.dir().dirName();
    const auto destBaseDir = chooser->filePath().toString();
    settings->setValueWithDefault(C_FALLBACK_ROOT, destBaseDir, defaultRootDirectory);
    QDir toDirWithExamplesDir(destBaseDir);
    if (toDirWithExamplesDir.cd(exampleDirName)) {
      toDirWithExamplesDir.cdUp(); // step out, just to not be in the way
      QMessageBox::warning(ICore::dialogParent(), tr("Cannot Use Location"), tr("The specified location already exists. " "Please specify a valid location."), QMessageBox::Ok, QMessageBox::NoButton);
      return QString();
    } else {
      QString error;
      const QString targetDir = destBaseDir + QLatin1Char('/') + exampleDirName;
      if (FileUtils::copyRecursively(FilePath::fromString(projectDir), FilePath::fromString(targetDir), &error)) {
        // set vars to new location
        const auto end = filesToOpen.end();
        for (auto it = filesToOpen.begin(); it != end; ++it)
          it->replace(projectDir, targetDir);

        foreach(const QString &dependency, dependencies) {
          const auto targetFile = FilePath::fromString(targetDir).pathAppended(QDir(dependency).dirName());
          if (!FileUtils::copyRecursively(FilePath::fromString(dependency), targetFile, &error)) {
            QMessageBox::warning(ICore::dialogParent(), tr("Cannot Copy Project"), error);
            // do not fail, just warn;
          }
        }

        return targetDir + QLatin1Char('/') + proFileInfo.fileName();
      } else {
        QMessageBox::warning(ICore::dialogParent(), tr("Cannot Copy Project"), error);
      }

    }
  }
  if (code == Keep)
    return proFileInfo.absoluteFilePath();
  return QString();
}

auto ExamplesWelcomePage::openProject(const ExampleItem *item) -> void
{
  using namespace ProjectExplorer;
  auto proFile = item->projectPath;
  if (proFile.isEmpty())
    return;

  auto filesToOpen = item->filesToOpen;
  if (!item->mainFile.isEmpty()) {
    // ensure that the main file is opened on top (i.e. opened last)
    filesToOpen.removeAll(item->mainFile);
    filesToOpen.append(item->mainFile);
  }

  QFileInfo proFileInfo(proFile);
  if (!proFileInfo.exists())
    return;

  // If the Qt is a distro Qt on Linux, it will not be writable, hence compilation will fail
  // Same if it is installed in non-writable location for other reasons
  const auto needsCopy = withNtfsPermissions<bool>([proFileInfo] {
    const QFileInfo pathInfo(proFileInfo.path());
    return !proFileInfo.isWritable() || !pathInfo.isWritable() /* path of .pro file */
      || !QFileInfo(pathInfo.path()).isWritable() /* shadow build directory */;
  });
  if (needsCopy)
    proFile = copyToAlternativeLocation(proFileInfo, filesToOpen, item->dependencies);

  // don't try to load help and files if loading the help request is being cancelled
  if (proFile.isEmpty())
    return;
  const auto result = ProjectExplorerPlugin::openProject(FilePath::fromString(proFile));
  if (result) {
    ICore::openFiles(transform(filesToOpen, &FilePath::fromString));
    ModeManager::activateMode(Core::Constants::MODE_EDIT);
    const auto docUrl = QUrl::fromUserInput(item->docUrl);
    if (docUrl.isValid())
      showHelpUrl(docUrl, HelpManager::ExternalHelpAlways);
    ModeManager::activateMode(ProjectExplorer::Constants::MODE_SESSION);
  } else {
    ProjectExplorerPlugin::showOpenProjectError(result);
  }
}

class ExampleDelegate : public ListItemDelegate {
public:
  auto setShowExamples(bool showExamples) -> void
  {
    m_showExamples = showExamples;
    goon();
  }

protected:
  auto clickAction(const ListItem *item) const -> void override
  {
    QTC_ASSERT(item, return);
    const auto exampleItem = static_cast<const ExampleItem*>(item);

    if (exampleItem->isVideo)
      QDesktopServices::openUrl(QUrl::fromUserInput(exampleItem->videoUrl));
    else if (exampleItem->hasSourceCode)
      ExamplesWelcomePage::openProject(exampleItem);
    else
      showHelpUrl(QUrl::fromUserInput(exampleItem->docUrl), HelpManager::ExternalHelpAlways);
  }

  auto drawPixmapOverlay(const ListItem *item, QPainter *painter, const QStyleOptionViewItem &option, const QRect &currentPixmapRect) const -> void override
  {
    QTC_ASSERT(item, return);
    const auto exampleItem = static_cast<const ExampleItem*>(item);
    if (exampleItem->isVideo) {
      painter->save();
      painter->setFont(option.font);
      painter->setCompositionMode(QPainter::CompositionMode_Difference);
      painter->setPen(Qt::white);
      painter->drawText(currentPixmapRect.translated(0, -WelcomePageHelpers::G_ITEM_GAP), exampleItem->videoLength, Qt::AlignBottom | Qt::AlignHCenter);
      painter->restore();
    }
  }

  bool m_showExamples = true;
};

class ExamplesPageWidget : public QWidget {
public:
  ExamplesPageWidget(bool isExamples) : m_isExamples(isExamples)
  {
    m_exampleDelegate.setShowExamples(isExamples);
    static auto s_examplesModel = new ExamplesListModel(this);
    m_examplesModel = s_examplesModel;

    const auto filteredModel = new ExamplesListModelFilter(m_examplesModel, !m_isExamples, this);

    auto searchBox = new SearchBox(this);
    m_searcher = searchBox->m_line_edit;

    const auto grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, WelcomePageHelpers::G_ITEM_GAP);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(WelcomePageHelpers::G_ITEM_GAP);

    const auto searchBar = WelcomePageHelpers::panelBar(this);
    const auto hbox = new QHBoxLayout(searchBar);
    hbox->setContentsMargins(0, 0, 0, 0);
    if (m_isExamples) {
      m_searcher->setPlaceholderText(ExamplesWelcomePage::tr("Search in Examples..."));

      const auto exampleSetSelector = new QComboBox(this);
      auto pal = exampleSetSelector->palette();
      // for macOS dark mode
      pal.setColor(QPalette::Text, Utils::orcaTheme()->color(Theme::Welcome_TextColor));
      exampleSetSelector->setPalette(pal);
      exampleSetSelector->setMinimumWidth(ListItemDelegate::grid_item_width);
      exampleSetSelector->setMaximumWidth(ListItemDelegate::grid_item_width);
      const auto exampleSetModel = m_examplesModel->exampleSetModel();
      exampleSetSelector->setModel(exampleSetModel);
      exampleSetSelector->setCurrentIndex(exampleSetModel->selectedExampleSet());
      connect(exampleSetSelector, QOverload<int>::of(&QComboBox::activated), exampleSetModel, &ExampleSetModel::selectExampleSet);
      connect(exampleSetModel, &ExampleSetModel::selectedExampleSetChanged, exampleSetSelector, &QComboBox::setCurrentIndex);

      hbox->setSpacing(WelcomePageHelpers::G_H_SPACING);
      hbox->addWidget(exampleSetSelector);
    } else {
      m_searcher->setPlaceholderText(ExamplesWelcomePage::tr("Search in Tutorials..."));
    }
    hbox->addWidget(searchBox);
    grid->addWidget(WelcomePageHelpers::panelBar(this), 0, 0);
    grid->addWidget(searchBar, 0, 1);
    grid->addWidget(WelcomePageHelpers::panelBar(this), 0, 2);

    const auto gridView = new GridView(this);
    gridView->setModel(filteredModel);
    gridView->setItemDelegate(&m_exampleDelegate);
    if (const auto sb = gridView->verticalScrollBar())
      sb->setSingleStep(25);
    grid->addWidget(gridView, 1, 1, 1, 2);

    connect(&m_exampleDelegate, &ExampleDelegate::tagClicked, this, &ExamplesPageWidget::onTagClicked);
    connect(m_searcher, &QLineEdit::textChanged, filteredModel, &ExamplesListModelFilter::setSearchString);
  }

  auto onTagClicked(const QString &tag) -> void
  {
    const auto text = m_searcher->text();
    m_searcher->setText((text.startsWith("tag:\"") ? text.trimmed() + " " : QString()) + QString("tag:\"%1\" ").arg(tag));
  }

  const bool m_isExamples;
  ExampleDelegate m_exampleDelegate;
  QPointer<ExamplesListModel> m_examplesModel;
  QLineEdit *m_searcher;
};

auto ExamplesWelcomePage::createWidget() const -> QWidget*
{
  return new ExamplesPageWidget(m_showExamples);
}

} // namespace Internal
} // namespace QtSupport
