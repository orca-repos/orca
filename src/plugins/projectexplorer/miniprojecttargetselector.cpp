// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildconfiguration.hpp"
#include "buildmanager.hpp"
#include "deployconfiguration.hpp"
#include "kit.hpp"
#include "kitmanager.hpp"
#include "miniprojecttargetselector.hpp"
#include "projectexplorer.hpp"
#include "projectexplorericons.hpp"
#include "project.hpp"
#include "projectmodels.hpp"
#include "runconfiguration.hpp"
#include "session.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>
#include <utils/itemviews.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/stringutils.hpp>
#include <utils/styledbar.hpp>
#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>
#include <utils/utilsicons.hpp>

#include <core/core-interface.hpp>
#include <core/core-constants.hpp>
#include <core/core-mode-manager.hpp>

#include <QGuiApplication>
#include <QTimer>
#include <QLayout>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QStatusBar>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyleFactory>
#include <QAction>
#include <QItemDelegate>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

constexpr int RunColumnWidth = 30;

static auto createCenteredIcon(const QIcon &icon, const QIcon &overlay) -> QIcon
{
  QPixmap targetPixmap;
  const auto appDevicePixelRatio = qApp->devicePixelRatio();
  const auto deviceSpaceIconSize = static_cast<int>(Orca::Plugin::Core::MODEBAR_ICON_SIZE * appDevicePixelRatio);
  targetPixmap = QPixmap(deviceSpaceIconSize, deviceSpaceIconSize);
  targetPixmap.setDevicePixelRatio(appDevicePixelRatio);
  targetPixmap.fill(Qt::transparent);
  QPainter painter(&targetPixmap); // painter in user space

  auto pixmap = icon.pixmap(Orca::Plugin::Core::MODEBAR_ICON_SIZE); // already takes app devicePixelRatio into account
  auto pixmapDevicePixelRatio = pixmap.devicePixelRatio();
  painter.drawPixmap((Orca::Plugin::Core::MODEBAR_ICON_SIZE - pixmap.width() / pixmapDevicePixelRatio) / 2, (Orca::Plugin::Core::MODEBAR_ICON_SIZE - pixmap.height() / pixmapDevicePixelRatio) / 2, pixmap);
  if (!overlay.isNull()) {
    pixmap = overlay.pixmap(Orca::Plugin::Core::MODEBAR_ICON_SIZE); // already takes app devicePixelRatio into account
    pixmapDevicePixelRatio = pixmap.devicePixelRatio();
    painter.drawPixmap((Orca::Plugin::Core::MODEBAR_ICON_SIZE - pixmap.width() / pixmapDevicePixelRatio) / 2, (Orca::Plugin::Core::MODEBAR_ICON_SIZE - pixmap.height() / pixmapDevicePixelRatio) / 2, pixmap);
  }

  return QIcon(targetPixmap);
}

class GenericItem : public TreeItem {
public:
  GenericItem() = default;
  GenericItem(QObject *object) : m_object(object) {}
  auto object() const -> QObject* { return m_object; }

  auto rawDisplayName() const -> QString
  {
    if (const auto p = qobject_cast<Project*>(object()))
      return p->displayName();
    if (const auto t = qobject_cast<Target*>(object()))
      return t->displayName();
    return static_cast<ProjectConfiguration*>(object())->expandedDisplayName();
  }

  auto displayName() const -> QString
  {
    if (const auto p = qobject_cast<Project*>(object())) {
      const auto hasSameProjectName = [this](TreeItem *ti) {
        return ti != this && static_cast<GenericItem*>(ti)->rawDisplayName() == rawDisplayName();
      };
      auto displayName = p->displayName();
      if (parent()->findAnyChild(hasSameProjectName)) {
        displayName.append(" (").append(p->projectFilePath().toUserOutput()).append(')');
      }
      return displayName;
    }
    return rawDisplayName();
  }

private:
  auto toolTip() const -> QVariant
  {
    if (qobject_cast<Project*>(object()))
      return {};
    if (const auto t = qobject_cast<Target*>(object()))
      return t->toolTip();
    return static_cast<ProjectConfiguration*>(object())->toolTip();
  }

  auto data(int column, int role) const -> QVariant override
  {
    if (column == 1 && role == Qt::ToolTipRole)
      return QCoreApplication::translate("RunConfigSelector", "Run Without Deployment");
    if (column != 0)
      return {};
    switch (role) {
    case Qt::DisplayRole:
      return displayName();
    case Qt::ToolTipRole:
      return toolTip();
    default:
      break;
    }
    return {};
  }

  QObject *m_object = nullptr;
};

static auto compareItems(const TreeItem *ti1, const TreeItem *ti2) -> bool
{
  const auto result = caseFriendlyCompare(static_cast<const GenericItem*>(ti1)->rawDisplayName(), static_cast<const GenericItem*>(ti2)->rawDisplayName());
  if (result != 0)
    return result < 0;
  return ti1 < ti2;
}

class GenericModel : public TreeModel<GenericItem, GenericItem> {
  Q_OBJECT 

public:
  GenericModel(QObject *parent) : TreeModel(parent) { }

  auto rebuild(const QList<QObject*> &objects) -> void
  {
    clear();
    for (const auto e : objects)
      addItemForObject(e);
  }

  auto addItemForObject(QObject *object) -> const GenericItem*
  {
    const auto item = new GenericItem(object);
    rootItem()->insertOrderedChild(item, &compareItems);
    if (const auto project = qobject_cast<Project*>(object)) {
      connect(project, &Project::displayNameChanged, this, &GenericModel::displayNameChanged);
    } else if (const auto target = qobject_cast<Target*>(object)) {
      connect(target, &Target::kitChanged, this, &GenericModel::displayNameChanged);
    } else {
      const auto pc = qobject_cast<ProjectConfiguration*>(object);
      QTC_CHECK(pc);
      connect(pc, &ProjectConfiguration::displayNameChanged, this, &GenericModel::displayNameChanged);
      connect(pc, &ProjectConfiguration::toolTipChanged, this, &GenericModel::updateToolTips);
    }
    return item;
  }

  auto itemForObject(const QObject *object) const -> GenericItem*
  {
    return findItemAtLevel<1>([object](const GenericItem *item) {
      return item->object() == object;
    });
  }

  auto setColumnCount(int columns) -> void { m_columnCount = columns; }

signals:
  auto displayNameChanged() -> void;

private:
  auto updateToolTips() -> void
  {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::ToolTipRole});
  }
};

class SelectorView : public TreeView {
  Q_OBJECT 

public:
  SelectorView(QWidget *parent);

  auto setMaxCount(int maxCount) -> void;
  auto maxCount() -> int;

  auto optimalWidth() const -> int;
  auto setOptimalWidth(int width) -> void;

  auto padding() -> int;

  auto theModel() const -> GenericModel* { return static_cast<GenericModel*>(model()); }

protected:
  auto resetOptimalWidth() -> void
  {
    if (m_resetScheduled)
      return;
    m_resetScheduled = true;
    QMetaObject::invokeMethod(this, &SelectorView::doResetOptimalWidth, Qt::QueuedConnection);
  }

private:
  auto keyPressEvent(QKeyEvent *event) -> void override;
  auto keyReleaseEvent(QKeyEvent *event) -> void override;

  auto doResetOptimalWidth() -> void
  {
    m_resetScheduled = false;
    auto width = 0;
    QFontMetrics fn(font());
    theModel()->forItemsAtLevel<1>([this, &width, &fn](const GenericItem *item) {
      width = qMax(fn.horizontalAdvance(item->displayName()) + padding(), width);
    });
    setOptimalWidth(width);
  }

  int m_maxCount = 0;
  int m_optimalWidth = 0;
  bool m_resetScheduled = false;
};

class ProjectListView : public SelectorView {
  Q_OBJECT 

public:
  explicit ProjectListView(QWidget *parent = nullptr) : SelectorView(parent)
  {
    const auto model = new GenericModel(this);
    model->rebuild(transform<QList<QObject*>>(SessionManager::projects(), [](Project *p) { return p; }));
    connect(SessionManager::instance(), &SessionManager::projectAdded, this, [this, model](Project *project) {
      const auto projectItem = model->addItemForObject(project);
      const QFontMetrics fn(font());
      const auto width = fn.horizontalAdvance(projectItem->displayName()) + padding();
      if (width > optimalWidth())
        setOptimalWidth(width);
      restoreCurrentIndex();
    });
    connect(SessionManager::instance(), &SessionManager::aboutToRemoveProject, this, [this, model](const Project *project) {
      const auto item = model->itemForObject(project);
      if (!item)
        return;
      model->destroyItem(item);
      resetOptimalWidth();
    });
    connect(SessionManager::instance(), &SessionManager::startupProjectChanged, this, [this, model](const Project *project) {
      const GenericItem *const item = model->itemForObject(project);
      if (item)
        setCurrentIndex(item->index());
    });
    connect(model, &GenericModel::displayNameChanged, this, [this, model] {
      model->rootItem()->sortChildren(&compareItems);
      resetOptimalWidth();
      restoreCurrentIndex();
    });
    setModel(model);
    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, [model](const QModelIndex &index) {
      const GenericItem *const item = model->itemForIndex(index);
      if (item && item->object())
        SessionManager::setStartupProject(qobject_cast<Project*>(item->object()));
    });
  }

private:
  auto restoreCurrentIndex() -> void
  {
    const GenericItem *const itemForStartupProject = theModel()->itemForObject(SessionManager::startupProject());
    if (itemForStartupProject)
      setCurrentIndex(theModel()->indexForItem(itemForStartupProject));
  }
};

class GenericListWidget : public SelectorView {
  Q_OBJECT 

public:
  explicit GenericListWidget(QWidget *parent = nullptr) : SelectorView(parent)
  {
    const auto model = new GenericModel(this);
    connect(model, &GenericModel::displayNameChanged, this, [this, model] {
      const GenericItem *const activeItem = model->itemForIndex(currentIndex());
      model->rootItem()->sortChildren(&compareItems);
      resetOptimalWidth();
      if (activeItem)
        setCurrentIndex(activeItem->index());
    });
    setModel(model);
    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &GenericListWidget::rowChanged);
  }

signals:
  auto changeActiveProjectConfiguration(QObject *pc) -> void;

public:
  auto setProjectConfigurations(const QList<QObject*> &list, QObject *active) -> void
  {
    theModel()->rebuild(list);
    resetOptimalWidth();
    setActiveProjectConfiguration(active);
  }

  auto setActiveProjectConfiguration(QObject *active) -> void
  {
    if (const GenericItem *const item = theModel()->itemForObject(active))
      setCurrentIndex(item->index());
  }

  auto addProjectConfiguration(QObject *pc) -> void
  {
    const auto activeItem = theModel()->itemForIndex(currentIndex());
    const auto item = theModel()->addItemForObject(pc);
    const QFontMetrics fn(font());
    const auto width = fn.horizontalAdvance(item->displayName()) + padding();
    if (width > optimalWidth())
      setOptimalWidth(width);
    if (activeItem)
      setCurrentIndex(activeItem->index());
  }

  auto removeProjectConfiguration(QObject *pc) -> void
  {
    const auto activeItem = theModel()->itemForIndex(currentIndex());
    if (const auto item = theModel()->itemForObject(pc)) {
      theModel()->destroyItem(item);
      resetOptimalWidth();
      if (activeItem && activeItem != item)
        setCurrentIndex(activeItem->index());
    }
  }

private:
  auto mousePressEvent(QMouseEvent *event) -> void override
  {
    const auto pressedIndex = indexAt(event->pos());
    if (pressedIndex.column() == 1) {
      m_pressedIndex = pressedIndex;
      return; // Clicking on the run button should not change the current index
    }
    m_pressedIndex = QModelIndex();
    TreeView::mousePressEvent(event);
  }

  auto mouseReleaseEvent(QMouseEvent *event) -> void override
  {
    const auto pressedIndex = m_pressedIndex;
    m_pressedIndex = QModelIndex();
    if (pressedIndex.isValid() && pressedIndex == indexAt(event->pos())) {
      const auto rc = qobject_cast<RunConfiguration*>(theModel()->itemForIndex(pressedIndex)->object());
      QTC_ASSERT(rc, return);
      if (!BuildManager::isBuilding(rc->project()))
        ProjectExplorerPlugin::runRunConfiguration(rc, Constants::NORMAL_RUN_MODE, true);
      return;
    }
    TreeView::mouseReleaseEvent(event);
  }

  auto objectAt(const QModelIndex &index) const -> QObject*
  {
    return theModel()->itemForIndex(index)->object();
  }

  auto rowChanged(const QModelIndex &index) -> void
  {
    if (index.isValid()) emit changeActiveProjectConfiguration(objectAt(index));
  }

  QModelIndex m_pressedIndex;
};

////////
// TargetSelectorDelegate
////////
class TargetSelectorDelegate : public QItemDelegate {
public:
  TargetSelectorDelegate(SelectorView *parent) : QItemDelegate(parent), m_view(parent) { }
private:
  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override;
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
  SelectorView *m_view;
};

auto TargetSelectorDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize
{
  Q_UNUSED(option)
  Q_UNUSED(index)
  return QSize(m_view->size().width(), 30);
}

auto TargetSelectorDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  painter->save();
  painter->setClipping(false);

  QColor textColor = orcaTheme()->color(Theme::MiniProjectTargetSelectorTextColor);
  if (option.state & QStyle::State_Selected) {
    QColor color;
    if (m_view->hasFocus()) {
      color = option.palette.highlight().color();
      textColor = option.palette.highlightedText().color();
    } else {
      color = option.palette.dark().color();
    }

    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      painter->fillRect(option.rect, color);
    } else {
      painter->fillRect(option.rect, color.darker(140));
      static const QImage selectionGradient(":/projectexplorer/images/targetpanel_gradient.png");
      StyleHelper::drawCornerImage(selectionGradient, painter, option.rect.adjusted(0, 0, 0, -1), 5, 5, 5, 5);
      const auto borderRect = QRectF(option.rect).adjusted(0.5, 0.5, -0.5, -0.5);
      painter->setPen(QColor(255, 255, 255, 60));
      painter->drawLine(borderRect.topLeft(), borderRect.topRight());
      painter->setPen(QColor(255, 255, 255, 30));
      painter->drawLine(borderRect.bottomLeft() - QPointF(0, 1), borderRect.bottomRight() - QPointF(0, 1));
      painter->setPen(QColor(0, 0, 0, 80));
      painter->drawLine(borderRect.bottomLeft(), borderRect.bottomRight());
    }
  }

  QFontMetrics fm(option.font);
  auto text = index.data(Qt::DisplayRole).toString();
  painter->setPen(textColor);
  auto elidedText = fm.elidedText(text, Qt::ElideMiddle, option.rect.width() - 12);
  if (elidedText != text)
    const_cast<QAbstractItemModel*>(index.model())->setData(index, text, Qt::ToolTipRole);
  else
    const_cast<QAbstractItemModel*>(index.model())->setData(index, index.model()->data(index, Qt::UserRole + 1).toString(), Qt::ToolTipRole);
  painter->drawText(option.rect.left() + 6, option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent(), elidedText);
  if (index.column() == 1 && option.state & QStyle::State_MouseOver) {
    const auto icon = Utils::Icons::RUN_SMALL_TOOLBAR.icon();
    QRect iconRect(0, 0, 16, 16);
    iconRect.moveCenter(option.rect.center());
    icon.paint(painter, iconRect);
  }

  painter->restore();
}

////////
// ListWidget
////////
SelectorView::SelectorView(QWidget *parent) : TreeView(parent)
{
  setFocusPolicy(Qt::NoFocus);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setAlternatingRowColors(false);
  setUniformRowHeights(true);
  setIndentation(0);
  setFocusPolicy(Qt::WheelFocus);
  setItemDelegate(new TargetSelectorDelegate(this));
  setSelectionBehavior(SelectRows);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  const QColor bgColor = orcaTheme()->color(Theme::MiniProjectTargetSelectorBackgroundColor);
  const auto bgColorName = orcaTheme()->flag(Theme::FlatToolBars) ? bgColor.lighter(120).name() : bgColor.name();
  setStyleSheet(QString::fromLatin1("QAbstractItemView { background: %1; border-style: none; }").arg(bgColorName));
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

auto SelectorView::keyPressEvent(QKeyEvent *event) -> void
{
  if (event->key() == Qt::Key_Left)
    focusPreviousChild();
  else if (event->key() == Qt::Key_Right)
    focusNextChild();
  else
    TreeView::keyPressEvent(event);
}

auto SelectorView::keyReleaseEvent(QKeyEvent *event) -> void
{
  if (event->key() != Qt::Key_Left && event->key() != Qt::Key_Right)
    TreeView::keyReleaseEvent(event);
}

auto SelectorView::setMaxCount(int maxCount) -> void
{
  m_maxCount = maxCount;
  updateGeometry();
}

auto SelectorView::maxCount() -> int
{
  return m_maxCount;
}

auto SelectorView::optimalWidth() const -> int
{
  return m_optimalWidth;
}

auto SelectorView::setOptimalWidth(int width) -> void
{
  m_optimalWidth = width;
  if (model()->columnCount() == 2)
    m_optimalWidth += RunColumnWidth;
  updateGeometry();
}

auto SelectorView::padding() -> int
{
  // there needs to be enough extra pixels to show a scrollbar
  return 2 * style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, this) + style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this) + 10;
}

/////////
// KitAreaWidget
/////////
class KitAreaWidget : public QWidget {
  Q_OBJECT 

public:
  explicit KitAreaWidget(QWidget *parent = nullptr) : QWidget(parent)
  {
    connect(KitManager::instance(), &KitManager::kitUpdated, this, &KitAreaWidget::updateKit);
  }

  ~KitAreaWidget() override { setKit(nullptr); }

  auto setKit(Kit *k) -> void
  {
    qDeleteAll(m_widgets);
    m_widgets.clear();

    if (!k)
      return;

    delete layout();

    LayoutBuilder builder(LayoutBuilder::GridLayout);
    for (const auto aspect : KitManager::kitAspects()) {
      if (k && k->isMutable(aspect->id())) {
        const auto widget = aspect->createConfigWidget(k);
        m_widgets << widget;
        const auto label = new QLabel(aspect->displayName());
        builder.addItem(label);
        widget->addToLayout(builder);
        builder.finishRow();
      }
    }
    builder.attachTo(this);
    layout()->setContentsMargins(3, 3, 3, 3);

    m_kit = k;

    setHidden(m_widgets.isEmpty());
  }

private:
  auto updateKit(Kit *k) -> void
  {
    if (!m_kit || m_kit != k)
      return;

    auto addedMutables = false;
    auto knownList = transform(m_widgets, &KitAspectWidget::kitInformation);

    for (auto aspect : KitManager::kitAspects()) {
      const auto currentId = aspect->id();
      if (m_kit->isMutable(currentId) && !knownList.removeOne(aspect)) {
        addedMutables = true;
        break;
      }
    }
    const auto removedMutables = !knownList.isEmpty();

    if (addedMutables || removedMutables) {
      // Redo whole setup if the number of mutable settings did change
      setKit(m_kit);
    } else {
      // Refresh all widgets if the number of mutable settings did not change
      foreach(KitAspectWidget *w, m_widgets)
        w->refresh();
    }
  }

  Kit *m_kit = nullptr;
  QList<KitAspectWidget*> m_widgets;
};

/////////
// MiniProjectTargetSelector
/////////

auto MiniProjectTargetSelector::createTitleLabel(const QString &text) -> QWidget*
{
  auto *bar = new StyledBar(this);
  bar->setSingleRow(true);
  auto *toolLayout = new QVBoxLayout(bar);
  toolLayout->setContentsMargins(6, 0, 6, 0);
  toolLayout->setSpacing(0);

  const auto l = new QLabel(text);
  auto f = l->font();
  f.setBold(true);
  l->setFont(f);
  toolLayout->addWidget(l);

  const auto panelHeight = l->fontMetrics().height() + 12;
  bar->ensurePolished(); // Required since manhattanstyle overrides height
  bar->setFixedHeight(panelHeight);
  return bar;
}

MiniProjectTargetSelector::MiniProjectTargetSelector(QAction *targetSelectorAction, QWidget *parent) : QWidget(parent), m_projectAction(targetSelectorAction)
{
  setProperty("panelwidget", true);
  setContentsMargins(QMargins(0, 1, 1, 8));
  setWindowFlags(Qt::Popup);

  targetSelectorAction->setIcon(orcaTheme()->flag(Theme::FlatSideBarIcons) ? Icons::DESKTOP_DEVICE.icon() : style()->standardIcon(QStyle::SP_ComputerIcon));
  targetSelectorAction->setProperty("titledAction", true);

  m_kitAreaWidget = new KitAreaWidget(this);

  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setContentsMargins(3, 3, 3, 3);
  m_summaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  auto pal = m_summaryLabel->palette();
  pal.setColor(QPalette::Window, StyleHelper::baseColor());
  m_summaryLabel->setPalette(pal);
  m_summaryLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_summaryLabel->setTextInteractionFlags(m_summaryLabel->textInteractionFlags() | Qt::LinksAccessibleByMouse);

  m_listWidgets.resize(LAST);
  m_titleWidgets.resize(LAST);
  m_listWidgets[PROJECT] = nullptr; //project is not a generic list widget

  m_titleWidgets[PROJECT] = createTitleLabel(tr("Project"));
  m_projectListWidget = new ProjectListView(this);
  connect(m_projectListWidget, &QAbstractItemView::doubleClicked, this, &MiniProjectTargetSelector::hide);

  QStringList titles;
  titles << tr("Kit") << tr("Build") << tr("Deploy") << tr("Run");

  for (int i = TARGET; i < LAST; ++i) {
    m_titleWidgets[i] = createTitleLabel(titles.at(i - 1));
    m_listWidgets[i] = new GenericListWidget(this);
    connect(m_listWidgets[i], &QAbstractItemView::doubleClicked, this, &MiniProjectTargetSelector::hide);
  }
  m_listWidgets[RUN]->theModel()->setColumnCount(2);
  m_listWidgets[RUN]->viewport()->setAttribute(Qt::WA_Hover);

  // Validate state: At this point the session is still empty!
  const auto startup = SessionManager::startupProject();
  QTC_CHECK(!startup);
  QTC_CHECK(SessionManager::projects().isEmpty());

  connect(m_summaryLabel, &QLabel::linkActivated, this, &MiniProjectTargetSelector::switchToProjectsMode);

  const auto sessionManager = SessionManager::instance();
  connect(sessionManager, &SessionManager::startupProjectChanged, this, &MiniProjectTargetSelector::changeStartupProject);

  connect(sessionManager, &SessionManager::projectAdded, this, &MiniProjectTargetSelector::projectAdded);
  connect(sessionManager, &SessionManager::projectRemoved, this, &MiniProjectTargetSelector::projectRemoved);
  connect(sessionManager, &SessionManager::projectDisplayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);

  // for icon changes:
  connect(KitManager::instance(), &KitManager::kitUpdated, this, &MiniProjectTargetSelector::kitChanged);

  connect(m_listWidgets[TARGET], &GenericListWidget::changeActiveProjectConfiguration, this, [this](QObject *pc) {
    SessionManager::setActiveTarget(m_project, static_cast<Target*>(pc), SetActive::Cascade);
  });
  connect(m_listWidgets[BUILD], &GenericListWidget::changeActiveProjectConfiguration, this, [this](QObject *pc) {
    SessionManager::setActiveBuildConfiguration(m_project->activeTarget(), static_cast<BuildConfiguration*>(pc), SetActive::Cascade);
  });
  connect(m_listWidgets[DEPLOY], &GenericListWidget::changeActiveProjectConfiguration, this, [this](QObject *pc) {
    SessionManager::setActiveDeployConfiguration(m_project->activeTarget(), static_cast<DeployConfiguration*>(pc), SetActive::Cascade);
  });
  connect(m_listWidgets[RUN], &GenericListWidget::changeActiveProjectConfiguration, this, [this](QObject *pc) {
    m_project->activeTarget()->setActiveRunConfiguration(static_cast<RunConfiguration*>(pc));
  });
}

auto MiniProjectTargetSelector::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::LayoutRequest) {
    doLayout(true);
    return true;
  } else if (event->type() == QEvent::ShortcutOverride) {
    if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
      event->accept();
      return true;
    }
  }
  return QWidget::event(event);
}

// does some fancy calculations to ensure proper widths for the list widgets
auto MiniProjectTargetSelector::listWidgetWidths(int minSize, int maxSize) -> QVector<int>
{
  QVector<int> result;
  result.resize(LAST);
  if (m_projectListWidget->isVisibleTo(this))
    result[PROJECT] = m_projectListWidget->optimalWidth();
  else
    result[PROJECT] = -1;

  for (int i = TARGET; i < LAST; ++i) {
    if (m_listWidgets[i]->isVisibleTo(this))
      result[i] = m_listWidgets[i]->optimalWidth();
    else
      result[i] = -1;
  }

  auto totalWidth = 0;
  // Adjust to minimum width of title
  for (int i = PROJECT; i < LAST; ++i) {
    if (result[i] != -1) {
      // We want at least 100 pixels per column
      const auto width = qMax(m_titleWidgets[i]->sizeHint().width(), 100);
      if (result[i] < width)
        result[i] = width;
      totalWidth += result[i];
    }
  }

  if (totalWidth == 0) // All hidden
    return result;

  bool tooSmall;
  if (totalWidth < minSize)
    tooSmall = true;
  else if (totalWidth > maxSize)
    tooSmall = false;
  else
    return result;

  auto widthToDistribute = tooSmall ? (minSize - totalWidth) : (totalWidth - maxSize);
  QVector<int> indexes;
  indexes.reserve(LAST);
  for (int i = PROJECT; i < LAST; ++i)
    if (result[i] != -1)
      indexes.append(i);

  if (tooSmall) {
    sort(indexes, [&result](int i, int j) {
      return result[i] < result[j];
    });
  } else {
    sort(indexes, [&result](int i, int j) {
      return result[i] > result[j];
    });
  }

  auto i = 0;
  auto first = result[indexes.first()]; // biggest or smallest

  // we resize the biggest columns until they are the same size as the second biggest
  // since it looks prettiest if all the columns are the same width
  while (true) {
    for (; i < indexes.size(); ++i) {
      if (result[indexes[i]] != first)
        break;
    }
    auto next = tooSmall ? INT_MAX : 0;
    if (i < indexes.size())
      next = result[indexes[i]];

    int delta;
    if (tooSmall)
      delta = qMin(next - first, widthToDistribute / qMax(i, 1));
    else
      delta = qMin(first - next, widthToDistribute / qMax(i, 1));

    if (delta == 0)
      return result;

    if (tooSmall) {
      for (auto j = 0; j < i; ++j)
        result[indexes[j]] += delta;
    } else {
      for (auto j = 0; j < i; ++j)
        result[indexes[j]] -= delta;
    }

    widthToDistribute -= delta * i;
    if (widthToDistribute <= 0)
      return result;

    first = result[indexes.first()];
    i = 0; // TODO can we do better?
  }
}

auto MiniProjectTargetSelector::doLayout(bool keepSize) -> void
{
  // An unconfigured project shows empty build/deploy/run sections
  // if there's a configured project in the seesion
  // that could be improved
  static auto statusBar = Orca::Plugin::Core::ICore::statusBar();
  static auto *actionBar = Orca::Plugin::Core::ICore::mainWindow()->findChild<QWidget*>(QLatin1String("actionbar"));
  Q_ASSERT(actionBar);

  m_kitAreaWidget->move(0, 0);

  const auto oldSummaryLabelY = m_summaryLabel->y();

  const auto kitAreaHeight = m_kitAreaWidget->isVisibleTo(this) ? m_kitAreaWidget->sizeHint().height() : 0;

  // 1. Calculate the summary label height
  const auto summaryLabelY = 1 + kitAreaHeight;

  auto summaryLabelHeight = 0;
  const auto oldSummaryLabelHeight = m_summaryLabel->height();
  auto onlySummary = false;
  // Count the number of lines
  auto visibleLineCount = m_projectListWidget->isVisibleTo(this) ? 0 : 1;
  for (int i = TARGET; i < LAST; ++i)
    visibleLineCount += m_listWidgets[i]->isVisibleTo(this) ? 0 : 1;

  if (visibleLineCount == LAST) {
    summaryLabelHeight = m_summaryLabel->sizeHint().height();
    onlySummary = true;
  } else {
    if (visibleLineCount < 3) {
      if (anyOf(SessionManager::projects(), &Project::needsConfiguration))
        visibleLineCount = 3;
    }
    if (visibleLineCount)
      summaryLabelHeight = m_summaryLabel->sizeHint().height();
  }

  if (keepSize && oldSummaryLabelHeight > summaryLabelHeight)
    summaryLabelHeight = oldSummaryLabelHeight;

  m_summaryLabel->move(0, summaryLabelY);

  // Height to be aligned with side bar button
  auto alignedWithActionHeight = 210;
  if (actionBar->isVisible())
    alignedWithActionHeight = actionBar->height() - statusBar->height();
  const auto bottomMargin = 9;
  auto heightWithoutKitArea = 0;

  if (!onlySummary) {
    // list widget height
    auto maxItemCount = m_projectListWidget->maxCount();
    for (int i = TARGET; i < LAST; ++i)
      maxItemCount = qMax(maxItemCount, m_listWidgets[i]->maxCount());

    const auto titleWidgetsHeight = m_titleWidgets.first()->height();
    if (keepSize) {
      heightWithoutKitArea = height() - oldSummaryLabelY + 1;
    } else {
      // Clamp the size of the listwidgets to be
      // at least as high as the sidebar button
      // and at most twice as high
      heightWithoutKitArea = summaryLabelHeight + qBound(alignedWithActionHeight, maxItemCount * 30 + bottomMargin + titleWidgetsHeight, alignedWithActionHeight * 2);
    }

    const auto titleY = summaryLabelY + summaryLabelHeight;
    const auto listY = titleY + titleWidgetsHeight;
    const auto listHeight = heightWithoutKitArea + kitAreaHeight - bottomMargin - listY + 1;

    // list widget widths
    auto minWidth = qMax(m_summaryLabel->sizeHint().width(), 250);
    minWidth = qMax(minWidth, m_kitAreaWidget->sizeHint().width());
    if (keepSize) {
      // Do not make the widget smaller then it was before
      auto oldTotalListWidgetWidth = m_projectListWidget->isVisibleTo(this) ? m_projectListWidget->width() : 0;
      for (int i = TARGET; i < LAST; ++i)
        oldTotalListWidgetWidth += m_listWidgets[i]->width();
      minWidth = qMax(minWidth, oldTotalListWidgetWidth);
    }

    auto widths = listWidgetWidths(minWidth, 1000);

    const auto runColumnWidth = widths[RUN] == -1 ? 0 : RunColumnWidth;
    auto x = 0;
    for (int i = PROJECT; i < LAST; ++i) {
      auto optimalWidth = widths[i];
      if (i == PROJECT) {
        m_projectListWidget->resize(optimalWidth, listHeight);
        m_projectListWidget->move(x, listY);
      } else {
        if (i == RUN)
          optimalWidth += runColumnWidth;
        m_listWidgets[i]->resize(optimalWidth, listHeight);
        m_listWidgets[i]->move(x, listY);
      }
      m_titleWidgets[i]->resize(optimalWidth, titleWidgetsHeight);
      m_titleWidgets[i]->move(x, titleY);
      x += optimalWidth + 1; //1 extra pixel for the separators or the right border
    }

    m_listWidgets[RUN]->setColumnWidth(0, m_listWidgets[RUN]->size().width() - runColumnWidth - m_listWidgets[RUN]->padding());
    m_listWidgets[RUN]->setColumnWidth(1, runColumnWidth);
    m_summaryLabel->resize(x - 1, summaryLabelHeight);
    m_kitAreaWidget->resize(x - 1, kitAreaHeight);
    setFixedSize(x, heightWithoutKitArea + kitAreaHeight);
  } else {
    if (keepSize)
      heightWithoutKitArea = height() - oldSummaryLabelY + 1;
    else
      heightWithoutKitArea = qMax(summaryLabelHeight + bottomMargin, alignedWithActionHeight);
    m_summaryLabel->resize(m_summaryLabel->sizeHint().width(), heightWithoutKitArea - bottomMargin);
    m_kitAreaWidget->resize(m_kitAreaWidget->sizeHint());
    setFixedSize(m_summaryLabel->width() + 1, heightWithoutKitArea + kitAreaHeight); //1 extra pixel for the border
  }

  auto moveTo = statusBar->mapToGlobal(QPoint(0, 0));
  moveTo -= QPoint(0, height());
  move(moveTo);
}

auto MiniProjectTargetSelector::projectAdded(Project *project) -> void
{
  connect(project, &Project::addedTarget, this, &MiniProjectTargetSelector::handleNewTarget);
  connect(project, &Project::removedTarget, this, &MiniProjectTargetSelector::handleRemovalOfTarget);

  foreach(Target *t, project->targets())
    addedTarget(t);

  updateProjectListVisible();
  updateTargetListVisible();
  updateBuildListVisible();
  updateDeployListVisible();
  updateRunListVisible();
}

auto MiniProjectTargetSelector::projectRemoved(Project *project) -> void
{
  disconnect(project, &Project::addedTarget, this, &MiniProjectTargetSelector::handleNewTarget);
  disconnect(project, &Project::removedTarget, this, &MiniProjectTargetSelector::handleRemovalOfTarget);

  foreach(Target *t, project->targets())
    removedTarget(t);

  updateProjectListVisible();
  updateTargetListVisible();
  updateBuildListVisible();
  updateDeployListVisible();
  updateRunListVisible();
}

auto MiniProjectTargetSelector::handleNewTarget(Target *target) -> void
{
  addedTarget(target);
  updateTargetListVisible();
  updateBuildListVisible();
  updateDeployListVisible();
  updateRunListVisible();
}

auto MiniProjectTargetSelector::handleRemovalOfTarget(Target *target) -> void
{
  removedTarget(target);

  updateTargetListVisible();
  updateBuildListVisible();
  updateDeployListVisible();
  updateRunListVisible();
}

auto MiniProjectTargetSelector::addedTarget(Target *target) -> void
{
  if (target->project() != m_project)
    return;

  m_listWidgets[TARGET]->addProjectConfiguration(target);

  for (const auto bc : target->buildConfigurations())
    addedBuildConfiguration(bc, false);
  for (const auto dc : target->deployConfigurations())
    addedDeployConfiguration(dc, false);
  for (const auto rc : target->runConfigurations())
    addedRunConfiguration(rc, false);
}

auto MiniProjectTargetSelector::removedTarget(Target *target) -> void
{
  if (target->project() != m_project)
    return;

  m_listWidgets[TARGET]->removeProjectConfiguration(target);

  for (const auto bc : target->buildConfigurations())
    removedBuildConfiguration(bc, false);
  for (const auto dc : target->deployConfigurations())
    removedDeployConfiguration(dc, false);
  for (const auto rc : target->runConfigurations())
    removedRunConfiguration(rc, false);
}

auto MiniProjectTargetSelector::addedBuildConfiguration(BuildConfiguration *bc, bool update) -> void
{
  if (!m_project || bc->target() != m_project->activeTarget())
    return;

  m_listWidgets[BUILD]->addProjectConfiguration(bc);
  if (update)
    updateBuildListVisible();
}

auto MiniProjectTargetSelector::removedBuildConfiguration(BuildConfiguration *bc, bool update) -> void
{
  if (!m_project || bc->target() != m_project->activeTarget())
    return;

  m_listWidgets[BUILD]->removeProjectConfiguration(bc);
  if (update)
    updateBuildListVisible();
}

auto MiniProjectTargetSelector::addedDeployConfiguration(DeployConfiguration *dc, bool update) -> void
{
  if (!m_project || dc->target() != m_project->activeTarget())
    return;

  m_listWidgets[DEPLOY]->addProjectConfiguration(dc);
  if (update)
    updateDeployListVisible();
}

auto MiniProjectTargetSelector::removedDeployConfiguration(DeployConfiguration *dc, bool update) -> void
{
  if (!m_project || dc->target() != m_project->activeTarget())
    return;

  m_listWidgets[DEPLOY]->removeProjectConfiguration(dc);
  if (update)
    updateDeployListVisible();
}

auto MiniProjectTargetSelector::addedRunConfiguration(RunConfiguration *rc, bool update) -> void
{
  if (!m_project || rc->target() != m_project->activeTarget())
    return;

  m_listWidgets[RUN]->addProjectConfiguration(rc);
  if (update)
    updateRunListVisible();
}

auto MiniProjectTargetSelector::removedRunConfiguration(RunConfiguration *rc, bool update) -> void
{
  if (!m_project || rc->target() != m_project->activeTarget())
    return;

  m_listWidgets[RUN]->removeProjectConfiguration(rc);
  if (update)
    updateRunListVisible();
}

auto MiniProjectTargetSelector::updateProjectListVisible() -> void
{
  const int count = SessionManager::projects().size();
  const auto visible = count > 1;

  m_projectListWidget->setVisible(visible);
  m_projectListWidget->setMaxCount(count);
  m_titleWidgets[PROJECT]->setVisible(visible);

  updateSummary();
}

auto MiniProjectTargetSelector::updateTargetListVisible() -> void
{
  auto maxCount = 0;
  for (const auto p : SessionManager::projects())
    maxCount = qMax(p->targets().size(), maxCount);

  const auto visible = maxCount > 1;
  m_listWidgets[TARGET]->setVisible(visible);
  m_listWidgets[TARGET]->setMaxCount(maxCount);
  m_titleWidgets[TARGET]->setVisible(visible);
  updateSummary();
}

auto MiniProjectTargetSelector::updateBuildListVisible() -> void
{
  auto maxCount = 0;
  for (const auto p : SessionManager::projects())
    foreach(Target *t, p->targets())
      maxCount = qMax(t->buildConfigurations().size(), maxCount);

  const auto visible = maxCount > 1;
  m_listWidgets[BUILD]->setVisible(visible);
  m_listWidgets[BUILD]->setMaxCount(maxCount);
  m_titleWidgets[BUILD]->setVisible(visible);
  updateSummary();
}

auto MiniProjectTargetSelector::updateDeployListVisible() -> void
{
  auto maxCount = 0;
  for (const auto p : SessionManager::projects())
    foreach(Target *t, p->targets())
      maxCount = qMax(t->deployConfigurations().size(), maxCount);

  const auto visible = maxCount > 1;
  m_listWidgets[DEPLOY]->setVisible(visible);
  m_listWidgets[DEPLOY]->setMaxCount(maxCount);
  m_titleWidgets[DEPLOY]->setVisible(visible);
  updateSummary();
}

auto MiniProjectTargetSelector::updateRunListVisible() -> void
{
  auto maxCount = 0;
  for (const auto p : SessionManager::projects())
    foreach(Target *t, p->targets())
      maxCount = qMax(t->runConfigurations().size(), maxCount);

  const auto visible = maxCount > 1;
  m_listWidgets[RUN]->setVisible(visible);
  m_listWidgets[RUN]->setMaxCount(maxCount);
  m_titleWidgets[RUN]->setVisible(visible);
  updateSummary();
}

auto MiniProjectTargetSelector::changeStartupProject(Project *project) -> void
{
  if (m_project) {
    disconnect(m_project, &Project::activeTargetChanged, this, &MiniProjectTargetSelector::activeTargetChanged);
  }
  m_project = project;
  if (m_project) {
    connect(m_project, &Project::activeTargetChanged, this, &MiniProjectTargetSelector::activeTargetChanged);
    activeTargetChanged(m_project->activeTarget());
  } else {
    activeTargetChanged(nullptr);
  }

  if (project) {
    QList<QObject*> list;
    foreach(Target *t, project->targets())
      list.append(t);
    m_listWidgets[TARGET]->setProjectConfigurations(list, project->activeTarget());
  } else {
    m_listWidgets[TARGET]->setProjectConfigurations(QList<QObject*>(), nullptr);
  }

  updateActionAndSummary();
}

auto MiniProjectTargetSelector::activeTargetChanged(Target *target) -> void
{
  if (m_target) {
    disconnect(m_target, &Target::kitChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    disconnect(m_target, &Target::iconChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    disconnect(m_target, &Target::activeBuildConfigurationChanged, this, &MiniProjectTargetSelector::activeBuildConfigurationChanged);
    disconnect(m_target, &Target::activeDeployConfigurationChanged, this, &MiniProjectTargetSelector::activeDeployConfigurationChanged);
    disconnect(m_target, &Target::activeRunConfigurationChanged, this, &MiniProjectTargetSelector::activeRunConfigurationChanged);
  }

  m_target = target;

  m_kitAreaWidget->setKit(m_target ? m_target->kit() : nullptr);

  m_listWidgets[TARGET]->setActiveProjectConfiguration(m_target);

  if (m_buildConfiguration)
    disconnect(m_buildConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  if (m_deployConfiguration)
    disconnect(m_deployConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);

  if (m_runConfiguration)
    disconnect(m_runConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);

  if (m_target) {
    QList<QObject*> bl;
    for (const auto bc : target->buildConfigurations())
      bl.append(bc);
    m_listWidgets[BUILD]->setProjectConfigurations(bl, target->activeBuildConfiguration());

    QList<QObject*> dl;
    for (const auto dc : target->deployConfigurations())
      dl.append(dc);
    m_listWidgets[DEPLOY]->setProjectConfigurations(dl, target->activeDeployConfiguration());

    QList<QObject*> rl;
    for (const auto rc : target->runConfigurations())
      rl.append(rc);
    m_listWidgets[RUN]->setProjectConfigurations(rl, target->activeRunConfiguration());

    m_buildConfiguration = m_target->activeBuildConfiguration();
    if (m_buildConfiguration)
      connect(m_buildConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    m_deployConfiguration = m_target->activeDeployConfiguration();
    if (m_deployConfiguration)
      connect(m_deployConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    m_runConfiguration = m_target->activeRunConfiguration();
    if (m_runConfiguration)
      connect(m_runConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);

    connect(m_target, &Target::kitChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    connect(m_target, &Target::iconChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
    connect(m_target, &Target::activeBuildConfigurationChanged, this, &MiniProjectTargetSelector::activeBuildConfigurationChanged);
    connect(m_target, &Target::activeDeployConfigurationChanged, this, &MiniProjectTargetSelector::activeDeployConfigurationChanged);
    connect(m_target, &Target::activeRunConfigurationChanged, this, &MiniProjectTargetSelector::activeRunConfigurationChanged);
  } else {
    m_listWidgets[BUILD]->setProjectConfigurations(QList<QObject*>(), nullptr);
    m_listWidgets[DEPLOY]->setProjectConfigurations(QList<QObject*>(), nullptr);
    m_listWidgets[RUN]->setProjectConfigurations(QList<QObject*>(), nullptr);
    m_buildConfiguration = nullptr;
    m_deployConfiguration = nullptr;
    m_runConfiguration = nullptr;
  }
  updateActionAndSummary();
}

auto MiniProjectTargetSelector::kitChanged(Kit *k) -> void
{
  if (m_target && m_target->kit() == k)
    updateActionAndSummary();
}

auto MiniProjectTargetSelector::activeBuildConfigurationChanged(BuildConfiguration *bc) -> void
{
  if (m_buildConfiguration)
    disconnect(m_buildConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_buildConfiguration = bc;
  if (m_buildConfiguration)
    connect(m_buildConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_listWidgets[BUILD]->setActiveProjectConfiguration(bc);
  updateActionAndSummary();
}

auto MiniProjectTargetSelector::activeDeployConfigurationChanged(DeployConfiguration *dc) -> void
{
  if (m_deployConfiguration)
    disconnect(m_deployConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_deployConfiguration = dc;
  if (m_deployConfiguration)
    connect(m_deployConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_listWidgets[DEPLOY]->setActiveProjectConfiguration(dc);
  updateActionAndSummary();
}

auto MiniProjectTargetSelector::activeRunConfigurationChanged(RunConfiguration *rc) -> void
{
  if (m_runConfiguration)
    disconnect(m_runConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_runConfiguration = rc;
  if (m_runConfiguration)
    connect(m_runConfiguration, &ProjectConfiguration::displayNameChanged, this, &MiniProjectTargetSelector::updateActionAndSummary);
  m_listWidgets[RUN]->setActiveProjectConfiguration(rc);
  updateActionAndSummary();
}

auto MiniProjectTargetSelector::setVisible(bool visible) -> void
{
  doLayout(false);
  QWidget::setVisible(visible);
  m_projectAction->setChecked(visible);
  if (visible) {
    if (!focusWidget() || !focusWidget()->isVisibleTo(this)) {
      // Does the second part actually work?
      if (m_projectListWidget->isVisibleTo(this))
        m_projectListWidget->setFocus();
      for (int i = TARGET; i < LAST; ++i) {
        if (m_listWidgets[i]->isVisibleTo(this)) {
          m_listWidgets[i]->setFocus();
          break;
        }
      }
    }
  }
}

auto MiniProjectTargetSelector::toggleVisible() -> void
{
  setVisible(!isVisible());
}

auto MiniProjectTargetSelector::nextOrShow() -> void
{
  if (!isVisible()) {
    show();
  } else {
    m_hideOnRelease = true;
    m_earliestHidetime = QDateTime::currentDateTime().addMSecs(800);
    if (auto *lw = qobject_cast<SelectorView*>(focusWidget())) {
      if (lw->currentIndex().row() < lw->model()->rowCount() - 1)
        lw->setCurrentIndex(lw->model()->index(lw->currentIndex().row() + 1, 0));
      else
        lw->setCurrentIndex(lw->model()->index(0, 0));
    }
  }
}

auto MiniProjectTargetSelector::keyPressEvent(QKeyEvent *ke) -> void
{
  if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Escape) {
    hide();
  } else {
    QWidget::keyPressEvent(ke);
  }
}

auto MiniProjectTargetSelector::keyReleaseEvent(QKeyEvent *ke) -> void
{
  if (m_hideOnRelease) {
    if (ke->modifiers() == 0
      /*HACK this is to overcome some event inconsistencies between platforms*/
      || (ke->modifiers() == Qt::AltModifier && (ke->key() == Qt::Key_Alt || ke->key() == -1))) {
      delayedHide();
      m_hideOnRelease = false;
    }
  }
  if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Escape)
    return;
  QWidget::keyReleaseEvent(ke);
}

auto MiniProjectTargetSelector::delayedHide() -> void
{
  const auto current = QDateTime::currentDateTime();
  if (m_earliestHidetime > current) {
    // schedule for later
    QTimer::singleShot(current.msecsTo(m_earliestHidetime) + 50, this, &MiniProjectTargetSelector::delayedHide);
  } else {
    hide();
  }
}

// This is a workaround for the problem that Windows
// will let the mouse events through when you click
// outside a popup to close it. This causes the popup
// to open on mouse release if you hit the button, which
//
//
// A similar case can be found in QComboBox
auto MiniProjectTargetSelector::mousePressEvent(QMouseEvent *e) -> void
{
  setAttribute(Qt::WA_NoMouseReplay);
  QWidget::mousePressEvent(e);
}

auto MiniProjectTargetSelector::updateActionAndSummary() -> void
{
  QString projectName = QLatin1String(" ");
  QString fileName; // contains the path if projectName is not unique
  QString targetName;
  QString targetToolTipText;
  QString buildConfig;
  QString deployConfig;
  QString runConfig;
  auto targetIcon = orcaTheme()->flag(Theme::FlatSideBarIcons) ? Icons::DESKTOP_DEVICE.icon() : style()->standardIcon(QStyle::SP_ComputerIcon);

  const auto project = SessionManager::startupProject();
  if (project) {
    projectName = project->displayName();
    for (const auto p : SessionManager::projects()) {
      if (p != project && p->displayName() == projectName) {
        fileName = project->projectFilePath().toUserOutput();
        break;
      }
    }

    if (const auto target = project->activeTarget()) {
      targetName = project->activeTarget()->displayName();

      if (const auto bc = target->activeBuildConfiguration())
        buildConfig = bc->displayName();

      if (const auto dc = target->activeDeployConfiguration())
        deployConfig = dc->displayName();

      if (const auto rc = target->activeRunConfiguration())
        runConfig = rc->expandedDisplayName();

      targetToolTipText = target->overlayIconToolTip();
      targetIcon = createCenteredIcon(target->icon(), target->overlayIcon());
    }
  }
  m_projectAction->setProperty("heading", projectName);
  if (project && project->needsConfiguration())
    m_projectAction->setProperty("subtitle", tr("Unconfigured"));
  else
    m_projectAction->setProperty("subtitle", buildConfig);
  m_projectAction->setIcon(targetIcon);
  QStringList lines;
  lines << tr("<b>Project:</b> %1").arg(projectName);
  if (!fileName.isEmpty())
    lines << tr("<b>Path:</b> %1").arg(fileName);
  if (!targetName.isEmpty())
    lines << tr("<b>Kit:</b> %1").arg(targetName);
  if (!buildConfig.isEmpty())
    lines << tr("<b>Build:</b> %1").arg(buildConfig);
  if (!deployConfig.isEmpty())
    lines << tr("<b>Deploy:</b> %1").arg(deployConfig);
  if (!runConfig.isEmpty())
    lines << tr("<b>Run:</b> %1").arg(runConfig);
  if (!targetToolTipText.isEmpty())
    lines << tr("%1").arg(targetToolTipText);
  const auto toolTip = QString("<html><nobr>%1</html>").arg(lines.join(QLatin1String("<br/>")));
  m_projectAction->setToolTip(toolTip);
  updateSummary();
}

auto MiniProjectTargetSelector::updateSummary() -> void
{
  QString summary;
  if (const auto startupProject = SessionManager::startupProject()) {
    if (!m_projectListWidget->isVisibleTo(this))
      summary.append(tr("Project: <b>%1</b><br/>").arg(startupProject->displayName()));
    if (const auto activeTarget = startupProject->activeTarget()) {
      if (!m_listWidgets[TARGET]->isVisibleTo(this))
        summary.append(tr("Kit: <b>%1</b><br/>").arg(activeTarget->displayName()));
      if (!m_listWidgets[BUILD]->isVisibleTo(this) && activeTarget->activeBuildConfiguration())
        summary.append(tr("Build: <b>%1</b><br/>").arg(activeTarget->activeBuildConfiguration()->displayName()));
      if (!m_listWidgets[DEPLOY]->isVisibleTo(this) && activeTarget->activeDeployConfiguration())
        summary.append(tr("Deploy: <b>%1</b><br/>").arg(activeTarget->activeDeployConfiguration()->displayName()));
      if (!m_listWidgets[RUN]->isVisibleTo(this) && activeTarget->activeRunConfiguration())
        summary.append(tr("Run: <b>%1</b><br/>").arg(activeTarget->activeRunConfiguration()->expandedDisplayName()));
    } else if (startupProject->needsConfiguration()) {
      summary = tr("<style type=text/css>" "a:link {color: rgb(128, 128, 255);}</style>" "The project <b>%1</b> is not yet configured<br/><br/>" "You can configure it in the <a href=\"projectmode\">Projects mode</a><br/>").arg(startupProject->displayName());
    } else {
      if (!m_listWidgets[TARGET]->isVisibleTo(this))
        summary.append(QLatin1String("<br/>"));
      if (!m_listWidgets[BUILD]->isVisibleTo(this))
        summary.append(QLatin1String("<br/>"));
      if (!m_listWidgets[DEPLOY]->isVisibleTo(this))
        summary.append(QLatin1String("<br/>"));
      if (!m_listWidgets[RUN]->isVisibleTo(this))
        summary.append(QLatin1String("<br/>"));
    }
  }
  m_summaryLabel->setText(summary);
}

auto MiniProjectTargetSelector::paintEvent(QPaintEvent *) -> void
{
  QPainter painter(this);
  painter.fillRect(rect(), StyleHelper::baseColor());
  painter.setPen(orcaTheme()->color(Theme::MiniProjectTargetSelectorBorderColor));
  // draw border on top and right
  const auto borderRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.drawLine(borderRect.topLeft(), borderRect.topRight());
  painter.drawLine(borderRect.topRight(), borderRect.bottomRight());
  if (orcaTheme()->flag(Theme::DrawTargetSelectorBottom)) {
    // draw thicker border on the bottom
    const QRect bottomRect(0, rect().height() - 8, rect().width(), 8);
    static const QImage image(":/projectexplorer/images/targetpanel_bottom.png");
    StyleHelper::drawCornerImage(image, &painter, bottomRect, 1, 1, 1, 1);
  }
}

auto MiniProjectTargetSelector::switchToProjectsMode() -> void
{
  Orca::Plugin::Core::ModeManager::activateMode(Constants::MODE_SESSION);
  hide();
}

} // namespace Internal
} // namespace ProjectExplorer

#include <miniprojecttargetselector.moc>
