// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "addrunconfigdialog.hpp"

#include "project.hpp"
#include "target.hpp"

#include <utils/itemviews.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

constexpr Qt::ItemDataRole IsCustomRole = Qt::UserRole;

class CandidateTreeItem : public TreeItem {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::AddRunConfigDialog)

public:
  CandidateTreeItem(const RunConfigurationCreationInfo &rci, const Target *target) : m_creationInfo(rci), m_projectRoot(target->project()->projectDirectory()), m_displayName(target->macroExpander()->expand(rci.displayName)) { }

  auto creationInfo() const -> RunConfigurationCreationInfo { return m_creationInfo; }

private:
  auto data(int column, int role) const -> QVariant override
  {
    QTC_ASSERT(column < 2, return QVariant());
    if (role == IsCustomRole)
      return m_creationInfo.projectFilePath.isEmpty();
    if (column == 0 && role == Qt::DisplayRole)
      return m_displayName;
    if (column == 1 && role == Qt::DisplayRole) {
      auto displayPath = m_creationInfo.projectFilePath.relativeChildPath(m_projectRoot);
      if (displayPath.isEmpty()) {
        displayPath = m_creationInfo.projectFilePath;
        QTC_CHECK(displayPath.isEmpty());
      }
      return displayPath.isEmpty() ? tr("[none]") : displayPath.toUserOutput();
    }
    return QVariant();
  }

  const RunConfigurationCreationInfo m_creationInfo;
  const FilePath m_projectRoot;
  const QString m_displayName;
};

class CandidatesModel : public TreeModel<TreeItem, CandidateTreeItem> {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::AddRunConfigDialog)
public:
  CandidatesModel(Target *target, QObject *parent) : TreeModel(parent)
  {
    setHeader({tr("Name"), tr("Source")});
    for (const auto &rci : RunConfigurationFactory::creatorsForTarget(target)) {
      rootItem()->appendChild(new CandidateTreeItem(rci, target));
    }
  }
};

class ProxyModel : public QSortFilterProxyModel {
public:
  ProxyModel(QObject *parent) : QSortFilterProxyModel(parent) { }

private:
  auto lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const -> bool override
  {
    if (source_left.column() == 0) {
      // Let's put the fallback candidates last.
      const auto leftIsCustom = sourceModel()->data(source_left, IsCustomRole).toBool();
      const auto rightIsCustom = sourceModel()->data(source_right, IsCustomRole).toBool();
      if (leftIsCustom != rightIsCustom)
        return rightIsCustom;
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
  }
};

class CandidatesTreeView : public TreeView {
public:
  CandidatesTreeView(QWidget *parent) : TreeView(parent)
  {
    setUniformRowHeights(true);
  }

private:
  auto sizeHint() const -> QSize override
  {
    const auto width = columnWidth(0) + columnWidth(1);
    const auto height = qMin(model()->rowCount() + 10, 10) * rowHeight(model()->index(0, 0)) + header()->sizeHint().height();
    return {width, height};
  }
};

AddRunConfigDialog::AddRunConfigDialog(Target *target, QWidget *parent) : QDialog(parent), m_view(new CandidatesTreeView(this))
{
  setWindowTitle(tr("Create Run Configuration"));
  const auto model = new CandidatesModel(target, this);
  const auto proxyModel = new ProxyModel(this);
  proxyModel->setSourceModel(model);
  const auto filterEdit = new FancyLineEdit(this);
  filterEdit->setFiltering(true);
  filterEdit->setPlaceholderText(tr("Filter candidates by name"));
  m_view->setSelectionMode(TreeView::SingleSelection);
  m_view->setSelectionBehavior(TreeView::SelectRows);
  m_view->setSortingEnabled(true);
  m_view->setModel(proxyModel);
  m_view->resizeColumnToContents(0);
  m_view->resizeColumnToContents(1);
  m_view->sortByColumn(0, Qt::AscendingOrder);
  const auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create"));

  connect(filterEdit, &FancyLineEdit::textChanged, this, [proxyModel](const QString &text) {
    proxyModel->setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
  });
  connect(m_view, &TreeView::doubleClicked, this, [this] { accept(); });
  const auto updateOkButton = [buttonBox, this] {
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_view->selectionModel()->hasSelection());
  };
  connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateOkButton);
  updateOkButton();
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  const auto layout = new QVBoxLayout(this);
  layout->addWidget(filterEdit);
  layout->addWidget(m_view);
  layout->addWidget(buttonBox);
}

auto AddRunConfigDialog::accept() -> void
{
  const auto selected = m_view->selectionModel()->selectedRows();
  QTC_ASSERT(selected.count() == 1, return);
  const auto *const proxyModel = static_cast<ProxyModel*>(m_view->model());
  const auto *const model = static_cast<CandidatesModel*>(proxyModel->sourceModel());
  const TreeItem *const item = model->itemForIndex(proxyModel->mapToSource(selected.first()));
  QTC_ASSERT(item, return);
  m_creationInfo = static_cast<const CandidateTreeItem*>(item)->creationInfo();
  QTC_ASSERT(m_creationInfo.factory, return);
  QDialog::accept();
}

} // namespace Internal
} // namespace ProjectExplorer
