// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppeditoroutline.hpp"
#include "cppmodelmanager.hpp"
#include "cppoverviewmodel.hpp"
#include "cpptoolsreuse.hpp"
#include "cpptoolssettings.hpp"

#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>
#include <core/editormanager/editormanager.hpp>

#include <utils/linecolumn.hpp>
#include <utils/treeviewcombobox.hpp>

#include <QAction>
#include <QSortFilterProxyModel>
#include <QTimer>

/*!
    \class CppEditor::CppEditorOutline
    \brief A helper class that provides the outline model and widget,
           e.g. for the editor's tool bar.

    The caller is responsible for deleting the widget returned by widget().
 */

enum {
  UpdateOutlineIntervalInMs = 500
};

namespace {

class OverviewProxyModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  OverviewProxyModel(CppEditor::AbstractOverviewModel &sourceModel, QObject *parent) : QSortFilterProxyModel(parent), m_sourceModel(sourceModel) { }

  auto filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const -> bool override
  {
    // Ignore generated symbols, e.g. by macro expansion (Q_OBJECT)
    const auto sourceIndex = m_sourceModel.index(sourceRow, 0, sourceParent);
    if (m_sourceModel.isGenerated(sourceIndex))
      return false;

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
  }

private:
  CppEditor::AbstractOverviewModel &m_sourceModel;
};

auto newSingleShotTimer(QObject *parent, int msInternal, const QString &objectName) -> QTimer*
{
  auto *timer = new QTimer(parent);
  timer->setObjectName(objectName);
  timer->setSingleShot(true);
  timer->setInterval(msInternal);
  return timer;
}

} // anonymous namespace

namespace CppEditor::Internal {

CppEditorOutline::CppEditorOutline(TextEditor::TextEditorWidget *editorWidget) : QObject(editorWidget), m_editorWidget(editorWidget), m_combo(new Utils::TreeViewComboBox)
{
  m_model = CppModelManager::instance()->createOverviewModel();
  m_proxyModel = new OverviewProxyModel(*m_model, this);
  m_proxyModel->setSourceModel(m_model.get());

  // Set up proxy model
  if (CppToolsSettings::instance()->sortedEditorDocumentOutline())
    m_proxyModel->sort(0, Qt::AscendingOrder);
  else
    m_proxyModel->sort(-1, Qt::AscendingOrder); // don't sort yet, but set column for sortedOutline()
  m_proxyModel->setDynamicSortFilter(true);

  // Set up combo box
  m_combo->setModel(m_proxyModel);

  m_combo->setMinimumContentsLength(13);
  auto policy = m_combo->sizePolicy();
  policy.setHorizontalPolicy(QSizePolicy::Expanding);
  m_combo->setSizePolicy(policy);
  m_combo->setMaxVisibleItems(40);

  m_combo->setContextMenuPolicy(Qt::ActionsContextMenu);
  m_sortAction = new QAction(tr("Sort Alphabetically"), m_combo);
  m_sortAction->setCheckable(true);
  m_sortAction->setChecked(isSorted());
  connect(m_sortAction, &QAction::toggled, CppToolsSettings::instance(), &CppToolsSettings::setSortedEditorDocumentOutline);
  m_combo->addAction(m_sortAction);

  connect(m_combo, QOverload<int>::of(&QComboBox::activated), this, &CppEditorOutline::gotoSymbolInEditor);
  connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CppEditorOutline::updateToolTip);

  // Set up timers
  m_updateTimer = newSingleShotTimer(this, UpdateOutlineIntervalInMs, QLatin1String("CppEditorOutline::m_updateTimer"));
  connect(m_updateTimer, &QTimer::timeout, this, &CppEditorOutline::updateNow);
  connect(m_model.get(), &AbstractOverviewModel::needsUpdate, this, &CppEditorOutline::updateNow);

  m_updateIndexTimer = newSingleShotTimer(this, UpdateOutlineIntervalInMs, QLatin1String("CppEditorOutline::m_updateIndexTimer"));
  connect(m_updateIndexTimer, &QTimer::timeout, this, &CppEditorOutline::updateIndexNow);
}

auto CppEditorOutline::update() -> void
{
  m_updateTimer->start();
}

auto CppEditorOutline::isSorted() const -> bool
{
  return m_proxyModel->sortColumn() == 0;
}

auto CppEditorOutline::setSorted(bool sort) -> void
{
  if (sort != isSorted()) {
    if (sort)
      m_proxyModel->sort(0, Qt::AscendingOrder);
    else
      m_proxyModel->sort(-1, Qt::AscendingOrder);
    {
      QSignalBlocker blocker(m_sortAction);
      m_sortAction->setChecked(m_proxyModel->sortColumn() == 0);
    }
    updateIndexNow();
  }
}

auto CppEditorOutline::model() const -> AbstractOverviewModel*
{
  return m_model.get();
}

auto CppEditorOutline::modelIndex() -> QModelIndex
{
  if (!m_modelIndex.isValid()) {
    auto line = 0, column = 0;
    m_editorWidget->convertPosition(m_editorWidget->position(), &line, &column);
    m_modelIndex = indexForPosition(line, column);
    emit modelIndexChanged(m_modelIndex);
  }

  return m_modelIndex;
}

auto CppEditorOutline::widget() const -> QWidget*
{
  return m_combo;
}

auto getDocument(const QString &filePath) -> QSharedPointer<CPlusPlus::Document>
{
  const CPlusPlus::Snapshot snapshot = CppModelManager::instance()->snapshot();
  return snapshot.document(filePath);
}

auto CppEditorOutline::updateNow() -> void
{
  const auto filePath = m_editorWidget->textDocument()->filePath().toString();
  m_document = getDocument(filePath);
  if (!m_document)
    return;

  if (m_document->editorRevision() != static_cast<unsigned>(m_editorWidget->document()->revision())) {
    m_updateTimer->start();
    return;
  }

  if (!m_model->rebuild(filePath))
    m_model->rebuild(m_document);

  m_combo->view()->expandAll();
  updateIndexNow();
}

auto CppEditorOutline::updateIndex() -> void
{
  m_updateIndexTimer->start();
}

auto CppEditorOutline::updateIndexNow() -> void
{
  if (!m_document)
    return;

  const auto revision = static_cast<unsigned>(m_editorWidget->document()->revision());
  if (m_document->editorRevision() != revision) {
    m_updateIndexTimer->start();
    return;
  }

  m_updateIndexTimer->stop();

  m_modelIndex = QModelIndex(); //invalidate
  auto comboIndex = modelIndex();

  if (comboIndex.isValid()) {
    QSignalBlocker blocker(m_combo);
    m_combo->setCurrentIndex(m_proxyModel->mapFromSource(comboIndex));
    updateToolTip();
  }
}

auto CppEditorOutline::updateToolTip() -> void
{
  m_combo->setToolTip(m_combo->currentText());
}

auto CppEditorOutline::gotoSymbolInEditor() -> void
{
  const auto modelIndex = m_combo->view()->currentIndex();
  const auto sourceIndex = m_proxyModel->mapToSource(modelIndex);

  const auto link = m_model->linkFromIndex(sourceIndex);
  if (!link.hasValidTarget())
    return;

  Core::EditorManager::cutForwardNavigationHistory();
  Core::EditorManager::addCurrentPositionToNavigationHistory();
  m_editorWidget->gotoLine(link.targetLine, link.targetColumn, true, true);
  emit m_editorWidget->activateEditor();
}

static auto contains(const AbstractOverviewModel::Range &range, int line, int column) -> bool
{
  if (line < range.first.line || line > range.second.line)
    return false;
  if (line == range.first.line && column < range.first.column)
    return false;
  if (line == range.second.line && column > range.second.column)
    return false;
  return true;
}

auto CppEditorOutline::indexForPosition(int line, int column, const QModelIndex &rootIndex) const -> QModelIndex
{
  auto lastIndex = rootIndex;
  const auto rowCount = m_model->rowCount(rootIndex);
  for (auto row = 0; row < rowCount; ++row) {
    const auto index = m_model->index(row, 0, rootIndex);
    const auto range = m_model->rangeFromIndex(index);
    if (range.first.line > line)
      break;
    // Skip ranges that do not include current line and column.
    if (range.second != range.first && !contains(range, line, column))
      continue;
    lastIndex = index;
  }

  if (lastIndex != rootIndex) {
    // recurse
    lastIndex = indexForPosition(line, column, lastIndex);
  }

  return lastIndex;
}

} // namespace CppEditor::Internal

#include <cppeditoroutline.moc>
