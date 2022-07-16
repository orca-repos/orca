// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deviceprocessesdialog.hpp"
#include "deviceprocesslist.hpp"
#include <projectexplorer/kitchooser.hpp>
#include <projectexplorer/kitinformation.hpp>

#include <utils/fancylineedit.hpp>
#include <utils/itemviews.hpp>
#include <utils/qtcassert.hpp>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class ProcessListFilterModel : public QSortFilterProxyModel {
public:
  ProcessListFilterModel();
  auto lessThan(const QModelIndex &left, const QModelIndex &right) const -> bool override;
};

ProcessListFilterModel::ProcessListFilterModel()
{
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  setDynamicSortFilter(true);
  setFilterKeyColumn(-1);
}

auto ProcessListFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const -> bool
{
  const auto l = sourceModel()->data(left).toString();
  const auto r = sourceModel()->data(right).toString();
  if (left.column() == 0)
    return l.toInt() < r.toInt();
  return l < r;
}

class DeviceProcessesDialogPrivate : public QObject {
  Q_OBJECT

public:
  DeviceProcessesDialogPrivate(KitChooser *chooser, QDialog *parent);

  auto setDevice(const IDevice::ConstPtr &device) -> void;
  auto updateProcessList() -> void;
  auto updateDevice() -> void;
  auto killProcess() -> void;
  auto handleRemoteError(const QString &errorMsg) -> void;
  auto handleProcessListUpdated() -> void;
  auto handleProcessKilled() -> void;
  auto updateButtons() -> void;
  auto selectedProcess() const -> DeviceProcessItem;

  QDialog *q;
  DeviceProcessList *processList;
  ProcessListFilterModel proxyModel;
  QLabel *kitLabel;
  KitChooser *kitChooser;
  TreeView *procView;
  QTextBrowser *errorText;
  FancyLineEdit *processFilterLineEdit;
  QPushButton *updateListButton;
  QPushButton *killProcessButton;
  QPushButton *acceptButton;
  QDialogButtonBox *buttonBox;
};

DeviceProcessesDialogPrivate::DeviceProcessesDialogPrivate(KitChooser *chooser, QDialog *parent) : q(parent), kitLabel(new QLabel(DeviceProcessesDialog::tr("Kit:"), parent)), kitChooser(chooser), acceptButton(nullptr), buttonBox(new QDialogButtonBox(parent))
{
  q->setWindowTitle(DeviceProcessesDialog::tr("List of Processes"));
  q->setMinimumHeight(500);

  processList = nullptr;

  processFilterLineEdit = new FancyLineEdit(q);
  processFilterLineEdit->setPlaceholderText(DeviceProcessesDialog::tr("Filter"));
  processFilterLineEdit->setFocus(Qt::TabFocusReason);
  processFilterLineEdit->setHistoryCompleter(QLatin1String("DeviceProcessDialogFilter"), true /*restoreLastItemFromHistory*/);
  processFilterLineEdit->setFiltering(true);

  kitChooser->populate();

  procView = new TreeView(q);
  procView->setModel(&proxyModel);
  procView->setSelectionBehavior(QAbstractItemView::SelectRows);
  procView->setSelectionMode(QAbstractItemView::SingleSelection);
  procView->setUniformRowHeights(true);
  procView->setRootIsDecorated(false);
  procView->setAlternatingRowColors(true);
  procView->setSortingEnabled(true);
  procView->header()->setDefaultSectionSize(100);
  procView->header()->setStretchLastSection(true);
  procView->sortByColumn(1, Qt::AscendingOrder);
  procView->setActivationMode(DoubleClickActivation);

  errorText = new QTextBrowser(q);

  updateListButton = new QPushButton(DeviceProcessesDialog::tr("&Update List"), q);
  killProcessButton = new QPushButton(DeviceProcessesDialog::tr("&Kill Process"), q);

  buttonBox->addButton(updateListButton, QDialogButtonBox::ActionRole);
  buttonBox->addButton(killProcessButton, QDialogButtonBox::ActionRole);

  auto *leftColumn = new QFormLayout();
  leftColumn->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  leftColumn->addRow(kitLabel, kitChooser);
  leftColumn->addRow(DeviceProcessesDialog::tr("&Filter:"), processFilterLineEdit);

  //    QVBoxLayout *rightColumn = new QVBoxLayout();
  //    rightColumn->addWidget(updateListButton);
  //    rightColumn->addWidget(killProcessButton);
  //    rightColumn->addStretch();

  //    QHBoxLayout *horizontalLayout = new QHBoxLayout();
  //    horizontalLayout->addLayout(leftColumn);
  //    horizontalLayout->addLayout(rightColumn);

  auto *mainLayout = new QVBoxLayout(q);
  mainLayout->addLayout(leftColumn);
  mainLayout->addWidget(procView);
  mainLayout->addWidget(errorText);
  mainLayout->addWidget(buttonBox);

  //    QFrame *line = new QFrame(this);
  //    line->setFrameShape(QFrame::HLine);
  //    line->setFrameShadow(QFrame::Sunken);

  proxyModel.setFilterRegularExpression(processFilterLineEdit->text());

  connect(processFilterLineEdit, QOverload<const QString&>::of(&FancyLineEdit::textChanged), &proxyModel, QOverload<const QString&>::of(&ProcessListFilterModel::setFilterRegularExpression));
  connect(procView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DeviceProcessesDialogPrivate::updateButtons);
  connect(updateListButton, &QAbstractButton::clicked, this, &DeviceProcessesDialogPrivate::updateProcessList);
  connect(kitChooser, &KitChooser::currentIndexChanged, this, &DeviceProcessesDialogPrivate::updateDevice);
  connect(killProcessButton, &QAbstractButton::clicked, this, &DeviceProcessesDialogPrivate::killProcess);
  connect(&proxyModel, &QAbstractItemModel::layoutChanged, this, &DeviceProcessesDialogPrivate::handleProcessListUpdated);
  connect(buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

  QWidget::setTabOrder(kitChooser, processFilterLineEdit);
  QWidget::setTabOrder(processFilterLineEdit, procView);
  QWidget::setTabOrder(procView, buttonBox);
}

auto DeviceProcessesDialogPrivate::setDevice(const IDevice::ConstPtr &device) -> void
{
  delete processList;
  processList = nullptr;
  proxyModel.setSourceModel(nullptr);
  if (!device)
    return;

  processList = device->createProcessListModel();
  QTC_ASSERT(processList, return);
  proxyModel.setSourceModel(processList->model());

  connect(processList, &DeviceProcessList::error, this, &DeviceProcessesDialogPrivate::handleRemoteError);
  connect(processList, &DeviceProcessList::processListUpdated, this, &DeviceProcessesDialogPrivate::handleProcessListUpdated);
  connect(processList, &DeviceProcessList::processKilled, this, &DeviceProcessesDialogPrivate::handleProcessKilled, Qt::QueuedConnection);

  updateButtons();
  updateProcessList();
}

auto DeviceProcessesDialogPrivate::handleRemoteError(const QString &errorMsg) -> void
{
  QMessageBox::critical(q, tr("Remote Error"), errorMsg);
  updateListButton->setEnabled(true);
  updateButtons();
}

auto DeviceProcessesDialogPrivate::handleProcessListUpdated() -> void
{
  updateListButton->setEnabled(true);
  procView->resizeColumnToContents(0);
  procView->resizeColumnToContents(1);
  updateButtons();
}

auto DeviceProcessesDialogPrivate::updateProcessList() -> void
{
  updateListButton->setEnabled(false);
  killProcessButton->setEnabled(false);
  if (processList)
    processList->update();
}

auto DeviceProcessesDialogPrivate::killProcess() -> void
{
  const auto indexes = procView->selectionModel()->selectedIndexes();
  if (indexes.empty() || !processList)
    return;
  updateListButton->setEnabled(false);
  killProcessButton->setEnabled(false);
  processList->killProcess(proxyModel.mapToSource(indexes.first()).row());
}

auto DeviceProcessesDialogPrivate::updateDevice() -> void
{
  setDevice(DeviceKitAspect::device(kitChooser->currentKit()));
}

auto DeviceProcessesDialogPrivate::handleProcessKilled() -> void
{
  updateProcessList();
}

auto DeviceProcessesDialogPrivate::updateButtons() -> void
{
  const auto hasSelection = procView->selectionModel()->hasSelection();
  if (acceptButton)
    acceptButton->setEnabled(hasSelection);
  killProcessButton->setEnabled(hasSelection);
  errorText->setVisible(!errorText->document()->isEmpty());
}

auto DeviceProcessesDialogPrivate::selectedProcess() const -> DeviceProcessItem
{
  const auto indexes = procView->selectionModel()->selectedIndexes();
  if (indexes.empty() || !processList)
    return DeviceProcessItem();
  return processList->at(proxyModel.mapToSource(indexes.first()).row());
}

} // namespace Internal

/*!
     \class ProjectExplorer::DeviceProcessesDialog

     \brief The DeviceProcessesDialog class shows a list of processes.

     The dialog can be used as a:
     \list
     \li Non-modal dialog showing a list of processes. Call addCloseButton()
         to add a \gui Close button.
     \li Modal dialog with an \gui Accept button to select a process. Call
         addAcceptButton() passing the label text. This will create a
         \gui Cancel button as well.
     \endlist
*/

DeviceProcessesDialog::DeviceProcessesDialog(QWidget *parent) : QDialog(parent), d(std::make_unique<Internal::DeviceProcessesDialogPrivate>(new KitChooser(this), this)) { }
DeviceProcessesDialog::DeviceProcessesDialog(KitChooser *chooser, QWidget *parent) : QDialog(parent), d(std::make_unique<Internal::DeviceProcessesDialogPrivate>(chooser, this)) { }

DeviceProcessesDialog::~DeviceProcessesDialog() = default;

auto DeviceProcessesDialog::addAcceptButton(const QString &label) -> void
{
  d->acceptButton = new QPushButton(label);
  d->buttonBox->addButton(d->acceptButton, QDialogButtonBox::AcceptRole);
  connect(d->procView, &QAbstractItemView::activated, d->acceptButton, &QAbstractButton::click);
  d->buttonBox->addButton(QDialogButtonBox::Cancel);
}

auto DeviceProcessesDialog::addCloseButton() -> void
{
  d->buttonBox->addButton(QDialogButtonBox::Close);
}

auto DeviceProcessesDialog::setKitVisible(bool v) -> void
{
  d->kitLabel->setVisible(v);
  d->kitChooser->setVisible(v);
}

auto DeviceProcessesDialog::setDevice(const IDevice::ConstPtr &device) -> void
{
  setKitVisible(false);
  d->setDevice(device);
}

auto DeviceProcessesDialog::showAllDevices() -> void
{
  setKitVisible(true);
  d->updateDevice();
}

auto DeviceProcessesDialog::currentProcess() const -> DeviceProcessItem
{
  return d->selectedProcess();
}

auto DeviceProcessesDialog::kitChooser() const -> KitChooser*
{
  return d->kitChooser;
}

auto DeviceProcessesDialog::logMessage(const QString &line) -> void
{
  d->errorText->setVisible(true);
  d->errorText->append(line);
}

} // namespace ProjectExplorer

#include "deviceprocessesdialog.moc"
