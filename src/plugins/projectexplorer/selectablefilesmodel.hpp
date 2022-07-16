// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/fileutils.hpp>

#include <QAbstractItemModel>
#include <QDialog>
#include <QFutureInterface>
#include <QFutureWatcher>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QTreeView>

namespace Utils {
class FancyLineEdit;
class PathChooser;
}

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace ProjectExplorer {

class Tree {
public:
  virtual ~Tree()
  {
    qDeleteAll(childDirectories);
    qDeleteAll(files);
  }

  QString name;
  Qt::CheckState checked = Qt::Unchecked;
  bool isDir = false;
  QList<Tree*> childDirectories;
  QList<Tree*> files;
  QList<Tree*> visibleFiles;
  QIcon icon;
  Utils::FilePath fullPath;
  Tree *parent = nullptr;
};

class Glob {
public:
  enum Mode {
    EXACT,
    ENDSWITH,
    REGEXP
  };

  Mode mode;
  QString matchString;
  QRegularExpression matchRegexp;

  auto isMatch(const QString &text) const -> bool
  {
    if (mode == EXACT) {
      if (text == matchString)
        return true;
    } else if (mode == ENDSWITH) {
      if (text.endsWith(matchString))
        return true;
    } else if (mode == REGEXP) {
      if (matchRegexp.match(text).hasMatch())
        return true;
    }
    return false;
  }

  auto operator ==(const Glob &other) const -> bool
  {
    return (mode == other.mode) && (matchString == other.matchString) && (matchRegexp == other.matchRegexp);
  }
};

class PROJECTEXPLORER_EXPORT SelectableFilesModel : public QAbstractItemModel {
  Q_OBJECT

public:
  SelectableFilesModel(QObject *parent);
  ~SelectableFilesModel() override;

  auto setInitialMarkedFiles(const Utils::FilePaths &files) -> void;
  auto columnCount(const QModelIndex &parent) const -> int override;
  auto rowCount(const QModelIndex &parent) const -> int override;
  auto index(int row, int column, const QModelIndex &parent) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto selectedFiles() const -> Utils::FilePaths;
  auto selectedPaths() const -> Utils::FilePaths;
  auto preservedFiles() const -> Utils::FilePaths;
  auto hasCheckedFiles() const -> bool;
  auto applyFilter(const QString &selectFilesfilter, const QString &hideFilesfilter) -> void;
  auto selectAllFiles() -> void;

  enum class FilterState {
    HIDDEN,
    SHOWN,
    CHECKED
  };

  auto filter(Tree *t) -> FilterState;

signals:
  auto checkedFilesChanged() -> void;

protected:
  auto propagateUp(const QModelIndex &index) -> void;
  auto propagateDown(const QModelIndex &idx) -> void;

private:
  auto parseFilter(const QString &filter) -> QList<Glob>;
  auto applyFilter(const QModelIndex &idx) -> Qt::CheckState;
  auto collectFiles(Tree *root, Utils::FilePaths *result) const -> void;
  auto collectPaths(Tree *root, Utils::FilePaths *result) const -> void;
  auto selectAllFiles(Tree *root) -> void;

protected:
  bool m_allFiles = true;
  QSet<Utils::FilePath> m_outOfBaseDirFiles;
  QSet<Utils::FilePath> m_files;
  Tree *m_root = nullptr;

private:
  QList<Glob> m_hideFilesFilter;
  QList<Glob> m_selectFilesFilter;
};

class PROJECTEXPLORER_EXPORT SelectableFilesFromDirModel : public SelectableFilesModel {
  Q_OBJECT public:
  SelectableFilesFromDirModel(QObject *parent);
  ~SelectableFilesFromDirModel() override;

  auto startParsing(const Utils::FilePath &baseDir) -> void;
  auto cancel() -> void;

signals:
  auto parsingFinished() -> void;
  auto parsingProgress(const Utils::FilePath &fileName) -> void;

private:
  auto buildTree(const Utils::FilePath &baseDir, Tree *tree, QFutureInterface<void> &fi, int symlinkDepth) -> void;
  auto run(QFutureInterface<void> &fi) -> void;
  auto buildTreeFinished() -> void;

  // Used in the future thread need to all not used after calling startParsing
  Utils::FilePath m_baseDir;
  QFutureWatcher<void> m_watcher;
  Tree *m_rootForFuture = nullptr;
  int m_futureCount = 0;
};

class PROJECTEXPLORER_EXPORT SelectableFilesWidget : public QWidget {
  Q_OBJECT

public:
  explicit SelectableFilesWidget(QWidget *parent = nullptr);
  SelectableFilesWidget(const Utils::FilePath &path, const Utils::FilePaths &files, QWidget *parent = nullptr);

  auto setAddFileFilter(const QString &filter) -> void;
  auto setBaseDirEditable(bool edit) -> void;
  auto selectedFiles() const -> Utils::FilePaths;
  auto selectedPaths() const -> Utils::FilePaths;
  auto hasFilesSelected() const -> bool;
  auto resetModel(const Utils::FilePath &path, const Utils::FilePaths &files) -> void;
  auto cancelParsing() -> void;
  auto enableFilterHistoryCompletion(const QString &keyPrefix) -> void;

signals:
  auto selectedFilesChanged() -> void;

private:
  auto enableWidgets(bool enabled) -> void;
  auto applyFilter() -> void;
  auto baseDirectoryChanged(bool validState) -> void;
  auto startParsing(const Utils::FilePath &baseDir) -> void;
  auto parsingProgress(const Utils::FilePath &fileName) -> void;
  auto parsingFinished() -> void;
  auto smartExpand(const QModelIndex &idx) -> void;

  SelectableFilesFromDirModel *m_model = nullptr;
  Utils::PathChooser *m_baseDirChooser;
  QLabel *m_baseDirLabel;
  QPushButton *m_startParsingButton;
  QLabel *m_selectFilesFilterLabel;
  Utils::FancyLineEdit *m_selectFilesFilterEdit;
  QLabel *m_hideFilesFilterLabel;
  Utils::FancyLineEdit *m_hideFilesFilterEdit;
  QPushButton *m_applyFiltersButton;
  QTreeView *m_view;
  QLabel *m_preservedFilesLabel;
  QLabel *m_progressLabel;
  bool m_filteringScheduled = false;
};

class PROJECTEXPLORER_EXPORT SelectableFilesDialogEditFiles : public QDialog {
  Q_OBJECT

public:
  SelectableFilesDialogEditFiles(const Utils::FilePath &path, const Utils::FilePaths &files, QWidget *parent);

  auto selectedFiles() const -> Utils::FilePaths;
  auto setAddFileFilter(const QString &filter) -> void { m_filesWidget->setAddFileFilter(filter); }

protected:
  SelectableFilesWidget *m_filesWidget;
};

class SelectableFilesDialogAddDirectory : public SelectableFilesDialogEditFiles {
  Q_OBJECT

public:
  SelectableFilesDialogAddDirectory(const Utils::FilePath &path, const Utils::FilePaths &files, QWidget *parent);
};

} // namespace ProjectExplorer
