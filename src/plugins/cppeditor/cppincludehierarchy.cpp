// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppincludehierarchy.hpp"

#include "baseeditordocumentprocessor.hpp"
#include "editordocumenthandle.hpp"
#include "cppeditorwidget.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditordocument.hpp"
#include "cppeditorplugin.hpp"
#include "cppelementevaluator.hpp"
#include "cppmodelmanager.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/fileiconprovider.hpp>
#include <core/find/itemviewfind.hpp>

#include <cplusplus/CppDocument.h>

#include <texteditor/texteditor.hpp>

#include <utils/delegates.hpp>
#include <utils/dropsupport.hpp>
#include <utils/fileutils.hpp>
#include <utils/navigationtreeview.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcsettings.hpp>
#include <utils/utilsicons.hpp>

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Core;
using namespace CPlusPlus;
using namespace TextEditor;
using namespace Utils;

namespace CppEditor {
namespace Internal {

enum {
  AnnotationRole = Qt::UserRole + 1,
  LinkRole
};

static auto globalSnapshot() -> Snapshot
{
  return CppModelManager::instance()->snapshot();
}

struct FileAndLine {
  FileAndLine() = default;
  FileAndLine(const QString &f, int l) : file(f), line(l) {}

  QString file;
  int line = 0;
};

using FileAndLines = QList<FileAndLine>;

static auto findIncluders(const QString &filePath) -> FileAndLines
{
  FileAndLines result;
  const Snapshot snapshot = globalSnapshot();
  for (auto cit = snapshot.begin(), citEnd = snapshot.end(); cit != citEnd; ++cit) {
    const QString filePathFromSnapshot = cit.key().toString();
    Document::Ptr doc = cit.value();
    const QList<Document::Include> resolvedIncludes = doc->resolvedIncludes();
    for (const auto &includeFile : resolvedIncludes) {
      const QString includedFilePath = includeFile.resolvedFileName();
      if (includedFilePath == filePath)
        result.append(FileAndLine(filePathFromSnapshot, int(includeFile.line())));
    }
  }
  return result;
}

static auto findIncludes(const QString &filePath, const Snapshot &snapshot) -> FileAndLines
{
  FileAndLines result;
  if (Document::Ptr doc = snapshot.document(filePath)) {
    const QList<Document::Include> resolvedIncludes = doc->resolvedIncludes();
    for (const auto &includeFile : resolvedIncludes)
      result.append(FileAndLine(includeFile.resolvedFileName(), 0));
  }
  return result;
}

class CppIncludeHierarchyItem : public TypedTreeItem<CppIncludeHierarchyItem, CppIncludeHierarchyItem> {
public:
  enum SubTree {
    RootItem,
    InIncludes,
    InIncludedBy
  };

  CppIncludeHierarchyItem() = default;

  auto createChild(const QString &filePath, SubTree subTree, int line = 0, bool definitelyNoChildren = false) -> void
  {
    auto item = new CppIncludeHierarchyItem;
    item->m_fileName = filePath.mid(filePath.lastIndexOf('/') + 1);
    item->m_filePath = filePath;
    item->m_line = line;
    item->m_subTree = subTree;
    appendChild(item);
    for (auto ancestor = this; ancestor; ancestor = ancestor->parent()) {
      if (ancestor->filePath() == filePath) {
        item->m_isCyclic = true;
        break;
      }
    }
    if (filePath == model()->editorFilePath() || definitelyNoChildren)
      item->setChildrenChecked();
  }

  auto filePath() const -> QString
  {
    return isPhony() ? model()->editorFilePath() : m_filePath;
  }

private:
  auto isPhony() const -> bool { return !parent() || !parent()->parent(); }
  auto setChildrenChecked() -> void { m_checkedForChildren = true; }

  auto model() const -> CppIncludeHierarchyModel*
  {
    return static_cast<CppIncludeHierarchyModel*>(TreeItem::model());
  }

  auto data(int column, int role) const -> QVariant override;

  auto flags(int) const -> Qt::ItemFlags override
  {
    const Utils::Link link(Utils::FilePath::fromString(m_filePath), m_line);
    if (link.hasValidTarget())
      return Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

  auto canFetchMore() const -> bool override;
  auto fetchMore() -> void override;

  QString m_fileName;
  QString m_filePath;
  int m_line = 0;
  SubTree m_subTree = RootItem;
  bool m_isCyclic = false;
  bool m_checkedForChildren = false;
};

auto CppIncludeHierarchyItem::data(int column, int role) const -> QVariant
{
  Q_UNUSED(column)
  if (role == Qt::DisplayRole) {
    if (isPhony() && childCount() == 0)
      return QString(m_fileName + ' ' + CppIncludeHierarchyModel::tr("(none)"));
    if (m_isCyclic)
      return QString(m_fileName + ' ' + CppIncludeHierarchyModel::tr("(cyclic)"));
    return m_fileName;
  }

  if (isPhony())
    return QVariant();

  switch (role) {
  case Qt::ToolTipRole:
    return m_filePath;
  case Qt::DecorationRole:
    return FileIconProvider::icon(FilePath::fromString(m_filePath));
  case LinkRole:
    return QVariant::fromValue(Link(FilePath::fromString(m_filePath), m_line));
  }

  return QVariant();
}

auto CppIncludeHierarchyItem::canFetchMore() const -> bool
{
  if (m_isCyclic || m_checkedForChildren || childCount() > 0)
    return false;

  return !model()->m_searching || !model()->m_seen.contains(m_filePath);
}

auto CppIncludeHierarchyItem::fetchMore() -> void
{
  QTC_ASSERT(canFetchMore(), setChildrenChecked(); return);
  QTC_ASSERT(model(), return);
  QTC_ASSERT(m_subTree != RootItem, return); // Root should always be populated.

  model()->m_seen.insert(m_filePath);

  const auto editorFilePath = model()->editorFilePath();

  setChildrenChecked();
  if (m_subTree == InIncludes) {
    auto processor = CppModelManager::cppEditorDocumentProcessor(editorFilePath);
    QTC_ASSERT(processor, return);
    const Snapshot snapshot = processor->snapshot();
    const FileAndLines includes = findIncludes(filePath(), snapshot);
    for (const auto &include : includes) {
      const FileAndLines subIncludes = findIncludes(include.file, snapshot);
      bool definitelyNoChildren = subIncludes.isEmpty();
      createChild(include.file, InIncludes, include.line, definitelyNoChildren);
    }
  } else if (m_subTree == InIncludedBy) {
    const auto includers = findIncluders(filePath());
    for (const auto &includer : includers) {
      const auto subIncluders = findIncluders(includer.file);
      auto definitelyNoChildren = subIncluders.isEmpty();
      createChild(includer.file, InIncludedBy, includer.line, definitelyNoChildren);
    }
  }
}

auto CppIncludeHierarchyModel::buildHierarchy(const QString &document) -> void
{
  m_editorFilePath = document;
  rootItem()->removeChildren();
  rootItem()->createChild(tr("Includes"), CppIncludeHierarchyItem::InIncludes);
  rootItem()->createChild(tr("Included by"), CppIncludeHierarchyItem::InIncludedBy);
}

auto CppIncludeHierarchyModel::setSearching(bool on) -> void
{
  m_searching = on;
  m_seen.clear();
}

// CppIncludeHierarchyModel

CppIncludeHierarchyModel::CppIncludeHierarchyModel()
{
  setRootItem(new CppIncludeHierarchyItem); // FIXME: Remove in 4.2
}

auto CppIncludeHierarchyModel::supportedDragActions() const -> Qt::DropActions
{
  return Qt::MoveAction;
}

auto CppIncludeHierarchyModel::mimeTypes() const -> QStringList
{
  return DropSupport::mimeTypesForFilePaths();
}

auto CppIncludeHierarchyModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  auto data = new DropMimeData;
  for (const auto &index : indexes) {
    auto link = index.data(LinkRole).value<Utils::Link>();
    if (link.hasValidTarget())
      data->addFile(link.targetFilePath, link.targetLine, link.targetColumn);
  }
  return data;
}

// CppIncludeHierarchyTreeView

class CppIncludeHierarchyTreeView : public NavigationTreeView {
public:
  CppIncludeHierarchyTreeView()
  {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
  }

protected:
  auto keyPressEvent(QKeyEvent *event) -> void override
  {
    if (event->key())
      QAbstractItemView::keyPressEvent(event);
    else
      NavigationTreeView::keyPressEvent(event);
  }
};

// IncludeFinder

class IncludeFinder : public ItemViewFind {
public:
  IncludeFinder(QAbstractItemView *view, CppIncludeHierarchyModel *model) : ItemViewFind(view, Qt::DisplayRole, FetchMoreWhileSearching), m_model(model) {}

private:
  auto findIncremental(const QString &txt, FindFlags findFlags) -> Result override
  {
    m_model->setSearching(true);
    auto result = ItemViewFind::findIncremental(txt, findFlags);
    m_model->setSearching(false);
    return result;
  }

  auto findStep(const QString &txt, FindFlags findFlags) -> Result override
  {
    m_model->setSearching(true);
    auto result = ItemViewFind::findStep(txt, findFlags);
    m_model->setSearching(false);
    return result;
  }

  CppIncludeHierarchyModel *m_model; // Not owned.
};

// CppIncludeHierarchyWidget

class CppIncludeHierarchyWidget : public QWidget {
  Q_OBJECT public:
  CppIncludeHierarchyWidget();
  ~CppIncludeHierarchyWidget() override { delete m_treeView; }

  auto perform() -> void;

  auto saveSettings(QSettings *settings, int position) -> void;
  auto restoreSettings(QSettings *settings, int position) -> void;

private:
  auto onItemActivated(const QModelIndex &index) -> void;
  auto editorsClosed(const QList<IEditor*> &editors) -> void;
  auto showNoIncludeHierarchyLabel() -> void;
  auto showIncludeHierarchy() -> void;
  auto syncFromEditorManager() -> void;

  CppIncludeHierarchyTreeView *m_treeView = nullptr;
  CppIncludeHierarchyModel m_model;
  AnnotatedItemDelegate m_delegate;
  TextEditorLinkLabel *m_inspectedFile = nullptr;
  QLabel *m_includeHierarchyInfoLabel = nullptr;
  QToolButton *m_toggleSync = nullptr;
  BaseTextEditor *m_editor = nullptr;
  QTimer *m_timer = nullptr;

  // CppIncludeHierarchyFactory needs private members for button access
  friend class CppIncludeHierarchyFactory;
};

CppIncludeHierarchyWidget::CppIncludeHierarchyWidget()
{
  m_delegate.setDelimiter(" ");
  m_delegate.setAnnotationRole(AnnotationRole);

  m_inspectedFile = new TextEditorLinkLabel(this);
  m_inspectedFile->setContentsMargins(5, 5, 5, 5);

  m_treeView = new CppIncludeHierarchyTreeView;
  m_treeView->setModel(&m_model);
  m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_treeView->setItemDelegate(&m_delegate);
  connect(m_treeView, &QAbstractItemView::activated, this, &CppIncludeHierarchyWidget::onItemActivated);

  m_includeHierarchyInfoLabel = new QLabel(tr("No include hierarchy available"), this);
  m_includeHierarchyInfoLabel->setAlignment(Qt::AlignCenter);
  m_includeHierarchyInfoLabel->setAutoFillBackground(true);
  m_includeHierarchyInfoLabel->setBackgroundRole(QPalette::Base);
  m_includeHierarchyInfoLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_timer = new QTimer(this);
  m_timer->setInterval(2000);
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, &CppIncludeHierarchyWidget::perform);

  m_toggleSync = new QToolButton(this);
  m_toggleSync->setIcon(Utils::Icons::LINK_TOOLBAR.icon());
  m_toggleSync->setCheckable(true);
  m_toggleSync->setToolTip(tr("Synchronize with Editor"));
  connect(m_toggleSync, &QToolButton::clicked, this, &CppIncludeHierarchyWidget::syncFromEditorManager);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_inspectedFile);
  layout->addWidget(ItemViewFind::createSearchableWrapper(new IncludeFinder(m_treeView, &m_model)));
  layout->addWidget(m_includeHierarchyInfoLabel);

  connect(CppEditorPlugin::instance(), &CppEditorPlugin::includeHierarchyRequested, this, &CppIncludeHierarchyWidget::perform);
  connect(EditorManager::instance(), &EditorManager::editorsClosed, this, &CppIncludeHierarchyWidget::editorsClosed);
  connect(EditorManager::instance(), &EditorManager::currentEditorChanged, this, &CppIncludeHierarchyWidget::syncFromEditorManager);

  syncFromEditorManager();
}

auto CppIncludeHierarchyWidget::perform() -> void
{
  showNoIncludeHierarchyLabel();

  m_editor = qobject_cast<BaseTextEditor*>(EditorManager::currentEditor());
  if (!m_editor)
    return;

  const auto documentPath = m_editor->textDocument()->filePath();
  m_model.buildHierarchy(documentPath.toString());

  m_inspectedFile->setText(m_editor->textDocument()->displayName());
  m_inspectedFile->setLink(Utils::Link(documentPath));

  // expand "Includes" and "Included by"
  m_treeView->expand(m_model.index(0, 0));
  m_treeView->expand(m_model.index(1, 0));

  showIncludeHierarchy();
}

const bool kSyncDefault = false;

auto CppIncludeHierarchyWidget::saveSettings(QSettings *settings, int position) -> void
{
  const auto key = QString("IncludeHierarchy.%1.SyncWithEditor").arg(position);
  QtcSettings::setValueWithDefault(settings, key, m_toggleSync->isChecked(), kSyncDefault);
}

auto CppIncludeHierarchyWidget::restoreSettings(QSettings *settings, int position) -> void
{
  const auto key = QString("IncludeHierarchy.%1.SyncWithEditor").arg(position);
  m_toggleSync->setChecked(settings->value(key, kSyncDefault).toBool());
}

auto CppIncludeHierarchyWidget::onItemActivated(const QModelIndex &index) -> void
{
  const auto link = index.data(LinkRole).value<Utils::Link>();
  if (link.hasValidTarget())
    EditorManager::openEditorAt(link, Constants::CPPEDITOR_ID);
}

auto CppIncludeHierarchyWidget::editorsClosed(const QList<IEditor*> &editors) -> void
{
  for (const IEditor *editor : editors) {
    if (m_editor == editor)
      perform();
  }
}

auto CppIncludeHierarchyWidget::showNoIncludeHierarchyLabel() -> void
{
  m_inspectedFile->hide();
  m_treeView->hide();
  m_includeHierarchyInfoLabel->show();
}

auto CppIncludeHierarchyWidget::showIncludeHierarchy() -> void
{
  m_inspectedFile->show();
  m_treeView->show();
  m_includeHierarchyInfoLabel->hide();
}

auto CppIncludeHierarchyWidget::syncFromEditorManager() -> void
{
  if (!m_toggleSync->isChecked())
    return;

  const auto editor = qobject_cast<BaseTextEditor*>(EditorManager::currentEditor());
  if (!editor)
    return;

  auto document = qobject_cast<CppEditorDocument*>(editor->textDocument());
  if (!document)
    return;

  // Update the hierarchy immediately after a document change. If the
  // document is already parsed, cppDocumentUpdated is not triggered again.
  perform();

  // Use cppDocumentUpdated to catch parsing finished and later file updates.
  // The timer limits the amount of hierarchy updates.
  connect(document, &CppEditorDocument::cppDocumentUpdated, m_timer, QOverload<>::of(&QTimer::start), Qt::UniqueConnection);
}

// CppIncludeHierarchyFactory

CppIncludeHierarchyFactory::CppIncludeHierarchyFactory()
{
  setDisplayName(tr("Include Hierarchy"));
  setPriority(800);
  setId(Constants::INCLUDE_HIERARCHY_ID);
}

auto CppIncludeHierarchyFactory::createWidget() -> NavigationView
{
  auto hierarchyWidget = new CppIncludeHierarchyWidget;
  hierarchyWidget->perform();

  auto stack = new QStackedWidget;
  stack->addWidget(hierarchyWidget);

  NavigationView navigationView;
  navigationView.dock_tool_bar_widgets << hierarchyWidget->m_toggleSync;
  navigationView.widget = stack;
  return navigationView;
}

static auto hierarchyWidget(QWidget *widget) -> CppIncludeHierarchyWidget*
{
  auto stack = qobject_cast<QStackedWidget*>(widget);
  Q_ASSERT(stack);
  auto hierarchyWidget = qobject_cast<CppIncludeHierarchyWidget*>(stack->currentWidget());
  Q_ASSERT(hierarchyWidget);
  return hierarchyWidget;
}

auto CppIncludeHierarchyFactory::saveSettings(QtcSettings *settings, int position, QWidget *widget) -> void
{
  hierarchyWidget(widget)->saveSettings(settings, position);
}

auto CppIncludeHierarchyFactory::restoreSettings(QSettings *settings, int position, QWidget *widget) -> void
{
  hierarchyWidget(widget)->restoreSettings(settings, position);
}

} // namespace Internal
} // namespace CppEditor

#include "cppincludehierarchy.moc"
