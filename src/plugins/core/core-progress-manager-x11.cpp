// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "progress-manager-private.hpp"

namespace Orca::Plugin::Core {

void ProgressManagerPrivate::initInternal() {}
void ProgressManagerPrivate::cleanup() {}
void ProgressManagerPrivate::doSetApplicationLabel(const QString &text) {}
void ProgressManagerPrivate::setApplicationProgressRange(int min, int max) {}
void ProgressManagerPrivate::setApplicationProgressValue(int value) {}
void ProgressManagerPrivate::setApplicationProgressVisible(bool visible) {}

} // namespace Orca::Plugin::Core
