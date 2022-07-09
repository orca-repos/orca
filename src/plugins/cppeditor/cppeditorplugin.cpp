// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppeditorplugin.hpp"

#include "cppautocompleter.hpp"
#include "cppcodemodelinspectordialog.hpp"
#include "cppcodemodelsettings.hpp"
#include "cppcodemodelsettingspage.hpp"
#include "cppcodestylesettingspage.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditordocument.hpp"
#include "cppeditorwidget.hpp"
#include "cppfilesettingspage.hpp"
#include "cpphighlighter.hpp"
#include "cppincludehierarchy.hpp"
#include "cppmodelmanager.hpp"
#include "cppoutline.hpp"
#include "cppprojectfile.hpp"
#include "cppprojectupdater.hpp"
#include "cppquickfixassistant.hpp"
#include "cppquickfixes.hpp"
#include "cppquickfixprojectsettingswidget.hpp"
#include "cppquickfixsettingspage.hpp"
#include "cpptoolsreuse.hpp"
#include "cpptoolssettings.hpp"
#include "cpptypehierarchy.hpp"
#include "projectinfo.hpp"
#include "resourcepreviewhoverhandler.hpp"
#include "stringtable.hpp"

#ifdef WITH_TESTS
#include "compileroptionsbuilder_test.hpp"
#include "cppcodegen_test.hpp"
#include "cppcompletion_test.hpp"
#include "cppdoxygen_test.hpp"
#include "cppheadersource_test.hpp"
#include "cppincludehierarchy_test.hpp"
#include "cppinsertvirtualmethods.hpp"
#include "cpplocalsymbols_test.hpp"
#include "cpplocatorfilter_test.hpp"
#include "cppmodelmanager_test.hpp"
#include "cpppointerdeclarationformatter_test.hpp"
#include "cppquickfix_test.hpp"
#include "cppsourceprocessor_test.hpp"
#include "cppuseselections_test.hpp"
#include "fileandtokenactions_test.hpp"
#include "followsymbol_switchmethoddecldef_test.hpp"
#include "functionutils.hpp"
#include "includeutils.hpp"
#include "projectinfo_test.hpp"
#include "senddocumenttracker.hpp"
#include "symbolsearcher_test.hpp"
#include "typehierarchybuilder_test.hpp"
#endif

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreconstants.hpp>
#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditorfactory.hpp>
#include <core/fileiconprovider.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>
#include <core/navigationwidget.hpp>
#include <core/progressmanager/progressmanager.hpp>
#include <core/vcsmanager.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectpanelfactory.hpp>
#include <projectexplorer/projecttree.hpp>

#include <texteditor/colorpreviewhoverhandler.hpp>
#include <texteditor/snippets/snippetprovider.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditoractionhandler.hpp>
#include <texteditor/texteditorconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>

#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QStringList>

using namespace CPlusPlus;
using namespace Core;
using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace CppEditor {
namespace Internal {

enum {
  QUICKFIX_INTERVAL = 20
};

enum {
  debug = 0
};

static auto currentCppEditorWidget() -> CppEditorWidget*
{
  if (auto currentEditor = EditorManager::currentEditor())
    return qobject_cast<CppEditorWidget*>(currentEditor->widget());
  return nullptr;
}

class CppEditorFactory : public TextEditorFactory {
public:
  CppEditorFactory()
  {
    setId(Constants::CPPEDITOR_ID);
    setDisplayName(QCoreApplication::translate("OpenWith::Editors", Constants::CPPEDITOR_DISPLAY_NAME));
    addMimeType(Constants::C_SOURCE_MIMETYPE);
    addMimeType(Constants::C_HEADER_MIMETYPE);
    addMimeType(Constants::CPP_SOURCE_MIMETYPE);
    addMimeType(Constants::CPP_HEADER_MIMETYPE);
    addMimeType(Constants::QDOC_MIMETYPE);
    addMimeType(Constants::MOC_MIMETYPE);

    setDocumentCreator([]() { return new CppEditorDocument; });
    setEditorWidgetCreator([]() { return new CppEditorWidget; });
    setEditorCreator([]() {
      const auto editor = new BaseTextEditor;
      editor->addContext(ProjectExplorer::Constants::CXX_LANGUAGE_ID);
      return editor;
    });
    setAutoCompleterCreator([]() { return new CppAutoCompleter; });
    setCommentDefinition(CommentDefinition::CppStyle);
    setCodeFoldingSupported(true);
    setParenthesesMatchingEnabled(true);

    setEditorActionHandlers(TextEditorActionHandler::Format | TextEditorActionHandler::UnCommentSelection | TextEditorActionHandler::UnCollapseAll | TextEditorActionHandler::FollowSymbolUnderCursor | TextEditorActionHandler::RenameSymbol);
  }
};

class CppEditorPluginPrivate : public QObject {
public:
  ~CppEditorPluginPrivate()
  {
    ExtensionSystem::PluginManager::removeObject(&m_cppProjectUpdaterFactory);
    delete m_clangdSettingsPage;
  }

  auto initialize() -> void { m_codeModelSettings.fromSettings(ICore::settings()); }
  auto onTaskStarted(Utils::Id type) -> void;
  auto onAllTasksFinished(Utils::Id type) -> void;
  auto inspectCppCodeModel() -> void;

  QAction *m_reparseExternallyChangedFiles = nullptr;
  QAction *m_findRefsCategorizedAction = nullptr;
  QAction *m_openTypeHierarchyAction = nullptr;
  QAction *m_openIncludeHierarchyAction = nullptr;

  CppQuickFixAssistProvider m_quickFixProvider;
  CppQuickFixSettingsPage m_quickFixSettingsPage;

  QPointer<CppCodeModelInspectorDialog> m_cppCodeModelInspectorDialog;

  QPointer<TextEditor::BaseTextEditor> m_currentEditor;

  CppOutlineWidgetFactory m_cppOutlineWidgetFactory;
  CppTypeHierarchyFactory m_cppTypeHierarchyFactory;
  CppIncludeHierarchyFactory m_cppIncludeHierarchyFactory;
  CppEditorFactory m_cppEditorFactory;

  StringTable stringTable;
  CppModelManager modelManager;
  CppCodeModelSettings m_codeModelSettings;
  CppToolsSettings settings;
  CppFileSettings m_fileSettings;
  CppFileSettingsPage m_cppFileSettingsPage{&m_fileSettings};
  CppCodeModelSettingsPage m_cppCodeModelSettingsPage{&m_codeModelSettings};
  ClangdSettingsPage *m_clangdSettingsPage = nullptr;
  CppCodeStyleSettingsPage m_cppCodeStyleSettingsPage;
  CppProjectUpdaterFactory m_cppProjectUpdaterFactory;
};

static CppEditorPlugin *m_instance = nullptr;
static QHash<QString, QString> m_headerSourceMapping;

CppEditorPlugin::CppEditorPlugin()
{
  m_instance = this;
}

CppEditorPlugin::~CppEditorPlugin()
{
  destroyCppQuickFixes();
  delete d;
  d = nullptr;
  m_instance = nullptr;
}

auto CppEditorPlugin::instance() -> CppEditorPlugin*
{
  return m_instance;
}

auto CppEditorPlugin::quickFixProvider() const -> CppQuickFixAssistProvider*
{
  return &d->m_quickFixProvider;
}

auto CppEditorPlugin::initialize(const QStringList & /*arguments*/, QString *errorMessage) -> bool
{
  Q_UNUSED(errorMessage)

  d = new CppEditorPluginPrivate;
  d->initialize();

  CppModelManager::instance()->registerJsExtension();
  ExtensionSystem::PluginManager::addObject(&d->m_cppProjectUpdaterFactory);

  // Menus
  auto mtools = ActionManager::actionContainer(Core::Constants::M_TOOLS);
  auto mcpptools = ActionManager::createMenu(Constants::M_TOOLS_CPP);
  auto menu = mcpptools->menu();
  menu->setTitle(tr("&C++"));
  menu->setEnabled(true);
  mtools->addMenu(mcpptools);

  // Actions
  Context context(Constants::CPPEDITOR_ID);

  auto switchAction = new QAction(tr("Switch Header/Source"), this);
  auto command = ActionManager::registerAction(switchAction, Constants::SWITCH_HEADER_SOURCE, context, true);
  command->setDefaultKeySequence(QKeySequence(Qt::Key_F4));
  mcpptools->addAction(command);
  connect(switchAction, &QAction::triggered, this, &CppEditorPlugin::switchHeaderSource);

  auto openInNextSplitAction = new QAction(tr("Open Corresponding Header/Source in Next Split"), this);
  command = ActionManager::registerAction(openInNextSplitAction, Constants::OPEN_HEADER_SOURCE_IN_NEXT_SPLIT, context, true);
  command->setDefaultKeySequence(QKeySequence(HostOsInfo::isMacHost() ? tr("Meta+E, F4") : tr("Ctrl+E, F4")));
  mcpptools->addAction(command);
  connect(openInNextSplitAction, &QAction::triggered, this, &CppEditorPlugin::switchHeaderSourceInNextSplit);

  auto expander = globalMacroExpander();
  expander->registerVariable("Cpp:LicenseTemplate", tr("The license template."), []() { return CppEditorPlugin::licenseTemplate(); });
  expander->registerFileVariables("Cpp:LicenseTemplatePath", tr("The configured path to the license template"), []() { return CppEditorPlugin::licenseTemplatePath(); });

  expander->registerVariable("Cpp:PragmaOnce", tr("Insert \"#pragma once\" instead of \"#ifndef\" include guards into header file"), [] { return usePragmaOnce() ? QString("true") : QString(); });

  const auto quickFixSettingsPanelFactory = new ProjectPanelFactory;
  quickFixSettingsPanelFactory->setPriority(100);
  quickFixSettingsPanelFactory->setId(Constants::QUICK_FIX_PROJECT_PANEL_ID);
  quickFixSettingsPanelFactory->setDisplayName(QCoreApplication::translate("CppEditor", Constants::QUICK_FIX_SETTINGS_DISPLAY_NAME));
  quickFixSettingsPanelFactory->setCreateWidgetFunction([](Project *project) {
    return new CppQuickFixProjectSettingsWidget(project);
  });
  ProjectPanelFactory::registerFactory(quickFixSettingsPanelFactory);

  SnippetProvider::registerGroup(Constants::CPP_SNIPPETS_GROUP_ID, tr("C++", "SnippetProvider"), &decorateCppEditor);

  createCppQuickFixes();

  auto contextMenu = ActionManager::createMenu(Constants::M_CONTEXT);
  contextMenu->insertGroup(Core::Constants::G_DEFAULT_ONE, Constants::G_CONTEXT_FIRST);

  Command *cmd;
  auto cppToolsMenu = ActionManager::actionContainer(Constants::M_TOOLS_CPP);
  auto touchBar = ActionManager::actionContainer(Core::Constants::TOUCH_BAR);

  cmd = ActionManager::command(Constants::SWITCH_HEADER_SOURCE);
  cmd->setTouchBarText(tr("Header/Source", "text on macOS touch bar"));
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  touchBar->addAction(cmd, Core::Constants::G_TOUCHBAR_NAVIGATION);

  cmd = ActionManager::command(TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR);
  cmd->setTouchBarText(tr("Follow", "text on macOS touch bar"));
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);
  touchBar->addAction(cmd, Core::Constants::G_TOUCHBAR_NAVIGATION);

  auto openPreprocessorDialog = new QAction(tr("Additional Preprocessor Directives..."), this);
  cmd = ActionManager::registerAction(openPreprocessorDialog, Constants::OPEN_PREPROCESSOR_DIALOG, context);
  cmd->setDefaultKeySequence(QKeySequence());
  connect(openPreprocessorDialog, &QAction::triggered, this, &CppEditorPlugin::showPreProcessorDialog);
  cppToolsMenu->addAction(cmd);

  auto switchDeclarationDefinition = new QAction(tr("Switch Between Function Declaration/Definition"), this);
  cmd = ActionManager::registerAction(switchDeclarationDefinition, Constants::SWITCH_DECLARATION_DEFINITION, context, true);
  cmd->setDefaultKeySequence(QKeySequence(tr("Shift+F2")));
  cmd->setTouchBarText(tr("Decl/Def", "text on macOS touch bar"));
  connect(switchDeclarationDefinition, &QAction::triggered, this, &CppEditorPlugin::switchDeclarationDefinition);
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);
  touchBar->addAction(cmd, Core::Constants::G_TOUCHBAR_NAVIGATION);

  cmd = ActionManager::command(TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR_IN_NEXT_SPLIT);
  cppToolsMenu->addAction(cmd);

  auto openDeclarationDefinitionInNextSplit = new QAction(tr("Open Function Declaration/Definition in Next Split"), this);
  cmd = ActionManager::registerAction(openDeclarationDefinitionInNextSplit, Constants::OPEN_DECLARATION_DEFINITION_IN_NEXT_SPLIT, context, true);
  cmd->setDefaultKeySequence(QKeySequence(HostOsInfo::isMacHost() ? tr("Meta+E, Shift+F2") : tr("Ctrl+E, Shift+F2")));
  connect(openDeclarationDefinitionInNextSplit, &QAction::triggered, this, &CppEditorPlugin::openDeclarationDefinitionInNextSplit);
  cppToolsMenu->addAction(cmd);

  cmd = ActionManager::command(TextEditor::Constants::FIND_USAGES);
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);

  d->m_findRefsCategorizedAction = new QAction(tr("Find References With Access Type"), this);
  cmd = ActionManager::registerAction(d->m_findRefsCategorizedAction, "CppEditor.FindRefsCategorized", context);
  connect(d->m_findRefsCategorizedAction, &QAction::triggered, this, [this] {
    if (const auto w = currentCppEditorWidget()) {
      codeModelSettings()->setCategorizeFindReferences(true);
      w->findUsages();
      codeModelSettings()->setCategorizeFindReferences(false);
    }
  });
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);

  d->m_openTypeHierarchyAction = new QAction(tr("Open Type Hierarchy"), this);
  cmd = ActionManager::registerAction(d->m_openTypeHierarchyAction, Constants::OPEN_TYPE_HIERARCHY, context);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Shift+T") : tr("Ctrl+Shift+T")));
  connect(d->m_openTypeHierarchyAction, &QAction::triggered, this, &CppEditorPlugin::openTypeHierarchy);
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);

  d->m_openIncludeHierarchyAction = new QAction(tr("Open Include Hierarchy"), this);
  cmd = ActionManager::registerAction(d->m_openIncludeHierarchyAction, Constants::OPEN_INCLUDE_HIERARCHY, context);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Shift+I") : tr("Ctrl+Shift+I")));
  connect(d->m_openIncludeHierarchyAction, &QAction::triggered, this, &CppEditorPlugin::openIncludeHierarchy);
  contextMenu->addAction(cmd, Constants::G_CONTEXT_FIRST);
  cppToolsMenu->addAction(cmd);

  // Refactoring sub-menu
  auto sep = contextMenu->addSeparator();
  sep->action()->setObjectName(QLatin1String(Constants::M_REFACTORING_MENU_INSERTION_POINT));
  contextMenu->addSeparator();
  cppToolsMenu->addAction(ActionManager::command(TextEditor::Constants::RENAME_SYMBOL));

  // Update context in global context
  cppToolsMenu->addSeparator(Core::Constants::G_DEFAULT_THREE);
  d->m_reparseExternallyChangedFiles = new QAction(tr("Reparse Externally Changed Files"), this);
  cmd = ActionManager::registerAction(d->m_reparseExternallyChangedFiles, Constants::UPDATE_CODEMODEL);
  auto cppModelManager = CppModelManager::instance();
  connect(d->m_reparseExternallyChangedFiles, &QAction::triggered, cppModelManager, &CppModelManager::updateModifiedSourceFiles);
  cppToolsMenu->addAction(cmd, Core::Constants::G_DEFAULT_THREE);

  auto toolsDebug = ActionManager::actionContainer(Core::Constants::M_TOOLS_DEBUG);
  auto inspectCppCodeModel = new QAction(tr("Inspect C++ Code Model..."), this);
  cmd = ActionManager::registerAction(inspectCppCodeModel, Constants::INSPECT_CPP_CODEMODEL);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Shift+F12") : tr("Ctrl+Shift+F12")));
  connect(inspectCppCodeModel, &QAction::triggered, d, &CppEditorPluginPrivate::inspectCppCodeModel);
  toolsDebug->addAction(cmd);

  contextMenu->addSeparator(context);

  cmd = ActionManager::command(TextEditor::Constants::AUTO_INDENT_SELECTION);
  contextMenu->addAction(cmd);

  cmd = ActionManager::command(TextEditor::Constants::UN_COMMENT_SELECTION);
  contextMenu->addAction(cmd);

  connect(ProgressManager::instance(), &ProgressManager::taskStarted, d, &CppEditorPluginPrivate::onTaskStarted);
  connect(ProgressManager::instance(), &ProgressManager::allTasksFinished, d, &CppEditorPluginPrivate::onAllTasksFinished);

  return true;
}

auto CppEditorPlugin::extensionsInitialized() -> void
{
  d->m_fileSettings.fromSettings(ICore::settings());
  if (!d->m_fileSettings.applySuffixesToMimeDB())
    qWarning("Unable to apply cpp suffixes to mime database (cpp mime types not found).\n");

  if (CppModelManager::instance()->isClangCodeModelActive()) {
    d->m_clangdSettingsPage = new ClangdSettingsPage;
    const auto clangdPanelFactory = new ProjectPanelFactory;
    clangdPanelFactory->setPriority(100);
    clangdPanelFactory->setDisplayName(tr("Clangd"));
    clangdPanelFactory->setCreateWidgetFunction([](Project *project) {
      return new ClangdProjectSettingsWidget(project);
    });
    ProjectPanelFactory::registerFactory(clangdPanelFactory);
  }

  // Add the hover handler factories here instead of in initialize()
  // so that the Clang Code Model has a chance to hook in.
  d->m_cppEditorFactory.addHoverHandler(CppModelManager::instance()->createHoverHandler());
  d->m_cppEditorFactory.addHoverHandler(new ColorPreviewHoverHandler);
  d->m_cppEditorFactory.addHoverHandler(new ResourcePreviewHoverHandler);

  FileIconProvider::registerIconOverlayForMimeType(orcaTheme()->imageFile(Theme::IconOverlayCppSource, ProjectExplorer::Constants::FILEOVERLAY_CPP), Constants::CPP_SOURCE_MIMETYPE);
  FileIconProvider::registerIconOverlayForMimeType(orcaTheme()->imageFile(Theme::IconOverlayCSource, ProjectExplorer::Constants::FILEOVERLAY_C), Constants::C_SOURCE_MIMETYPE);
  FileIconProvider::registerIconOverlayForMimeType(orcaTheme()->imageFile(Theme::IconOverlayCppHeader, ProjectExplorer::Constants::FILEOVERLAY_H), Constants::CPP_HEADER_MIMETYPE);
}

auto CppEditorPlugin::switchDeclarationDefinition() -> void
{
  if (auto editorWidget = currentCppEditorWidget())
    editorWidget->switchDeclarationDefinition(/*inNextSplit*/ false);
}

auto CppEditorPlugin::openDeclarationDefinitionInNextSplit() -> void
{
  if (auto editorWidget = currentCppEditorWidget())
    editorWidget->switchDeclarationDefinition(/*inNextSplit*/ true);
}

auto CppEditorPlugin::renameSymbolUnderCursor() -> void
{
  if (auto editorWidget = currentCppEditorWidget())
    editorWidget->renameSymbolUnderCursor();
}

auto CppEditorPlugin::showPreProcessorDialog() -> void
{
  if (auto editorWidget = currentCppEditorWidget())
    editorWidget->showPreProcessorWidget();
}

auto CppEditorPluginPrivate::onTaskStarted(Id type) -> void
{
  if (type == Constants::TASK_INDEX) {
    ActionManager::command(TextEditor::Constants::FIND_USAGES)->action()->setEnabled(false);
    ActionManager::command(TextEditor::Constants::RENAME_SYMBOL)->action()->setEnabled(false);
    m_reparseExternallyChangedFiles->setEnabled(false);
    m_openTypeHierarchyAction->setEnabled(false);
    m_openIncludeHierarchyAction->setEnabled(false);
  }
}

auto CppEditorPluginPrivate::onAllTasksFinished(Id type) -> void
{
  if (type == Constants::TASK_INDEX) {
    ActionManager::command(TextEditor::Constants::FIND_USAGES)->action()->setEnabled(true);
    ActionManager::command(TextEditor::Constants::RENAME_SYMBOL)->action()->setEnabled(true);
    m_reparseExternallyChangedFiles->setEnabled(true);
    m_openTypeHierarchyAction->setEnabled(true);
    m_openIncludeHierarchyAction->setEnabled(true);
  }
}

auto CppEditorPluginPrivate::inspectCppCodeModel() -> void
{
  if (m_cppCodeModelInspectorDialog) {
    ICore::raiseWindow(m_cppCodeModelInspectorDialog);
  } else {
    m_cppCodeModelInspectorDialog = new CppCodeModelInspectorDialog(ICore::dialogParent());
    ICore::registerWindow(m_cppCodeModelInspectorDialog, Context("CppEditor.Inspector"));
    m_cppCodeModelInspectorDialog->show();
  }
}

auto CppEditorPlugin::createTestObjects() const -> QVector<QObject*>
{
  return {
    #ifdef WITH_TESTS
        new CodegenTest,
        new CompilerOptionsBuilderTest,
        new CompletionTest,
        new FunctionUtilsTest,
        new HeaderPathFilterTest,
        new HeaderSourceTest,
        new IncludeGroupsTest,
        new LocalSymbolsTest,
        new LocatorFilterTest,
        new ModelManagerTest,
        new PointerDeclarationFormatterTest,
        new ProjectFileCategorizerTest,
        new ProjectInfoGeneratorTest,
        new ProjectPartChooserTest,
        new DocumentTrackerTest,
        new SourceProcessorTest,
        new SymbolSearcherTest,
        new TypeHierarchyBuilderTest,
        new Tests::AutoCompleterTest,
        new Tests::DoxygenTest,
        new Tests::FileAndTokenActionsTest,
        new Tests::FollowSymbolTest,
        new Tests::IncludeHierarchyTest,
        new Tests::InsertVirtualMethodsTest,
        new Tests::QuickfixTest,
        new Tests::SelectionsTest,
    #endif
  };
}

auto CppEditorPlugin::openTypeHierarchy() -> void
{
  if (currentCppEditorWidget()) {
    emit typeHierarchyRequested();
    NavigationWidget::activateSubWidget(Constants::TYPE_HIERARCHY_ID, Side::Left);
  }
}

auto CppEditorPlugin::openIncludeHierarchy() -> void
{
  if (currentCppEditorWidget()) {
    emit includeHierarchyRequested();
    NavigationWidget::activateSubWidget(Constants::INCLUDE_HIERARCHY_ID, Side::Left);
  }
}

auto CppEditorPlugin::clearHeaderSourceCache() -> void
{
  m_headerSourceMapping.clear();
}

auto CppEditorPlugin::licenseTemplatePath() -> FilePath
{
  return FilePath::fromString(m_instance->d->m_fileSettings.licenseTemplatePath);
}

auto CppEditorPlugin::licenseTemplate() -> QString
{
  return CppFileSettings::licenseTemplate();
}

auto CppEditorPlugin::usePragmaOnce() -> bool
{
  return m_instance->d->m_fileSettings.headerPragmaOnce;
}

auto CppEditorPlugin::headerSearchPaths() -> const QStringList&
{
  return m_instance->d->m_fileSettings.headerSearchPaths;
}

auto CppEditorPlugin::sourceSearchPaths() -> const QStringList&
{
  return m_instance->d->m_fileSettings.sourceSearchPaths;
}

auto CppEditorPlugin::headerPrefixes() -> const QStringList&
{
  return m_instance->d->m_fileSettings.headerPrefixes;
}

auto CppEditorPlugin::sourcePrefixes() -> const QStringList&
{
  return m_instance->d->m_fileSettings.sourcePrefixes;
}

auto CppEditorPlugin::codeModelSettings() -> CppCodeModelSettings*
{
  return &d->m_codeModelSettings;
}

auto CppEditorPlugin::fileSettings() -> CppFileSettings*
{
  return &instance()->d->m_fileSettings;
}

auto CppEditorPlugin::switchHeaderSource() -> void
{
  CppEditor::switchHeaderSource();
}

auto CppEditorPlugin::switchHeaderSourceInNextSplit() -> void
{
  const auto otherFile = FilePath::fromString(correspondingHeaderOrSource(EditorManager::currentDocument()->filePath().toString()));
  if (!otherFile.isEmpty())
    EditorManager::openEditor(otherFile, Id(), EditorManager::OpenInOtherSplit);
}

static auto findFilesInProject(const QString &name, const Project *project) -> QStringList
{
  if (debug)
    qDebug() << Q_FUNC_INFO << name << project;

  if (!project)
    return QStringList();

  auto pattern = QString(1, QLatin1Char('/'));
  pattern += name;
  const auto projectFiles = transform(project->files(Project::AllFiles), &FilePath::toString);
  const auto pcend = projectFiles.constEnd();
  QStringList candidateList;
  for (auto it = projectFiles.constBegin(); it != pcend; ++it) {
    if (it->endsWith(pattern, HostOsInfo::fileNameCaseSensitivity()))
      candidateList.append(*it);
  }
  return candidateList;
}

// Return the suffixes that should be checked when trying to find a
// source belonging to a header and vice versa
static auto matchingCandidateSuffixes(ProjectFile::Kind kind) -> QStringList
{
  switch (kind) {
  case ProjectFile::AmbiguousHeader:
  case ProjectFile::CHeader:
  case ProjectFile::CXXHeader:
  case ProjectFile::ObjCHeader:
  case ProjectFile::ObjCXXHeader:
    return mimeTypeForName(Constants::C_SOURCE_MIMETYPE).suffixes() + mimeTypeForName(Constants::CPP_SOURCE_MIMETYPE).suffixes() + mimeTypeForName(Constants::OBJECTIVE_C_SOURCE_MIMETYPE).suffixes() + mimeTypeForName(Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE).suffixes() + mimeTypeForName(Constants::CUDA_SOURCE_MIMETYPE).suffixes();
  case ProjectFile::CSource:
  case ProjectFile::ObjCSource:
    return mimeTypeForName(Constants::C_HEADER_MIMETYPE).suffixes();
  case ProjectFile::CXXSource:
  case ProjectFile::ObjCXXSource:
  case ProjectFile::CudaSource:
  case ProjectFile::OpenCLSource:
    return mimeTypeForName(Constants::CPP_HEADER_MIMETYPE).suffixes();
  default:
    return QStringList();
  }
}

static auto baseNameWithAllSuffixes(const QString &baseName, const QStringList &suffixes) -> QStringList
{
  QStringList result;
  const QChar dot = QLatin1Char('.');
  for (const auto &suffix : suffixes) {
    auto fileName = baseName;
    fileName += dot;
    fileName += suffix;
    result += fileName;
  }
  return result;
}

static auto baseNamesWithAllPrefixes(const QStringList &baseNames, bool isHeader) -> QStringList
{
  QStringList result;
  const auto &sourcePrefixes = m_instance->sourcePrefixes();
  const auto &headerPrefixes = m_instance->headerPrefixes();

  for (const auto &name : baseNames) {
    for (const auto &prefix : isHeader ? headerPrefixes : sourcePrefixes) {
      if (name.startsWith(prefix)) {
        auto nameWithoutPrefix = name.mid(prefix.size());
        result += nameWithoutPrefix;
        for (const auto &prefix : isHeader ? sourcePrefixes : headerPrefixes)
          result += prefix + nameWithoutPrefix;
      }
    }
    for (const auto &prefix : isHeader ? sourcePrefixes : headerPrefixes)
      result += prefix + name;

  }
  return result;
}

static auto baseDirWithAllDirectories(const QDir &baseDir, const QStringList &directories) -> QStringList
{
  QStringList result;
  for (const auto &dir : directories)
    result << QDir::cleanPath(baseDir.absoluteFilePath(dir));
  return result;
}

static auto commonFilePathLength(const QString &s1, const QString &s2) -> int
{
  int length = qMin(s1.length(), s2.length());
  for (auto i = 0; i < length; ++i)
    if (HostOsInfo::fileNameCaseSensitivity() == Qt::CaseSensitive) {
      if (s1[i] != s2[i])
        return i;
    } else {
      if (s1[i].toLower() != s2[i].toLower())
        return i;
    }
  return length;
}

static auto correspondingHeaderOrSourceInProject(const QFileInfo &fileInfo, const QStringList &candidateFileNames, const Project *project, CacheUsage cacheUsage) -> QString
{
  QString bestFileName;
  auto compareValue = 0;
  const auto filePath = fileInfo.filePath();
  for (const auto &candidateFileName : candidateFileNames) {
    const auto projectFiles = findFilesInProject(candidateFileName, project);
    // Find the file having the most common path with fileName
    for (const auto &projectFile : projectFiles) {
      auto value = commonFilePathLength(filePath, projectFile);
      if (value > compareValue) {
        compareValue = value;
        bestFileName = projectFile;
      }
    }
  }
  if (!bestFileName.isEmpty()) {
    const QFileInfo candidateFi(bestFileName);
    QTC_ASSERT(candidateFi.isFile(), return QString());
    if (cacheUsage == CacheUsage::ReadWrite) {
      m_headerSourceMapping[fileInfo.absoluteFilePath()] = candidateFi.absoluteFilePath();
      m_headerSourceMapping[candidateFi.absoluteFilePath()] = fileInfo.absoluteFilePath();
    }
    return candidateFi.absoluteFilePath();
  }

  return QString();
}

} // namespace Internal

using namespace Internal;

auto correspondingHeaderOrSource(const QString &fileName, bool *wasHeader, CacheUsage cacheUsage) -> QString
{
  const QFileInfo fi(fileName);
  auto kind = ProjectFile::classify(fileName);
  const auto isHeader = ProjectFile::isHeader(kind);
  if (wasHeader)
    *wasHeader = isHeader;
  if (m_headerSourceMapping.contains(fi.absoluteFilePath()))
    return m_headerSourceMapping.value(fi.absoluteFilePath());

  if (debug)
    qDebug() << Q_FUNC_INFO << fileName << kind;

  if (kind == ProjectFile::Unsupported)
    return QString();

  const auto baseName = fi.completeBaseName();
  const QString privateHeaderSuffix = QLatin1String("_p");
  const auto suffixes = matchingCandidateSuffixes(kind);

  auto candidateFileNames = baseNameWithAllSuffixes(baseName, suffixes);
  if (isHeader) {
    if (baseName.endsWith(privateHeaderSuffix)) {
      auto sourceBaseName = baseName;
      sourceBaseName.truncate(sourceBaseName.size() - privateHeaderSuffix.size());
      candidateFileNames += baseNameWithAllSuffixes(sourceBaseName, suffixes);
    }
  } else {
    auto privateHeaderBaseName = baseName;
    privateHeaderBaseName.append(privateHeaderSuffix);
    candidateFileNames += baseNameWithAllSuffixes(privateHeaderBaseName, suffixes);
  }

  const auto absoluteDir = fi.absoluteDir();
  QStringList candidateDirs(absoluteDir.absolutePath());
  // If directory is not root, try matching against its siblings
  const auto searchPaths = isHeader ? m_instance->sourceSearchPaths() : m_instance->headerSearchPaths();
  candidateDirs += baseDirWithAllDirectories(absoluteDir, searchPaths);

  candidateFileNames += baseNamesWithAllPrefixes(candidateFileNames, isHeader);

  // Try to find a file in the same or sibling directories first
  for (const auto &candidateDir : qAsConst(candidateDirs)) {
    for (const auto &candidateFileName : qAsConst(candidateFileNames)) {
      const QString candidateFilePath = candidateDir + QLatin1Char('/') + candidateFileName;
      const auto normalized = FileUtils::normalizedPathName(candidateFilePath);
      const QFileInfo candidateFi(normalized);
      if (candidateFi.isFile()) {
        if (cacheUsage == CacheUsage::ReadWrite) {
          m_headerSourceMapping[fi.absoluteFilePath()] = candidateFi.absoluteFilePath();
          if (!isHeader || !baseName.endsWith(privateHeaderSuffix))
            m_headerSourceMapping[candidateFi.absoluteFilePath()] = fi.absoluteFilePath();
        }
        return candidateFi.absoluteFilePath();
      }
    }
  }

  // Find files in the current project
  auto currentProject = ProjectTree::currentProject();
  if (currentProject) {
    const auto path = correspondingHeaderOrSourceInProject(fi, candidateFileNames, currentProject, cacheUsage);
    if (!path.isEmpty())
      return path;

    // Find files in other projects
  } else {
    auto modelManager = CppModelManager::instance();
    const auto projectInfos = modelManager->projectInfos();
    for (const auto &projectInfo : projectInfos) {
      const Project *project = projectForProjectInfo(*projectInfo);
      if (project == currentProject)
        continue; // We have already checked the current project.

      const auto path = correspondingHeaderOrSourceInProject(fi, candidateFileNames, project, cacheUsage);
      if (!path.isEmpty())
        return path;
    }
  }

  return QString();
}

} // namespace CppEditor
