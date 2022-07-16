// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../wizards/qtwizard.hpp"

#include <QSharedPointer>

namespace QmakeProjectManager {
namespace Internal {

class CustomWidgetWidgetsWizardPage;
class CustomWidgetPluginWizardPage;
struct PluginOptions;
struct FileNamingParameters;

class CustomWidgetWizardDialog : public BaseQmakeProjectWizardDialog {
  Q_OBJECT

public:
  explicit CustomWidgetWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, const QString &templateName, const QIcon &icon, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);

  auto pluginOptions() const -> QSharedPointer<PluginOptions>;
  auto fileNamingParameters() const -> FileNamingParameters;
  auto setFileNamingParameters(const FileNamingParameters &fnp) -> void;

private:
  auto slotCurrentIdChanged(int id) -> void;

  CustomWidgetWidgetsWizardPage *m_widgetsPage;
  CustomWidgetPluginWizardPage *m_pluginPage;
  int m_pluginPageId;
};

} // namespace Internal
} // namespace QmakeProjectManager
