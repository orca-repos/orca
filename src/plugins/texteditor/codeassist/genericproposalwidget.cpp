// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "genericproposalwidget.hpp"
#include "genericproposalmodel.hpp"
#include "assistproposalitem.hpp"
#include "codeassistant.hpp"

#include <texteditor/texteditorsettings.hpp>
#include <texteditor/completionsettings.hpp>
#include <texteditor/texteditorconstants.hpp>
#include <texteditor/codeassist/assistproposaliteminterface.hpp>

#include <utils/algorithm.hpp>
#include <utils/faketooltip.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/utilsicons.hpp>

#include <QRect>
#include <QLatin1String>
#include <QPointer>
#include <QTimer>
#include <QApplication>
#include <QVBoxLayout>
#include <QListView>
#include <QAbstractItemView>
#include <QScreen>
#include <QScrollBar>
#include <QKeyEvent>
#include <QLabel>
#include <QStyledItemDelegate>

using namespace Utils;

namespace TextEditor {

class ModelAdapter : public QAbstractListModel {
  Q_OBJECT public:
  ModelAdapter(GenericProposalModelPtr completionModel, QWidget *parent);

  auto rowCount(const QModelIndex &) const -> int override;
  auto data(const QModelIndex &index, int role) const -> QVariant override;

  enum UserRoles {
    FixItRole = Qt::UserRole,
    DetailTextFormatRole
  };

private:
  GenericProposalModelPtr m_completionModel;
};

ModelAdapter::ModelAdapter(GenericProposalModelPtr completionModel, QWidget *parent) : QAbstractListModel(parent), m_completionModel(completionModel) {}

auto ModelAdapter::rowCount(const QModelIndex &index) const -> int
{
  return index.isValid() ? 0 : m_completionModel->size();
}

auto ModelAdapter::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid() || index.row() >= m_completionModel->size())
    return QVariant();

  if (role == Qt::DisplayRole) {
    const auto text = m_completionModel->text(index.row());
    const int lineBreakPos = text.indexOf('\n');
    if (lineBreakPos < 0)
      return text;
    return QString(text.left(lineBreakPos) + QLatin1String(" (...)"));
  }
  if (role == Qt::DecorationRole) {
    return m_completionModel->icon(index.row());
  }
  if (role == Qt::WhatsThisRole) {
    return m_completionModel->detail(index.row());
  }
  if (role == DetailTextFormatRole) {
    return m_completionModel->detailFormat(index.row());
  }
  if (role == FixItRole) {
    return m_completionModel->proposalItem(index.row())->requiresFixIts();
  }

  return QVariant();
}

class GenericProposalInfoFrame : public FakeToolTip {
public:
  GenericProposalInfoFrame(QWidget *parent = nullptr) : FakeToolTip(parent), m_label(new QLabel(this))
  {
    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_label);

    // Limit horizontal width
    m_label->setSizePolicy(QSizePolicy::Fixed, m_label->sizePolicy().verticalPolicy());

    m_label->setForegroundRole(QPalette::ToolTipText);
    m_label->setBackgroundRole(QPalette::ToolTipBase);
  }

  auto setText(const QString &text) -> void
  {
    m_label->setText(text);
  }

  auto setTextFormat(Qt::TextFormat format) -> void
  {
    m_label->setTextFormat(format);
  }

  // Workaround QTCREATORBUG-11653
  auto calculateMaximumWidth() -> void
  {
    const auto screenGeometry = screen()->availableGeometry();
    const auto xOnScreen = this->pos().x() - screenGeometry.x();
    const auto widgetMargins = contentsMargins();
    const auto layoutMargins = layout()->contentsMargins();
    const auto margins = widgetMargins.left() + widgetMargins.right() + layoutMargins.left() + layoutMargins.right();
    m_label->setMaximumWidth(qMax(0, screenGeometry.width() - xOnScreen - margins));
  }

private:
  QLabel *m_label;
};

class GenericProposalListView : public QListView {
  friend class ProposalItemDelegate;

public:
  GenericProposalListView(QWidget *parent);

  auto calculateSize() const -> QSize;
  auto infoFramePos() const -> QPoint;
  auto rowSelected() const -> int { return currentIndex().row(); }
  auto isFirstRowSelected() const -> bool { return rowSelected() == 0; }
  auto isLastRowSelected() const -> bool { return rowSelected() == model()->rowCount() - 1; }
  auto selectRow(int row) -> void { setCurrentIndex(model()->index(row, 0)); }
  auto selectFirstRow() -> void { selectRow(0); }
  auto selectLastRow() -> void { selectRow(model()->rowCount() - 1); }
};

class ProposalItemDelegate : public QStyledItemDelegate {
  Q_OBJECT public:
  explicit ProposalItemDelegate(GenericProposalListView *parent = nullptr) : QStyledItemDelegate(parent), m_parent(parent) { }

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override
  {
    static const auto fixItIcon = Icons::CODEMODEL_FIXIT.icon();

    QStyledItemDelegate::paint(painter, option, index);

    if (m_parent->model()->data(index, ModelAdapter::FixItRole).toBool()) {
      const auto itemRect = m_parent->rectForIndex(index);
      const QScrollBar *verticalScrollBar = m_parent->verticalScrollBar();

      const auto x = m_parent->width() - itemRect.height() - (verticalScrollBar->isVisible() ? verticalScrollBar->width() : 0);
      const auto iconSize = itemRect.height() - 5;
      fixItIcon.paint(painter, QRect(x, itemRect.y() - m_parent->verticalOffset(), iconSize, iconSize));
    }
  }

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override
  {
    auto size(QStyledItemDelegate::sizeHint(option, index));
    if (m_parent->model()->data(index, ModelAdapter::FixItRole).toBool())
      size.setWidth(size.width() + m_parent->rectForIndex(index).height() - 5);
    return size;
  }

private:
  GenericProposalListView *m_parent;
};

GenericProposalListView::GenericProposalListView(QWidget *parent) : QListView(parent)
{
  setVerticalScrollMode(ScrollPerItem);
  setItemDelegate(new ProposalItemDelegate(this));
}

auto GenericProposalListView::calculateSize() const -> QSize
{
  static const auto maxVisibleItems = 10;

  // Determine size by calculating the space of the visible items
  const auto visibleItems = qMin(model()->rowCount(), maxVisibleItems);
  const auto firstVisibleRow = verticalScrollBar()->value();

  QSize shint;
  for (auto i = 0; i < visibleItems; ++i) {
    auto tmp = sizeHintForIndex(model()->index(i + firstVisibleRow, 0));
    if (shint.width() < tmp.width())
      shint = tmp;
  }
  shint.rheight() *= visibleItems;

  return shint;
}

auto GenericProposalListView::infoFramePos() const -> QPoint
{
  const auto &r = rectForIndex(currentIndex());
  const QPoint p(parentWidget()->mapToGlobal(parentWidget()->rect().topRight()).x() + 3, mapToGlobal(r.topRight()).y() - verticalOffset());
  return p;
}

class GenericProposalWidgetPrivate : public QObject {
  Q_OBJECT

public:
  GenericProposalWidgetPrivate(QWidget *completionWidget);

  const QWidget *m_underlyingWidget = nullptr;
  GenericProposalListView *m_completionListView;
  GenericProposalModelPtr m_model;
  QRect m_displayRect;
  bool m_isSynchronized = true;
  bool m_explicitlySelected = false;
  AssistReason m_reason = IdleEditor;
  AssistKind m_kind = Completion;
  bool m_justInvoked = false;
  QPointer<GenericProposalInfoFrame> m_infoFrame;
  QTimer m_infoTimer;
  CodeAssistant *m_assistant = nullptr;
  bool m_autoWidth = true;

  auto handleActivation(const QModelIndex &modelIndex) -> void;
  auto maybeShowInfoTip() -> void;
};

GenericProposalWidgetPrivate::GenericProposalWidgetPrivate(QWidget *completionWidget) : m_completionListView(new GenericProposalListView(completionWidget))
{
  m_completionListView->setIconSize(QSize(16, 16));
  connect(m_completionListView, &QAbstractItemView::activated, this, &GenericProposalWidgetPrivate::handleActivation);

  m_infoTimer.setInterval(Constants::COMPLETION_ASSIST_TOOLTIP_DELAY);
  m_infoTimer.setSingleShot(true);
  connect(&m_infoTimer, &QTimer::timeout, this, &GenericProposalWidgetPrivate::maybeShowInfoTip);
}

auto GenericProposalWidgetPrivate::handleActivation(const QModelIndex &modelIndex) -> void
{
  static_cast<GenericProposalWidget*>(m_completionListView->parent())->notifyActivation(modelIndex.row());
}

auto GenericProposalWidgetPrivate::maybeShowInfoTip() -> void
{
  const auto &current = m_completionListView->currentIndex();
  if (!current.isValid())
    return;

  const auto &infoTip = current.data(Qt::WhatsThisRole).toString();
  if (infoTip.isEmpty()) {
    delete m_infoFrame.data();
    m_infoTimer.setInterval(200);
    return;
  }

  if (m_infoFrame.isNull())
    m_infoFrame = new GenericProposalInfoFrame(m_completionListView);

  m_infoFrame->move(m_completionListView->infoFramePos());
  m_infoFrame->setTextFormat(current.data(ModelAdapter::DetailTextFormatRole).value<Qt::TextFormat>());
  m_infoFrame->setText(infoTip);
  m_infoFrame->calculateMaximumWidth();
  m_infoFrame->adjustSize();
  m_infoFrame->show();
  m_infoFrame->raise();

  m_infoTimer.setInterval(0);
}

GenericProposalWidget::GenericProposalWidget() : d(new GenericProposalWidgetPrivate(this))
{
  if (HostOsInfo::isMacHost()) {
    if (d->m_completionListView->horizontalScrollBar())
      d->m_completionListView->horizontalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
    if (d->m_completionListView->verticalScrollBar())
      d->m_completionListView->verticalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
  }
  // This improves the look with QGTKStyle.
  setFrameStyle(d->m_completionListView->frameStyle());
  d->m_completionListView->setFrameStyle(NoFrame);
  d->m_completionListView->setAttribute(Qt::WA_MacShowFocusRect, false);
  d->m_completionListView->setUniformItemSizes(true);
  d->m_completionListView->setSelectionBehavior(QAbstractItemView::SelectItems);
  d->m_completionListView->setSelectionMode(QAbstractItemView::SingleSelection);
  d->m_completionListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d->m_completionListView->setMinimumSize(1, 1);
  connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &GenericProposalWidget::updatePositionAndSize);
  connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::sliderPressed, this, &GenericProposalWidget::turnOffAutoWidth);
  connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::sliderReleased, this, &GenericProposalWidget::turnOnAutoWidth);

  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->m_completionListView);

  d->m_completionListView->installEventFilter(this);

  setObjectName(QLatin1String("m_popupFrame"));
  setMinimumSize(1, 1);
}

GenericProposalWidget::~GenericProposalWidget()
{
  delete d;
}

auto GenericProposalWidget::setAssistant(CodeAssistant *assistant) -> void
{
  d->m_assistant = assistant;
}

auto GenericProposalWidget::setReason(AssistReason reason) -> void
{
  d->m_reason = reason;
  if (d->m_reason == ExplicitlyInvoked)
    d->m_justInvoked = true;
}

auto GenericProposalWidget::setKind(AssistKind kind) -> void
{
  d->m_kind = kind;
}

auto GenericProposalWidget::setUnderlyingWidget(const QWidget *underlyingWidget) -> void
{
  setFont(underlyingWidget->font());
  d->m_underlyingWidget = underlyingWidget;
}

auto GenericProposalWidget::setModel(ProposalModelPtr model) -> void
{
  d->m_model = model.staticCast<GenericProposalModel>();
  d->m_completionListView->setModel(new ModelAdapter(d->m_model, d->m_completionListView));

  connect(d->m_completionListView->selectionModel(), &QItemSelectionModel::currentChanged, &d->m_infoTimer, QOverload<>::of(&QTimer::start));
}

auto GenericProposalWidget::setDisplayRect(const QRect &rect) -> void
{
  d->m_displayRect = rect;
}

auto GenericProposalWidget::setIsSynchronized(bool isSync) -> void
{
  d->m_isSynchronized = isSync;
}

auto GenericProposalWidget::supportsModelUpdate(const Id &proposalId) const -> bool
{
  return proposalId == Constants::GENERIC_PROPOSAL_ID;
}

auto GenericProposalWidget::updateModel(ProposalModelPtr model) -> void
{
  QString currentText;
  if (d->m_explicitlySelected)
    currentText = d->m_model->text(d->m_completionListView->currentIndex().row());
  d->m_model = model.staticCast<GenericProposalModel>();
  if (d->m_model->containsDuplicates())
    d->m_model->removeDuplicates();
  d->m_completionListView->setModel(new ModelAdapter(d->m_model, d->m_completionListView));
  connect(d->m_completionListView->selectionModel(), &QItemSelectionModel::currentChanged, &d->m_infoTimer, QOverload<>::of(&QTimer::start));
  auto currentRow = -1;
  if (!currentText.isEmpty()) {
    currentRow = d->m_model->indexOf(equal(&AssistProposalItemInterface::text, currentText));
  }
  if (currentRow >= 0)
    d->m_completionListView->selectRow(currentRow);
  else
    d->m_explicitlySelected = false;
}

auto GenericProposalWidget::showProposal(const QString &prefix) -> void
{
  ensurePolished();
  if (d->m_model->containsDuplicates())
    d->m_model->removeDuplicates();
  if (!updateAndCheck(prefix))
    return;
  show();
  d->m_completionListView->setFocus();
}

auto GenericProposalWidget::updateProposal(const QString &prefix) -> void
{
  if (!isVisible())
    return;
  updateAndCheck(prefix);
}

auto GenericProposalWidget::closeProposal() -> void
{
  abort();
}

auto GenericProposalWidget::notifyActivation(int index) -> void
{
  abort();
  emit proposalItemActivated(d->m_model->proposalItem(index));
}

auto GenericProposalWidget::abort() -> void
{
  deleteLater();
  if (isVisible())
    close();
}

auto GenericProposalWidget::updateAndCheck(const QString &prefix) -> bool
{
  // Keep track in the case there has been an explicit selection.
  auto preferredItemId = -1;
  if (d->m_explicitlySelected)
    preferredItemId = d->m_model->persistentId(d->m_completionListView->currentIndex().row());

  // Filter, sort, etc.
  if (!d->m_model->isPrefiltered(prefix)) {
    d->m_model->reset();
    if (!prefix.isEmpty())
      d->m_model->filter(prefix);
  }
  if (!d->m_model->hasItemsToPropose(prefix, d->m_reason)) {
    d->m_completionListView->reset();
    abort();
    return false;
  }
  if (d->m_model->isSortable(prefix))
    d->m_model->sort(prefix);
  d->m_completionListView->reset();

  // Try to find the previously explicit selection (if any). If we can find the item set it
  // as the current. Otherwise (it might have been filtered out) select the first row.
  if (d->m_explicitlySelected) {
    Q_ASSERT(preferredItemId != -1);
    for (auto i = 0; i < d->m_model->size(); ++i) {
      if (d->m_model->persistentId(i) == preferredItemId) {
        d->m_completionListView->selectRow(i);
        break;
      }
    }
  }
  if (!d->m_completionListView->currentIndex().isValid()) {
    d->m_completionListView->selectFirstRow();
    if (d->m_explicitlySelected)
      d->m_explicitlySelected = false;
  }

  if (TextEditorSettings::completionSettings().m_partiallyComplete && d->m_kind == Completion && d->m_justInvoked && d->m_isSynchronized) {
    if (d->m_model->size() == 1) {
      const auto item = d->m_model->proposalItem(0);
      if (item->implicitlyApplies()) {
        d->m_completionListView->reset();
        abort();
        emit proposalItemActivated(item);
        return false;
      }
    }
    if (d->m_model->supportsPrefixExpansion()) {
      const auto &proposalPrefix = d->m_model->proposalPrefix();
      if (proposalPrefix.length() > prefix.length()) emit prefixExpanded(proposalPrefix);
    }
  }

  if (d->m_justInvoked)
    d->m_justInvoked = false;

  updatePositionAndSize();
  return true;
}

auto GenericProposalWidget::updatePositionAndSize() -> void
{
  if (!d->m_autoWidth)
    return;

  const auto &shint = d->m_completionListView->calculateSize();
  const auto fw = frameWidth();
  const auto width = shint.width() + fw * 2 + 30;
  const auto height = shint.height() + fw * 2;

  // Determine the position, keeping the popup on the screen
  const auto screen = d->m_underlyingWidget->screen()->availableGeometry();

  auto pos = d->m_displayRect.bottomLeft();
  pos.rx() -= 16 + fw; // Space for the icons
  if (pos.y() + height > screen.bottom())
    pos.setY(qMax(0, d->m_displayRect.top() - height));
  if (pos.x() + width > screen.right())
    pos.setX(qMax(0, screen.right() - width));
  setGeometry(pos.x(), pos.y(), qMin(width, screen.width()), qMin(height, screen.height()));
}

auto GenericProposalWidget::turnOffAutoWidth() -> void
{
  d->m_autoWidth = false;
}

auto GenericProposalWidget::turnOnAutoWidth() -> void
{
  d->m_autoWidth = true;
  updatePositionAndSize();
}

auto GenericProposalWidget::eventFilter(QObject *o, QEvent *e) -> bool
{
  if (e->type() == QEvent::FocusOut) {
    abort();
    if (d->m_infoFrame)
      d->m_infoFrame->close();
    return true;
  }
  if (e->type() == QEvent::ShortcutOverride) {
    const auto ke = static_cast<QKeyEvent*>(e);
    switch (ke->key()) {
    case Qt::Key_N:
    case Qt::Key_P:
    case Qt::Key_BracketLeft:
      if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        e->accept();
        return true;
      }
    }
  } else if (e->type() == QEvent::KeyPress) {
    const auto ke = static_cast<QKeyEvent*>(e);
    switch (ke->key()) {
    case Qt::Key_Escape:
      abort();
      emit explicitlyAborted();
      e->accept();
      return true;

    case Qt::Key_BracketLeft:
      // vim-style behavior
      if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        abort();
        emit explicitlyAborted();
        e->accept();
        return true;
      }
      break;

    case Qt::Key_N:
    case Qt::Key_P:
      // select next/previous completion
      if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        d->m_explicitlySelected = true;
        const auto change = ke->key() == Qt::Key_N ? 1 : -1;
        const auto nrows = d->m_model->size();
        const auto row = d->m_completionListView->currentIndex().row();
        const auto newRow = (row + change + nrows) % nrows;
        if (newRow == row + change || !ke->isAutoRepeat())
          d->m_completionListView->selectRow(newRow);
        return true;
      }
      break;

    case Qt::Key_Tab:
    case Qt::Key_Return:
    case Qt::Key_Enter:
      abort();
      activateCurrentProposalItem();
      return true;

    case Qt::Key_Up:
      d->m_explicitlySelected = true;
      if (!ke->isAutoRepeat() && d->m_completionListView->isFirstRowSelected()) {
        d->m_completionListView->selectLastRow();
        return true;
      }
      return false;

    case Qt::Key_Down:
      d->m_explicitlySelected = true;
      if (!ke->isAutoRepeat() && d->m_completionListView->isLastRowSelected()) {
        d->m_completionListView->selectFirstRow();
        return true;
      }
      return false;

    case Qt::Key_PageDown:
    case Qt::Key_PageUp:
      return false;

    case Qt::Key_Right:
    case Qt::Key_Left:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Backspace:
      // We want these navigation keys to work in the editor.
      QApplication::sendEvent(const_cast<QWidget*>(d->m_underlyingWidget), e);
      if (isVisible())
        d->m_assistant->notifyChange();
      return true;

    default:
      // Only forward keys that insert text and refine the completion.
      if (ke->text().isEmpty() && !(ke == QKeySequence::Paste))
        return true;
      break;
    }

    if (ke->text().length() == 1 && d->m_completionListView->currentIndex().isValid() && QApplication::focusWidget() == o) {
      const auto &typedChar = ke->text().at(0);
      const auto item = d->m_model->proposalItem(d->m_completionListView->currentIndex().row());
      if (item->prematurelyApplies(typedChar) && (d->m_reason == ExplicitlyInvoked || item->text().endsWith(typedChar))) {
        abort();
        emit proposalItemActivated(item);
        return true;
      }
    }

    QApplication::sendEvent(const_cast<QWidget*>(d->m_underlyingWidget), e);

    return true;
  }
  return false;
}

auto GenericProposalWidget::activateCurrentProposalItem() -> bool
{
  if (d->m_completionListView->currentIndex().isValid()) {
    const auto currentRow = d->m_completionListView->currentIndex().row();
    emit proposalItemActivated(d->m_model->proposalItem(currentRow));
    return true;
  }
  return false;
}

auto GenericProposalWidget::model() -> GenericProposalModelPtr
{
  return d->m_model;
}

} // namespace TextEditor

#include "genericproposalwidget.moc"
