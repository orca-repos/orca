// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "abi.hpp"

#include <QWidget>

#include <memory>

namespace ProjectExplorer {

namespace Internal {
class AbiWidgetPrivate;
}

// --------------------------------------------------------------------------
// AbiWidget:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT AbiWidget : public QWidget {
  Q_OBJECT

public:
  AbiWidget(QWidget *parent = nullptr);
  ~AbiWidget() override;

  auto setAbis(const Abis &, const Abi &currentAbi) -> void;
  auto supportedAbis() const -> Abis;
  auto isCustomAbi() const -> bool;
  auto currentAbi() const -> Abi;

signals:
  auto abiChanged() -> void;

private:
  auto mainComboBoxChanged() -> void;
  auto customOsComboBoxChanged() -> void;
  auto customComboBoxesChanged() -> void;
  auto setCustomAbiComboBoxes(const Abi &current) -> void;
  auto emitAbiChanged(const Abi &current) -> void;

  const std::unique_ptr<Internal::AbiWidgetPrivate> d;
};

} // namespace ProjectExplorer
