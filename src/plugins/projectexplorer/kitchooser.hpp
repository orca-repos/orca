// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "kit.hpp"

#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
QT_END_NAMESPACE

namespace ProjectExplorer {

// Let the user pick a kit.
class PROJECTEXPLORER_EXPORT KitChooser : public QWidget {
  Q_OBJECT

public:
  explicit KitChooser(QWidget *parent = nullptr);

  auto setCurrentKitId(Utils::Id id) -> void;
  auto currentKitId() const -> Utils::Id;
  auto setKitPredicate(const Kit::Predicate &predicate) -> void;
  auto setShowIcons(bool showIcons) -> void;
  auto currentKit() const -> Kit*;
  auto hasStartupKit() const -> bool { return m_hasStartupKit; }

signals:
  auto currentIndexChanged() -> void;
  auto activated() -> void;

public slots:
  auto populate() -> void;

protected:
  virtual auto kitText(const Kit *k) const -> QString;
  virtual auto kitToolTip(Kit *k) const -> QString;

private:
  auto onActivated() -> void;
  auto onCurrentIndexChanged() -> void;
  auto onManageButtonClicked() -> void;

  Kit::Predicate m_kitPredicate;
  QComboBox *m_chooser;
  QPushButton *m_manageButton;
  bool m_hasStartupKit = false;
  bool m_showIcons = false;
};

} // namespace ProjectExplorer
