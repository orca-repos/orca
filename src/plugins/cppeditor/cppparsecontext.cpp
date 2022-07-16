// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppparsecontext.hpp"

#include "cppeditorwidget.hpp"

#include <QAction>
#include <QDir>
#include <QDebug>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

namespace CppEditor {
namespace Internal {

auto ParseContextModel::update(const ProjectPartInfo &projectPartInfo) -> void
{
  beginResetModel();
  reset(projectPartInfo);
  endResetModel();

  emit updated(areMultipleAvailable());
}

auto ParseContextModel::currentToolTip() const -> QString
{
  const auto index = createIndex(m_currentIndex, 0);
  if (!index.isValid())
    return QString();

  return tr("<p><b>Active Parse Context</b>:<br/>%1</p>" "<p>Multiple parse contexts (set of defines, include paths, and so on) " "are available for this file.</p>" "<p>Choose a parse context to set it as the preferred one. " "Clear the preference from the context menu.</p>").arg(data(index, Qt::ToolTipRole).toString());
}

auto ParseContextModel::setPreferred(int index) -> void
{
  if (index < 0)
    return;

  emit preferredParseContextChanged(m_projectParts[index]->id());
}

auto ParseContextModel::clearPreferred() -> void
{
  emit preferredParseContextChanged(QString());
}

auto ParseContextModel::areMultipleAvailable() const -> bool
{
  return m_projectParts.size() >= 2;
}

auto ParseContextModel::reset(const ProjectPartInfo &projectPartInfo) -> void
{
  // Sort
  m_hints = projectPartInfo.hints;
  m_projectParts = projectPartInfo.projectParts;
  Utils::sort(m_projectParts, &ProjectPart::displayName);

  // Determine index for current
  const auto id = projectPartInfo.projectPart->id();
  m_currentIndex = Utils::indexOf(m_projectParts, [id](const ProjectPart::ConstPtr &pp) {
    return pp->id() == id;
  });
  QTC_CHECK(m_currentIndex >= 0);
}

auto ParseContextModel::currentIndex() const -> int
{
  return m_currentIndex;
}

auto ParseContextModel::isCurrentPreferred() const -> bool
{
  return m_hints & ProjectPartInfo::IsPreferredMatch;
}

auto ParseContextModel::currentId() const -> QString
{
  if (m_currentIndex < 0)
    return QString();

  return m_projectParts[m_currentIndex]->id();
}

auto ParseContextModel::rowCount(const QModelIndex &parent) const -> int
{
  if (parent.isValid())
    return 0;
  return m_projectParts.size();
}

auto ParseContextModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_projectParts.size())
    return QVariant();

  const auto row = index.row();
  if (role == Qt::DisplayRole)
    return m_projectParts[row]->displayName;
  else if (role == Qt::ToolTipRole)
    return QDir::toNativeSeparators(m_projectParts[row]->projectFile);

  return QVariant();
}

ParseContextWidget::ParseContextWidget(ParseContextModel &parseContextModel, QWidget *parent) : QComboBox(parent), m_parseContextModel(parseContextModel)
{
  setSizeAdjustPolicy(QComboBox::AdjustToContents);
  auto policy = sizePolicy();
  policy.setHorizontalStretch(1);
  policy.setHorizontalPolicy(QSizePolicy::Maximum);
  setSizePolicy(policy);
  // Set up context menu with a clear action
  setContextMenuPolicy(Qt::ActionsContextMenu);
  m_clearPreferredAction = new QAction(tr("Clear Preferred Parse Context"), this);
  connect(m_clearPreferredAction, &QAction::triggered, [&]() {
    m_parseContextModel.clearPreferred();
  });
  addAction(m_clearPreferredAction);

  // Set up sync of this widget and model in both directions
  connect(this, QOverload<int>::of(&QComboBox::activated), &m_parseContextModel, &ParseContextModel::setPreferred);
  connect(&m_parseContextModel, &ParseContextModel::updated, this, &ParseContextWidget::syncToModel);

  // Set up model
  setModel(&m_parseContextModel);
}

auto ParseContextWidget::syncToModel() -> void
{
  const auto index = m_parseContextModel.currentIndex();
  if (index < 0)
    return; // E.g. editor was duplicated but no project context was determined yet.

  if (currentIndex() != index)
    setCurrentIndex(index);

  setToolTip(m_parseContextModel.currentToolTip());

  const auto isPreferred = m_parseContextModel.isCurrentPreferred();
  m_clearPreferredAction->setEnabled(isPreferred);
  CppEditorWidget::updateWidgetHighlighting(this, isPreferred);
}

auto ParseContextWidget::minimumSizeHint() const -> QSize
{
  // QComboBox always returns the same from sizeHint() and minimumSizeHint().
  // We want sizeHint() to be the preferred and maximum size
  // (horizontalPolicy == Maximum), but want it to be shrinkable, which is not the case
  // if the minimumSizeHint() is the same as sizeHint()
  auto size = QComboBox::minimumSizeHint();
  size.setWidth(120);
  return size;
}

} // namespace Internal
} // namespace CppEditor
