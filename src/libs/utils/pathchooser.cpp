// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "pathchooser.hpp"

#include "commandline.hpp"
#include "environment.hpp"
#include "hostosinfo.hpp"
#include "macroexpander.hpp"
#include "qtcassert.hpp"
#include "qtcprocess.hpp"
#include "theme/theme.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QStandardPaths>

/*!
    \class Utils::PathChooser
    \inmodule Orca

    \brief The PathChooser class is a control that lets the user choose a path.
    The control consist of a QLineEdit and a "Browse" button, and is optionally
    able to perform variable substitution.

    This class has some validation logic for embedding into QWizardPage.
*/

/*!
    \enum Utils::PathChooser::Kind
    \inmodule Orca

    The Kind enum describes the kind of path a PathChooser considers valid.

    \value ExistingDirectory An existing directory
    \value Directory A directory that does not need to exist
    \value File An existing file
    \value SaveFile A file that does not need to exist
    \value ExistingCommand An executable file that must exist at the time of selection
    \value Command An executable file that may or may not exist at the time of selection (e.g. result of a build)
    \value Any No restriction on the selected path

    \sa setExpectedKind(), expectedKind()
*/

namespace Utils {

static auto appBundleExpandedPath(const FilePath &path) -> FilePath
{
  if (path.osType() == OsTypeMac && path.endsWith(".app")) {
    // possibly expand to Foo.app/Contents/MacOS/Foo
    if (path.isDir()) {
      const FilePath exePath = path / "Contents/MacOS" / path.completeBaseName();
      if (exePath.exists())
        return exePath;
    }
  }
  return path;
}

PathChooser::AboutToShowContextMenuHandler PathChooser::s_aboutToShowContextMenuHandler;

// ------------------ BinaryVersionToolTipEventFilter
// Event filter to be installed on a lineedit used for entering
// executables, taking the arguments to print the version ('--version').
// On a tooltip event, the version is obtained by running the binary and
// setting its stdout as tooltip.

class BinaryVersionToolTipEventFilter : public QObject {
public:
  explicit BinaryVersionToolTipEventFilter(QLineEdit *le);

  auto eventFilter(QObject *, QEvent *) -> bool override;

  auto arguments() const -> QStringList { return m_arguments; }
  auto setArguments(const QStringList &arguments) -> void { m_arguments = arguments; }

  static auto toolVersion(const CommandLine &cmd) -> QString;

private:
  // Extension point for concatenating existing tooltips.
  virtual auto defaultToolTip() const -> QString { return QString(); }

  QStringList m_arguments;
};

BinaryVersionToolTipEventFilter::BinaryVersionToolTipEventFilter(QLineEdit *le) : QObject(le)
{
  le->installEventFilter(this);
}

auto BinaryVersionToolTipEventFilter::eventFilter(QObject *o, QEvent *e) -> bool
{
  if (e->type() != QEvent::ToolTip)
    return false;
  auto le = qobject_cast<QLineEdit*>(o);
  QTC_ASSERT(le, return false);

  const QString binary = le->text();
  if (!binary.isEmpty()) {
    const QString version = BinaryVersionToolTipEventFilter::toolVersion(CommandLine(FilePath::fromString(QDir::cleanPath(binary)), m_arguments));
    if (!version.isEmpty()) {
      // Concatenate tooltips.
      QString tooltip = "<html><head/><body>";
      const QString defaultValue = defaultToolTip();
      if (!defaultValue.isEmpty()) {
        tooltip += "<p>";
        tooltip += defaultValue;
        tooltip += "</p>";
      }
      tooltip += "<pre>";
      tooltip += version;
      tooltip += "</pre><body></html>";
      le->setToolTip(tooltip);
    }
  }
  return false;
}

auto BinaryVersionToolTipEventFilter::toolVersion(const CommandLine &cmd) -> QString
{
  if (cmd.executable().isEmpty())
    return QString();
  QtcProcess proc;
  proc.setTimeoutS(1);
  proc.setCommand(cmd);
  proc.runBlocking();
  if (proc.result() != QtcProcess::FinishedWithSuccess)
    return QString();
  return proc.allOutput();
}

// Extends BinaryVersionToolTipEventFilter to prepend the existing pathchooser
// tooltip to display the full path.
class PathChooserBinaryVersionToolTipEventFilter : public BinaryVersionToolTipEventFilter {
public:
  explicit PathChooserBinaryVersionToolTipEventFilter(PathChooser *pe) : BinaryVersionToolTipEventFilter(pe->lineEdit()), m_pathChooser(pe) {}

private:
  auto defaultToolTip() const -> QString override { return m_pathChooser->errorMessage(); }

  const PathChooser *m_pathChooser = nullptr;
};

// ------------------ PathChooserPrivate

class PathChooserPrivate {
public:
  PathChooserPrivate();

  auto expandedPath(const QString &path) const -> FilePath;

  QHBoxLayout *m_hLayout = nullptr;
  FancyLineEdit *m_lineEdit = nullptr;

  PathChooser::Kind m_acceptingKind = PathChooser::ExistingDirectory;
  QString m_dialogTitleOverride;
  QString m_dialogFilter;
  FilePath m_initialBrowsePathOverride;
  QString m_defaultValue;
  FilePath m_baseDirectory;
  EnvironmentChange m_environmentChange;
  BinaryVersionToolTipEventFilter *m_binaryVersionToolTipEventFilter = nullptr;
  QList<QAbstractButton*> m_buttons;
  MacroExpander *m_macroExpander = globalMacroExpander();
  std::function<void()> m_openTerminal;
};

PathChooserPrivate::PathChooserPrivate() : m_hLayout(new QHBoxLayout), m_lineEdit(new FancyLineEdit) {}

auto PathChooserPrivate::expandedPath(const QString &input) const -> FilePath
{
  if (input.isEmpty())
    return {};

  FilePath path = FilePath::fromUserInput(input);

  Environment env = path.deviceEnvironment();
  m_environmentChange.applyToEnvironment(env);
  path = env.expandVariables(path);

  if (m_macroExpander)
    path = m_macroExpander->expand(path);

  if (path.isEmpty())
    return path;

  switch (m_acceptingKind) {
  case PathChooser::Command:
  case PathChooser::ExistingCommand: {
    const FilePath expanded = path.searchInPath({m_baseDirectory});
    return expanded.isEmpty() ? path : expanded;
  }
  case PathChooser::Any:
    break;
  case PathChooser::Directory:
  case PathChooser::ExistingDirectory:
  case PathChooser::File:
  case PathChooser::SaveFile:
    if (!m_baseDirectory.isEmpty()) {
      Utils::FilePath fp = m_baseDirectory.resolvePath(path.path()).absoluteFilePath();
      // FIXME bad hotfix for manually editing PathChooser (invalid paths, jumping cursor)
      // examples: have an absolute path and try to change the device letter by typing the new
      // letter and removing the original afterwards ends up in
      // D:\\dev\\project\\cD:\\dev\\build-project (before trying to remove the original)
      // as 'cD:\\dev\\build-project' is considered is handled as being relative
      // input = "cD:\\dev\build-project"; // prepended 'c' to change the device letter
      // m_baseDirectory = "D:\\dev\\project"
      if (!fp.needsDevice() && HostOsInfo::isWindowsHost() && fp.toString().count(':') > 1)
        return path;
      return fp;
    }
    break;
  }
  return path;
}

PathChooser::PathChooser(QWidget *parent) : QWidget(parent), d(new PathChooserPrivate)
{
  d->m_hLayout->setContentsMargins(0, 0, 0, 0);

  d->m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(d->m_lineEdit, &FancyLineEdit::customContextMenuRequested, this, &PathChooser::contextMenuRequested);
  connect(d->m_lineEdit, &FancyLineEdit::validReturnPressed, this, &PathChooser::returnPressed);
  connect(d->m_lineEdit, &QLineEdit::textChanged, this, [this] { emit rawPathChanged(rawPath()); });
  connect(d->m_lineEdit, &FancyLineEdit::validChanged, this, &PathChooser::validChanged);
  connect(d->m_lineEdit, &QLineEdit::editingFinished, this, &PathChooser::editingFinished);
  connect(d->m_lineEdit, &QLineEdit::textChanged, this, [this] {
    const QString text = d->m_lineEdit->text();
    emit pathChanged(text);
    emit filePathChanged(FilePath::fromUserInput(text));
  });

  d->m_lineEdit->setMinimumWidth(120);
  d->m_hLayout->addWidget(d->m_lineEdit);
  d->m_hLayout->setSizeConstraint(QLayout::SetMinimumSize);

  addButton(browseButtonLabel(), this, [this] { slotBrowse(); });

  setLayout(d->m_hLayout);
  setFocusProxy(d->m_lineEdit);
  setFocusPolicy(d->m_lineEdit->focusPolicy());

  d->m_lineEdit->setValidationFunction(defaultValidationFunction());
}

PathChooser::~PathChooser()
{
  // Since it is our focusProxy it can receive focus-out and emit the signal
  // even when the possible ancestor-receiver is in mid of its destruction.
  disconnect(d->m_lineEdit, &QLineEdit::editingFinished, this, &PathChooser::editingFinished);

  delete d;
}

auto PathChooser::addButton(const QString &text, QObject *context, const std::function<void ()> &callback) -> void
{
  insertButton(d->m_buttons.count(), text, context, callback);
}

auto PathChooser::insertButton(int index, const QString &text, QObject *context, const std::function<void ()> &callback) -> void
{
  auto button = new QPushButton;
  button->setText(text);
  connect(button, &QAbstractButton::clicked, context, callback);
  d->m_hLayout->insertWidget(index + 1/*line edit*/, button);
  d->m_buttons.insert(index, button);
}

auto PathChooser::browseButtonLabel() -> QString
{
  return HostOsInfo::isMacHost() ? tr("Choose...") : tr("Browse...");
}

auto PathChooser::buttonAtIndex(int index) const -> QAbstractButton*
{
  return d->m_buttons.at(index);
}

auto PathChooser::setBaseDirectory(const FilePath &base) -> void
{
  if (d->m_baseDirectory == base)
    return;
  d->m_baseDirectory = base;
  triggerChanged();
}

auto PathChooser::baseDirectory() const -> FilePath
{
  return d->m_baseDirectory;
}

auto PathChooser::setEnvironmentChange(const EnvironmentChange &env) -> void
{
  QString oldExpand = filePath().toString();
  d->m_environmentChange = env;
  if (filePath().toString() != oldExpand) {
    triggerChanged();
    emit rawPathChanged(rawPath());
  }
}

auto PathChooser::rawPath() const -> QString
{
  return rawFilePath().toString();
}

auto PathChooser::rawFilePath() const -> FilePath
{
  return FilePath::fromUserInput(d->m_lineEdit->text());
}

auto PathChooser::filePath() const -> FilePath
{
  return d->expandedPath(rawFilePath().toString());
}

auto PathChooser::absoluteFilePath() const -> FilePath
{
  return d->m_baseDirectory.resolvePath(filePath());
}

// FIXME: try to remove again
auto PathChooser::expandedDirectory(const QString &input, const Environment &env, const QString &baseDir) -> QString
{
  if (input.isEmpty())
    return input;
  const QString path = QDir::cleanPath(env.expandVariables(input));
  if (path.isEmpty())
    return path;
  if (!baseDir.isEmpty() && QFileInfo(path).isRelative())
    return QFileInfo(baseDir + '/' + path).absoluteFilePath();
  return path;
}

auto PathChooser::setPath(const QString &path) -> void
{
  d->m_lineEdit->setTextKeepingActiveCursor(QDir::toNativeSeparators(path));
}

auto PathChooser::setFilePath(const FilePath &fn) -> void
{
  d->m_lineEdit->setTextKeepingActiveCursor(fn.toUserOutput());
}

auto PathChooser::isReadOnly() const -> bool
{
  return d->m_lineEdit->isReadOnly();
}

auto PathChooser::setReadOnly(bool b) -> void
{
  d->m_lineEdit->setReadOnly(b);
  const auto buttons = d->m_buttons;
  for (QAbstractButton *button : buttons)
    button->setEnabled(!b);
}

auto PathChooser::slotBrowse() -> void
{
  emit beforeBrowsing();

  FilePath predefined = filePath();

  if (!predefined.isEmpty() && !predefined.isDir()) {
    predefined = predefined.parentDir();
  }

  if ((predefined.isEmpty() || !predefined.isDir()) && !d->m_initialBrowsePathOverride.isEmpty()) {
    predefined = d->m_initialBrowsePathOverride;
    if (!predefined.isDir())
      predefined.clear();
  }

  // Prompt for a file/dir
  FilePath newPath;
  switch (d->m_acceptingKind) {
  case PathChooser::Directory:
  case PathChooser::ExistingDirectory:
    newPath = FileUtils::getExistingDirectory(this, makeDialogTitle(tr("Choose Directory")), predefined);
    break;
  case PathChooser::ExistingCommand:
  case PathChooser::Command:
    newPath = FileUtils::getOpenFilePath(this, makeDialogTitle(tr("Choose Executable")), predefined, d->m_dialogFilter);
    newPath = appBundleExpandedPath(newPath);
    break;
  case PathChooser::File: // fall through
    newPath = FileUtils::getOpenFilePath(this, makeDialogTitle(tr("Choose File")), predefined, d->m_dialogFilter);
    newPath = appBundleExpandedPath(newPath);
    break;
  case PathChooser::SaveFile:
    newPath = FileUtils::getSaveFilePath(this, makeDialogTitle(tr("Choose File")), predefined, d->m_dialogFilter);
    break;
  case PathChooser::Any: {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setWindowTitle(makeDialogTitle(tr("Choose File")));
    if (predefined.exists())
      dialog.setDirectory(predefined.absolutePath().toDir());
    // FIXME: fix QFileDialog so that it filters properly: lib*.a
    dialog.setNameFilter(d->m_dialogFilter);
    if (dialog.exec() == QDialog::Accepted) {
      // probably loop here until the *.framework dir match
      QStringList paths = dialog.selectedFiles();
      if (!paths.isEmpty())
        newPath = FilePath::fromString(paths.at(0));
    }
    break;
  }

  default:
    break;
  }

  // work around QTBUG-61004 / ORCABUG-22906
  window()->raise();
  window()->activateWindow();

  // Delete trailing slashes unless it is "/" only.
  if (!newPath.isEmpty()) {
    if (newPath.endsWith("/") && newPath.path().size() > 1)
      newPath = newPath.withNewPath(newPath.path().chopped(1));
    setFilePath(newPath);
  }

  emit browsingFinished();
  triggerChanged();
}

auto PathChooser::contextMenuRequested(const QPoint &pos) -> void
{
  if (QMenu *menu = d->m_lineEdit->createStandardContextMenu()) {
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (s_aboutToShowContextMenuHandler)
      s_aboutToShowContextMenuHandler(this, menu);

    menu->popup(d->m_lineEdit->mapToGlobal(pos));
  }
}

auto PathChooser::isValid() const -> bool
{
  return d->m_lineEdit->isValid();
}

auto PathChooser::errorMessage() const -> QString
{
  return d->m_lineEdit->errorMessage();
}

auto PathChooser::triggerChanged() -> void
{
  d->m_lineEdit->validate();
}

auto PathChooser::setAboutToShowContextMenuHandler(PathChooser::AboutToShowContextMenuHandler handler) -> void
{
  s_aboutToShowContextMenuHandler = handler;
}

auto PathChooser::setOpenTerminalHandler(const std::function<void ()> &openTerminal) -> void
{
  d->m_openTerminal = openTerminal;
}

auto PathChooser::openTerminalHandler() const -> std::function<void()>
{
  return d->m_openTerminal;
}

auto PathChooser::setDefaultValue(const QString &defaultValue) -> void
{
  d->m_defaultValue = defaultValue;
  d->m_lineEdit->setPlaceholderText(defaultValue);
  d->m_lineEdit->validate();
}

auto PathChooser::defaultValidationFunction() const -> FancyLineEdit::ValidationFunction
{
  return std::bind(&PathChooser::validatePath, this, std::placeholders::_1, std::placeholders::_2);
}

auto PathChooser::validatePath(FancyLineEdit *edit, QString *errorMessage) const -> bool
{
  QString path = edit->text();

  if (path.isEmpty()) {
    if (!d->m_defaultValue.isEmpty()) {
      path = d->m_defaultValue;
    } else {
      if (errorMessage)
        *errorMessage = tr("The path must not be empty.");
      return false;
    }
  }

  const FilePath filePath = d->expandedPath(path);
  if (filePath.isEmpty()) {
    if (errorMessage)
      *errorMessage = tr("The path \"%1\" expanded to an empty string.").arg(QDir::toNativeSeparators(path));
    return false;
  }

  // Check if existing
  switch (d->m_acceptingKind) {
  case PathChooser::ExistingDirectory:
    if (!filePath.exists()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" does not exist.").arg(filePath.toUserOutput());
      return false;
    }
    if (!filePath.isDir()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" is not a directory.").arg(filePath.toUserOutput());
      return false;
    }
    break;
  case PathChooser::File:
    if (!filePath.exists()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" does not exist.").arg(filePath.toUserOutput());
      return false;
    }
    if (!filePath.isFile()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" is not a file.").arg(filePath.toUserOutput());
      return false;
    }
    break;
  case PathChooser::SaveFile:
    if (!filePath.parentDir().exists()) {
      if (errorMessage)
        *errorMessage = tr("The directory \"%1\" does not exist.").arg(filePath.toUserOutput());
      return false;
    }
    if (filePath.exists() && filePath.isDir()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" is not a file.").arg(filePath.toUserOutput());
      return false;
    }
    break;
  case PathChooser::ExistingCommand:
    if (!filePath.exists()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" does not exist.").arg(filePath.toUserOutput());
      return false;
    }
    if (!filePath.isExecutableFile()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" is not an executable file.").arg(filePath.toUserOutput());
      return false;
    }
    break;
  case PathChooser::Directory:
    if (filePath.exists() && !filePath.isDir()) {
      if (errorMessage)
        *errorMessage = tr("The path \"%1\" is not a directory.").arg(filePath.toUserOutput());
      return false;
    }
    if (HostOsInfo::isWindowsHost() && !filePath.startsWithDriveLetter() && !filePath.startsWith("\\\\") && !filePath.startsWith("//")) {
      if (errorMessage)
        *errorMessage = tr("Invalid path \"%1\".").arg(filePath.toUserOutput());
      return false;
    }
    break;
  case PathChooser::Command:
    if (filePath.exists() && !filePath.isExecutableFile()) {
      if (errorMessage)
        *errorMessage = tr("Cannot execute \"%1\".").arg(filePath.toUserOutput());
      return false;
    }
    break;

  default: ;
  }

  if (errorMessage)
    *errorMessage = tr("Full path: \"%1\"").arg(filePath.toUserOutput());
  return true;
}

auto PathChooser::setValidationFunction(const FancyLineEdit::ValidationFunction &fn) -> void
{
  d->m_lineEdit->setValidationFunction(fn);
}

auto PathChooser::label() -> QString
{
  return tr("Path:");
}

auto PathChooser::homePath() -> FilePath
{
  // Return 'users/<name>/Documents' on Windows, since Windows explorer
  // does not let people actually display the contents of their home
  // directory. Alternatively, create a Orca-specific directory?
  if (HostOsInfo::isWindowsHost())
    return FilePath::fromString(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  return FilePath::fromString(QDir::homePath());
}

/*!
    Sets the kind of path the PathChooser will consider valid to select
    to \a expected.

    \sa Utils::PathChooser::Kind, expectedKind()
*/

auto PathChooser::setExpectedKind(Kind expected) -> void
{
  if (d->m_acceptingKind == expected)
    return;
  d->m_acceptingKind = expected;
  d->m_lineEdit->validate();
}

/*!
    Returns the kind of path the PathChooser considers valid to select.

    \sa Utils::PathChooser::Kind, setExpectedKind()
*/

auto PathChooser::expectedKind() const -> PathChooser::Kind
{
  return d->m_acceptingKind;
}

auto PathChooser::setPromptDialogTitle(const QString &title) -> void
{
  d->m_dialogTitleOverride = title;
}

auto PathChooser::promptDialogTitle() const -> QString
{
  return d->m_dialogTitleOverride;
}

auto PathChooser::setPromptDialogFilter(const QString &filter) -> void
{
  d->m_dialogFilter = filter;
  d->m_lineEdit->validate();
}

auto PathChooser::promptDialogFilter() const -> QString
{
  return d->m_dialogFilter;
}

auto PathChooser::setInitialBrowsePathBackup(const FilePath &path) -> void
{
  d->m_initialBrowsePathOverride = path;
}

auto PathChooser::makeDialogTitle(const QString &title) -> QString
{
  if (d->m_dialogTitleOverride.isNull())
    return title;
  else
    return d->m_dialogTitleOverride;
}

auto PathChooser::lineEdit() const -> FancyLineEdit*
{
  // HACK: Make it work with HistoryCompleter.
  if (d->m_lineEdit->objectName().isEmpty())
    d->m_lineEdit->setObjectName(objectName() + "LineEdit");
  return d->m_lineEdit;
}

auto PathChooser::toolVersion(const CommandLine &cmd) -> QString
{
  return BinaryVersionToolTipEventFilter::toolVersion(cmd);
}

auto PathChooser::installLineEditVersionToolTip(QLineEdit *le, const QStringList &arguments) -> void
{
  auto ef = new BinaryVersionToolTipEventFilter(le);
  ef->setArguments(arguments);
}

auto PathChooser::setHistoryCompleter(const QString &historyKey, bool restoreLastItemFromHistory) -> void
{
  d->m_lineEdit->setHistoryCompleter(historyKey, restoreLastItemFromHistory);
}

auto PathChooser::setMacroExpander(MacroExpander *macroExpander) -> void
{
  d->m_macroExpander = macroExpander;
}

auto PathChooser::commandVersionArguments() const -> QStringList
{
  return d->m_binaryVersionToolTipEventFilter ? d->m_binaryVersionToolTipEventFilter->arguments() : QStringList();
}

auto PathChooser::setCommandVersionArguments(const QStringList &arguments) -> void
{
  if (arguments.isEmpty()) {
    if (d->m_binaryVersionToolTipEventFilter) {
      delete d->m_binaryVersionToolTipEventFilter;
      d->m_binaryVersionToolTipEventFilter = nullptr;
    }
  } else {
    if (!d->m_binaryVersionToolTipEventFilter)
      d->m_binaryVersionToolTipEventFilter = new PathChooserBinaryVersionToolTipEventFilter(this);
    d->m_binaryVersionToolTipEventFilter->setArguments(arguments);
  }
}

} // namespace Utils
