// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "filenamingparameters.hpp"

#include <QWizardPage>
#include <QSharedPointer>

namespace QmakeProjectManager {
namespace Internal {

struct PluginOptions;
class CustomWidgetWidgetsWizardPage;

namespace Ui {
class CustomWidgetPluginWizardPage;
}

class CustomWidgetPluginWizardPage : public QWizardPage {
  Q_OBJECT

public:
  explicit CustomWidgetPluginWizardPage(QWidget *parent = nullptr);
  ~CustomWidgetPluginWizardPage() override;

  auto init(const CustomWidgetWidgetsWizardPage *widgetsPage) -> void;
  auto isComplete() const -> bool override;
  auto fileNamingParameters() const -> FileNamingParameters { return m_fileNamingParameters; }
  auto setFileNamingParameters(const FileNamingParameters &fnp) -> void { m_fileNamingParameters = fnp; }

  // Fills the plugin fields, excluding widget list.
  auto basicPluginOptions() const -> QSharedPointer<PluginOptions>;

private:
  auto slotCheckCompleteness() -> void;
  inline auto collectionClassName() const -> QString;
  inline auto pluginName() const -> QString;
  auto setCollectionEnabled(bool enColl) -> void;

  Ui::CustomWidgetPluginWizardPage *m_ui;
  FileNamingParameters m_fileNamingParameters;
  int m_classCount;
  bool m_complete;
};

} // namespace Internal
} // namespace QmakeProjectManager
