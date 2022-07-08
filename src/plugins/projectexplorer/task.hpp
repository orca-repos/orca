// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/id.hpp>
#include <utils/fileutils.hpp>
#include <utils/porting.hpp>

#include <QIcon>
#include <QMetaType>
#include <QStringList>
#include <QTextLayout>

namespace TextEditor {
class TextMark;
}

namespace ProjectExplorer {

class TaskHub;

// Documentation inside.
class PROJECTEXPLORER_EXPORT Task {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Task)

public:
  enum TaskType : char {
    Unknown,
    Error,
    Warning
  };

  enum Option : char {
    NoOptions = 0,
    AddTextMark = 1 << 0,
    FlashWorthy = 1 << 1,
  };

  using Options = char;

  Task() = default;
  Task(TaskType type, const QString &description, const Utils::FilePath &file, int line, Utils::Id category, const QIcon &icon = QIcon(), Options options = AddTextMark | FlashWorthy);
  static auto compilerMissingTask() -> Task;
  auto isNull() const -> bool;
  auto clear() -> void;
  auto setFile(const Utils::FilePath &file) -> void;
  auto description() const -> QString;
  auto icon() const -> QIcon;

  friend PROJECTEXPLORER_EXPORT auto operator==(const Task &t1, const Task &t2) -> bool;
  friend PROJECTEXPLORER_EXPORT auto operator<(const Task &a, const Task &b) -> bool;
  friend PROJECTEXPLORER_EXPORT auto qHash(const Task &task) -> Utils::QHashValueType;

  unsigned int taskId = 0;
  TaskType type = Unknown;
  Options options = AddTextMark | FlashWorthy;
  QString summary;
  QStringList details;
  Utils::FilePath file;
  Utils::FilePaths fileCandidates;
  int line = -1;
  int movedLine = -1; // contains a line number if the line was moved in the editor
  int column = 0;
  Utils::Id category;

  // Having a container of QTextLayout::FormatRange in Task isn't that great
  // It would be cleaner to split up the text into
  // the logical hunks and then assemble them again
  // (That is different consumers of tasks could Show them in
  // different ways!)
  // But then again, the wording of the text most likely
  // doesn't work if you split it up, nor are our parsers
  // anywhere near being that good
  QVector<QTextLayout::FormatRange> formats;

private:
  auto setMark(TextEditor::TextMark *mark) -> void;

  QSharedPointer<TextEditor::TextMark> m_mark;
  mutable QIcon m_icon;
  static unsigned int s_nextId;

  friend class TaskHub;
};

class PROJECTEXPLORER_EXPORT CompileTask : public Task {
public:
  CompileTask(TaskType type, const QString &description, const Utils::FilePath &file = {}, int line = -1, int column = 0);
};

class PROJECTEXPLORER_EXPORT BuildSystemTask : public Task {
public:
  BuildSystemTask(TaskType type, const QString &description, const Utils::FilePath &file = {}, int line = -1);
};

class PROJECTEXPLORER_EXPORT DeploymentTask : public Task {
public:
  DeploymentTask(TaskType type, const QString &description);
};

using Tasks = QList<Task>;

PROJECTEXPLORER_EXPORT auto toHtml(const Tasks &issues) -> QString;
PROJECTEXPLORER_EXPORT auto containsType(const Tasks &issues, Task::TaskType) -> bool;

} //namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Task)
