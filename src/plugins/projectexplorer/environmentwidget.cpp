// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environmentwidget.hpp"

#include <core/core-file-utils.hpp>
#include <core/core-item-view-find.hpp>

#include <utils/algorithm.hpp>
#include <utils/detailswidget.hpp>
#include <utils/environment.hpp>
#include <utils/environmentdialog.hpp>
#include <utils/environmentmodel.hpp>
#include <utils/headerviewstretcher.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/itemviews.hpp>
#include <utils/namevaluevalidator.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/tooltip/tooltip.hpp>

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {

class PathTreeWidget : public QTreeWidget {
public:
  auto sizeHint() const -> QSize override
  {
    return QSize(800, 600);
  }
};

class PathListDialog : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(EnvironmentWidget)

public:
  PathListDialog(const QString &varName, const QString &paths, QWidget *parent) : QDialog(parent)
  {
    const auto mainLayout = new QVBoxLayout(this);
    const auto viewLayout = new QHBoxLayout;
    const auto buttonsLayout = new QVBoxLayout;
    const auto addButton = new QPushButton(tr("Add..."));
    const auto removeButton = new QPushButton(tr("Remove"));
    const auto editButton = new QPushButton(tr("Edit..."));
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addWidget(editButton);
    buttonsLayout->addStretch(1);
    const auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    viewLayout->addWidget(&m_view);
    viewLayout->addLayout(buttonsLayout);
    mainLayout->addLayout(viewLayout);
    mainLayout->addWidget(buttonBox);

    m_view.setHeaderLabel(varName);
    m_view.setDragDropMode(QAbstractItemView::InternalMove);
    const auto pathList = paths.split(HostOsInfo::pathListSeparator(), Qt::SkipEmptyParts);
    for (const auto &path : pathList)
      addPath(path);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addButton, &QPushButton::clicked, this, [this] {
      const auto dir = FileUtils::getExistingDirectory(this, tr("Choose Directory"));
      if (!dir.isEmpty())
        addPath(dir.toUserOutput());
    });
    connect(removeButton, &QPushButton::clicked, this, [this] {
      const auto selected = m_view.selectedItems();
      QTC_ASSERT(selected.count() == 1, return);
      delete selected.first();
    });
    connect(editButton, &QPushButton::clicked, this, [this] {
      const auto selected = m_view.selectedItems();
      QTC_ASSERT(selected.count() == 1, return);
      m_view.editItem(selected.first(), 0);
    });
    const auto updateButtonStates = [this, removeButton, editButton] {
      const auto hasSelection = !m_view.selectedItems().isEmpty();
      removeButton->setEnabled(hasSelection);
      editButton->setEnabled(hasSelection);
    };
    connect(m_view.selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtonStates);
    updateButtonStates();
  }

  auto paths() const -> QString
  {
    QStringList pathList;
    for (auto i = 0; i < m_view.topLevelItemCount(); ++i)
      pathList << m_view.topLevelItem(i)->text(0);
    return pathList.join(HostOsInfo::pathListSeparator());
  }

private:
  auto addPath(const QString &path) -> void
  {
    const auto item = new QTreeWidgetItem(&m_view, {path});
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
  }

  PathTreeWidget m_view;
};

class EnvironmentDelegate : public QStyledItemDelegate {
public:
  EnvironmentDelegate(EnvironmentModel *model, QTreeView *view) : QStyledItemDelegate(view), m_model(model), m_view(view) {}

  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* override
  {
    const auto w = QStyledItemDelegate::createEditor(parent, option, index);
    if (index.column() != 0)
      return w;

    if (const auto edit = qobject_cast<QLineEdit*>(w))
      edit->setValidator(new NameValueValidator(edit, m_model, m_view, index, EnvironmentWidget::tr("Variable already exists.")));
    return w;
  }

private:
  EnvironmentModel *m_model;
  QTreeView *m_view;
};


////
// EnvironmentWidget::EnvironmentWidget
////

class EnvironmentWidgetPrivate {
public:
  EnvironmentModel *m_model;
  EnvironmentWidget::Type m_type = EnvironmentWidget::TypeLocal;
  QString m_baseEnvironmentText;
  EnvironmentWidget::OpenTerminalFunc m_openTerminalFunc;
  DetailsWidget *m_detailsContainer;
  QTreeView *m_environmentView;
  QPushButton *m_editButton;
  QPushButton *m_addButton;
  QPushButton *m_resetButton;
  QPushButton *m_unsetButton;
  QPushButton *m_toggleButton;
  QPushButton *m_batchEditButton;
  QPushButton *m_appendPathButton = nullptr;
  QPushButton *m_prependPathButton = nullptr;
  QPushButton *m_terminalButton;
};

EnvironmentWidget::EnvironmentWidget(QWidget *parent, Type type, QWidget *additionalDetailsWidget) : QWidget(parent), d(std::make_unique<EnvironmentWidgetPrivate>())
{
  d->m_model = new EnvironmentModel();
  d->m_type = type;
  connect(d->m_model, &EnvironmentModel::userChangesChanged, this, &EnvironmentWidget::userChangesChanged);
  connect(d->m_model, &QAbstractItemModel::modelReset, this, &EnvironmentWidget::invalidateCurrentIndex);

  connect(d->m_model, &EnvironmentModel::focusIndex, this, &EnvironmentWidget::focusIndex);

  const auto vbox = new QVBoxLayout(this);
  vbox->setContentsMargins(0, 0, 0, 0);

  d->m_detailsContainer = new DetailsWidget(this);

  const auto details = new QWidget(d->m_detailsContainer);
  d->m_detailsContainer->setWidget(details);
  details->setVisible(false);

  const auto vbox2 = new QVBoxLayout(details);
  vbox2->setContentsMargins(0, 0, 0, 0);

  if (additionalDetailsWidget)
    vbox2->addWidget(additionalDetailsWidget);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  auto tree = new TreeView(this);
  connect(tree, &QAbstractItemView::activated, tree, [tree](const QModelIndex &idx) { tree->edit(idx); });
  d->m_environmentView = tree;
  d->m_environmentView->setModel(d->m_model);
  d->m_environmentView->setItemDelegate(new EnvironmentDelegate(d->m_model, d->m_environmentView));
  d->m_environmentView->setMinimumHeight(400);
  d->m_environmentView->setRootIsDecorated(false);
  d->m_environmentView->setUniformRowHeights(true);
  const auto stretcher = new HeaderViewStretcher(d->m_environmentView->header(), 1);
  connect(d->m_model, &QAbstractItemModel::dataChanged, stretcher, &HeaderViewStretcher::softStretch);
  connect(d->m_model, &EnvironmentModel::userChangesChanged, stretcher, &HeaderViewStretcher::softStretch);
  d->m_environmentView->setSelectionMode(QAbstractItemView::SingleSelection);
  d->m_environmentView->setSelectionBehavior(QAbstractItemView::SelectItems);
  d->m_environmentView->setFrameShape(QFrame::NoFrame);
  const auto findWrapper = Orca::Plugin::Core::ItemViewFind::createSearchableWrapper(d->m_environmentView, Orca::Plugin::Core::ItemViewFind::LightColored);
  findWrapper->setFrameStyle(QFrame::StyledPanel);
  horizontalLayout->addWidget(findWrapper);

  const auto buttonLayout = new QVBoxLayout();

  d->m_editButton = new QPushButton(this);
  d->m_editButton->setText(tr("Ed&it"));
  buttonLayout->addWidget(d->m_editButton);

  d->m_addButton = new QPushButton(this);
  d->m_addButton->setText(tr("&Add"));
  buttonLayout->addWidget(d->m_addButton);

  d->m_resetButton = new QPushButton(this);
  d->m_resetButton->setEnabled(false);
  d->m_resetButton->setText(tr("&Reset"));
  buttonLayout->addWidget(d->m_resetButton);

  d->m_unsetButton = new QPushButton(this);
  d->m_unsetButton->setEnabled(false);
  d->m_unsetButton->setText(tr("&Unset"));
  buttonLayout->addWidget(d->m_unsetButton);

  d->m_toggleButton = new QPushButton(tr("Disable"), this);
  buttonLayout->addWidget(d->m_toggleButton);
  connect(d->m_toggleButton, &QPushButton::clicked, this, [this] {
    d->m_model->toggleVariable(d->m_environmentView->currentIndex());
    updateButtons();
  });

  if (type == TypeLocal) {
    d->m_appendPathButton = new QPushButton(this);
    d->m_appendPathButton->setEnabled(false);
    d->m_appendPathButton->setText(tr("Append Path..."));
    buttonLayout->addWidget(d->m_appendPathButton);
    d->m_prependPathButton = new QPushButton(this);
    d->m_prependPathButton->setEnabled(false);
    d->m_prependPathButton->setText(tr("Prepend Path..."));
    buttonLayout->addWidget(d->m_prependPathButton);
    connect(d->m_appendPathButton, &QAbstractButton::clicked, this, &EnvironmentWidget::appendPathButtonClicked);
    connect(d->m_prependPathButton, &QAbstractButton::clicked, this, &EnvironmentWidget::prependPathButtonClicked);
  }

  d->m_batchEditButton = new QPushButton(this);
  d->m_batchEditButton->setText(tr("&Batch Edit..."));
  buttonLayout->addWidget(d->m_batchEditButton);

  d->m_terminalButton = new QPushButton(this);
  d->m_terminalButton->setText(tr("Open &Terminal"));
  d->m_terminalButton->setToolTip(tr("Open a terminal with this environment set up."));
  d->m_terminalButton->setEnabled(type == TypeLocal);
  buttonLayout->addWidget(d->m_terminalButton);
  buttonLayout->addStretch();

  horizontalLayout->addLayout(buttonLayout);
  vbox2->addLayout(horizontalLayout);

  vbox->addWidget(d->m_detailsContainer);

  connect(d->m_model, &QAbstractItemModel::dataChanged, this, &EnvironmentWidget::updateButtons);

  connect(d->m_editButton, &QAbstractButton::clicked, this, &EnvironmentWidget::editEnvironmentButtonClicked);
  connect(d->m_addButton, &QAbstractButton::clicked, this, &EnvironmentWidget::addEnvironmentButtonClicked);
  connect(d->m_resetButton, &QAbstractButton::clicked, this, &EnvironmentWidget::removeEnvironmentButtonClicked);
  connect(d->m_unsetButton, &QAbstractButton::clicked, this, &EnvironmentWidget::unsetEnvironmentButtonClicked);
  connect(d->m_batchEditButton, &QAbstractButton::clicked, this, &EnvironmentWidget::batchEditEnvironmentButtonClicked);
  connect(d->m_environmentView->selectionModel(), &QItemSelectionModel::currentChanged, this, &EnvironmentWidget::environmentCurrentIndexChanged);
  connect(d->m_terminalButton, &QAbstractButton::clicked, this, [this] {
    auto env = d->m_model->baseEnvironment();
    env.modify(d->m_model->userChanges());
    if (d->m_openTerminalFunc)
      d->m_openTerminalFunc(env);
    else
      Orca::Plugin::Core::FileUtils::openTerminal(FilePath::fromString(QDir::currentPath()), env);
  });
  connect(d->m_detailsContainer, &DetailsWidget::linkActivated, this, &EnvironmentWidget::linkActivated);

  connect(d->m_model, &EnvironmentModel::userChangesChanged, this, &EnvironmentWidget::updateSummaryText);
}

EnvironmentWidget::~EnvironmentWidget()
{
  delete d->m_model;
  d->m_model = nullptr;
}

auto EnvironmentWidget::focusIndex(const QModelIndex &index) -> void
{
  d->m_environmentView->setCurrentIndex(index);
  d->m_environmentView->setFocus();
  // When the current item changes as a result of the call above,
  // QAbstractItemView::currentChanged() is called. That calls scrollTo(current),
  // using the default EnsureVisible scroll hint, whereas we want PositionAtTop,
  // because it ensures that the user doesn't have to scroll down when they've
  // added a new environment variable and want to edit its value; they'll be able
  // to see its value as they're typing it.
  // This only helps to a certain degree - variables whose names start with letters
  // later in the alphabet cause them fall within the "end" of the view's range,
  // making it impossible to position them at the top of the view.
  d->m_environmentView->scrollTo(index, QAbstractItemView::PositionAtTop);
}

auto EnvironmentWidget::setBaseEnvironment(const Environment &env) -> void
{
  d->m_model->setBaseEnvironment(env);
}

auto EnvironmentWidget::setBaseEnvironmentText(const QString &text) -> void
{
  d->m_baseEnvironmentText = text;
  updateSummaryText();
}

auto EnvironmentWidget::userChanges() const -> EnvironmentItems
{
  return d->m_model->userChanges();
}

auto EnvironmentWidget::setUserChanges(const EnvironmentItems &list) -> void
{
  d->m_model->setUserChanges(list);
  updateSummaryText();
}

auto EnvironmentWidget::setOpenTerminalFunc(const OpenTerminalFunc &func) -> void
{
  d->m_openTerminalFunc = func;
  d->m_terminalButton->setVisible(bool(func));
}

auto EnvironmentWidget::expand() -> void
{
  d->m_detailsContainer->setState(DetailsWidget::Expanded);
}

auto EnvironmentWidget::updateSummaryText() -> void
{
  auto list = d->m_model->userChanges();
  EnvironmentItem::sort(&list);

  QString text;
  foreach(const Utils::EnvironmentItem &item, list) {
    if (item.name != EnvironmentModel::tr("<VARIABLE>")) {
      if (!d->m_baseEnvironmentText.isEmpty() || !text.isEmpty())
        text.append(QLatin1String("<br>"));
      switch (item.operation) {
      case EnvironmentItem::Unset:
        text.append(tr("Unset <a href=\"%1\"><b>%1</b></a>").arg(item.name.toHtmlEscaped()));
        break;
      case EnvironmentItem::SetEnabled:
        text.append(tr("Set <a href=\"%1\"><b>%1</b></a> to <b>%2</b>").arg(item.name.toHtmlEscaped(), item.value.toHtmlEscaped()));
        break;
      case EnvironmentItem::Append:
        text.append(tr("Append <b>%2</b> to <a href=\"%1\"><b>%1</b></a>").arg(item.name.toHtmlEscaped(), item.value.toHtmlEscaped()));
        break;
      case EnvironmentItem::Prepend:
        text.append(tr("Prepend <b>%2</b> to <a href=\"%1\"><b>%1</b></a>").arg(item.name.toHtmlEscaped(), item.value.toHtmlEscaped()));
        break;
      case EnvironmentItem::SetDisabled:
        text.append(tr("Set <a href=\"%1\"><b>%1</b></a> to <b>%2</b> [disabled]").arg(item.name.toHtmlEscaped(), item.value.toHtmlEscaped()));
        break;
      }
    }
  }

  if (text.isEmpty()) {
    //: %1 is "System Environment" or some such.
    if (!d->m_baseEnvironmentText.isEmpty())
      text.prepend(tr("Use <b>%1</b>").arg(d->m_baseEnvironmentText));
    else
      text.prepend(tr("<b>No environment changes</b>"));
  } else {
    //: Yup, word puzzle. The Set/Unset phrases above are appended to this.
    //: %1 is "System Environment" or some such.
    if (!d->m_baseEnvironmentText.isEmpty())
      text.prepend(tr("Use <b>%1</b> and").arg(d->m_baseEnvironmentText));
  }

  d->m_detailsContainer->setSummaryText(text);
}

auto EnvironmentWidget::linkActivated(const QString &link) -> void
{
  d->m_detailsContainer->setState(DetailsWidget::Expanded);
  const auto idx = d->m_model->variableToIndex(link);
  focusIndex(idx);
}

auto EnvironmentWidget::updateButtons() -> void
{
  environmentCurrentIndexChanged(d->m_environmentView->currentIndex());
}

auto EnvironmentWidget::editEnvironmentButtonClicked() -> void
{
  const auto current = d->m_environmentView->currentIndex();
  if (current.column() == 1 && d->m_type == TypeLocal && d->m_model->currentEntryIsPathList(current)) {
    PathListDialog dlg(d->m_model->indexToVariable(current), d->m_model->data(current).toString(), this);
    if (dlg.exec() == QDialog::Accepted)
      d->m_model->setData(current, dlg.paths());
  } else {
    d->m_environmentView->edit(current);
  }
}

auto EnvironmentWidget::addEnvironmentButtonClicked() -> void
{
  const auto index = d->m_model->addVariable();
  d->m_environmentView->setCurrentIndex(index);
  d->m_environmentView->edit(index);
}

auto EnvironmentWidget::removeEnvironmentButtonClicked() -> void
{
  const auto &name = d->m_model->indexToVariable(d->m_environmentView->currentIndex());
  d->m_model->resetVariable(name);
}

// unset in Merged Environment Mode means, unset if it comes from the base environment
// or remove when it is just a change we added
auto EnvironmentWidget::unsetEnvironmentButtonClicked() -> void
{
  const auto &name = d->m_model->indexToVariable(d->m_environmentView->currentIndex());
  if (!d->m_model->canReset(name))
    d->m_model->resetVariable(name);
  else
    d->m_model->unsetVariable(name);
}

auto EnvironmentWidget::amendPathList(NameValueItem::Operation op) -> void
{
  const auto varName = d->m_model->indexToVariable(d->m_environmentView->currentIndex());
  const auto dir = FileUtils::getExistingDirectory(this, tr("Choose Directory"));
  if (dir.isEmpty())
    return;
  auto changes = d->m_model->userChanges();
  changes.append({varName, dir.toUserOutput(), op});
  d->m_model->setUserChanges(changes);
}

auto EnvironmentWidget::appendPathButtonClicked() -> void
{
  amendPathList(NameValueItem::Append);
}

auto EnvironmentWidget::prependPathButtonClicked() -> void
{
  amendPathList(NameValueItem::Prepend);
}

auto EnvironmentWidget::batchEditEnvironmentButtonClicked() -> void
{
  const auto changes = d->m_model->userChanges();

  const auto newChanges = EnvironmentDialog::getEnvironmentItems(this, changes);

  if (newChanges)
    d->m_model->setUserChanges(*newChanges);
}

auto EnvironmentWidget::environmentCurrentIndexChanged(const QModelIndex &current) -> void
{
  if (current.isValid()) {
    d->m_editButton->setEnabled(true);
    const auto &name = d->m_model->indexToVariable(current);
    const auto modified = d->m_model->canReset(name) && d->m_model->changes(name);
    const auto unset = d->m_model->isUnset(name);
    d->m_resetButton->setEnabled(modified || unset);
    d->m_unsetButton->setEnabled(!unset);
    d->m_toggleButton->setEnabled(!unset);
    d->m_toggleButton->setText(d->m_model->isEnabled(name) ? tr("Disable") : tr("Enable"));
  } else {
    d->m_editButton->setEnabled(false);
    d->m_resetButton->setEnabled(false);
    d->m_unsetButton->setEnabled(false);
    d->m_toggleButton->setEnabled(false);
    d->m_toggleButton->setText(tr("Disable"));
  }
  if (d->m_appendPathButton) {
    const auto isPathList = d->m_model->currentEntryIsPathList(current);
    d->m_appendPathButton->setEnabled(isPathList);
    d->m_prependPathButton->setEnabled(isPathList);
  }
}

auto EnvironmentWidget::invalidateCurrentIndex() -> void
{
  environmentCurrentIndexChanged(QModelIndex());
}

} // namespace ProjectExplorer
