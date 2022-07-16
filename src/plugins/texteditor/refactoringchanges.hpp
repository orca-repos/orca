// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <utils/changeset.hpp>
#include <utils/fileutils.hpp>
#include <utils/textfileformat.hpp>

#include <QList>
#include <QSharedPointer>
#include <QString>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace TextEditor {

class TextDocument;
class TextEditorWidget;
class RefactoringChanges;
class RefactoringFile;
class RefactoringChangesData;
using RefactoringFilePtr = QSharedPointer<RefactoringFile>;
using RefactoringSelections = QVector<QPair<QTextCursor, QTextCursor>>;

// ### listen to the m_editor::destroyed signal?
class TEXTEDITOR_EXPORT RefactoringFile {
  Q_DISABLE_COPY(RefactoringFile)

public:
  using Range = Utils::ChangeSet::Range;

  virtual ~RefactoringFile();

  auto isValid() const -> bool;
  auto document() const -> const QTextDocument*;
  // mustn't use the cursor to change the document
  auto cursor() const -> const QTextCursor;
  auto filePath() const -> Utils::FilePath;
  auto editor() const -> TextEditorWidget*;
  // converts 1-based line and column into 0-based source offset
  auto position(int line, int column) const -> int;
  // converts 0-based source offset into 1-based line and column
  auto lineAndColumn(int offset, int *line, int *column) const -> void;
  auto charAt(int pos) const -> QChar;
  auto textOf(int start, int end) const -> QString;
  auto textOf(const Range &range) const -> QString;
  auto changeSet() const -> Utils::ChangeSet;
  auto setChangeSet(const Utils::ChangeSet &changeSet) -> void;
  auto appendIndentRange(const Range &range) -> void;
  auto appendReindentRange(const Range &range) -> void;
  auto setOpenEditor(bool activate = false, int pos = -1) -> void;
  auto apply() -> bool;

protected:
  enum IndentType {
    Indent,
    Reindent
  };

  // users may only get const access to RefactoringFiles created through
  // this constructor, because it can't be used to apply changes
  RefactoringFile(QTextDocument *document, const Utils::FilePath &filePath);
  RefactoringFile(TextEditorWidget *editor);
  RefactoringFile(const Utils::FilePath &filePath, const QSharedPointer<RefactoringChangesData> &data);

  auto mutableDocument() const -> QTextDocument*;
  // derived classes may want to clear language specific extra data
  virtual auto fileChanged() -> void;
  auto indentOrReindent(const RefactoringSelections &ranges, IndentType indent) -> void;

  Utils::FilePath m_filePath;
  QSharedPointer<RefactoringChangesData> m_data;
  mutable Utils::TextFileFormat m_textFileFormat;
  mutable QTextDocument *m_document = nullptr;
  TextEditorWidget *m_editor = nullptr;
  Utils::ChangeSet m_changes;
  QList<Range> m_indentRanges;
  QList<Range> m_reindentRanges;
  bool m_openEditor = false;
  bool m_activateEditor = false;
  int m_editorCursorPosition = -1;
  bool m_appliedOnce = false;

  friend class RefactoringChanges; // access to constructor
};

/*!
   This class batches changes to multiple file, which are applied as a single big
   change.
*/
class TEXTEDITOR_EXPORT RefactoringChanges {
public:
  using Range = Utils::ChangeSet::Range;

  RefactoringChanges();
  virtual ~RefactoringChanges();

  static auto file(TextEditorWidget *editor) -> RefactoringFilePtr;
  auto file(const Utils::FilePath &filePath) const -> RefactoringFilePtr;
  auto createFile(const Utils::FilePath &filePath, const QString &contents, bool reindent = true, bool openEditor = true) const -> bool;
  auto removeFile(const Utils::FilePath &filePath) const -> bool;

protected:
  explicit RefactoringChanges(RefactoringChangesData *data);

  static auto openEditor(const Utils::FilePath &filePath, bool activate, int line, int column) -> TextEditorWidget*;
  static auto rangesToSelections(QTextDocument *document, const QList<Range> &ranges) -> RefactoringSelections;

  QSharedPointer<RefactoringChangesData> m_data;

  friend class RefactoringFile;
};

class TEXTEDITOR_EXPORT RefactoringChangesData {
  Q_DISABLE_COPY(RefactoringChangesData)

public:
  RefactoringChangesData() = default;
  virtual ~RefactoringChangesData();

  virtual auto indentSelection(const QTextCursor &selection, const Utils::FilePath &filePath, const TextDocument *textEditor) const -> void;
  virtual auto reindentSelection(const QTextCursor &selection, const Utils::FilePath &filePath, const TextDocument *textEditor) const -> void;
  virtual auto fileChanged(const Utils::FilePath &filePath) -> void;
};

} // namespace TextEditor
