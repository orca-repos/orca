// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodemodelinspectordialog.hpp"
#include "ui_cppcodemodelinspectordialog.h"

#include "baseeditordocumentprocessor.hpp"
#include "cppcodemodelinspectordumper.hpp"
#include "cppeditorwidget.hpp"
#include "cppeditordocument.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"
#include "cppworkingcopy.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/icore.hpp>
#include <projectexplorer/projectmacro.hpp>
#include <projectexplorer/project.hpp>

#include <cplusplus/CppDocument.h>
#include <cplusplus/Overview.h>
#include <cplusplus/Token.h>
#include <utils/qtcassert.hpp>
#include <utils/fancylineedit.hpp>

#include <QAbstractTableModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>

#include <algorithm>
#include <numeric>

using namespace CPlusPlus;
namespace CMI = CppEditor::CppCodeModelInspector;

namespace {

template <class T>
auto resizeColumns(QTreeView *view) -> void
{
  for (auto column = 0; column < T::ColumnCount - 1; ++column)
    view->resizeColumnToContents(column);
}

auto currentEditor() -> TextEditor::BaseTextEditor*
{
  return qobject_cast<TextEditor::BaseTextEditor*>(Core::EditorManager::currentEditor());
}

auto fileInCurrentEditor() -> QString
{
  if (auto editor = currentEditor())
    return editor->document()->filePath().toString();
  return QString();
}

auto sizePolicyWithStretchFactor(int stretchFactor) -> QSizePolicy
{
  QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  policy.setHorizontalStretch(stretchFactor);
  return policy;
}

class DepthFinder : public SymbolVisitor {
public:
  auto operator()(const Document::Ptr &document, Symbol *symbol) -> int
  {
    m_symbol = symbol;
    accept(document->globalNamespace());
    return m_foundDepth;
  }

  auto preVisit(Symbol *symbol) -> bool override
  {
    if (m_stop)
      return false;

    if (symbol->asScope()) {
      ++m_depth;
      if (symbol == m_symbol) {
        m_foundDepth = m_depth;
        m_stop = true;
      }
      return true;
    }

    return false;
  }

  auto postVisit(Symbol *symbol) -> void override
  {
    if (symbol->asScope())
      --m_depth;
  }

private:
  Symbol *m_symbol = nullptr;
  int m_depth = -1;
  int m_foundDepth = -1;
  bool m_stop = false;
};

} // anonymous namespace

namespace CppEditor {
namespace Internal {

class FilterableView : public QWidget {
  Q_OBJECT

public:
  FilterableView(QWidget *parent);

  auto setModel(QAbstractItemModel *model) -> void;
  auto selectionModel() const -> QItemSelectionModel*;
  auto selectIndex(const QModelIndex &index) -> void;
  auto resizeColumns(int columnCount) -> void;
  auto clearFilter() -> void;

signals:
  auto filterChanged(const QString &filterText) -> void;

private:
  QTreeView *view;
  Utils::FancyLineEdit *lineEdit;
};

FilterableView::FilterableView(QWidget *parent) : QWidget(parent)
{
  view = new QTreeView(this);
  view->setAlternatingRowColors(true);
  view->setTextElideMode(Qt::ElideMiddle);
  view->setSortingEnabled(true);

  lineEdit = new Utils::FancyLineEdit(this);
  lineEdit->setFiltering(true);
  lineEdit->setPlaceholderText(QLatin1String("File Path"));
  QObject::connect(lineEdit, &QLineEdit::textChanged, this, &FilterableView::filterChanged);

  auto label = new QLabel(QLatin1String("&Filter:"), this);
  label->setBuddy(lineEdit);

  auto filterBarLayout = new QHBoxLayout();
  filterBarLayout->addWidget(label);
  filterBarLayout->addWidget(lineEdit);

  auto mainLayout = new QVBoxLayout();
  mainLayout->addWidget(view);
  mainLayout->addLayout(filterBarLayout);

  setLayout(mainLayout);
}

auto FilterableView::setModel(QAbstractItemModel *model) -> void
{
  view->setModel(model);
}

auto FilterableView::selectionModel() const -> QItemSelectionModel*
{
  return view->selectionModel();
}

auto FilterableView::selectIndex(const QModelIndex &index) -> void
{
  if (index.isValid()) {
    view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  }
}

auto FilterableView::resizeColumns(int columnCount) -> void
{
  for (auto column = 0; column < columnCount - 1; ++column)
    view->resizeColumnToContents(column);
}

auto FilterableView::clearFilter() -> void
{
  lineEdit->clear();
}

class ProjectFilesModel : public QAbstractListModel {
  Q_OBJECT

public:
  ProjectFilesModel(QObject *parent);

  auto configure(const ProjectFiles &files) -> void;
  auto clear() -> void;

  enum Columns {
    FileKindColumn,
    FilePathColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  ProjectFiles m_files;
};

ProjectFilesModel::ProjectFilesModel(QObject *parent) : QAbstractListModel(parent) {}

auto ProjectFilesModel::configure(const ProjectFiles &files) -> void
{
  emit layoutAboutToBeChanged();
  m_files = files;
  emit layoutChanged();
}

auto ProjectFilesModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_files.clear();
  emit layoutChanged();
}

auto ProjectFilesModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_files.size();
}

auto ProjectFilesModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return ProjectFilesModel::ColumnCount;
}

auto ProjectFilesModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role == Qt::DisplayRole) {
    const auto row = index.row();
    const auto column = index.column();
    if (column == FileKindColumn) {
      return CMI::Utils::toString(m_files.at(row).kind);
    } else if (column == FilePathColumn) {
      return m_files.at(row).path;
    }
  } else if (role == Qt::ForegroundRole) {
    if (!m_files.at(index.row()).active) {
      return QApplication::palette().color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Text);
    }
  }
  return QVariant();
}

auto ProjectFilesModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case FileKindColumn:
      return QLatin1String("File Kind");
    case FilePathColumn:
      return QLatin1String("File Path");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- ProjectHeaderPathModel --------------------------------------------------------------------

class ProjectHeaderPathsModel : public QAbstractListModel {
  Q_OBJECT public:
  ProjectHeaderPathsModel(QObject *parent);
  auto configure(const ProjectExplorer::HeaderPaths &paths) -> void;
  auto clear() -> void;

  enum Columns {
    TypeColumn,
    PathColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  ProjectExplorer::HeaderPaths m_paths;
};

ProjectHeaderPathsModel::ProjectHeaderPathsModel(QObject *parent) : QAbstractListModel(parent) {}

auto ProjectHeaderPathsModel::configure(const ProjectExplorer::HeaderPaths &paths) -> void
{
  emit layoutAboutToBeChanged();
  m_paths = paths;
  emit layoutChanged();
}

auto ProjectHeaderPathsModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_paths.clear();
  emit layoutChanged();
}

auto ProjectHeaderPathsModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_paths.size();
}

auto ProjectHeaderPathsModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return ProjectFilesModel::ColumnCount;
}

auto ProjectHeaderPathsModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role == Qt::DisplayRole) {
    const auto row = index.row();
    const auto column = index.column();
    if (column == TypeColumn) {
      return CMI::Utils::toString(m_paths.at(row).type);
    } else if (column == PathColumn) {
      return m_paths.at(row).path;
    }
  }
  return QVariant();
}

auto ProjectHeaderPathsModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case TypeColumn:
      return QLatin1String("Type");
    case PathColumn:
      return QLatin1String("Path");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- KeyValueModel ------------------------------------------------------------------------------

class KeyValueModel : public QAbstractListModel {
  Q_OBJECT public:
  using Table = QList<QPair<QString, QString>>;

  KeyValueModel(QObject *parent);
  auto configure(const Table &table) -> void;
  auto clear() -> void;

  enum Columns {
    KeyColumn,
    ValueColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  Table m_table;
};

KeyValueModel::KeyValueModel(QObject *parent) : QAbstractListModel(parent) {}

auto KeyValueModel::configure(const Table &table) -> void
{
  emit layoutAboutToBeChanged();
  m_table = table;
  emit layoutChanged();
}

auto KeyValueModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_table.clear();
  emit layoutChanged();
}

auto KeyValueModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_table.size();
}

auto KeyValueModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return KeyValueModel::ColumnCount;
}

auto KeyValueModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role == Qt::DisplayRole) {
    const auto row = index.row();
    const auto column = index.column();
    if (column == KeyColumn) {
      return m_table.at(row).first;
    } else if (column == ValueColumn) {
      return m_table.at(row).second;
    }
  }
  return QVariant();
}

auto KeyValueModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case KeyColumn:
      return QLatin1String("Key");
    case ValueColumn:
      return QLatin1String("Value");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- SnapshotModel ------------------------------------------------------------------------------

class SnapshotModel : public QAbstractListModel {
  Q_OBJECT public:
  SnapshotModel(QObject *parent);
  auto configure(const Snapshot &snapshot) -> void;
  auto setGlobalSnapshot(const Snapshot &snapshot) -> void;

  auto indexForDocument(const QString &filePath) -> QModelIndex;

  enum Columns {
    SymbolCountColumn,
    SharedColumn,
    FilePathColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  QList<Document::Ptr> m_documents;
  Snapshot m_globalSnapshot;
};

SnapshotModel::SnapshotModel(QObject *parent) : QAbstractListModel(parent) {}

auto SnapshotModel::configure(const Snapshot &snapshot) -> void
{
  emit layoutAboutToBeChanged();
  m_documents = CMI::Utils::snapshotToList(snapshot);
  emit layoutChanged();
}

auto SnapshotModel::setGlobalSnapshot(const Snapshot &snapshot) -> void
{
  m_globalSnapshot = snapshot;
}

auto SnapshotModel::indexForDocument(const QString &filePath) -> QModelIndex
{
  for (int i = 0, total = m_documents.size(); i < total; ++i) {
    const Document::Ptr document = m_documents.at(i);
    if (document->fileName() == filePath)
      return index(i, FilePathColumn);
  }
  return {};
}

auto SnapshotModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_documents.size();
}

auto SnapshotModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return SnapshotModel::ColumnCount;
}

auto SnapshotModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role == Qt::DisplayRole) {
    const auto column = index.column();
    Document::Ptr document = m_documents.at(index.row());
    if (column == SymbolCountColumn) {
      return document->control()->symbolCount();
    } else if (column == SharedColumn) {
      Document::Ptr globalDocument = m_globalSnapshot.document(document->fileName());
      const bool isShared = globalDocument && globalDocument->fingerprint() == document->fingerprint();
      return CMI::Utils::toString(isShared);
    } else if (column == FilePathColumn) {
      return QDir::toNativeSeparators(document->fileName());
    }
  }
  return QVariant();
}

auto SnapshotModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case SymbolCountColumn:
      return QLatin1String("Symbols");
    case SharedColumn:
      return QLatin1String("Shared");
    case FilePathColumn:
      return QLatin1String("File Path");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- IncludesModel ------------------------------------------------------------------------------

static auto includesSorter(const Document::Include &i1, const Document::Include &i2) -> bool
{
  return i1.line() < i2.line();
}

class IncludesModel : public QAbstractListModel {
  Q_OBJECT public:
  IncludesModel(QObject *parent);
  auto configure(const QList<Document::Include> &includes) -> void;
  auto clear() -> void;

  enum Columns {
    ResolvedOrNotColumn,
    LineNumberColumn,
    FilePathsColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  QList<Document::Include> m_includes;
};

IncludesModel::IncludesModel(QObject *parent) : QAbstractListModel(parent) {}

auto IncludesModel::configure(const QList<Document::Include> &includes) -> void
{
  emit layoutAboutToBeChanged();
  m_includes = includes;
  std::stable_sort(m_includes.begin(), m_includes.end(), includesSorter);
  emit layoutChanged();
}

auto IncludesModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_includes.clear();
  emit layoutChanged();
}

auto IncludesModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_includes.size();
}

auto IncludesModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return IncludesModel::ColumnCount;
}

auto IncludesModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role != Qt::DisplayRole && role != Qt::ForegroundRole)
    return QVariant();

  static const QBrush greenBrush(QColor(0, 139, 69));
  static const QBrush redBrush(QColor(205, 38, 38));

  const Document::Include include = m_includes.at(index.row());
  const QString resolvedFileName = QDir::toNativeSeparators(include.resolvedFileName());
  const bool isResolved = !resolvedFileName.isEmpty();

  if (role == Qt::DisplayRole) {
    const auto column = index.column();
    if (column == ResolvedOrNotColumn) {
      return CMI::Utils::toString(isResolved);
    } else if (column == LineNumberColumn) {
      return include.line();
    } else if (column == FilePathsColumn) {
      return QVariant(CMI::Utils::unresolvedFileNameWithDelimiters(include) + QLatin1String(" --> ") + resolvedFileName);
    }
  } else if (role == Qt::ForegroundRole) {
    return isResolved ? greenBrush : redBrush;
  }

  return QVariant();
}

auto IncludesModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case ResolvedOrNotColumn:
      return QLatin1String("Resolved");
    case LineNumberColumn:
      return QLatin1String("Line");
    case FilePathsColumn:
      return QLatin1String("File Paths");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- DiagnosticMessagesModel --------------------------------------------------------------------

static auto diagnosticMessagesModelSorter(const Document::DiagnosticMessage &m1, const Document::DiagnosticMessage &m2) -> bool
{
  return m1.line() < m2.line();
}

class DiagnosticMessagesModel : public QAbstractListModel {
  Q_OBJECT public:
  DiagnosticMessagesModel(QObject *parent);
  auto configure(const QList<Document::DiagnosticMessage> &messages) -> void;
  auto clear() -> void;

  enum Columns {
    LevelColumn,
    LineColumnNumberColumn,
    MessageColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  QList<Document::DiagnosticMessage> m_messages;
};

DiagnosticMessagesModel::DiagnosticMessagesModel(QObject *parent) : QAbstractListModel(parent) {}

auto DiagnosticMessagesModel::configure(const QList<Document::DiagnosticMessage> &messages) -> void
{
  emit layoutAboutToBeChanged();
  m_messages = messages;
  std::stable_sort(m_messages.begin(), m_messages.end(), diagnosticMessagesModelSorter);
  emit layoutChanged();
}

auto DiagnosticMessagesModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_messages.clear();
  emit layoutChanged();
}

auto DiagnosticMessagesModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_messages.size();
}

auto DiagnosticMessagesModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return DiagnosticMessagesModel::ColumnCount;
}

auto DiagnosticMessagesModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role != Qt::DisplayRole && role != Qt::ForegroundRole)
    return QVariant();

  static const QBrush yellowOrangeBrush(QColor(237, 145, 33));
  static const QBrush redBrush(QColor(205, 38, 38));
  static const QBrush darkRedBrushQColor(QColor(139, 0, 0));

  const Document::DiagnosticMessage message = m_messages.at(index.row());
  const auto level = static_cast<Document::DiagnosticMessage::Level>(message.level());

  if (role == Qt::DisplayRole) {
    const auto column = index.column();
    if (column == LevelColumn) {
      return CMI::Utils::toString(level);
    } else if (column == LineColumnNumberColumn) {
      return QVariant(QString::number(message.line()) + QLatin1Char(':') + QString::number(message.column()));
    } else if (column == MessageColumn) {
      return message.text();
    }
  } else if (role == Qt::ForegroundRole) {
    switch (level) {
    case Document::DiagnosticMessage::Warning:
      return yellowOrangeBrush;
    case Document::DiagnosticMessage::Error:
      return redBrush;
    case Document::DiagnosticMessage::Fatal:
      return darkRedBrushQColor;
    default:
      return QVariant();
    }
  }

  return QVariant();
}

auto DiagnosticMessagesModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case LevelColumn:
      return QLatin1String("Level");
    case LineColumnNumberColumn:
      return QLatin1String("Line:Column");
    case MessageColumn:
      return QLatin1String("Message");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- MacrosModel --------------------------------------------------------------------------------

class MacrosModel : public QAbstractListModel {
  Q_OBJECT public:
  MacrosModel(QObject *parent);
  auto configure(const QList<CPlusPlus::Macro> &macros) -> void;
  auto clear() -> void;

  enum Columns {
    LineNumberColumn,
    MacroColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  QList<CPlusPlus::Macro> m_macros;
};

MacrosModel::MacrosModel(QObject *parent) : QAbstractListModel(parent) {}

auto MacrosModel::configure(const QList<CPlusPlus::Macro> &macros) -> void
{
  emit layoutAboutToBeChanged();
  m_macros = macros;
  emit layoutChanged();
}

auto MacrosModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_macros.clear();
  emit layoutChanged();
}

auto MacrosModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_macros.size();
}

auto MacrosModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return MacrosModel::ColumnCount;
}

auto MacrosModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto column = index.column();
  if (role == Qt::DisplayRole || (role == Qt::ToolTipRole && column == MacroColumn)) {
    const auto macro = m_macros.at(index.row());
    if (column == LineNumberColumn)
      return macro.line();
    else if (column == MacroColumn)
      return macro.toString();
  } else if (role == Qt::TextAlignmentRole) {
    return QVariant::fromValue(Qt::AlignTop | Qt::AlignLeft);
  }
  return QVariant();
}

auto MacrosModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case LineNumberColumn:
      return QLatin1String("Line");
    case MacroColumn:
      return QLatin1String("Macro");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- SymbolsModel -------------------------------------------------------------------------------

class SymbolsModel : public QAbstractItemModel {
  Q_OBJECT public:
  SymbolsModel(QObject *parent);
  auto configure(const Document::Ptr &document) -> void;
  auto clear() -> void;

  enum Columns {
    SymbolColumn,
    LineNumberColumn,
    ColumnCount
  };

  auto index(int row, int column, const QModelIndex &parent) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  Document::Ptr m_document;
};

SymbolsModel::SymbolsModel(QObject *parent) : QAbstractItemModel(parent) {}

auto SymbolsModel::configure(const Document::Ptr &document) -> void
{
  QTC_CHECK(document);
  emit layoutAboutToBeChanged();
  m_document = document;
  emit layoutChanged();
}

auto SymbolsModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_document.clear();
  emit layoutChanged();
}

static auto indexToSymbol(const QModelIndex &index) -> Symbol*
{
  return static_cast<Symbol*>(index.internalPointer());
}

static auto indexToScope(const QModelIndex &index) -> Scope*
{
  if (auto symbol = indexToSymbol(index))
    return symbol->asScope();
  return nullptr;
}

auto SymbolsModel::index(int row, int column, const QModelIndex &parent) const -> QModelIndex
{
  Scope *scope = nullptr;
  if (parent.isValid())
    scope = indexToScope(parent);
  else if (m_document)
    scope = m_document->globalNamespace();

  if (scope) {
    if (row < scope->memberCount())
      return createIndex(row, column, scope->memberAt(row));
  }

  return {};
}

auto SymbolsModel::parent(const QModelIndex &child) const -> QModelIndex
{
  if (!child.isValid())
    return {};

  if (auto symbol = indexToSymbol(child)) {
    if (Scope *scope = symbol->enclosingScope()) {
      const int row = DepthFinder()(m_document, scope);
      return createIndex(row, 0, scope);
    }
  }

  return {};
}

auto SymbolsModel::rowCount(const QModelIndex &parent) const -> int
{
  if (parent.isValid()) {
    if (Scope *scope = indexToScope(parent))
      return scope->memberCount();
  } else {
    if (m_document)
      return m_document->globalNamespace()->memberCount();
  }
  return 0;
}

auto SymbolsModel::columnCount(const QModelIndex &) const -> int
{
  return ColumnCount;
}

auto SymbolsModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto column = index.column();
  if (role == Qt::DisplayRole) {
    auto symbol = indexToSymbol(index);
    if (!symbol)
      return QVariant();
    if (column == LineNumberColumn) {
      return symbol->line();
    } else if (column == SymbolColumn) {
      QString name = Overview().prettyName(symbol->name());
      if (name.isEmpty())
        name = QLatin1String(symbol->isBlock() ? "<block>" : "<no name>");
      return name;
    }
  }
  return QVariant();
}

auto SymbolsModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case SymbolColumn:
      return QLatin1String("Symbol");
    case LineNumberColumn:
      return QLatin1String("Line");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- TokensModel --------------------------------------------------------------------------------

class TokensModel : public QAbstractListModel {
  Q_OBJECT public:
  TokensModel(QObject *parent);
  auto configure(TranslationUnit *translationUnit) -> void;
  auto clear() -> void;

  enum Columns {
    SpelledColumn,
    KindColumn,
    IndexColumn,
    OffsetColumn,
    LineColumnNumberColumn,
    BytesAndCodePointsColumn,
    GeneratedColumn,
    ExpandedColumn,
    WhiteSpaceColumn,
    NewlineColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  struct TokenInfo {
    Token token;
    int line;
    int column;
  };

  QList<TokenInfo> m_tokenInfos;
};

TokensModel::TokensModel(QObject *parent) : QAbstractListModel(parent) {}

auto TokensModel::configure(TranslationUnit *translationUnit) -> void
{
  if (!translationUnit)
    return;

  emit layoutAboutToBeChanged();
  m_tokenInfos.clear();
  for (int i = 0, total = translationUnit->tokenCount(); i < total; ++i) {
    TokenInfo info;
    info.token = translationUnit->tokenAt(i);
    translationUnit->getPosition(info.token.utf16charsBegin(), &info.line, &info.column);
    m_tokenInfos.append(info);
  }
  emit layoutChanged();
}

auto TokensModel::clear() -> void
{
  emit layoutAboutToBeChanged();
  m_tokenInfos.clear();
  emit layoutChanged();
}

auto TokensModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_tokenInfos.size();
}

auto TokensModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return TokensModel::ColumnCount;
}

auto TokensModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto column = index.column();
  if (role == Qt::DisplayRole) {
    const auto info = m_tokenInfos.at(index.row());
    const Token token = info.token;
    if (column == SpelledColumn)
      return QString::fromUtf8(token.spell());
    else if (column == KindColumn)
      return CMI::Utils::toString(static_cast<Kind>(token.kind()));
    else if (column == IndexColumn)
      return index.row();
    else if (column == OffsetColumn)
      return token.bytesBegin();
    else if (column == LineColumnNumberColumn)
      return QString::fromLatin1("%1:%2").arg(CMI::Utils::toString(info.line), CMI::Utils::toString(info.column));
    else if (column == BytesAndCodePointsColumn)
      return QString::fromLatin1("%1/%2").arg(CMI::Utils::toString(token.bytes()), CMI::Utils::toString(token.utf16chars()));
    else if (column == GeneratedColumn)
      return CMI::Utils::toString(token.generated());
    else if (column == ExpandedColumn)
      return CMI::Utils::toString(token.expanded());
    else if (column == WhiteSpaceColumn)
      return CMI::Utils::toString(token.whitespace());
    else if (column == NewlineColumn)
      return CMI::Utils::toString(token.newline());
  } else if (role == Qt::TextAlignmentRole) {
    return QVariant::fromValue(Qt::AlignTop | Qt::AlignLeft);
  }
  return QVariant();
}

auto TokensModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case SpelledColumn:
      return QLatin1String("Spelled");
    case KindColumn:
      return QLatin1String("Kind");
    case IndexColumn:
      return QLatin1String("Index");
    case OffsetColumn:
      return QLatin1String("Offset");
    case LineColumnNumberColumn:
      return QLatin1String("Line:Column");
    case BytesAndCodePointsColumn:
      return QLatin1String("Bytes/Codepoints");
    case GeneratedColumn:
      return QLatin1String("Generated");
    case ExpandedColumn:
      return QLatin1String("Expanded");
    case WhiteSpaceColumn:
      return QLatin1String("Whitespace");
    case NewlineColumn:
      return QLatin1String("Newline");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- ProjectPartsModel --------------------------------------------------------------------------

class ProjectPartsModel : public QAbstractListModel {
  Q_OBJECT public:
  ProjectPartsModel(QObject *parent);

  auto configure(const QList<ProjectInfo::ConstPtr> &projectInfos, const ProjectPart::ConstPtr &currentEditorsProjectPart) -> void;

  auto indexForCurrentEditorsProjectPart() const -> QModelIndex;
  auto projectPartForProjectId(const QString &projectPartId) const -> ProjectPart::ConstPtr;

  enum Columns {
    PartNameColumn,
    PartFilePathColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  QList<ProjectPart::ConstPtr> m_projectPartsList;
  int m_currentEditorsProjectPartIndex;
};

ProjectPartsModel::ProjectPartsModel(QObject *parent) : QAbstractListModel(parent), m_currentEditorsProjectPartIndex(-1) {}

auto ProjectPartsModel::configure(const QList<ProjectInfo::ConstPtr> &projectInfos, const ProjectPart::ConstPtr &currentEditorsProjectPart) -> void
{
  emit layoutAboutToBeChanged();
  m_projectPartsList.clear();
  foreach(const ProjectInfo::ConstPtr &info, projectInfos) {
    foreach(const ProjectPart::ConstPtr &projectPart, info->projectParts()) {
      if (!m_projectPartsList.contains(projectPart)) {
        m_projectPartsList << projectPart;
        if (projectPart == currentEditorsProjectPart)
          m_currentEditorsProjectPartIndex = m_projectPartsList.size() - 1;
      }
    }
  }
  emit layoutChanged();
}

auto ProjectPartsModel::indexForCurrentEditorsProjectPart() const -> QModelIndex
{
  if (m_currentEditorsProjectPartIndex == -1)
    return {};
  return createIndex(m_currentEditorsProjectPartIndex, PartFilePathColumn);
}

auto ProjectPartsModel::projectPartForProjectId(const QString &projectPartId) const -> ProjectPart::ConstPtr
{
  foreach(const ProjectPart::ConstPtr &part, m_projectPartsList) {
    if (part->id() == projectPartId)
      return part;
  }
  return ProjectPart::ConstPtr();
}

auto ProjectPartsModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_projectPartsList.size();
}

auto ProjectPartsModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return ProjectPartsModel::ColumnCount;
}

auto ProjectPartsModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto row = index.row();
  if (role == Qt::DisplayRole) {
    const auto column = index.column();
    if (column == PartNameColumn)
      return m_projectPartsList.at(row)->displayName;
    else if (column == PartFilePathColumn)
      return QDir::toNativeSeparators(m_projectPartsList.at(row)->projectFile);
  } else if (role == Qt::ForegroundRole) {
    if (!m_projectPartsList.at(row)->selectedForBuilding) {
      return QApplication::palette().color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Text);
    }
  } else if (role == Qt::UserRole) {
    return m_projectPartsList.at(row)->id();
  }
  return QVariant();
}

auto ProjectPartsModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case PartNameColumn:
      return QLatin1String("Name");
    case PartFilePathColumn:
      return QLatin1String("Project File Path");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- WorkingCopyModel ---------------------------------------------------------------------------

class WorkingCopyModel : public QAbstractListModel {
  Q_OBJECT public:
  WorkingCopyModel(QObject *parent);

  auto configure(const WorkingCopy &workingCopy) -> void;
  auto indexForFile(const QString &filePath) -> QModelIndex;

  enum Columns {
    RevisionColumn,
    FilePathColumn,
    ColumnCount
  };

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

private:
  struct WorkingCopyEntry {
    WorkingCopyEntry(const QString &filePath, const QByteArray &source, unsigned revision) : filePath(filePath), source(source), revision(revision) {}

    QString filePath;
    QByteArray source;
    unsigned revision;
  };

  QList<WorkingCopyEntry> m_workingCopyList;
};

WorkingCopyModel::WorkingCopyModel(QObject *parent) : QAbstractListModel(parent) {}

auto WorkingCopyModel::configure(const WorkingCopy &workingCopy) -> void
{
  emit layoutAboutToBeChanged();
  m_workingCopyList.clear();
  const auto &elements = workingCopy.elements();
  for (auto it = elements.cbegin(), end = elements.cend(); it != end; ++it) {
    m_workingCopyList << WorkingCopyEntry(it.key().toString(), it.value().first, it.value().second);
  }
  emit layoutChanged();
}

auto WorkingCopyModel::indexForFile(const QString &filePath) -> QModelIndex
{
  for (int i = 0, total = m_workingCopyList.size(); i < total; ++i) {
    const auto entry = m_workingCopyList.at(i);
    if (entry.filePath == filePath)
      return index(i, FilePathColumn);
  }
  return {};
}

auto WorkingCopyModel::rowCount(const QModelIndex &/*parent*/) const -> int
{
  return m_workingCopyList.size();
}

auto WorkingCopyModel::columnCount(const QModelIndex &/*parent*/) const -> int
{
  return WorkingCopyModel::ColumnCount;
}

auto WorkingCopyModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto row = index.row();
  if (role == Qt::DisplayRole) {
    const auto column = index.column();
    if (column == RevisionColumn)
      return m_workingCopyList.at(row).revision;
    else if (column == FilePathColumn)
      return m_workingCopyList.at(row).filePath;
  } else if (role == Qt::UserRole) {
    return m_workingCopyList.at(row).source;
  }
  return QVariant();
}

auto WorkingCopyModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case RevisionColumn:
      return QLatin1String("Revision");
    case FilePathColumn:
      return QLatin1String("File Path");
    default:
      return QVariant();
    }
  }
  return QVariant();
}

// --- SnapshotInfo -------------------------------------------------------------------------------

class SnapshotInfo {
public:
  enum Type {
    GlobalSnapshot,
    EditorSnapshot
  };

  SnapshotInfo(const Snapshot &snapshot, Type type) : snapshot(snapshot), type(type) {}

  Snapshot snapshot;
  Type type;
};

// --- CppCodeModelInspectorDialog ----------------------------------------------------------------

CppCodeModelInspectorDialog::CppCodeModelInspectorDialog(QWidget *parent) : QDialog(parent), m_ui(new Ui::CppCodeModelInspectorDialog), m_snapshotInfos(new QList<SnapshotInfo>()), m_snapshotView(new FilterableView(this)), m_snapshotModel(new SnapshotModel(this)), m_proxySnapshotModel(new QSortFilterProxyModel(this)), m_docGenericInfoModel(new KeyValueModel(this)), m_docIncludesModel(new IncludesModel(this)), m_docDiagnosticMessagesModel(new DiagnosticMessagesModel(this)), m_docMacrosModel(new MacrosModel(this)), m_docSymbolsModel(new SymbolsModel(this)), m_docTokensModel(new TokensModel(this)), m_projectPartsView(new FilterableView(this)), m_projectPartsModel(new ProjectPartsModel(this)), m_proxyProjectPartsModel(new QSortFilterProxyModel(this)), m_partGenericInfoModel(new KeyValueModel(this)), m_projectFilesModel(new ProjectFilesModel(this)), m_projectHeaderPathsModel(new ProjectHeaderPathsModel(this)), m_workingCopyView(new FilterableView(this)), m_workingCopyModel(new WorkingCopyModel(this)), m_proxyWorkingCopyModel(new QSortFilterProxyModel(this))
{
  m_ui->setupUi(this);
  m_ui->snapshotSelectorAndViewLayout->addWidget(m_snapshotView);
  m_ui->projectPartsSplitter->insertWidget(0, m_projectPartsView);
  m_ui->workingCopySplitter->insertWidget(0, m_workingCopyView);

  setAttribute(Qt::WA_DeleteOnClose);
  connect(Core::ICore::instance(), &Core::ICore::coreAboutToClose, this, &QWidget::close);

  m_ui->partGeneralView->setSizePolicy(sizePolicyWithStretchFactor(2));
  m_ui->partGeneralCompilerFlagsEdit->setSizePolicy(sizePolicyWithStretchFactor(1));

  m_proxySnapshotModel->setSourceModel(m_snapshotModel);
  m_proxySnapshotModel->setFilterKeyColumn(SnapshotModel::FilePathColumn);
  m_snapshotView->setModel(m_proxySnapshotModel);
  m_ui->docGeneralView->setModel(m_docGenericInfoModel);
  m_ui->docIncludesView->setModel(m_docIncludesModel);
  m_ui->docDiagnosticMessagesView->setModel(m_docDiagnosticMessagesModel);
  m_ui->docDefinedMacrosView->setModel(m_docMacrosModel);
  m_ui->docSymbolsView->setModel(m_docSymbolsModel);
  m_ui->docTokensView->setModel(m_docTokensModel);

  m_proxyProjectPartsModel->setSourceModel(m_projectPartsModel);
  m_proxyProjectPartsModel->setFilterKeyColumn(ProjectPartsModel::PartFilePathColumn);
  m_projectPartsView->setModel(m_proxyProjectPartsModel);
  m_ui->partGeneralView->setModel(m_partGenericInfoModel);
  m_ui->projectFilesView->setModel(m_projectFilesModel);
  m_ui->projectHeaderPathsView->setModel(m_projectHeaderPathsModel);

  m_proxyWorkingCopyModel->setSourceModel(m_workingCopyModel);
  m_proxyWorkingCopyModel->setFilterKeyColumn(WorkingCopyModel::FilePathColumn);
  m_workingCopyView->setModel(m_proxyWorkingCopyModel);

  connect(m_snapshotView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &CppCodeModelInspectorDialog::onDocumentSelected);
  connect(m_snapshotView, &FilterableView::filterChanged, this, &CppCodeModelInspectorDialog::onSnapshotFilterChanged);
  connect(m_ui->snapshotSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CppCodeModelInspectorDialog::onSnapshotSelected);
  connect(m_ui->docSymbolsView, &QTreeView::expanded, this, &CppCodeModelInspectorDialog::onSymbolsViewExpandedOrCollapsed);
  connect(m_ui->docSymbolsView, &QTreeView::collapsed, this, &CppCodeModelInspectorDialog::onSymbolsViewExpandedOrCollapsed);

  connect(m_projectPartsView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &CppCodeModelInspectorDialog::onProjectPartSelected);
  connect(m_projectPartsView, &FilterableView::filterChanged, this, &CppCodeModelInspectorDialog::onProjectPartFilterChanged);

  connect(m_workingCopyView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &CppCodeModelInspectorDialog::onWorkingCopyDocumentSelected);
  connect(m_workingCopyView, &FilterableView::filterChanged, this, &CppCodeModelInspectorDialog::onWorkingCopyFilterChanged);

  connect(m_ui->refreshButton, &QAbstractButton::clicked, this, &CppCodeModelInspectorDialog::onRefreshRequested);
  connect(m_ui->closeButton, &QAbstractButton::clicked, this, &QWidget::close);

  refresh();
}

CppCodeModelInspectorDialog::~CppCodeModelInspectorDialog()
{
  delete m_snapshotInfos;
  delete m_ui;
}

auto CppCodeModelInspectorDialog::onRefreshRequested() -> void
{
  refresh();
}

auto CppCodeModelInspectorDialog::onSnapshotFilterChanged(const QString &pattern) -> void
{
  m_proxySnapshotModel->setFilterWildcard(pattern);
}

auto CppCodeModelInspectorDialog::onSnapshotSelected(int row) -> void
{
  if (row < 0 || row >= m_snapshotInfos->size())
    return;

  m_snapshotView->clearFilter();
  const auto info = m_snapshotInfos->at(row);
  m_snapshotModel->configure(info.snapshot);
  m_snapshotView->resizeColumns(SnapshotModel::ColumnCount);

  if (info.type == SnapshotInfo::GlobalSnapshot) {
    // Select first document
    const auto index = m_proxySnapshotModel->index(0, SnapshotModel::FilePathColumn);
    m_snapshotView->selectIndex(index);
  } else if (info.type == SnapshotInfo::EditorSnapshot) {
    // Select first document, unless we can find the editor document
    auto index = m_snapshotModel->indexForDocument(fileInCurrentEditor());
    index = m_proxySnapshotModel->mapFromSource(index);
    if (!index.isValid())
      index = m_proxySnapshotModel->index(0, SnapshotModel::FilePathColumn);
    m_snapshotView->selectIndex(index);
  }
}

auto CppCodeModelInspectorDialog::onDocumentSelected(const QModelIndex &current, const QModelIndex &) -> void
{
  if (current.isValid()) {
    const auto index = m_proxySnapshotModel->index(current.row(), SnapshotModel::FilePathColumn);
    const auto filePath = QDir::fromNativeSeparators(m_proxySnapshotModel->data(index, Qt::DisplayRole).toString());
    const auto info = m_snapshotInfos->at(m_ui->snapshotSelector->currentIndex());
    updateDocumentData(info.snapshot.document(filePath));
  } else {
    clearDocumentData();
  }
}

auto CppCodeModelInspectorDialog::onSymbolsViewExpandedOrCollapsed(const QModelIndex &) -> void
{
  resizeColumns<SymbolsModel>(m_ui->docSymbolsView);
}

auto CppCodeModelInspectorDialog::onProjectPartFilterChanged(const QString &pattern) -> void
{
  m_proxyProjectPartsModel->setFilterWildcard(pattern);
}

auto CppCodeModelInspectorDialog::onProjectPartSelected(const QModelIndex &current, const QModelIndex &) -> void
{
  if (current.isValid()) {
    auto index = m_proxyProjectPartsModel->mapToSource(current);
    if (index.isValid()) {
      index = m_projectPartsModel->index(index.row(), ProjectPartsModel::PartFilePathColumn);
      const auto projectPartId = m_projectPartsModel->data(index, Qt::UserRole).toString();
      updateProjectPartData(m_projectPartsModel->projectPartForProjectId(projectPartId));
    }
  } else {
    clearProjectPartData();
  }
}

auto CppCodeModelInspectorDialog::onWorkingCopyFilterChanged(const QString &pattern) -> void
{
  m_proxyWorkingCopyModel->setFilterWildcard(pattern);
}

auto CppCodeModelInspectorDialog::onWorkingCopyDocumentSelected(const QModelIndex &current, const QModelIndex &) -> void
{
  if (current.isValid()) {
    const auto index = m_proxyWorkingCopyModel->mapToSource(current);
    if (index.isValid()) {
      const auto source = QString::fromUtf8(m_workingCopyModel->data(index, Qt::UserRole).toByteArray());
      m_ui->workingCopySourceEdit->setPlainText(source);
    }
  } else {
    m_ui->workingCopySourceEdit->clear();
  }
}

auto CppCodeModelInspectorDialog::refresh() -> void
{
  auto cmmi = CppModelManager::instance();

  const int oldSnapshotIndex = m_ui->snapshotSelector->currentIndex();
  const bool selectEditorRelevant = m_ui->selectEditorRelevantEntriesAfterRefreshCheckBox->isChecked();

  // Snapshots and Documents
  m_snapshotInfos->clear();
  m_ui->snapshotSelector->clear();

  const Snapshot globalSnapshot = cmmi->snapshot();
  CppCodeModelInspector::Dumper dumper(globalSnapshot);
  m_snapshotModel->setGlobalSnapshot(globalSnapshot);

  m_snapshotInfos->append(SnapshotInfo(globalSnapshot, SnapshotInfo::GlobalSnapshot));
  const QString globalSnapshotTitle = QString::fromLatin1("Global/Indexing Snapshot (%1 Documents)").arg(globalSnapshot.size());
  m_ui->snapshotSelector->addItem(globalSnapshotTitle);
  dumper.dumpSnapshot(globalSnapshot, globalSnapshotTitle, /*isGlobalSnapshot=*/ true);

  auto editor = currentEditor();
  CppEditorDocumentHandle *cppEditorDocument = nullptr;
  if (editor) {
    const auto editorFilePath = editor->document()->filePath().toString();
    cppEditorDocument = cmmi->cppEditorDocument(editorFilePath);
    if (auto documentProcessor = CppModelManager::cppEditorDocumentProcessor(editorFilePath)) {
      const Snapshot editorSnapshot = documentProcessor->snapshot();
      m_snapshotInfos->append(SnapshotInfo(editorSnapshot, SnapshotInfo::EditorSnapshot));
      const QString editorSnapshotTitle = QString::fromLatin1("Current Editor's Snapshot (%1 Documents)").arg(editorSnapshot.size());
      dumper.dumpSnapshot(editorSnapshot, editorSnapshotTitle);
      m_ui->snapshotSelector->addItem(editorSnapshotTitle);
    }
    auto cppEditorWidget = qobject_cast<CppEditorWidget*>(editor->editorWidget());
    if (cppEditorWidget) {
      auto semanticInfo = cppEditorWidget->semanticInfo();
      Snapshot snapshot;

      // Add semantic info snapshot
      snapshot = semanticInfo.snapshot;
      m_snapshotInfos->append(SnapshotInfo(snapshot, SnapshotInfo::EditorSnapshot));
      m_ui->snapshotSelector->addItem(QString::fromLatin1("Current Editor's Semantic Info Snapshot (%1 Documents)").arg(snapshot.size()));

      // Add a pseudo snapshot containing only the semantic info document since this document
      // is not part of the semantic snapshot.
      snapshot = Snapshot();
      snapshot.insert(cppEditorWidget->semanticInfo().doc);
      m_snapshotInfos->append(SnapshotInfo(snapshot, SnapshotInfo::EditorSnapshot));
      const QString snapshotTitle = QString::fromLatin1("Current Editor's Pseudo Snapshot with Semantic Info Document (%1 Documents)").arg(snapshot.size());
      dumper.dumpSnapshot(snapshot, snapshotTitle);
      m_ui->snapshotSelector->addItem(snapshotTitle);
    }
  }

  auto snapshotIndex = 0;
  if (selectEditorRelevant) {
    for (int i = 0, total = m_snapshotInfos->size(); i < total; ++i) {
      const auto info = m_snapshotInfos->at(i);
      if (info.type == SnapshotInfo::EditorSnapshot) {
        snapshotIndex = i;
        break;
      }
    }
  } else if (oldSnapshotIndex < m_snapshotInfos->size()) {
    snapshotIndex = oldSnapshotIndex;
  }
  m_ui->snapshotSelector->setCurrentIndex(snapshotIndex);
  onSnapshotSelected(snapshotIndex);

  // Project Parts
  const auto editorsProjectPart = cppEditorDocument ? cppEditorDocument->processor()->parser()->projectPartInfo().projectPart : ProjectPart::ConstPtr();

  const auto projectInfos = cmmi->projectInfos();
  dumper.dumpProjectInfos(projectInfos);
  m_projectPartsModel->configure(projectInfos, editorsProjectPart);
  m_projectPartsView->resizeColumns(ProjectPartsModel::ColumnCount);
  auto index = m_proxyProjectPartsModel->index(0, ProjectPartsModel::PartFilePathColumn);
  if (index.isValid()) {
    if (selectEditorRelevant && editorsProjectPart) {
      auto editorPartIndex = m_projectPartsModel->indexForCurrentEditorsProjectPart();
      editorPartIndex = m_proxyProjectPartsModel->mapFromSource(editorPartIndex);
      if (editorPartIndex.isValid())
        index = editorPartIndex;
    }
    m_projectPartsView->selectIndex(index);
  }

  // Working Copy
  const auto workingCopy = cmmi->workingCopy();
  dumper.dumpWorkingCopy(workingCopy);
  m_workingCopyModel->configure(workingCopy);
  m_workingCopyView->resizeColumns(WorkingCopyModel::ColumnCount);
  if (workingCopy.size() > 0) {
    auto index = m_proxyWorkingCopyModel->index(0, WorkingCopyModel::FilePathColumn);
    if (selectEditorRelevant) {
      const auto eindex = m_workingCopyModel->indexForFile(fileInCurrentEditor());
      if (eindex.isValid())
        index = m_proxyWorkingCopyModel->mapFromSource(eindex);
    }
    m_workingCopyView->selectIndex(index);
  }

  // Merged entities
  dumper.dumpMergedEntities(cmmi->headerPaths(), ProjectExplorer::Macro::toByteArray(cmmi->definedMacros()));
}

enum DocumentTabs {
  DocumentGeneralTab,
  DocumentIncludesTab,
  DocumentDiagnosticsTab,
  DocumentDefinedMacrosTab,
  DocumentPreprocessedSourceTab,
  DocumentSymbolsTab,
  DocumentTokensTab
};

static auto docTabName(int tabIndex, int numberOfEntries = -1) -> QString
{
  const char *names[] = {"&General", "&Includes", "&Diagnostic Messages", "(Un)Defined &Macros", "P&reprocessed Source", "&Symbols", "&Tokens"};
  QString result = QLatin1String(names[tabIndex]);
  if (numberOfEntries != -1)
    result += QString::fromLatin1(" (%1)").arg(numberOfEntries);
  return result;
}

auto CppCodeModelInspectorDialog::clearDocumentData() -> void
{
  m_docGenericInfoModel->clear();

  m_ui->docTab->setTabText(DocumentIncludesTab, docTabName(DocumentIncludesTab));
  m_docIncludesModel->clear();

  m_ui->docTab->setTabText(DocumentDiagnosticsTab, docTabName(DocumentDiagnosticsTab));
  m_docDiagnosticMessagesModel->clear();

  m_ui->docTab->setTabText(DocumentDefinedMacrosTab, docTabName(DocumentDefinedMacrosTab));
  m_docMacrosModel->clear();

  m_ui->docPreprocessedSourceEdit->clear();

  m_docSymbolsModel->clear();

  m_ui->docTab->setTabText(DocumentTokensTab, docTabName(DocumentTokensTab));
  m_docTokensModel->clear();
}

auto CppCodeModelInspectorDialog::updateDocumentData(const Document::Ptr &document) -> void
{
  QTC_ASSERT(document, return);

  // General
  const KeyValueModel::Table table = {{QString::fromLatin1("File Path"), QDir::toNativeSeparators(document->fileName())}, {QString::fromLatin1("Last Modified"), CMI::Utils::toString(document->lastModified())}, {QString::fromLatin1("Revision"), CMI::Utils::toString(document->revision())}, {QString::fromLatin1("Editor Revision"), CMI::Utils::toString(document->editorRevision())}, {QString::fromLatin1("Check Mode"), CMI::Utils::toString(document->checkMode())}, {QString::fromLatin1("Tokenized"), CMI::Utils::toString(document->isTokenized())}, {QString::fromLatin1("Parsed"), CMI::Utils::toString(document->isParsed())}, {QString::fromLatin1("Project Parts"), CMI::Utils::partsForFile(document->fileName())}};
  m_docGenericInfoModel->configure(table);
  resizeColumns<KeyValueModel>(m_ui->docGeneralView);

  // Includes
  m_docIncludesModel->configure(document->resolvedIncludes() + document->unresolvedIncludes());
  resizeColumns<IncludesModel>(m_ui->docIncludesView);
  m_ui->docTab->setTabText(DocumentIncludesTab, docTabName(DocumentIncludesTab, m_docIncludesModel->rowCount()));

  // Diagnostic Messages
  m_docDiagnosticMessagesModel->configure(document->diagnosticMessages());
  resizeColumns<DiagnosticMessagesModel>(m_ui->docDiagnosticMessagesView);
  m_ui->docTab->setTabText(DocumentDiagnosticsTab, docTabName(DocumentDiagnosticsTab, m_docDiagnosticMessagesModel->rowCount()));

  // Macros
  m_docMacrosModel->configure(document->definedMacros());
  resizeColumns<MacrosModel>(m_ui->docDefinedMacrosView);
  m_ui->docTab->setTabText(DocumentDefinedMacrosTab, docTabName(DocumentDefinedMacrosTab, m_docMacrosModel->rowCount()));

  // Source
  m_ui->docPreprocessedSourceEdit->setPlainText(QString::fromUtf8(document->utf8Source()));

  // Symbols
  m_docSymbolsModel->configure(document);
  resizeColumns<SymbolsModel>(m_ui->docSymbolsView);

  // Tokens
  m_docTokensModel->configure(document->translationUnit());
  resizeColumns<TokensModel>(m_ui->docTokensView);
  m_ui->docTab->setTabText(DocumentTokensTab, docTabName(DocumentTokensTab, m_docTokensModel->rowCount()));
}

enum ProjectPartTabs {
  ProjectPartGeneralTab,
  ProjectPartFilesTab,
  ProjectPartDefinesTab,
  ProjectPartHeaderPathsTab,
  ProjectPartPrecompiledHeadersTab
};

static auto partTabName(int tabIndex, int numberOfEntries = -1) -> QString
{
  const char *names[] = {"&General", "Project &Files", "&Defines", "&Header Paths", "Pre&compiled Headers"};
  QString result = QLatin1String(names[tabIndex]);
  if (numberOfEntries != -1)
    result += QString::fromLatin1(" (%1)").arg(numberOfEntries);
  return result;
}

auto CppCodeModelInspectorDialog::clearProjectPartData() -> void
{
  m_partGenericInfoModel->clear();
  m_projectFilesModel->clear();
  m_projectHeaderPathsModel->clear();

  m_ui->projectPartTab->setTabText(ProjectPartFilesTab, partTabName(ProjectPartFilesTab));

  m_ui->partToolchainDefinesEdit->clear();
  m_ui->partProjectDefinesEdit->clear();
  m_ui->projectPartTab->setTabText(ProjectPartDefinesTab, partTabName(ProjectPartDefinesTab));

  m_ui->projectPartTab->setTabText(ProjectPartHeaderPathsTab, partTabName(ProjectPartHeaderPathsTab));

  m_ui->partPrecompiledHeadersEdit->clear();
  m_ui->projectPartTab->setTabText(ProjectPartPrecompiledHeadersTab, partTabName(ProjectPartPrecompiledHeadersTab));
}

static auto defineCount(const ProjectExplorer::Macros &macros) -> int
{
  using ProjectExplorer::Macro;
  return int(std::count_if(macros.begin(), macros.end(), [](const Macro &macro) { return macro.type == ProjectExplorer::MacroType::Define; }));
}

auto CppCodeModelInspectorDialog::updateProjectPartData(const ProjectPart::ConstPtr &part) -> void
{
  QTC_ASSERT(part, return);

  // General
  QString projectName = QLatin1String("<None>");
  QString projectFilePath = QLatin1String("<None>");
  if (part->hasProject()) {
    projectFilePath = part->topLevelProject.toUserOutput();
    if (const ProjectExplorer::Project *const project = projectForProjectPart(*part))
      projectName = project->displayName();
  }
  const auto callGroupId = part->callGroupId.isEmpty() ? QString::fromLatin1("<None>") : part->callGroupId;
  const auto buildSystemTarget = part->buildSystemTarget.isEmpty() ? QString::fromLatin1("<None>") : part->buildSystemTarget;

  const auto precompiledHeaders = part->precompiledHeaders.isEmpty() ? QString::fromLatin1("<None>") : part->precompiledHeaders.join(',');

  KeyValueModel::Table table = {{QString::fromLatin1("Project Part Name"), part->displayName}, {QString::fromLatin1("Project Part File"), part->projectFileLocation()}, {QString::fromLatin1("Project Name"), projectName}, {QString::fromLatin1("Project File"), projectFilePath}, {QString::fromLatin1("Callgroup Id"), callGroupId}, {QString::fromLatin1("Precompiled Headers"), precompiledHeaders}, {QString::fromLatin1("Selected For Building"), CMI::Utils::toString(part->selectedForBuilding)}, {QString::fromLatin1("Buildsystem Target"), buildSystemTarget}, {QString::fromLatin1("Build Target Type"), CMI::Utils::toString(part->buildTargetType)}, {QString::fromLatin1("ToolChain Type"), part->toolchainType.toString()}, {QString::fromLatin1("ToolChain Target Triple"), part->toolChainTargetTriple}, {QString::fromLatin1("ToolChain Word Width"), CMI::Utils::toString(part->toolChainWordWidth)}, {QString::fromLatin1("ToolChain Install Dir"), part->toolChainInstallDir.toString()}, {QString::fromLatin1("Language Version"), CMI::Utils::toString(part->languageVersion)}, {QString::fromLatin1("Language Extensions"), CMI::Utils::toString(part->languageExtensions)}, {QString::fromLatin1("Qt Version"), CMI::Utils::toString(part->qtVersion)}};
  if (!part->projectConfigFile.isEmpty())
    table.prepend({QString::fromLatin1("Project Config File"), part->projectConfigFile});
  m_partGenericInfoModel->configure(table);
  resizeColumns<KeyValueModel>(m_ui->partGeneralView);

  // Compiler Flags
  m_ui->partGeneralCompilerFlagsEdit->setPlainText(part->compilerFlags.join("\n"));

  // Project Files
  m_projectFilesModel->configure(part->files);
  m_ui->projectPartTab->setTabText(ProjectPartFilesTab, partTabName(ProjectPartFilesTab, part->files.size()));

  auto numberOfDefines = defineCount(part->toolChainMacros) + defineCount(part->projectMacros);

  m_ui->partToolchainDefinesEdit->setPlainText(QString::fromUtf8(ProjectExplorer::Macro::toByteArray(part->toolChainMacros)));
  m_ui->partProjectDefinesEdit->setPlainText(QString::fromUtf8(ProjectExplorer::Macro::toByteArray(part->projectMacros)));
  m_ui->projectPartTab->setTabText(ProjectPartDefinesTab, partTabName(ProjectPartDefinesTab, numberOfDefines));

  // Header Paths
  m_projectHeaderPathsModel->configure(part->headerPaths);
  m_ui->projectPartTab->setTabText(ProjectPartHeaderPathsTab, partTabName(ProjectPartHeaderPathsTab, part->headerPaths.size()));

  // Precompiled Headers
  m_ui->partPrecompiledHeadersEdit->setPlainText(CMI::Utils::pathListToString(part->precompiledHeaders));
  m_ui->projectPartTab->setTabText(ProjectPartPrecompiledHeadersTab, partTabName(ProjectPartPrecompiledHeadersTab, part->precompiledHeaders.size()));
}

auto CppCodeModelInspectorDialog::event(QEvent *e) -> bool
{
  if (e->type() == QEvent::ShortcutOverride) {
    auto ke = static_cast<QKeyEvent*>(e);
    if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
      ke->accept();
      close();
      return false;
    }
  }
  return QDialog::event(e);
}

} // namespace Internal
} // namespace CppEditor

#include "cppcodemodelinspectordialog.moc"
