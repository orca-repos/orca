// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "infobar.h"

#include "algorithm.h"
#include "qtcassert.h"
#include "qtcsettings.h"
#include "theme/theme.h"
#include "utilsicons.h"

#include <QHBoxLayout>
#include <QSettings>
#include <QVBoxLayout>
#include <QLabel>
#include <QPaintEngine>
#include <QToolButton>
#include <QComboBox>

static constexpr char C_SUPPRESSED_WARNINGS[] = "SuppressedWarnings";

namespace Utils {

QSet<Id> InfoBar::globallySuppressed;
QSettings *InfoBar::m_settings = nullptr;

class InfoBarWidget : public QWidget {
public:
  InfoBarWidget(Qt::Edge edge, QWidget *parent = nullptr);

protected:
  auto paintEvent(QPaintEvent *event) -> void override;

private:
  const Qt::Edge m_edge;
};

InfoBarWidget::InfoBarWidget(Qt::Edge edge, QWidget *parent) : QWidget(parent), m_edge(edge)
{
  const bool topEdge = m_edge == Qt::TopEdge;
  setContentsMargins(2, topEdge ? 0 : 1, 0, topEdge ? 1 : 0);
}

auto InfoBarWidget::paintEvent(QPaintEvent *event) -> void
{
  QWidget::paintEvent(event);
  QPainter p(this);
  p.fillRect(rect(), orcaTheme()->color(Theme::InfoBarBackground));
  const QRectF adjustedRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const bool topEdge = m_edge == Qt::TopEdge;
  p.setPen(orcaTheme()->color(Theme::FancyToolBarSeparatorColor));
  p.drawLine(QLineF(topEdge ? adjustedRect.bottomLeft() : adjustedRect.topLeft(), topEdge ? adjustedRect.bottomRight() : adjustedRect.topRight()));
}

InfoBarEntry::InfoBarEntry(Id _id, const QString &_infoText, GlobalSuppression _globalSuppression) : m_id(_id), m_infoText(_infoText), m_globalSuppression(_globalSuppression) {}

auto InfoBarEntry::addCustomButton(const QString &buttonText, CallBack callBack) -> void
{
  m_buttons.append({buttonText, callBack});
}

auto InfoBarEntry::setCancelButtonInfo(CallBack callBack) -> void
{
  m_useCancelButton = true;
  m_cancelButtonCallBack = callBack;
}

auto InfoBarEntry::setCancelButtonInfo(const QString &_cancelButtonText, CallBack callBack) -> void
{
  m_useCancelButton = true;
  m_cancelButtonText = _cancelButtonText;
  m_cancelButtonCallBack = callBack;
}

auto InfoBarEntry::setComboInfo(const QStringList &list, InfoBarEntry::ComboCallBack callBack) -> void
{
  m_comboCallBack = callBack;
  m_comboInfo = list;
}

auto InfoBarEntry::removeCancelButton() -> void
{
  m_useCancelButton = false;
  m_cancelButtonText.clear();
  m_cancelButtonCallBack = nullptr;
}

auto InfoBarEntry::setDetailsWidgetCreator(const InfoBarEntry::DetailsWidgetCreator &creator) -> void
{
  m_detailsWidgetCreator = creator;
}

auto InfoBar::addInfo(const InfoBarEntry &info) -> void
{
  m_infoBarEntries << info;
  emit changed();
}

auto InfoBar::removeInfo(Id id) -> void
{
  const int size = m_infoBarEntries.size();
  Utils::erase(m_infoBarEntries, Utils::equal(&InfoBarEntry::m_id, id));
  if (size != m_infoBarEntries.size()) emit changed();
}

auto InfoBar::containsInfo(Id id) const -> bool
{
  return Utils::anyOf(m_infoBarEntries, Utils::equal(&InfoBarEntry::m_id, id));
}

// Remove and suppress id
auto InfoBar::suppressInfo(Id id) -> void
{
  removeInfo(id);
  m_suppressed << id;
}

// Info cannot be added more than once, or if it is suppressed
auto InfoBar::canInfoBeAdded(Id id) const -> bool
{
  return !containsInfo(id) && !m_suppressed.contains(id) && !globallySuppressed.contains(id);
}

auto InfoBar::unsuppressInfo(Id id) -> void
{
  m_suppressed.remove(id);
}

auto InfoBar::clear() -> void
{
  if (!m_infoBarEntries.isEmpty()) {
    m_infoBarEntries.clear();
    emit changed();
  }
}

auto InfoBar::globallySuppressInfo(Id id) -> void
{
  globallySuppressed.insert(id);
  writeGloballySuppressedToSettings();
}

auto InfoBar::globallyUnsuppressInfo(Id id) -> void
{
  globallySuppressed.remove(id);
  writeGloballySuppressedToSettings();
}

auto InfoBar::initialize(QSettings *settings) -> void
{
  m_settings = settings;

  if (QTC_GUARD(m_settings)) {
    const QStringList list = m_settings->value(QLatin1String(C_SUPPRESSED_WARNINGS)).toStringList();
    globallySuppressed = Utils::transform<QSet>(list, Id::fromString);
  }
}

auto InfoBar::clearGloballySuppressed() -> void
{
  globallySuppressed.clear();
  if (m_settings)
    m_settings->remove(C_SUPPRESSED_WARNINGS);
}

auto InfoBar::anyGloballySuppressed() -> bool
{
  return !globallySuppressed.isEmpty();
}

auto InfoBar::writeGloballySuppressedToSettings() -> void
{
  if (!m_settings)
    return;
  const QStringList list = Utils::transform<QList>(globallySuppressed, &Id::toString);
  QtcSettings::setValueWithDefault(m_settings, C_SUPPRESSED_WARNINGS, list);
}

InfoBarDisplay::InfoBarDisplay(QObject *parent) : QObject(parent) {}

auto InfoBarDisplay::setTarget(QBoxLayout *layout, int index) -> void
{
  m_boxLayout = layout;
  m_boxIndex = index;
}

auto InfoBarDisplay::setInfoBar(InfoBar *infoBar) -> void
{
  if (m_infoBar == infoBar)
    return;

  if (m_infoBar)
    m_infoBar->disconnect(this);
  m_infoBar = infoBar;
  if (m_infoBar) {
    connect(m_infoBar, &InfoBar::changed, this, &InfoBarDisplay::update);
    connect(m_infoBar, &QObject::destroyed, this, &InfoBarDisplay::infoBarDestroyed);
  }
  update();
}

auto InfoBarDisplay::setEdge(Qt::Edge edge) -> void
{
  m_edge = edge;
  update();
}

auto InfoBarDisplay::infoBar() const -> InfoBar*
{
  return m_infoBar;
}

auto InfoBarDisplay::infoBarDestroyed() -> void
{
  m_infoBar = nullptr;
  // Calling update() here causes a complicated crash on shutdown.
  // So instead we rely on the view now being either destroyed (in which case it
  // will delete the widgets itself) or setInfoBar() being called explicitly.
}

auto InfoBarDisplay::update() -> void
{
  for (QWidget *widget : qAsConst(m_infoWidgets)) {
    widget->disconnect(this); // We want no destroyed() signal now
    delete widget;
  }
  m_infoWidgets.clear();

  if (!m_infoBar)
    return;

  for (const InfoBarEntry &info : qAsConst(m_infoBar->m_infoBarEntries)) {
    auto infoWidget = new InfoBarWidget(m_edge);

    auto hbox = new QHBoxLayout;
    hbox->setContentsMargins(2, 2, 2, 2);

    auto vbox = new QVBoxLayout(infoWidget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addLayout(hbox);

    QLabel *infoWidgetLabel = new QLabel(info.m_infoText);
    infoWidgetLabel->setWordWrap(true);
    infoWidgetLabel->setOpenExternalLinks(true);
    hbox->addWidget(infoWidgetLabel, 1);

    if (info.m_detailsWidgetCreator) {
      if (m_isShowingDetailsWidget) {
        QWidget *detailsWidget = info.m_detailsWidgetCreator();
        vbox->addWidget(detailsWidget);
      }

      auto showDetailsButton = new QToolButton;
      showDetailsButton->setCheckable(true);
      showDetailsButton->setChecked(m_isShowingDetailsWidget);
      showDetailsButton->setText(tr("&Show Details"));
      connect(showDetailsButton, &QToolButton::clicked, [this, vbox, info](bool) {
        QWidget *detailsWidget = vbox->count() == 2 ? vbox->itemAt(1)->widget() : nullptr;
        if (!detailsWidget) {
          detailsWidget = info.m_detailsWidgetCreator();
          vbox->addWidget(detailsWidget);
        }

        m_isShowingDetailsWidget = !m_isShowingDetailsWidget;
        detailsWidget->setVisible(m_isShowingDetailsWidget);
      });

      hbox->addWidget(showDetailsButton);
    } else {
      m_isShowingDetailsWidget = false;
    }

    if (!info.m_comboInfo.isEmpty()) {
      auto cb = new QComboBox();
      cb->addItems(info.m_comboInfo);
      connect(cb, &QComboBox::currentTextChanged, [info](const QString &text) {
        info.m_comboCallBack(text);
      });

      hbox->addWidget(cb);
    }

    for (const InfoBarEntry::Button &button : qAsConst(info.m_buttons)) {
      auto infoWidgetButton = new QToolButton;
      infoWidgetButton->setText(button.text);
      connect(infoWidgetButton, &QAbstractButton::clicked, [button]() { button.callback(); });
      hbox->addWidget(infoWidgetButton);
    }

    const Id id = info.m_id;
    QToolButton *infoWidgetSuppressButton = nullptr;
    if (info.m_globalSuppression == InfoBarEntry::GlobalSuppression::Enabled) {
      infoWidgetSuppressButton = new QToolButton;
      infoWidgetSuppressButton->setText(tr("Do Not Show Again"));
      connect(infoWidgetSuppressButton, &QAbstractButton::clicked, this, [this, id] {
        m_infoBar->removeInfo(id);
        InfoBar::globallySuppressInfo(id);
      });
    }

    QToolButton *infoWidgetCloseButton = nullptr;
    if (info.m_useCancelButton) {
      infoWidgetCloseButton = new QToolButton;
      // need to connect to cancelObjectbefore connecting to cancelButtonClicked,
      // because the latter removes the button and with it any connect
      if (info.m_cancelButtonCallBack)
        connect(infoWidgetCloseButton, &QAbstractButton::clicked, info.m_cancelButtonCallBack);
      connect(infoWidgetCloseButton, &QAbstractButton::clicked, this, [this, id] {
        m_infoBar->removeInfo(id);
      });
    }

    if (info.m_cancelButtonText.isEmpty()) {
      if (infoWidgetCloseButton) {
        infoWidgetCloseButton->setAutoRaise(true);
        infoWidgetCloseButton->setIcon(Utils::Icons::CLOSE_FOREGROUND.icon());
        infoWidgetCloseButton->setToolTip(tr("Close"));
      }

      if (infoWidgetSuppressButton)
        hbox->addWidget(infoWidgetSuppressButton);

      if (infoWidgetCloseButton)
        hbox->addWidget(infoWidgetCloseButton);
    } else {
      infoWidgetCloseButton->setText(info.m_cancelButtonText);
      hbox->addWidget(infoWidgetCloseButton);
      if (infoWidgetSuppressButton)
        hbox->addWidget(infoWidgetSuppressButton);
    }

    connect(infoWidget, &QObject::destroyed, this, &InfoBarDisplay::widgetDestroyed);
    m_boxLayout->insertWidget(m_boxIndex, infoWidget);
    m_infoWidgets << infoWidget;
  }
}

auto InfoBarDisplay::widgetDestroyed() -> void
{
  m_infoWidgets.removeOne(static_cast<QWidget*>(sender()));
}

} // namespace Utils
