// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sidebarwidget.h"
#include "sidebar.h"
#include "navigationsubwidget.h"

#include <utils/algorithm.h>
#include <utils/utilsicons.h>

#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QVBoxLayout>

namespace Core {
namespace Internal {

class SideBarComboBox final : public CommandComboBox {
public:
  enum DataRoles {
    IdRole = Qt::UserRole
  };

  explicit SideBarComboBox(SideBarWidget *side_bar_widget) : m_side_bar_widget(side_bar_widget) {}

private:
  auto command(const QString &text) const -> const Command* override
  {
    return m_side_bar_widget->command(text);
  }

  SideBarWidget *m_side_bar_widget;
};

SideBarWidget::SideBarWidget(SideBar *side_bar, const QString &id) : m_side_bar(side_bar)
{
  m_combo_box = new SideBarComboBox(this);
  m_combo_box->setMinimumContentsLength(15);

  m_toolbar = new QToolBar(this);
  m_toolbar->setContentsMargins(0, 0, 0, 0);
  m_toolbar->addWidget(m_combo_box);

  const auto spacer_item = new QWidget(this);
  spacer_item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_toolbar->addWidget(spacer_item);

  m_split_action = new QAction(tr("Split"), m_toolbar);
  m_split_action->setToolTip(tr("Split"));
  m_split_action->setIcon(Utils::Icons::SPLIT_HORIZONTAL_TOOLBAR.icon());
  connect(m_split_action, &QAction::triggered, this, &SideBarWidget::splitMe);
  m_toolbar->addAction(m_split_action);

  m_close_action = new QAction(tr("Close"), m_toolbar);
  m_close_action->setToolTip(tr("Close"));
  m_close_action->setIcon(Utils::Icons::CLOSE_SPLIT_BOTTOM.icon());
  connect(m_close_action, &QAction::triggered, this, &SideBarWidget::closeMe);
  m_toolbar->addAction(m_close_action);

  const auto lay = new QVBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  setLayout(lay);
  lay->addWidget(m_toolbar);

  auto title_list = m_side_bar->availableItemTitles();
  Utils::sort(title_list);
  auto t = id;

  if (!title_list.isEmpty()) {
    for(const auto &item_title: title_list)
      m_combo_box->addItem(item_title, m_side_bar->idForTitle(item_title));
    m_combo_box->setCurrentIndex(0);
    if (t.isEmpty())
      t = m_combo_box->itemData(0, SideBarComboBox::IdRole).toString();
  }

  setCurrentItem(t);
  connect(m_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SideBarWidget::setCurrentIndex);
}

SideBarWidget::~SideBarWidget() = default;

auto SideBarWidget::currentItemTitle() const -> QString
{
  return m_combo_box->currentText();
}

auto SideBarWidget::currentItemId() const -> QString
{
  if (m_current_item)
    return m_current_item->id();
  return {};
}

auto SideBarWidget::setCurrentItem(const QString &id) -> void
{
  if (!id.isEmpty()) {
    auto idx = m_combo_box->findData(QVariant(id), SideBarComboBox::IdRole);

    if (idx < 0)
      idx = 0;

    QSignalBlocker blocker(m_combo_box);
    m_combo_box->setCurrentIndex(idx);
  }

  const auto item = m_side_bar->item(id);

  if (!item)
    return;

  removeCurrentItem();
  m_current_item = item;

  layout()->addWidget(m_current_item->widget());
  m_current_item->widget()->show();

  // Add buttons and remember their actions for later removal
  for(const auto &b: m_current_item->createToolBarWidgets())
    m_added_tool_bar_actions.append(m_toolbar->insertWidget(m_split_action, b));
}

auto SideBarWidget::updateAvailableItems() const -> void
{
  QSignalBlocker blocker(m_combo_box);
  const auto current_title = m_combo_box->currentText();
  m_combo_box->clear();
  auto title_list = m_side_bar->availableItemTitles();

  if (!current_title.isEmpty() && !title_list.contains(current_title))
    title_list.append(current_title);

  Utils::sort(title_list);

  for(const auto &item_title: title_list)
    m_combo_box->addItem(item_title, m_side_bar->idForTitle(item_title));

  auto idx = m_combo_box->findText(current_title);

  if (idx < 0)
    idx = 0;

  m_combo_box->setCurrentIndex(idx);
  m_split_action->setEnabled(title_list.count() > 1);
}

auto SideBarWidget::removeCurrentItem() -> void
{
  if (!m_current_item)
    return;

  const auto w = m_current_item->widget();
  w->hide();
  layout()->removeWidget(w);
  w->setParent(nullptr);
  m_side_bar->makeItemAvailable(m_current_item);

  // Delete custom toolbar widgets
  qDeleteAll(m_added_tool_bar_actions);
  m_added_tool_bar_actions.clear();

  m_current_item = nullptr;
}

auto SideBarWidget::setCurrentIndex(int) -> void
{
  setCurrentItem(m_combo_box->itemData(m_combo_box->currentIndex(), SideBarComboBox::IdRole).toString());
  emit currentWidgetChanged();
}

auto SideBarWidget::command(const QString &title) const -> Command*
{
  const auto id = m_side_bar->idForTitle(title);

  if (id.isEmpty())
    return nullptr;

  const auto shortcut_map = m_side_bar->shortcutMap();

  if (const auto r = shortcut_map.find(id); r != shortcut_map.end())
    return r.value();

  return nullptr;
}

auto SideBarWidget::setCloseIcon(const QIcon &icon) const -> void
{
  m_close_action->setIcon(icon);
}

} // namespace Internal
} // namespace Core
