// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findinfiles.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/find/findplugin.hpp>
#include <core/icore.hpp>

#include <utils/filesearch.hpp>
#include <utils/fileutils.hpp>
#include <utils/historycompleter.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QStackedWidget>

using namespace Core;
using namespace TextEditor;
using namespace Utils;

static FindInFiles *m_instance = nullptr;
static constexpr char HistoryKey[] = "FindInFiles.Directories.History";

FindInFiles::FindInFiles()
{
  m_instance = this;
  connect(EditorManager::instance(), &EditorManager::findOnFileSystemRequest, this, &FindInFiles::findOnFileSystem);
}

FindInFiles::~FindInFiles() = default;

auto FindInFiles::isValid() const -> bool
{
  return m_isValid;
}

auto FindInFiles::id() const -> QString
{
  return QLatin1String("Files on Disk");
}

auto FindInFiles::displayName() const -> QString
{
  return tr("Files in File System");
}

auto FindInFiles::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> FileIterator*
{
  return new SubDirFileIterator({additionalParameters.toString()}, nameFilters, exclusionFilters, EditorManager::defaultTextCodec());
}

auto FindInFiles::additionalParameters() const -> QVariant
{
  return QVariant::fromValue(path().toString());
}

auto FindInFiles::label() const -> QString
{
  const auto title = currentSearchEngine()->title();

  const QChar slash = QLatin1Char('/');
  const auto &nonEmptyComponents = path().toFileInfo().absoluteFilePath().split(slash, Qt::SkipEmptyParts);
  return tr("%1 \"%2\":").arg(title).arg(nonEmptyComponents.isEmpty() ? QString(slash) : nonEmptyComponents.last());
}

auto FindInFiles::toolTip() const -> QString
{
  //: the last arg is filled by BaseFileFind::runNewSearch
  auto tooltip = tr("Path: %1\nFilter: %2\nExcluding: %3\n%4").arg(path().toUserOutput()).arg(fileNameFilters().join(',')).arg(fileExclusionFilters().join(','));

  const auto searchEngineToolTip = currentSearchEngine()->toolTip();
  if (!searchEngineToolTip.isEmpty())
    tooltip = tooltip.arg(searchEngineToolTip);

  return tooltip;
}

auto FindInFiles::syncSearchEngineCombo(int selectedSearchEngineIndex) -> void
{
  QTC_ASSERT(m_searchEngineCombo && selectedSearchEngineIndex >= 0 && selectedSearchEngineIndex < searchEngines().size(), return);

  m_searchEngineCombo->setCurrentIndex(selectedSearchEngineIndex);
}

auto FindInFiles::setValid(bool valid) -> void
{
  if (valid == m_isValid)
    return;
  m_isValid = valid;
  emit validChanged(m_isValid);
}

auto FindInFiles::searchEnginesSelectionChanged(int index) -> void
{
  setCurrentSearchEngine(index);
  m_searchEngineWidget->setCurrentIndex(index);
}

auto FindInFiles::createConfigWidget() -> QWidget*
{
  if (!m_configWidget) {
    m_configWidget = new QWidget;
    const auto gridLayout = new QGridLayout(m_configWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    m_configWidget->setLayout(gridLayout);

    auto row = 0;
    const auto searchEngineLabel = new QLabel(tr("Search engine:"));
    gridLayout->addWidget(searchEngineLabel, row, 0, Qt::AlignRight);
    m_searchEngineCombo = new QComboBox;
    auto cc = QOverload<int>::of(&QComboBox::currentIndexChanged);
    connect(m_searchEngineCombo, cc, this, &FindInFiles::searchEnginesSelectionChanged);
    searchEngineLabel->setBuddy(m_searchEngineCombo);
    gridLayout->addWidget(m_searchEngineCombo, row, 1);

    m_searchEngineWidget = new QStackedWidget(m_configWidget);
    foreach(SearchEngine *searchEngine, searchEngines()) {
      m_searchEngineWidget->addWidget(searchEngine->widget());
      m_searchEngineCombo->addItem(searchEngine->title());
    }
    gridLayout->addWidget(m_searchEngineWidget, row++, 2);

    const auto dirLabel = new QLabel(tr("Director&y:"));
    gridLayout->addWidget(dirLabel, row, 0, Qt::AlignRight);
    m_directory = new PathChooser;
    m_directory->setExpectedKind(PathChooser::ExistingDirectory);
    m_directory->setPromptDialogTitle(tr("Directory to Search"));
    connect(m_directory.data(), &PathChooser::filePathChanged, this, &FindInFiles::pathChanged);
    m_directory->setHistoryCompleter(QLatin1String(HistoryKey),
                                     /*restoreLastItemFromHistory=*/ true);
    if (!HistoryCompleter::historyExistsFor(QLatin1String(HistoryKey))) {
      const auto completer = static_cast<HistoryCompleter*>(m_directory->lineEdit()->completer());
      const QStringList legacyHistory = ICore::settings()->value(QLatin1String("Find/FindInFiles/directories")).toStringList();
      for (const auto &dir : legacyHistory)
        completer->addEntry(dir);
    }
    dirLabel->setBuddy(m_directory);
    gridLayout->addWidget(m_directory, row++, 1, 1, 2);

    const auto patternWidgets = createPatternWidgets();
    for (const auto &p : patternWidgets) {
      gridLayout->addWidget(p.first, row, 0, Qt::AlignRight);
      gridLayout->addWidget(p.second, row, 1, 1, 2);
      ++row;
    }
    m_configWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // validity
    auto updateValidity = [this]() {
      setValid(currentSearchEngine()->isEnabled() && m_directory->isValid());
    };
    connect(this, &BaseFileFind::currentSearchEngineChanged, this, updateValidity);
    foreach(SearchEngine *searchEngine, searchEngines())
      connect(searchEngine, &SearchEngine::enabledChanged, this, updateValidity);
    connect(m_directory.data(), &PathChooser::validChanged, this, updateValidity);
    updateValidity();
  }
  return m_configWidget;
}

auto FindInFiles::path() const -> FilePath
{
  return m_directory->filePath();
}

auto FindInFiles::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInFiles"));
  writeCommonSettings(settings);
  settings->endGroup();
}

auto FindInFiles::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInFiles"));
  readCommonSettings(settings, "*.cpp,*.hpp", "*/.git/*,*/.cvs/*,*/.svn/*,*.autosave");
  settings->endGroup();
}

auto FindInFiles::setDirectory(const FilePath &directory) -> void
{
  m_directory->setFilePath(directory);
}

auto FindInFiles::setBaseDirectory(const FilePath &directory) -> void
{
  m_directory->setBaseDirectory(directory);
}

auto FindInFiles::directory() const -> FilePath
{
  return m_directory->filePath();
}

auto FindInFiles::findOnFileSystem(const QString &path) -> void
{
  QTC_ASSERT(m_instance, return);
  const QFileInfo fi(path);
  const auto folder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
  m_instance->setDirectory(FilePath::fromString(folder));
  Find::openFindDialog(m_instance);
}

auto FindInFiles::instance() -> FindInFiles*
{
  return m_instance;
}
