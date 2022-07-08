// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectwelcomepage.hpp"
#include "session.hpp"
#include "sessionmodel.hpp"
#include "projectexplorer.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreconstants.hpp>
#include <core/documentmanager.hpp>
#include <core/icontext.hpp>
#include <core/icore.hpp>
#include <core/iwizardfactory.hpp>
#include <core/welcomepagehelper.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/icon.hpp>
#include <utils/stringutils.hpp>
#include <utils/theme/theme.hpp>

#include <QAbstractItemDelegate>
#include <QAction>
#include <QBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHelpEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QToolTip>
#include <QTreeView>

using namespace Core;
using namespace WelcomePageHelpers;
using namespace Utils;

constexpr int  LINK_HEIGHT = 35;
constexpr int  TEXT_OFFSET_HORIZONTAL = 36;
constexpr int  SESSION_LINE_HEIGHT = 28;
constexpr int  SESSION_ARROW_RECT_WIDTH = 24;
constexpr char PROJECT_BASE_ID[] = "Welcome.OpenRecentProject";

namespace ProjectExplorer {
namespace Internal {

ProjectModel::ProjectModel(QObject *parent) : QAbstractListModel(parent)
{
  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::recentProjectsChanged, this, &ProjectModel::resetProjects);
}

auto ProjectModel::rowCount(const QModelIndex &) const -> int
{
  return ProjectExplorerPlugin::recentProjects().count();
}

auto ProjectModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto recentProjects = ProjectExplorerPlugin::recentProjects();
  if (recentProjects.count() <= index.row())
    return {};
  auto data = recentProjects.at(index.row());
  switch (role) {
  case Qt::DisplayRole:
    return data.second;
  case Qt::ToolTipRole:
  case FilePathRole:
    return data.first;
  case PrettyFilePathRole:
    return withTildeHomePath(data.first);
  case ShortcutRole: {
    const Id projectBase = PROJECT_BASE_ID;
    if (const auto cmd = ActionManager::command(projectBase.withSuffix(index.row() + 1)))
      return cmd->keySequence().toString(QKeySequence::NativeText);
    return QVariant();
  }
  default:
    return QVariant();
  }
}

auto ProjectModel::roleNames() const -> QHash<int, QByteArray>
{
  static QHash<int, QByteArray> extraRoles{{Qt::DisplayRole, "displayName"}, {FilePathRole, "filePath"}, {PrettyFilePathRole, "prettyFilePath"}};

  return extraRoles;
}

auto ProjectModel::resetProjects() -> void
{
  beginResetModel();
  endResetModel();
}

///////////////////

ProjectWelcomePage::ProjectWelcomePage() {}

auto ProjectWelcomePage::id() const -> Id
{
  return "Develop";
}

auto ProjectWelcomePage::reloadWelcomeScreenData() const -> void
{
  if (m_sessionModel)
    m_sessionModel->resetSessions();
  if (m_projectModel)
    m_projectModel->resetProjects();
}

auto ProjectWelcomePage::newProject() -> void
{
  ProjectExplorerPlugin::openNewProjectDialog();
}

auto ProjectWelcomePage::openProject() -> void
{
  ProjectExplorerPlugin::openOpenProjectDialog();
}

auto ProjectWelcomePage::openSessionAt(int index) -> void
{
  QTC_ASSERT(m_sessionModel, return);
  m_sessionModel->switchToSession(m_sessionModel->sessionAt(index));
}

auto ProjectWelcomePage::openProjectAt(int index) -> void
{
  QTC_ASSERT(m_projectModel, return);
  const auto projectFile = m_projectModel->data(m_projectModel->index(index, 0), ProjectModel::FilePathRole).toString();
  ProjectExplorerPlugin::openProjectWelcomePage(projectFile);
}

auto ProjectWelcomePage::createActions() -> void
{
  static auto actionsRegistered = false;

  if (actionsRegistered)
    return;

  actionsRegistered = true;

  const auto actionsCount = 9;
  const Context welcomeContext(Core::Constants::C_WELCOME_MODE);

  const Id projectBase = PROJECT_BASE_ID;
  const Id sessionBase = SESSION_BASE_ID;

  for (auto i = 1; i <= actionsCount; ++i) {
    auto act = new QAction(tr("Open Session #%1").arg(i), this);
    auto cmd = ActionManager::registerAction(act, sessionBase.withSuffix(i), welcomeContext);
    cmd->setDefaultKeySequence(QKeySequence((use_mac_shortcuts ? tr("Ctrl+Meta+%1") : tr("Ctrl+Alt+%1")).arg(i)));
    connect(act, &QAction::triggered, this, [this, i] {
      if (i <= m_sessionModel->rowCount())
        openSessionAt(i - 1);
    });

    act = new QAction(tr("Open Recent Project #%1").arg(i), this);
    cmd = ActionManager::registerAction(act, projectBase.withSuffix(i), welcomeContext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+%1").arg(i)));
    connect(act, &QAction::triggered, this, [this, i] {
      if (i <= m_projectModel->rowCount(QModelIndex()))
        openProjectAt(i - 1);
    });
  }
}

///////////////////

static auto themeColor(Theme::Color role) -> QColor
{
  return Utils::orcaTheme()->color(role);
}

static auto sizedFont(int size, const QWidget *widget, bool underline = false) -> QFont
{
  auto f = widget->font();
  f.setPixelSize(size);
  f.setUnderline(underline);
  return f;
}

static auto pixmap(const QString &id, const Theme::Color &color) -> QPixmap
{
  const auto fileName = QString(":/welcome/images/%1.png").arg(id);
  return Icon({{FilePath::fromString(fileName), color}}, Icon::Tint).pixmap();
}

class BaseDelegate : public QAbstractItemDelegate {
protected:
  virtual auto entryType() -> QString = 0;

  virtual auto toolTipArea(const QRect &itemRect, const QModelIndex &) const -> QRect
  {
    return itemRect;
  }

  virtual auto shortcutRole() const -> int = 0;

  auto helpEvent(QHelpEvent *ev, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &idx) -> bool final
  {
    if (!toolTipArea(option.rect, idx).contains(ev->pos())) {
      QToolTip::hideText();
      return false;
    }

    auto shortcut = idx.data(shortcutRole()).toString();

    auto name = idx.data(Qt::DisplayRole).toString();
    QString tooltipText;
    const auto type = entryType();
    if (shortcut.isEmpty())
      tooltipText = ProjectWelcomePage::tr("Open %1 \"%2\"").arg(type, name);
    else
      tooltipText = ProjectWelcomePage::tr("Open %1 \"%2\" (%3)").arg(type, name, shortcut);

    if (tooltipText.isEmpty())
      return false;

    QToolTip::showText(ev->globalPos(), tooltipText, view);
    return true;
  }
};

class SessionDelegate : public BaseDelegate {
protected:
  auto entryType() -> QString override
  {
    return ProjectWelcomePage::tr("session", "Appears in \"Open session <name>\"");
  }

  auto toolTipArea(const QRect &itemRect, const QModelIndex &idx) const -> QRect override
  {
    // in expanded state bottom contains 'Clone', 'Rename', etc links, where the tool tip
    // would be confusing
    const auto expanded = m_expandedSessions.contains(idx.data(Qt::DisplayRole).toString());
    return expanded ? itemRect.adjusted(0, 0, 0, -LINK_HEIGHT) : itemRect;
  }

  auto shortcutRole() const -> int override { return SessionModel::ShortcutRole; }

public:
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const -> void final
  {
    static const auto sessionIcon = pixmap("session", Theme::Welcome_ForegroundSecondaryColor);

    const auto rc = option.rect;
    const auto sessionName = idx.data(Qt::DisplayRole).toString();

    const auto mousePos = option.widget->mapFromGlobal(QCursor::pos());
    //const bool hovered = option.state & QStyle::State_MouseOver;
    const auto hovered = option.rect.contains(mousePos);
    const auto expanded = m_expandedSessions.contains(sessionName);
    painter->fillRect(rc, themeColor(Theme::Welcome_BackgroundSecondaryColor));
    painter->fillRect(rc.adjusted(0, 0, 0, -G_ITEM_GAP), hovered ? hoverColor : backgroundPrimaryColor);

    const auto x = rc.x();
    const auto x1 = x + TEXT_OFFSET_HORIZONTAL;
    const auto y = rc.y();
    const auto firstBase = y + 18;

    painter->drawPixmap(x + 11, y + 6, sessionIcon);

    if (hovered && !expanded) {
      const auto arrowRect = rc.adjusted(rc.width() - SESSION_ARROW_RECT_WIDTH, 0, 0, 0);
      const auto arrowRectHovered = arrowRect.contains(mousePos);
      painter->fillRect(arrowRect.adjusted(0, 0, 0, -G_ITEM_GAP), arrowRectHovered ? hoverColor : backgroundPrimaryColor);
    }

    if (hovered || expanded) {
      static const auto arrowUp = pixmap("expandarrow", Theme::Welcome_ForegroundSecondaryColor);
      static const auto arrowDown = QPixmap::fromImage(arrowUp.toImage().mirrored(false, true));
      painter->drawPixmap(rc.right() - 19, y + 6, expanded ? arrowDown : arrowUp);
    }

    if (idx.row() < 9) {
      painter->setPen(foregroundSecondaryColor);
      painter->setFont(sizedFont(10, option.widget));
      painter->drawText(x + 3, firstBase, QString::number(idx.row() + 1));
    }

    const auto isLastSession = idx.data(SessionModel::LastSessionRole).toBool();
    const auto isActiveSession = idx.data(SessionModel::ActiveSessionRole).toBool();
    const auto isDefaultVirgin = SessionManager::isDefaultVirgin();

    auto fullSessionName = sessionName;
    if (isLastSession && isDefaultVirgin)
      fullSessionName = ProjectWelcomePage::tr("%1 (last session)").arg(fullSessionName);
    if (isActiveSession && !isDefaultVirgin)
      fullSessionName = ProjectWelcomePage::tr("%1 (current session)").arg(fullSessionName);

    const auto switchRect = QRect(x, y, rc.width() - SESSION_ARROW_RECT_WIDTH, SESSION_LINE_HEIGHT);
    const auto switchActive = switchRect.contains(mousePos);
    const auto textSpace = rc.width() - TEXT_OFFSET_HORIZONTAL - 6;
    const auto sessionNameTextSpace = textSpace - (hovered || expanded ? SESSION_ARROW_RECT_WIDTH : 0);
    painter->setPen(linkColor);
    painter->setFont(sizedFont(13, option.widget, switchActive));
    const auto fullSessionNameElided = painter->fontMetrics().elidedText(fullSessionName, Qt::ElideRight, sessionNameTextSpace);
    painter->drawText(x1, firstBase, fullSessionNameElided);
    if (switchActive)
      m_activeSwitchToRect = switchRect;

    if (expanded) {
      painter->setPen(textColor);
      painter->setFont(sizedFont(12, option.widget));
      const auto projects = SessionManager::projectsForSessionName(sessionName);
      auto yy = firstBase + SESSION_LINE_HEIGHT - 3;
      QFontMetrics fm(option.widget->font());
      for (const auto &project : projects) {
        // Project name.
        auto projectPath = FilePath::fromString(project);
        auto completeBase = projectPath.completeBaseName();
        painter->setPen(textColor);
        painter->drawText(x1, yy, fm.elidedText(completeBase, Qt::ElideMiddle, textSpace));
        yy += 18;

        // Project path.
        auto pathWithTilde = withTildeHomePath(projectPath.toUserOutput());
        painter->setPen(foregroundPrimaryColor);
        painter->drawText(x1, yy, fm.elidedText(pathWithTilde, Qt::ElideMiddle, textSpace));
        yy += 22;
      }

      yy += 3;
      auto xx = x1;
      const QStringList actions = {ProjectWelcomePage::tr("Clone"), ProjectWelcomePage::tr("Rename"), ProjectWelcomePage::tr("Delete")};
      for (auto i = 0; i < 3; ++i) {
        const auto &action = actions.at(i);
        const auto ww = fm.horizontalAdvance(action);
        const auto spacing = 7; // Between action link and separator line
        const auto actionRect = QRect(xx, yy - 10, ww, 15).adjusted(-spacing, -spacing, spacing, spacing);
        const auto isForcedDisabled = (i != 0 && sessionName == "default");
        const auto isActive = actionRect.contains(mousePos) && !isForcedDisabled;
        painter->setPen(isForcedDisabled ? disabledLinkColor : linkColor);
        painter->setFont(sizedFont(12, option.widget, isActive));
        painter->drawText(xx, yy, action);
        if (i < 2) {
          xx += ww + 2 * spacing;
          auto pp = xx - spacing;
          painter->setPen(textColor);
          painter->drawLine(pp, yy - 10, pp, yy);
        }
        if (isActive)
          m_activeActionRects[i] = actionRect;
      }
    }
  }

  auto sizeHint(const QStyleOptionViewItem &, const QModelIndex &idx) const -> QSize final
  {
    auto h = SESSION_LINE_HEIGHT;
    const auto sessionName = idx.data(Qt::DisplayRole).toString();
    if (m_expandedSessions.contains(sessionName)) {
      const auto projects = SessionManager::projectsForSessionName(sessionName);
      h += projects.size() * 40 + LINK_HEIGHT - 6;
    }
    return QSize(380, h + G_ITEM_GAP);
  }

  auto editorEvent(QEvent *ev, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &idx) -> bool final
  {
    if (ev->type() == QEvent::MouseButtonRelease) {
      const QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(ev);
      const Qt::MouseButtons button = mouseEvent->button();
      const auto pos = static_cast<QMouseEvent*>(ev)->pos();
      const QRect rc(option.rect.right() - SESSION_ARROW_RECT_WIDTH, option.rect.top(), SESSION_ARROW_RECT_WIDTH, SESSION_LINE_HEIGHT);
      const auto sessionName = idx.data(Qt::DisplayRole).toString();
      if (rc.contains(pos) || button == Qt::RightButton) {
        // The expand/collapse "button".
        if (m_expandedSessions.contains(sessionName))
          m_expandedSessions.removeOne(sessionName);
        else
          m_expandedSessions.append(sessionName);
        emit model->layoutChanged({QPersistentModelIndex(idx)});
        return true;
      }
      if (button == Qt::LeftButton) {
        // One of the action links?
        const auto sessionModel = qobject_cast<SessionModel*>(model);
        QTC_ASSERT(sessionModel, return false);
        if (m_activeSwitchToRect.contains(pos))
          sessionModel->switchToSession(sessionName);
        else if (m_activeActionRects[0].contains(pos))
          sessionModel->cloneSession(ICore::dialogParent(), sessionName);
        else if (m_activeActionRects[1].contains(pos))
          sessionModel->renameSession(ICore::dialogParent(), sessionName);
        else if (m_activeActionRects[2].contains(pos))
          sessionModel->deleteSessions(QStringList(sessionName));
        return true;
      }
    }
    if (ev->type() == QEvent::MouseMove) {
      emit model->layoutChanged({QPersistentModelIndex(idx)}); // Somewhat brutish.
      //update(option.rect);
      return false;
    }
    return false;
  }

private:
  const QColor hoverColor = themeColor(Theme::Welcome_HoverColor);
  const QColor textColor = themeColor(Theme::Welcome_TextColor);
  const QColor linkColor = themeColor(Theme::Welcome_LinkColor);
  const QColor disabledLinkColor = themeColor(Theme::Welcome_DisabledLinkColor);
  const QColor backgroundPrimaryColor = themeColor(Theme::Welcome_BackgroundPrimaryColor);
  const QColor foregroundPrimaryColor = themeColor(Theme::Welcome_ForegroundPrimaryColor);
  const QColor foregroundSecondaryColor = themeColor(Theme::Welcome_ForegroundSecondaryColor);

  QStringList m_expandedSessions;

  mutable QRect m_activeSwitchToRect;
  mutable QRect m_activeActionRects[3];
};

class ProjectDelegate : public BaseDelegate {
  auto entryType() -> QString override
  {
    return ProjectWelcomePage::tr("project", "Appears in \"Open project <name>\"");
  }

  auto shortcutRole() const -> int override { return ProjectModel::ShortcutRole; }

public:
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const -> void final
  {
    const auto rc = option.rect;

    const auto hovered = option.widget->isActiveWindow() && option.state & QStyle::State_MouseOver;
    const QRect bgRect = rc.adjusted(0, 0, -G_ITEM_GAP, -G_ITEM_GAP);
    painter->fillRect(rc, themeColor(Theme::Welcome_BackgroundSecondaryColor));
    painter->fillRect(bgRect, themeColor(hovered ? Theme::Welcome_HoverColor : Theme::Welcome_BackgroundPrimaryColor));

    const auto x = rc.x();
    const auto y = rc.y();
    const auto firstBase = y + 18;
    const auto secondBase = firstBase + 19;

    static const auto projectIcon = pixmap("project", Theme::Welcome_ForegroundSecondaryColor);
    painter->drawPixmap(x + 11, y + 6, projectIcon);

    const auto projectName = idx.data(Qt::DisplayRole).toString();
    const auto projectPath = FilePath::fromVariant(idx.data(ProjectModel::FilePathRole));

    painter->setPen(themeColor(Theme::Welcome_ForegroundSecondaryColor));
    painter->setFont(sizedFont(10, option.widget));

    if (idx.row() < 9)
      painter->drawText(x + 3, firstBase, QString::number(idx.row() + 1));

    const int textSpace = rc.width() - TEXT_OFFSET_HORIZONTAL - G_ITEM_GAP - 6;

    painter->setPen(themeColor(Theme::Welcome_LinkColor));
    painter->setFont(sizedFont(13, option.widget, hovered));
    const QString projectNameElided = painter->fontMetrics().elidedText(projectName, Qt::ElideRight, textSpace);
    painter->drawText(x + TEXT_OFFSET_HORIZONTAL, firstBase, projectNameElided);

    painter->setPen(themeColor(Theme::Welcome_ForegroundPrimaryColor));
    painter->setFont(sizedFont(13, option.widget));
    const auto pathWithTilde = withTildeHomePath(projectPath.toUserOutput());
    const QString pathWithTildeElided = painter->fontMetrics().elidedText(pathWithTilde, Qt::ElideMiddle, textSpace);
    painter->drawText(x + TEXT_OFFSET_HORIZONTAL, secondBase, pathWithTildeElided);
  }

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const -> QSize final
  {
    const auto projectName = idx.data(Qt::DisplayRole).toString();
    const auto projectPath = idx.data(ProjectModel::FilePathRole).toString();
    const QFontMetrics fm(sizedFont(13, option.widget));
    const auto width = std::max(fm.horizontalAdvance(projectName), fm.horizontalAdvance(projectPath)) + TEXT_OFFSET_HORIZONTAL;
    return QSize(width, 47 + G_ITEM_GAP);
  }

  auto editorEvent(QEvent *ev, QAbstractItemModel *model, const QStyleOptionViewItem &, const QModelIndex &idx) -> bool final
  {
    if (ev->type() == QEvent::MouseButtonRelease) {
      const QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(ev);
      const Qt::MouseButtons button = mouseEvent->button();
      if (button == Qt::LeftButton) {
        const auto projectFile = idx.data(ProjectModel::FilePathRole).toString();
        ProjectExplorerPlugin::openProjectWelcomePage(projectFile);
        return true;
      }
      if (button == Qt::RightButton) {
        QMenu contextMenu;
        auto action = new QAction(ProjectWelcomePage::tr("Remove Project from Recent Projects"));
        const auto projectModel = qobject_cast<ProjectModel*>(model);
        contextMenu.addAction(action);
        connect(action, &QAction::triggered, [idx, projectModel]() {
          const auto projectFile = idx.data(ProjectModel::FilePathRole).toString();
          const auto displayName = idx.data(Qt::DisplayRole).toString();
          ProjectExplorerPlugin::removeFromRecentProjects(projectFile, displayName);
          projectModel->resetProjects();
        });
        contextMenu.addSeparator();
        action = new QAction(ProjectWelcomePage::tr("Clear Recent Project List"));
        connect(action, &QAction::triggered, [projectModel]() {
          ProjectExplorerPlugin::clearRecentProjects();
          projectModel->resetProjects();
        });
        contextMenu.addAction(action);
        contextMenu.exec(mouseEvent->globalPos());
        return true;
      }
    }
    return false;
  }
};

class TreeView : public QTreeView {
public:
  TreeView(QWidget *parent, const QString &name) : QTreeView(parent)
  {
    setObjectName(name);
    header()->hide();
    setMouseTracking(true); // To enable hover.
    setIndentation(0);
    setSelectionMode(NoSelection);
    setFrameShape(NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setFocusPolicy(Qt::NoFocus);

    QPalette pal;
    pal.setColor(QPalette::Base, themeColor(Theme::Welcome_BackgroundSecondaryColor));
    viewport()->setPalette(pal);
  }
};

class SessionsPage : public QWidget {
public:
  explicit SessionsPage(ProjectWelcomePage *projectWelcomePage)
  {
    // FIXME: Remove once facilitateQml() is gone.
    if (!projectWelcomePage->m_sessionModel)
      projectWelcomePage->m_sessionModel = new SessionModel(this);
    if (!projectWelcomePage->m_projectModel)
      projectWelcomePage->m_projectModel = new ProjectModel(this);

    const auto manageSessionsButton = new WelcomePageButton(this);
    manageSessionsButton->setText(ProjectWelcomePage::tr("Manage..."));
    manageSessionsButton->setWithAccentColor(true);
    manageSessionsButton->setOnClicked([] { ProjectExplorerPlugin::showSessionManager(); });

    const auto sessionsLabel = new QLabel(this);
    sessionsLabel->setFont(brandFont());
    sessionsLabel->setText(ProjectWelcomePage::tr("Sessions"));

    const auto recentProjectsLabel = new QLabel(this);
    recentProjectsLabel->setFont(brandFont());
    recentProjectsLabel->setText(ProjectWelcomePage::tr("Projects"));

    const auto sessionsList = new TreeView(this, "Sessions");
    sessionsList->setModel(projectWelcomePage->m_sessionModel);
    sessionsList->header()->setSectionHidden(1, true); // The "last modified" column.
    sessionsList->setItemDelegate(&m_sessionDelegate);
    sessionsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    const auto projectsList = new TreeView(this, "Recent Projects");
    projectsList->setUniformRowHeights(true);
    projectsList->setModel(projectWelcomePage->m_projectModel);
    projectsList->setItemDelegate(&m_projectDelegate);
    projectsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    const auto sessionHeader = panelBar(this);
    const auto hbox11 = new QHBoxLayout(sessionHeader);
    hbox11->setContentsMargins(12, 0, 0, 0);
    hbox11->addWidget(sessionsLabel);
    hbox11->addStretch(1);
    hbox11->addWidget(manageSessionsButton);

    const auto projectsHeader = panelBar(this);
    const auto hbox21 = new QHBoxLayout(projectsHeader);
    hbox21->setContentsMargins(hbox11->contentsMargins());
    hbox21->addWidget(recentProjectsLabel);

    const auto grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, G_ITEM_GAP);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(G_ITEM_GAP);
    grid->addWidget(panelBar(this), 0, 0);
    grid->addWidget(sessionHeader, 0, 1);
    grid->addWidget(sessionsList, 1, 1);
    grid->addWidget(panelBar(this), 0, 2);
    grid->setColumnStretch(1, 9);
    grid->setColumnMinimumWidth(1, 200);
    grid->addWidget(projectsHeader, 0, 3);
    grid->addWidget(projectsList, 1, 3);
    grid->setColumnStretch(3, 20);
  }

  SessionDelegate m_sessionDelegate;
  ProjectDelegate m_projectDelegate;
};

auto ProjectWelcomePage::createWidget() const -> QWidget*
{
  const auto that = const_cast<ProjectWelcomePage*>(this);
  QWidget *widget = new SessionsPage(that);
  that->createActions();

  return widget;
}

} // namespace Internal
} // namespace ProjectExplorer
