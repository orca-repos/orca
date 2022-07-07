// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "variablechooser.hpp"

#include "fancylineedit.hpp"
#include "headerviewstretcher.hpp" // IconButton
#include "macroexpander.hpp"
#include "treemodel.hpp"
#include "qtcassert.hpp"
#include "utilsicons.hpp"

#include <QApplication>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVector>

namespace Utils {
namespace Internal {

enum {
  UnexpandedTextRole = Qt::UserRole,
  ExpandedTextRole
};

class VariableTreeView : public QTreeView {
public:
  VariableTreeView(QWidget *parent, VariableChooserPrivate *target) : QTreeView(parent), m_target(target)
  {
    setAttribute(Qt::WA_MacSmallSize);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setIndentation(indentation() * 7 / 10);
    header()->hide();
    new HeaderViewStretcher(header(), 0);
  }

  auto contextMenuEvent(QContextMenuEvent *ev) -> void override;

  auto currentChanged(const QModelIndex &current, const QModelIndex &previous) -> void override;

private:
  VariableChooserPrivate *m_target;
};

class VariableSortFilterProxyModel : public QSortFilterProxyModel {
public:
  explicit VariableSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

  auto filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const -> bool override
  {
    const QModelIndex index = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);
    if (!index.isValid())
      return false;

    const QRegularExpression regexp = filterRegularExpression();
    if (regexp.pattern().isEmpty() || sourceModel()->rowCount(index) > 0)
      return true;

    const QString displayText = index.data(Qt::DisplayRole).toString();
    return displayText.contains(regexp);
  }
};

class VariableChooserPrivate : public QObject {
public:
  VariableChooserPrivate(VariableChooser *parent);

  auto createIconButton() -> void
  {
    m_iconButton = new IconButton;
    m_iconButton->setIcon(Utils::Icons::REPLACE.icon());
    m_iconButton->setToolTip(VariableChooser::tr("Insert Variable"));
    m_iconButton->hide();
    connect(m_iconButton.data(), static_cast<void(QAbstractButton::*)(bool)>(&QAbstractButton::clicked), this, &VariableChooserPrivate::updatePositionAndShow);
  }

  auto updateDescription(const QModelIndex &index) -> void;
  auto updateCurrentEditor(QWidget *old, QWidget *widget) -> void;
  auto handleItemActivated(const QModelIndex &index) -> void;
  auto insertText(const QString &variable) -> void;
  auto updatePositionAndShow(bool) -> void;
  auto updateFilter(const QString &filterText) -> void;

  auto currentWidget() const -> QWidget*;

  auto buttonMargin() const -> int;
  auto updateButtonGeometry() -> void;

public:
  VariableChooser *q;
  TreeModel<> m_model;

  QPointer<QLineEdit> m_lineEdit;
  QPointer<QTextEdit> m_textEdit;
  QPointer<QPlainTextEdit> m_plainTextEdit;
  QPointer<IconButton> m_iconButton;

  Utils::FancyLineEdit *m_variableFilter;
  VariableTreeView *m_variableTree;
  QLabel *m_variableDescription;
  QSortFilterProxyModel *m_sortModel;
  QString m_defaultDescription;
  QByteArray m_currentVariableName; // Prevent recursive insertion of currently expanded item
};

class VariableGroupItem : public TreeItem {
public:
  VariableGroupItem() = default;

  auto data(int column, int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
      if (column == 0)
        if (MacroExpander *expander = m_provider())
          return expander->displayName();
    }

    return QVariant();
  }

  auto canFetchMore() const -> bool override
  {
    return !m_populated;
  }

  auto fetchMore() -> void override
  {
    if (MacroExpander *expander = m_provider())
      populateGroup(expander);
    m_populated = true;
  }

  auto populateGroup(MacroExpander *expander) -> void;

public:
  VariableChooserPrivate *m_chooser = nullptr; // Not owned.
  bool m_populated = false;
  MacroExpanderProvider m_provider;
};

class VariableItem : public TypedTreeItem<TreeItem, VariableGroupItem> {
public:
  VariableItem() = default;

  auto flags(int) const -> Qt::ItemFlags override
  {
    if (m_variable == parent()->m_chooser->m_currentVariableName)
      return Qt::ItemIsSelectable;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
  }

  auto data(int column, int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
      if (column == 0)
        return m_variable;
    }

    if (role == Qt::ToolTipRole) {
      QString description = m_expander->variableDescription(m_variable);
      const QString value = m_expander->value(m_variable).toHtmlEscaped();
      if (!value.isEmpty())
        description += QLatin1String("<p>") + VariableChooser::tr("Current Value: %1").arg(value);
      return description;
    }

    if (role == UnexpandedTextRole)
      return QString::fromUtf8("%{" + m_variable + '}');

    if (role == ExpandedTextRole)
      return m_expander->expand(QString::fromUtf8("%{" + m_variable + '}'));

    return QVariant();
  }

public:
  MacroExpander *m_expander;
  QByteArray m_variable;
};

auto VariableTreeView::contextMenuEvent(QContextMenuEvent *ev) -> void
{
  const QModelIndex index = indexAt(ev->pos());

  QString unexpandedText = index.data(UnexpandedTextRole).toString();
  QString expandedText = index.data(ExpandedTextRole).toString();

  QMenu menu;
  QAction *insertUnexpandedAction = nullptr;
  QAction *insertExpandedAction = nullptr;

  if (unexpandedText.isEmpty()) {
    insertUnexpandedAction = menu.addAction(VariableChooser::tr("Insert Unexpanded Value"));
    insertUnexpandedAction->setEnabled(false);
  } else {
    insertUnexpandedAction = menu.addAction(VariableChooser::tr("Insert \"%1\"").arg(unexpandedText));
  }

  if (expandedText.isEmpty()) {
    insertExpandedAction = menu.addAction(VariableChooser::tr("Insert Expanded Value"));
    insertExpandedAction->setEnabled(false);
  } else {
    insertExpandedAction = menu.addAction(VariableChooser::tr("Insert \"%1\"").arg(expandedText));
  }

  QAction *act = menu.exec(ev->globalPos());

  if (act == insertUnexpandedAction)
    m_target->insertText(unexpandedText);
  else if (act == insertExpandedAction)
    m_target->insertText(expandedText);
}

auto VariableTreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous) -> void
{
  m_target->updateDescription(current);
  QTreeView::currentChanged(current, previous);
}

VariableChooserPrivate::VariableChooserPrivate(VariableChooser *parent) : q(parent), m_lineEdit(nullptr), m_textEdit(nullptr), m_plainTextEdit(nullptr), m_iconButton(nullptr), m_variableFilter(nullptr), m_variableTree(nullptr), m_variableDescription(nullptr)
{
  m_defaultDescription = VariableChooser::tr("Select a variable to insert.");

  m_variableFilter = new Utils::FancyLineEdit(q);
  m_variableTree = new VariableTreeView(q, this);
  m_variableDescription = new QLabel(q);

  m_variableFilter->setFiltering(true);

  m_sortModel = new VariableSortFilterProxyModel(this);
  m_sortModel->setSourceModel(&m_model);
  m_sortModel->sort(0);
  m_sortModel->setFilterKeyColumn(0);
  m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_variableTree->setModel(m_sortModel);

  m_variableDescription->setText(m_defaultDescription);
  m_variableDescription->setMinimumSize(QSize(0, 60));
  m_variableDescription->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_variableDescription->setWordWrap(true);
  m_variableDescription->setAttribute(Qt::WA_MacSmallSize);
  m_variableDescription->setTextInteractionFlags(Qt::TextBrowserInteraction);

  auto verticalLayout = new QVBoxLayout(q);
  verticalLayout->setContentsMargins(3, 3, 3, 12);
  verticalLayout->addWidget(m_variableFilter);
  verticalLayout->addWidget(m_variableTree);
  verticalLayout->addWidget(m_variableDescription);

  connect(m_variableFilter, &QLineEdit::textChanged, this, &VariableChooserPrivate::updateFilter);
  connect(m_variableTree, &QTreeView::activated, this, &VariableChooserPrivate::handleItemActivated);
  connect(qobject_cast<QApplication*>(qApp), &QApplication::focusChanged, this, &VariableChooserPrivate::updateCurrentEditor);
  updateCurrentEditor(nullptr, QApplication::focusWidget());
}

auto VariableGroupItem::populateGroup(MacroExpander *expander) -> void
{
  if (!expander)
    return;

  foreach(const QByteArray &variable, expander->visibleVariables()) {
    auto item = new VariableItem;
    item->m_variable = variable;
    item->m_expander = expander;
    appendChild(item);
  }

  foreach(const MacroExpanderProvider &subProvider, expander->subProviders()) {
    if (!subProvider)
      continue;
    if (expander->isAccumulating()) {
      populateGroup(subProvider());
    } else {
      auto item = new VariableGroupItem;
      item->m_chooser = m_chooser;
      item->m_provider = subProvider;
      appendChild(item);
    }
  }
}

} // namespace Internal

using namespace Internal;

/*!
    \class Utils::VariableChooser
    \inheaderfile coreplugin/variablechooser.h
    \inmodule Orca

    \brief The VariableChooser class is used to add a tool window for selecting \QC variables
    to line edits, text edits or plain text edits.

    If you allow users to add \QC variables to strings that are specified in your UI, for example
    when users can provide a string through a text control, you should add a variable chooser to it.
    The variable chooser allows users to open a tool window that contains the list of
    all available variables together with a description. Double-clicking a variable inserts the
    corresponding string into the corresponding text control like a line edit.

    \image variablechooser.png "External Tools Preferences with Variable Chooser"

    The variable chooser monitors focus changes of all children of its parent widget.
    When a text control gets focus, the variable chooser checks if it has variable support set.
    If the control supports variables,
    a tool button which opens the variable chooser is shown in it while it has focus.

    Supported text controls are QLineEdit, QTextEdit and QPlainTextEdit.

    The variable chooser is deleted when its parent widget is deleted.

    Example:
    \code
    QWidget *myOptionsContainerWidget = new QWidget;
    new Utils::VariableChooser(myOptionsContainerWidget)
    QLineEdit *myLineEditOption = new QLineEdit(myOptionsContainerWidget);
    myOptionsContainerWidget->layout()->addWidget(myLineEditOption);
    Utils::VariableChooser::addVariableSupport(myLineEditOption);
    \endcode
*/

/*!
 * \internal
 * \variable VariableChooser::kVariableSupportProperty
 * Property name that is checked for deciding if a widget supports \QC variables.
 * Can be manually set with
 * \c{textcontrol->setProperty(VariableChooser::kVariableSupportProperty, true)}
 */
const char kVariableSupportProperty[] = "Orca.VariableSupport";
const char kVariableNameProperty[] = "Orca.VariableName";

/*!
 * Creates a variable chooser that tracks all children of \a parent for variable support.
 * Ownership is also transferred to \a parent.
 */
VariableChooser::VariableChooser(QWidget *parent) : QWidget(parent), d(new VariableChooserPrivate(this))
{
  setWindowTitle(tr("Variables"));
  setWindowFlags(Qt::Tool);
  setFocusPolicy(Qt::StrongFocus);
  setFocusProxy(d->m_variableTree);
  setGeometry(QRect(0, 0, 400, 500));
  addMacroExpanderProvider([]() { return globalMacroExpander(); });
}

/*!
 * \internal
 */
VariableChooser::~VariableChooser()
{
  delete d->m_iconButton;
  delete d;
}

/*!
    Adds the macro expander provider \a provider.
*/
auto VariableChooser::addMacroExpanderProvider(const MacroExpanderProvider &provider) -> void
{
  auto item = new VariableGroupItem;
  item->m_chooser = d;
  item->m_provider = provider;
  d->m_model.rootItem()->prependChild(item);
}

/*!
 * Marks the control \a textcontrol as supporting variables.
 *
 * If the control provides a variable to the macro expander itself, set
 * \a ownName to the variable name to prevent the user from choosing the
 * variable, which would lead to endless recursion.
 */
auto VariableChooser::addSupportedWidget(QWidget *textcontrol, const QByteArray &ownName) -> void
{
  QTC_ASSERT(textcontrol, return);
  textcontrol->setProperty(kVariableSupportProperty, QVariant::fromValue<QWidget*>(this));
  textcontrol->setProperty(kVariableNameProperty, ownName);
}

auto VariableChooser::addSupportForChildWidgets(QWidget *parent, MacroExpander *expander) -> void
{
  auto chooser = new VariableChooser(parent);
  chooser->addMacroExpanderProvider([expander] { return expander; });
  foreach(QWidget *child, parent->findChildren<QWidget *>()) {
    if (qobject_cast<QLineEdit*>(child) || qobject_cast<QTextEdit*>(child) || qobject_cast<QPlainTextEdit*>(child))
      chooser->addSupportedWidget(child);
  }
}

/*!
 * \internal
 */
auto VariableChooserPrivate::updateDescription(const QModelIndex &index) -> void
{
  if (m_variableDescription)
    m_variableDescription->setText(m_model.data(m_sortModel->mapToSource(index), Qt::ToolTipRole).toString());
}

/*!
 * \internal
 */
auto VariableChooserPrivate::buttonMargin() const -> int
{
  return 24;
}

auto VariableChooserPrivate::updateButtonGeometry() -> void
{
  QWidget *current = currentWidget();
  int margin = buttonMargin();
  int rightPadding = 0;
  if (const auto scrollArea = qobject_cast<const QAbstractScrollArea*>(current)) {
    rightPadding = scrollArea->verticalScrollBar()->isVisible() ? scrollArea->verticalScrollBar()->width() : 0;
  }
  m_iconButton->setGeometry(current->rect().adjusted(current->width() - (margin + 4), 0, 0, -qMax(0, current->height() - (margin + 4))).translated(-rightPadding, 0));
}

auto VariableChooserPrivate::updateCurrentEditor(QWidget *old, QWidget *widget) -> void
{
  Q_UNUSED(old)
  if (!widget) // we might loose focus, but then keep the previous state
    return;
  // prevent children of the chooser itself, and limit to children of chooser's parent
  bool handle = false;
  QWidget *parent = widget;
  while (parent) {
    if (parent == q)
      return;
    if (parent == q->parentWidget()) {
      handle = true;
      break;
    }
    parent = parent->parentWidget();
  }
  if (!handle)
    return;

  QLineEdit *previousLineEdit = m_lineEdit;
  QWidget *previousWidget = currentWidget();
  m_lineEdit = nullptr;
  m_textEdit = nullptr;
  m_plainTextEdit = nullptr;
  auto chooser = widget->property(kVariableSupportProperty).value<QWidget*>();
  m_currentVariableName = widget->property(kVariableNameProperty).toByteArray();
  bool supportsVariables = chooser == q;
  if (auto lineEdit = qobject_cast<QLineEdit*>(widget))
    m_lineEdit = (supportsVariables ? lineEdit : nullptr);
  else if (auto textEdit = qobject_cast<QTextEdit*>(widget))
    m_textEdit = (supportsVariables ? textEdit : nullptr);
  else if (auto plainTextEdit = qobject_cast<QPlainTextEdit*>(widget))
    m_plainTextEdit = (supportsVariables ? plainTextEdit : nullptr);

  QWidget *current = currentWidget();
  if (current != previousWidget) {
    if (previousWidget)
      previousWidget->removeEventFilter(q);
    if (previousLineEdit)
      previousLineEdit->setTextMargins(0, 0, 0, 0);
    if (m_iconButton) {
      m_iconButton->hide();
      m_iconButton->setParent(nullptr);
    }
    if (current) {
      current->installEventFilter(q); // escape key handling and geometry changes
      if (!m_iconButton)
        createIconButton();
      int margin = buttonMargin();
      if (m_lineEdit)
        m_lineEdit->setTextMargins(0, 0, margin, 0);
      m_iconButton->setParent(current);
      updateButtonGeometry();
      m_iconButton->show();
    } else {
      q->hide();
    }
  }
}

/*!
 * \internal
 */
auto VariableChooserPrivate::updatePositionAndShow(bool) -> void
{
  if (QWidget *w = q->parentWidget()) {
    QPoint parentCenter = w->mapToGlobal(w->geometry().center());
    q->move(parentCenter.x() - q->width() / 2, qMax(parentCenter.y() - q->height() / 2, 0));
  }
  q->show();
  q->raise();
  q->activateWindow();
  m_variableTree->expandAll();
}

auto VariableChooserPrivate::updateFilter(const QString &filterText) -> void
{
  const QString pattern = QRegularExpression::escape(filterText);
  m_sortModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
  m_variableTree->expandAll();
}

/*!
 * \internal
 */
auto VariableChooserPrivate::currentWidget() const -> QWidget*
{
  if (m_lineEdit)
    return m_lineEdit;
  if (m_textEdit)
    return m_textEdit;
  return m_plainTextEdit;
}

/*!
 * \internal
 */
auto VariableChooserPrivate::handleItemActivated(const QModelIndex &index) -> void
{
  QString text = m_model.data(m_sortModel->mapToSource(index), UnexpandedTextRole).toString();
  if (!text.isEmpty())
    insertText(text);
}

/*!
 * \internal
 */
auto VariableChooserPrivate::insertText(const QString &text) -> void
{
  if (m_lineEdit) {
    m_lineEdit->insert(text);
    m_lineEdit->activateWindow();
  } else if (m_textEdit) {
    m_textEdit->insertPlainText(text);
    m_textEdit->activateWindow();
  } else if (m_plainTextEdit) {
    m_plainTextEdit->insertPlainText(text);
    m_plainTextEdit->activateWindow();
  }
}

/*!
 * \internal
 */
static auto handleEscapePressed(QKeyEvent *ke, QWidget *widget) -> bool
{
  if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
    ke->accept();
    QTimer::singleShot(0, widget, &QWidget::close);
    return true;
  }
  return false;
}

/*!
 * \internal
 */
auto VariableChooser::event(QEvent *ev) -> bool
{
  if (ev->type() == QEvent::KeyPress || ev->type() == QEvent::ShortcutOverride) {
    auto ke = static_cast<QKeyEvent*>(ev);
    if (handleEscapePressed(ke, this))
      return true;
  }
  return QWidget::event(ev);
}

/*!
 * \internal
 */
auto VariableChooser::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj != d->currentWidget())
    return false;
  if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) && isVisible()) {
    auto ke = static_cast<QKeyEvent*>(event);
    return handleEscapePressed(ke, this);
  } else if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) {
    d->updateButtonGeometry();
  } else if (event->type() == QEvent::Hide) {
    close();
  }
  return false;
}

} // namespace Internal
