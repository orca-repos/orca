// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/imode.hpp>

namespace Core {
class IEditor;

/**
  * A global mode for Design pane - used by Bauhaus (QML Designer) and
  * Qt Designer. Other plugins can register themselves by registerDesignWidget()
  * and giving a list of mimetypes that the editor understands, as well as an instance
  * to the main editor widget itself.
  */

class CORE_EXPORT DesignMode final : public IMode {
  Q_OBJECT

public:
  static auto instance() -> DesignMode*;
  static auto setDesignModeIsRequired() -> void;
  static auto registerDesignWidget(QWidget *widget, const QStringList &mime_types, const Context &context) -> void;
  static auto unregisterDesignWidget(QWidget *widget) -> void;
  static auto createModeIfRequired() -> void;
  static auto destroyModeIfRequired() -> void;

signals:
  auto actionsUpdated(Core::IEditor *editor) -> void;

private:
  DesignMode();
  ~DesignMode() override;

  auto updateActions() -> void;
  auto currentEditorChanged(IEditor *editor) -> void;
  auto updateContext(Utils::Id new_mode, Utils::Id old_mode) const -> void;
  auto setActiveContext(const Context &context) const -> void;
};

} // namespace Core
