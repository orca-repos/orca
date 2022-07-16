// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <utils/aspects.hpp>

namespace ProjectExplorer {
class Kit;
}

namespace QtSupport {

class QTSUPPORT_EXPORT QmlDebuggingAspect : public Utils::TriStateAspect {
  Q_OBJECT

public:
  QmlDebuggingAspect();

  auto setKit(const ProjectExplorer::Kit *kit) -> void { m_kit = kit; }
  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;

private:
  const ProjectExplorer::Kit *m_kit = nullptr;
};

class QTSUPPORT_EXPORT QtQuickCompilerAspect : public Utils::TriStateAspect {
  Q_OBJECT

public:
  QtQuickCompilerAspect();

  auto setKit(const ProjectExplorer::Kit *kit) -> void { m_kit = kit; }

private:
  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  auto acquaintSiblings(const Utils::AspectContainer &siblings) -> void override;

  const ProjectExplorer::Kit *m_kit = nullptr;
  const QmlDebuggingAspect *m_qmlDebuggingAspect = nullptr;
};

} // namespace QtSupport
