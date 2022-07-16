// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitchooser.hpp"

#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "session.hpp"
#include "target.hpp"

#include <core/core-interface.hpp>

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {

constexpr char lastKitKey[] = "LastSelectedKit";

KitChooser::KitChooser(QWidget *parent) : QWidget(parent), m_kitPredicate([](const Kit *k) { return k->isValid(); })
{
  m_chooser = new QComboBox(this);
  m_chooser->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  m_manageButton = new QPushButton(KitAspectWidget::msgManage(), this);

  const auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_chooser);
  layout->addWidget(m_manageButton);
  setFocusProxy(m_manageButton);

  connect(m_chooser, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KitChooser::onCurrentIndexChanged);
  connect(m_chooser, QOverload<int>::of(&QComboBox::activated), this, &KitChooser::onActivated);
  connect(m_manageButton, &QAbstractButton::clicked, this, &KitChooser::onManageButtonClicked);
  connect(KitManager::instance(), &KitManager::kitsChanged, this, &KitChooser::populate);
}

auto KitChooser::onManageButtonClicked() -> void
{
  ICore::showOptionsDialog(Constants::KITS_SETTINGS_PAGE_ID, this);
}

auto KitChooser::setShowIcons(bool showIcons) -> void
{
  m_showIcons = showIcons;
}

auto KitChooser::onCurrentIndexChanged() -> void
{
  const auto id = Id::fromSetting(m_chooser->currentData());
  const auto kit = KitManager::kit(id);
  setToolTip(kit ? kitToolTip(kit) : QString());
  emit currentIndexChanged();
}

auto KitChooser::onActivated() -> void
{
  // Active user interaction.
  auto id = Id::fromSetting(m_chooser->currentData());
  if (m_hasStartupKit && m_chooser->currentIndex() == 0)
    id = Id(); // Special value to indicate startup kit.
  ICore::settings()->setValueWithDefault(lastKitKey, id.toSetting(), Id().toSetting());
  emit activated();
}

auto KitChooser::kitText(const Kit *k) const -> QString
{
  return k->displayName();
}

auto KitChooser::kitToolTip(Kit *k) const -> QString
{
  return k->toHtml();
}

auto KitChooser::populate() -> void
{
  m_chooser->clear();

  const auto lastKit = Id::fromSetting(ICore::settings()->value(lastKitKey));
  auto didActivate = false;

  if (const auto target = SessionManager::startupTarget()) {
    const auto kit = target->kit();
    if (m_kitPredicate(kit)) {
      const auto display = tr("Kit of Active Project: %1").arg(kitText(kit));
      m_chooser->addItem(display, kit->id().toSetting());
      m_chooser->setItemData(0, kitToolTip(kit), Qt::ToolTipRole);
      if (!lastKit.isValid()) {
        m_chooser->setCurrentIndex(0);
        didActivate = true;
      }
      m_chooser->insertSeparator(1);
      m_hasStartupKit = true;
    }
  }

  foreach(Kit *kit, KitManager::sortKits(KitManager::kits())) {
    if (m_kitPredicate(kit)) {
      m_chooser->addItem(kitText(kit), kit->id().toSetting());
      const auto pos = m_chooser->count() - 1;
      m_chooser->setItemData(pos, kitToolTip(kit), Qt::ToolTipRole);
      if (m_showIcons)
        m_chooser->setItemData(pos, kit->displayIcon(), Qt::DecorationRole);
      if (!didActivate && kit->id() == lastKit) {
        m_chooser->setCurrentIndex(pos);
        didActivate = true;
      }
    }
  }

  const auto n = m_chooser->count();
  m_chooser->setEnabled(n > 1);

  if (n > 1)
    setFocusProxy(m_chooser);
  else
    setFocusProxy(m_manageButton);
}

auto KitChooser::currentKit() const -> Kit*
{
  const auto id = Id::fromSetting(m_chooser->currentData());
  return KitManager::kit(id);
}

auto KitChooser::setCurrentKitId(Id id) -> void
{
  const auto v = id.toSetting();
  for (auto i = 0, n = m_chooser->count(); i != n; ++i) {
    if (m_chooser->itemData(i) == v) {
      m_chooser->setCurrentIndex(i);
      break;
    }
  }
}

auto KitChooser::currentKitId() const -> Id
{
  const auto kit = currentKit();
  return kit ? kit->id() : Id();
}

auto KitChooser::setKitPredicate(const Kit::Predicate &predicate) -> void
{
  m_kitPredicate = predicate;
  populate();
}

} // namespace ProjectExplorer
