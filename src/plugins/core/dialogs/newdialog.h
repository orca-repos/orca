// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/filepath.h>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Core {

class IWizardFactory;

class CORE_EXPORT NewDialog {
public:
  NewDialog();
  virtual ~NewDialog() = 0;

  virtual auto widget() -> QWidget* = 0;
  virtual auto setWizardFactories(QList<IWizardFactory*> factories, const Utils::FilePath &default_location, const QVariantMap &extra_variables) -> void = 0;
  virtual auto setWindowTitle(const QString &title) -> void = 0;
  virtual auto showDialog() -> void = 0;

  static auto currentDialog() -> QWidget*
  {
    return m_current_dialog ? m_current_dialog->widget() : nullptr;
  }

private:
  inline static NewDialog *m_current_dialog = nullptr;
};

} // namespace Core
