// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "pathlisteditor.hpp"

#include "hostosinfo.hpp"
#include "stringutils.hpp"
#include "fileutils.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSharedPointer>
#include <QTextBlock>
#include <QVBoxLayout>

/*!
    \class Utils::PathListEditor

    \brief The PathListEditor class is a control that lets the user edit a list
    of (directory) paths
    using the platform separator (';',':').

    Typically used for
    path lists controlled by environment variables, such as
    PATH. It is based on a QPlainTextEdit as it should
    allow for convenient editing and non-directory type elements like
    \code
    "etc/mydir1:$SPECIAL_SYNTAX:/etc/mydir2".
    \endcode

    When pasting text into it, the platform separator will be replaced
    by new line characters for convenience.
 */

namespace Utils {

constexpr int PathListEditor::lastInsertButtonIndex = 0;

// ------------ PathListPlainTextEdit:
// Replaces the platform separator ';',':' by '\n'
// when inserting, allowing for pasting in paths
// from the terminal or such.

class PathListPlainTextEdit : public QPlainTextEdit {
public:
  explicit PathListPlainTextEdit(QWidget *parent = nullptr);
protected:
  auto insertFromMimeData(const QMimeData *source) -> void override;
};

PathListPlainTextEdit::PathListPlainTextEdit(QWidget *parent) : QPlainTextEdit(parent)
{
  // No wrapping, scroll at all events
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setLineWrapMode(QPlainTextEdit::NoWrap);
}

auto PathListPlainTextEdit::insertFromMimeData(const QMimeData *source) -> void
{
  if (source->hasText()) {
    // replace separator
    QString text = source->text().trimmed();
    text.replace(HostOsInfo::pathListSeparator(), QLatin1Char('\n'));
    QSharedPointer<QMimeData> fixed(new QMimeData);
    fixed->setText(text);
    QPlainTextEdit::insertFromMimeData(fixed.data());
  } else {
    QPlainTextEdit::insertFromMimeData(source);
  }
}

// ------------ PathListEditorPrivate
struct PathListEditorPrivate {
  PathListEditorPrivate();

  QHBoxLayout *layout;
  QVBoxLayout *buttonLayout;
  QPlainTextEdit *edit;
  QString fileDialogTitle;
};

PathListEditorPrivate::PathListEditorPrivate() : layout(new QHBoxLayout), buttonLayout(new QVBoxLayout), edit(new PathListPlainTextEdit)
{
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(edit);
  layout->addLayout(buttonLayout);
  buttonLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
}

PathListEditor::PathListEditor(QWidget *parent) : QWidget(parent), d(new PathListEditorPrivate)
{
  setLayout(d->layout);
  addButton(tr("Insert..."), this, [this] {
    const FilePath dir = FileUtils::getExistingDirectory(this, d->fileDialogTitle);
    if (!dir.isEmpty())
      insertPathAtCursor(dir.toUserOutput());
  });
  addButton(tr("Delete Line"), this, [this] { deletePathAtCursor(); });
  addButton(tr("Clear"), this, [this] { d->edit->clear(); });
  connect(d->edit, &QPlainTextEdit::textChanged, this, &PathListEditor::changed);
}

PathListEditor::~PathListEditor()
{
  delete d;
}

auto PathListEditor::addButton(const QString &text, QObject *parent, std::function<void()> slotFunc) -> QPushButton*
{
  return insertButton(d->buttonLayout->count() - 1, text, parent, slotFunc);
}

auto PathListEditor::insertButton(int index /* -1 */, const QString &text, QObject *parent, std::function<void()> slotFunc) -> QPushButton*
{
  auto rc = new QPushButton(text, this);
  QObject::connect(rc, &QPushButton::pressed, parent, slotFunc);
  d->buttonLayout->insertWidget(index, rc);
  return rc;
}

auto PathListEditor::pathListString() const -> QString
{
  return pathList().join(HostOsInfo::pathListSeparator());
}

auto PathListEditor::pathList() const -> QStringList
{
  const QString text = d->edit->toPlainText().trimmed();
  if (text.isEmpty())
    return QStringList();
  // trim each line
  QStringList rc = text.split('\n', Qt::SkipEmptyParts);
  const QStringList::iterator end = rc.end();
  for (QStringList::iterator it = rc.begin(); it != end; ++it)
    *it = it->trimmed();
  return rc;
}

auto PathListEditor::setPathList(const QStringList &l) -> void
{
  d->edit->setPlainText(l.join(QLatin1Char('\n')));
}

auto PathListEditor::setPathList(const QString &pathString) -> void
{
  if (pathString.isEmpty()) {
    clear();
  } else {
    setPathList(pathString.split(HostOsInfo::pathListSeparator(), Qt::SkipEmptyParts));
  }
}

auto PathListEditor::fileDialogTitle() const -> QString
{
  return d->fileDialogTitle;
}

auto PathListEditor::setFileDialogTitle(const QString &l) -> void
{
  d->fileDialogTitle = l;
}

auto PathListEditor::clear() -> void
{
  d->edit->clear();
}

auto PathListEditor::text() const -> QString
{
  return d->edit->toPlainText();
}

auto PathListEditor::setText(const QString &t) -> void
{
  d->edit->setPlainText(t);
}

auto PathListEditor::insertPathAtCursor(const QString &path) -> void
{
  // If the cursor is at an empty line or at end(),
  // just insert. Else insert line before
  QTextCursor cursor = d->edit->textCursor();
  QTextBlock block = cursor.block();
  const bool needNewLine = !block.text().isEmpty();
  if (needNewLine) {
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    cursor.insertBlock();
    cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
  }
  cursor.insertText(path);
  if (needNewLine) {
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    d->edit->setTextCursor(cursor);
  }
}

auto PathListEditor::deletePathAtCursor() -> void
{
  // Delete current line
  QTextCursor cursor = d->edit->textCursor();
  if (cursor.block().isValid()) {
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    // Select down or until end of [last] line
    if (!cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor))
      cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    d->edit->setTextCursor(cursor);
  }
}

} // namespace Utils
