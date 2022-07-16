// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "extracompiler.hpp"

#include "buildconfiguration.hpp"
#include "buildmanager.hpp"
#include "kitinformation.hpp"
#include "session.hpp"
#include "target.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-document-interface.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/texteditorconstants.hpp>
#include <texteditor/fontsettings.hpp>

#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/runextensions.hpp>

#include <QDateTime>
#include <QFutureInterface>
#include <QFutureWatcher>
#include <QTextBlock>
#include <QThreadPool>
#include <QTimer>

using namespace Utils;

namespace ProjectExplorer {

Q_GLOBAL_STATIC(QThreadPool, s_extraCompilerThreadPool);
Q_GLOBAL_STATIC(QList<ExtraCompilerFactory *>, factories);

class ExtraCompilerPrivate {
public:
  const Project *project;
  FilePath source;
  FileNameToContentsHash contents;
  Tasks issues;
  QDateTime compileTime;
  Orca::Plugin::Core::IEditor *lastEditor = nullptr;
  QMetaObject::Connection activeBuildConfigConnection;
  QMetaObject::Connection activeEnvironmentConnection;
  bool dirty = false;

  QTimer timer;
  auto updateIssues() -> void;
};

ExtraCompiler::ExtraCompiler(const Project *project, const FilePath &source, const FilePaths &targets, QObject *parent) : QObject(parent), d(std::make_unique<ExtraCompilerPrivate>())
{
  d->project = project;
  d->source = source;
  for (const auto &target : targets)
    d->contents.insert(target, QByteArray());
  d->timer.setSingleShot(true);

  connect(&d->timer, &QTimer::timeout, this, [this]() {
    if (d->dirty && d->lastEditor) {
      d->dirty = false;
      run(d->lastEditor->document()->contents());
    }
  });

  connect(BuildManager::instance(), &BuildManager::buildStateChanged, this, &ExtraCompiler::onTargetsBuilt);

  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, [this](Project *project) {
    if (project == d->project)
      deleteLater();
  });

  const auto editorManager = Orca::Plugin::Core::EditorManager::instance();
  connect(editorManager, &Orca::Plugin::Core::EditorManager::currentEditorChanged, this, &ExtraCompiler::onEditorChanged);
  connect(editorManager, &Orca::Plugin::Core::EditorManager::editorAboutToClose, this, &ExtraCompiler::onEditorAboutToClose);

  // Use existing target files, where possible. Otherwise run the compiler.
  const auto sourceTime = d->source.lastModified();
  for (const auto &target : targets) {
    auto targetFileInfo(target.toFileInfo());
    if (!targetFileInfo.exists()) {
      d->dirty = true;
      continue;
    }

    auto lastModified = targetFileInfo.lastModified();
    if (lastModified < sourceTime)
      d->dirty = true;

    if (!d->compileTime.isValid() || d->compileTime > lastModified)
      d->compileTime = lastModified;

    QFile file(target.toString());
    if (file.open(QFile::ReadOnly | QFile::Text))
      setContent(target, file.readAll());
  }
}

ExtraCompiler::~ExtraCompiler() = default;

auto ExtraCompiler::project() const -> const Project*
{
  return d->project;
}

auto ExtraCompiler::source() const -> FilePath
{
  return d->source;
}

auto ExtraCompiler::content(const FilePath &file) const -> QByteArray
{
  return d->contents.value(file);
}

auto ExtraCompiler::targets() const -> FilePaths
{
  return d->contents.keys();
}

auto ExtraCompiler::forEachTarget(std::function<void (const FilePath &)> func) -> void
{
  for (auto it = d->contents.constBegin(), end = d->contents.constEnd(); it != end; ++it)
    func(it.key());
}

auto ExtraCompiler::setCompileTime(const QDateTime &time) -> void
{
  d->compileTime = time;
}

auto ExtraCompiler::compileTime() const -> QDateTime
{
  return d->compileTime;
}

auto ExtraCompiler::extraCompilerThreadPool() -> QThreadPool*
{
  return s_extraCompilerThreadPool();
}

auto ExtraCompiler::isDirty() const -> bool
{
  return d->dirty;
}

auto ExtraCompiler::onTargetsBuilt(Project *project) -> void
{
  if (project != d->project || BuildManager::isBuilding(project))
    return;

  // This is mostly a fall back for the cases when the generator couldn't be run.
  // It pays special attention to the case where a source file was newly created
  const auto sourceTime = d->source.lastModified();
  if (d->compileTime.isValid() && d->compileTime >= sourceTime)
    return;

  forEachTarget([&](const FilePath &target) {
    const auto fi(target.toFileInfo());
    const auto generateTime = fi.exists() ? fi.lastModified() : QDateTime();
    if (generateTime.isValid() && (generateTime > sourceTime)) {
      if (d->compileTime >= generateTime)
        return;

      QFile file(target.toString());
      if (file.open(QFile::ReadOnly | QFile::Text)) {
        d->compileTime = generateTime;
        setContent(target, file.readAll());
      }
    }
  });
}

auto ExtraCompiler::onEditorChanged(Orca::Plugin::Core::IEditor *editor) -> void
{
  // Handle old editor
  if (d->lastEditor) {
    const auto doc = d->lastEditor->document();
    disconnect(doc, &Orca::Plugin::Core::IDocument::contentsChanged, this, &ExtraCompiler::setDirty);

    if (d->dirty) {
      d->dirty = false;
      run(doc->contents());
    }
  }

  if (editor && editor->document()->filePath() == d->source) {
    d->lastEditor = editor;
    d->updateIssues();

    // Handle new editor
    connect(d->lastEditor->document(), &Orca::Plugin::Core::IDocument::contentsChanged, this, &ExtraCompiler::setDirty);
  } else {
    d->lastEditor = nullptr;
  }
}

auto ExtraCompiler::setDirty() -> void
{
  d->dirty = true;
  d->timer.start(1000);
}

auto ExtraCompiler::onEditorAboutToClose(Orca::Plugin::Core::IEditor *editor) -> void
{
  if (d->lastEditor != editor)
    return;

  // Oh no our editor is going to be closed
  // get the content first
  const auto doc = d->lastEditor->document();
  disconnect(doc, &Orca::Plugin::Core::IDocument::contentsChanged, this, &ExtraCompiler::setDirty);
  if (d->dirty) {
    d->dirty = false;
    run(doc->contents());
  }
  d->lastEditor = nullptr;
}

auto ExtraCompiler::buildEnvironment() const -> Environment
{
  if (const auto target = project()->activeTarget()) {
    if (const auto bc = target->activeBuildConfiguration()) {
      return bc->environment();
    } else {
      const auto changes = EnvironmentKitAspect::environmentChanges(target->kit());
      auto env = Environment::systemEnvironment();
      env.modify(changes);
      return env;
    }
  }

  return Environment::systemEnvironment();
}

auto ExtraCompiler::setCompileIssues(const Tasks &issues) -> void
{
  d->issues = issues;
  d->updateIssues();
}

auto ExtraCompilerPrivate::updateIssues() -> void
{
  if (!lastEditor)
    return;

  const auto widget = qobject_cast<TextEditor::TextEditorWidget*>(lastEditor->widget());
  if (!widget)
    return;

  QList<QTextEdit::ExtraSelection> selections;
  const QTextDocument *document = widget->document();
  for (const auto &issue : qAsConst(issues)) {
    QTextEdit::ExtraSelection selection;
    QTextCursor cursor(document->findBlockByNumber(issue.line - 1));
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    selection.cursor = cursor;

    const auto fontSettings = TextEditor::TextEditorSettings::fontSettings();
    selection.format = fontSettings.toTextCharFormat(issue.type == Task::Warning ? TextEditor::C_WARNING : TextEditor::C_ERROR);
    selection.format.setToolTip(issue.description());
    selections.append(selection);
  }

  widget->setExtraSelections(TextEditor::TextEditorWidget::CodeWarningsSelection, selections);
}

auto ExtraCompiler::setContent(const FilePath &file, const QByteArray &contents) -> void
{
  const auto it = d->contents.find(file);
  if (it != d->contents.end()) {
    if (it.value() != contents) {
      it.value() = contents;
      emit contentsChanged(file);
    }
  }
}

ExtraCompilerFactory::ExtraCompilerFactory(QObject *parent) : QObject(parent)
{
  factories->append(this);
}

ExtraCompilerFactory::~ExtraCompilerFactory()
{
  factories->removeAll(this);
}

auto ExtraCompilerFactory::extraCompilerFactories() -> QList<ExtraCompilerFactory*>
{
  return *factories();
}

ProcessExtraCompiler::ProcessExtraCompiler(const Project *project, const FilePath &source, const FilePaths &targets, QObject *parent) : ExtraCompiler(project, source, targets, parent) { }

ProcessExtraCompiler::~ProcessExtraCompiler()
{
  if (!m_watcher)
    return;
  m_watcher->cancel();
  m_watcher->waitForFinished();
}

auto ProcessExtraCompiler::run(const QByteArray &sourceContents) -> void
{
  const ContentProvider contents = [sourceContents]() { return sourceContents; };
  runImpl(contents);
}

auto ProcessExtraCompiler::run() -> QFuture<FileNameToContentsHash>
{
  const auto fileName = source();
  const ContentProvider contents = [fileName]() {
    QFile file(fileName.toString());
    if (!file.open(QFile::ReadOnly | QFile::Text))
      return QByteArray();
    return file.readAll();
  };
  return runImpl(contents);
}

auto ProcessExtraCompiler::workingDirectory() const -> FilePath
{
  return FilePath();
}

auto ProcessExtraCompiler::arguments() const -> QStringList
{
  return QStringList();
}

auto ProcessExtraCompiler::prepareToRun(const QByteArray &sourceContents) -> bool
{
  Q_UNUSED(sourceContents)
  return true;
}

auto ProcessExtraCompiler::parseIssues(const QByteArray &stdErr) -> Tasks
{
  Q_UNUSED(stdErr)
  return {};
}

auto ProcessExtraCompiler::runImpl(const ContentProvider &provider) -> QFuture<FileNameToContentsHash>
{
  if (m_watcher)
    delete m_watcher;

  m_watcher = new QFutureWatcher<FileNameToContentsHash>();
  connect(m_watcher, &QFutureWatcher<FileNameToContentsHash>::finished, this, &ProcessExtraCompiler::cleanUp);

  m_watcher->setFuture(runAsync(extraCompilerThreadPool(), &ProcessExtraCompiler::runInThread, this, command(), workingDirectory(), arguments(), provider, buildEnvironment()));
  return m_watcher->future();
}

auto ProcessExtraCompiler::runInThread(QFutureInterface<FileNameToContentsHash> &futureInterface, const FilePath &cmd, const FilePath &workDir, const QStringList &args, const ContentProvider &provider, const Environment &env) -> void
{
  if (cmd.isEmpty() || !cmd.toFileInfo().isExecutable())
    return;

  const auto sourceContents = provider();
  if (sourceContents.isNull() || !prepareToRun(sourceContents))
    return;

  QtcProcess process;

  process.setEnvironment(env);
  if (!workDir.isEmpty())
    process.setWorkingDirectory(workDir);
  process.setCommand({cmd, args});
  process.setWriteData(sourceContents);
  process.start();
  if (!process.waitForStarted())
    return;

  while (!futureInterface.isCanceled())
    if (process.waitForFinished(200))
      break;

  if (futureInterface.isCanceled()) {
    process.kill();
    process.waitForFinished();
    return;
  }

  futureInterface.reportResult(handleProcessFinished(&process));
}

auto ProcessExtraCompiler::cleanUp() -> void
{
  QTC_ASSERT(m_watcher, return);
  const auto future = m_watcher->future();
  delete m_watcher;
  m_watcher = nullptr;
  if (!future.resultCount())
    return;
  const auto data = future.result();

  if (data.isEmpty())
    return; // There was some kind of error...

  for (auto it = data.constBegin(), end = data.constEnd(); it != end; ++it)
    setContent(it.key(), it.value());

  setCompileTime(QDateTime::currentDateTime());
}

} // namespace ProjectExplorer
