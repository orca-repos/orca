// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "functionhintproposalwidget.hpp"
#include "ifunctionhintproposalmodel.hpp"
#include "codeassistant.hpp"

#include <utils/algorithm.hpp>
#include <utils/faketooltip.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QApplication>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPointer>
#include <QScreen>

namespace TextEditor {

static const int maxSelectedFunctionHints = 20;

class SelectedFunctionHints {
public:
  auto insert(int basePosition, const QString &hintId) -> void
  {
    if (basePosition < 0 || hintId.isEmpty())
      return;

    const auto index = indexOf(basePosition);

    // Add new item
    if (index == -1) {
      if (m_items.size() + 1 > maxSelectedFunctionHints)
        m_items.removeLast();
      m_items.prepend(FunctionHintItem(basePosition, hintId));
      return;
    }

    // Update existing item
    m_items[index].hintId = hintId;
  }

  auto hintId(int basePosition) const -> QString
  {
    const auto index = indexOf(basePosition);
    return index == -1 ? QString() : m_items.at(index).hintId;
  }

private:
  auto indexOf(int basePosition) const -> int
  {
    return Utils::indexOf(m_items, [&](const FunctionHintItem &item) {
      return item.basePosition == basePosition;
    });
  }

  struct FunctionHintItem {
    FunctionHintItem(int basePosition, const QString &hintId) : basePosition(basePosition), hintId(hintId) {}

    int basePosition = -1;
    QString hintId;
  };

  QList<FunctionHintItem> m_items;
};

struct FunctionHintProposalWidgetPrivate {
  FunctionHintProposalWidgetPrivate();

  const QWidget *m_underlyingWidget = nullptr;
  CodeAssistant *m_assistant = nullptr;
  FunctionHintProposalModelPtr m_model;
  QPointer<Utils::FakeToolTip> m_popupFrame; // guard WA_DeleteOnClose widget
  QLabel *m_numberLabel = nullptr;
  QLabel *m_hintLabel = nullptr;
  QWidget *m_pager = nullptr;
  QRect m_displayRect;
  int m_currentHint = -1;
  int m_totalHints = 0;
  int m_currentArgument = -1;
  bool m_escapePressed = false;
};

FunctionHintProposalWidgetPrivate::FunctionHintProposalWidgetPrivate() : m_popupFrame(new Utils::FakeToolTip), m_numberLabel(new QLabel), m_hintLabel(new QLabel), m_pager(new QWidget)
{
  m_hintLabel->setTextFormat(Qt::RichText);
}

FunctionHintProposalWidget::FunctionHintProposalWidget() : d(new FunctionHintProposalWidgetPrivate)
{
  const auto downArrow = new QToolButton;
  downArrow->setArrowType(Qt::DownArrow);
  downArrow->setFixedSize(16, 16);
  downArrow->setAutoRaise(true);

  const auto upArrow = new QToolButton;
  upArrow->setArrowType(Qt::UpArrow);
  upArrow->setFixedSize(16, 16);
  upArrow->setAutoRaise(true);

  const auto pagerLayout = new QHBoxLayout(d->m_pager);
  pagerLayout->setContentsMargins(0, 0, 0, 0);
  pagerLayout->setSpacing(0);
  pagerLayout->addWidget(upArrow);
  pagerLayout->addWidget(d->m_numberLabel);
  pagerLayout->addWidget(downArrow);

  const auto popupLayout = new QHBoxLayout(d->m_popupFrame);
  popupLayout->setContentsMargins(0, 0, 0, 0);
  popupLayout->setSpacing(0);
  popupLayout->addWidget(d->m_pager);
  popupLayout->addWidget(d->m_hintLabel);

  connect(upArrow, &QAbstractButton::clicked, this, &FunctionHintProposalWidget::previousPage);
  connect(downArrow, &QAbstractButton::clicked, this, &FunctionHintProposalWidget::nextPage);
  connect(d->m_popupFrame.data(), &QObject::destroyed, this, [this]() {
    qApp->removeEventFilter(this);
    deleteLater();
  });

  setFocusPolicy(Qt::NoFocus);
}

FunctionHintProposalWidget::~FunctionHintProposalWidget()
{
  delete d;
}

auto FunctionHintProposalWidget::setAssistant(CodeAssistant *assistant) -> void
{
  d->m_assistant = assistant;
}

auto FunctionHintProposalWidget::setReason(AssistReason) -> void {}

auto FunctionHintProposalWidget::setKind(AssistKind) -> void {}

auto FunctionHintProposalWidget::setUnderlyingWidget(const QWidget *underlyingWidget) -> void
{
  d->m_underlyingWidget = underlyingWidget;
}

auto FunctionHintProposalWidget::setModel(ProposalModelPtr model) -> void
{
  d->m_model = model.staticCast<IFunctionHintProposalModel>();
}

auto FunctionHintProposalWidget::setDisplayRect(const QRect &rect) -> void
{
  d->m_displayRect = rect;
}

auto FunctionHintProposalWidget::setIsSynchronized(bool) -> void {}

auto FunctionHintProposalWidget::showProposal(const QString &prefix) -> void
{
  QTC_ASSERT(d->m_model && d->m_assistant, abort(); return;);

  d->m_totalHints = d->m_model->size();
  QTC_ASSERT(d->m_totalHints != 0, abort(); return;);

  d->m_pager->setVisible(d->m_totalHints > 1);
  d->m_currentHint = loadSelectedHint();
  if (!updateAndCheck(prefix))
    return;

  qApp->installEventFilter(this);
  d->m_popupFrame->show();
}

auto FunctionHintProposalWidget::updateProposal(const QString &prefix) -> void
{
  updateAndCheck(prefix);
}

auto FunctionHintProposalWidget::closeProposal() -> void
{
  abort();
}

auto FunctionHintProposalWidget::proposalIsVisible() const -> bool
{
  return d->m_popupFrame && d->m_popupFrame->isVisible();
}

auto FunctionHintProposalWidget::abort() -> void
{
  qApp->removeEventFilter(this);
  if (proposalIsVisible())
    d->m_popupFrame->close();
  deleteLater();
}

static auto selectedFunctionHints(CodeAssistant &codeAssistant) -> SelectedFunctionHints
{
  const auto variant = codeAssistant.userData();
  return variant.value<SelectedFunctionHints>();
}

auto FunctionHintProposalWidget::loadSelectedHint() const -> int
{
  const auto hintId = selectedFunctionHints(*d->m_assistant).hintId(basePosition());

  for (auto i = 0; i < d->m_model->size(); ++i) {
    if (d->m_model->id(i) == hintId)
      return i;
  }

  return 0;
}

auto FunctionHintProposalWidget::storeSelectedHint() -> void
{
  auto table = selectedFunctionHints(*d->m_assistant);
  table.insert(basePosition(), d->m_model->id(d->m_currentHint));

  d->m_assistant->setUserData(QVariant::fromValue(table));
}

auto FunctionHintProposalWidget::eventFilter(QObject *obj, QEvent *e) -> bool
{
  switch (e->type()) {
  case QEvent::ShortcutOverride:
    if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
      d->m_escapePressed = true;
      e->accept();
    }
    break;
  case QEvent::KeyPress:
    if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
      d->m_escapePressed = true;
      e->accept();
    }
    QTC_CHECK(d->m_model);
    if (d->m_model && d->m_model->size() > 1) {
      const auto ke = static_cast<QKeyEvent*>(e);
      if (ke->key() == Qt::Key_Up) {
        previousPage();
        return true;
      }
      if (ke->key() == Qt::Key_Down) {
        nextPage();
        return true;
      }
      return false;
    }
    break;
  case QEvent::KeyRelease: {
    const auto ke = static_cast<QKeyEvent*>(e);
    if (ke->key() == Qt::Key_Escape && d->m_escapePressed) {
      abort();
      emit explicitlyAborted();
      return false;
    }
    if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
      QTC_CHECK(d->m_model);
      if (d->m_model && d->m_model->size() > 1)
        return false;
    }
    if (QTC_GUARD(d->m_assistant))
      d->m_assistant->notifyChange();
  }
  break;
  case QEvent::WindowDeactivate:
  case QEvent::FocusOut:
    if (obj != d->m_underlyingWidget)
      break;
    abort();
    break;
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
  case QEvent::Wheel:
    if (const auto widget = qobject_cast<QWidget*>(obj)) {
      if (d->m_popupFrame && !d->m_popupFrame->isAncestorOf(widget)) {
        abort();
      } else if (e->type() == QEvent::Wheel) {
        if (static_cast<QWheelEvent*>(e)->angleDelta().y() > 0)
          previousPage();
        else
          nextPage();
        return true;
      }
    }
    break;
  default:
    break;
  }
  return false;
}

auto FunctionHintProposalWidget::nextPage() -> void
{
  d->m_currentHint = (d->m_currentHint + 1) % d->m_totalHints;

  storeSelectedHint();
  updateContent();
}

auto FunctionHintProposalWidget::previousPage() -> void
{
  if (d->m_currentHint == 0)
    d->m_currentHint = d->m_totalHints - 1;
  else
    --d->m_currentHint;

  storeSelectedHint();
  updateContent();
}

auto FunctionHintProposalWidget::updateAndCheck(const QString &prefix) -> bool
{
  const auto activeArgument = d->m_model->activeArgument(prefix);
  if (activeArgument == -1) {
    abort();
    return false;
  }
  if (activeArgument != d->m_currentArgument) {
    d->m_currentArgument = activeArgument;
    updateContent();
  }

  return true;
}

auto FunctionHintProposalWidget::updateContent() -> void
{
  d->m_hintLabel->setText(d->m_model->text(d->m_currentHint));
  d->m_numberLabel->setText(tr("%1 of %2").arg(d->m_currentHint + 1).arg(d->m_totalHints));
  updatePosition();
}

auto FunctionHintProposalWidget::updatePosition() -> void
{
  const auto widgetScreen = d->m_underlyingWidget->screen();
  const auto &screen = Utils::HostOsInfo::isMacHost() ? widgetScreen->availableGeometry() : widgetScreen->geometry();

  d->m_pager->setFixedWidth(d->m_pager->minimumSizeHint().width());

  d->m_hintLabel->setWordWrap(false);
  const auto maxDesiredWidth = screen.width() - 10;
  const auto &minHint = d->m_popupFrame->minimumSizeHint();
  if (minHint.width() > maxDesiredWidth) {
    d->m_hintLabel->setWordWrap(true);
    d->m_popupFrame->setFixedWidth(maxDesiredWidth);
    const auto extra = d->m_popupFrame->contentsMargins().bottom() + d->m_popupFrame->contentsMargins().top();
    d->m_popupFrame->setFixedHeight(d->m_hintLabel->heightForWidth(maxDesiredWidth - d->m_pager->width()) + extra);
  } else {
    d->m_popupFrame->setFixedSize(minHint);
  }

  const auto &sz = d->m_popupFrame->size();
  auto pos = d->m_displayRect.topLeft();
  pos.setY(pos.y() - sz.height() - 1);
  if (pos.x() + sz.width() > screen.right())
    pos.setX(screen.right() - sz.width());
  d->m_popupFrame->move(pos);
}

} // TextEditor

Q_DECLARE_METATYPE(TextEditor::SelectedFunctionHints)
