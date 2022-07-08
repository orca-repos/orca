// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "kitmanager.hpp"

#include <QWidget>

#include <memory>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QToolButton;
QT_END_NAMESPACE namespace ProjectExplorer {

class Kit;

namespace Internal {

class KitManagerConfigWidget : public QWidget {
  Q_OBJECT

public:
  explicit KitManagerConfigWidget(Kit *k);
  ~KitManagerConfigWidget() override;

  auto displayName() const -> QString;
  auto displayIcon() const -> QIcon;
  auto apply() -> void;
  auto discard() -> void;
  auto isDirty() const -> bool;
  auto validityMessage() const -> QString;
  auto addAspectToWorkingCopy(KitAspect *aspect) -> void;
  auto makeStickySubWidgetsReadOnly() -> void;
  auto workingCopy() const -> Kit*;
  auto configures(Kit *k) const -> bool;
  auto isRegistering() const -> bool { return m_isRegistering; }
  auto setIsDefaultKit(bool d) -> void;
  auto isDefaultKit() const -> bool;
  auto removeKit() -> void;
  auto updateVisibility() -> void;
  auto setHasUniqueName(bool unique) -> void;

signals:
  auto dirty() -> void;
  auto isAutoDetectedChanged() -> void;

private:
  auto setIcon() -> void;
  auto resetIcon() -> void;
  auto setDisplayName() -> void;
  auto setFileSystemFriendlyName() -> void;
  auto workingCopyWasUpdated(Kit *k) -> void;
  auto kitWasUpdated(Kit *k) -> void;

  enum LayoutColumns {
    LabelColumn,
    WidgetColumn,
    ButtonColumn
  };

  auto showEvent(QShowEvent *event) -> void override;

  QToolButton *m_iconButton;
  QLineEdit *m_nameEdit;
  QLineEdit *m_fileSystemFriendlyNameLineEdit;
  QList<KitAspectWidget*> m_widgets;
  Kit *m_kit;
  std::unique_ptr<Kit> m_modifiedKit;
  bool m_isDefaultKit = false;
  bool m_fixingKit = false;
  bool m_hasUniqueName = true;
  bool m_isRegistering = false;
  mutable QString m_cachedDisplayName;
};

} // namespace Internal
} // namespace ProjectExplorer
