// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "pluginoptions.hpp"
#include "filenamingparameters.hpp"

#include <QList>
#include <QWizardPage>

QT_BEGIN_NAMESPACE
class QStackedLayout;
QT_END_NAMESPACE

namespace QmakeProjectManager {
namespace Internal {

class ClassDefinition;
struct PluginOptions;

namespace Ui {
class CustomWidgetWidgetsWizardPage;
}

class CustomWidgetWidgetsWizardPage : public QWizardPage {
  Q_OBJECT

public:
  explicit CustomWidgetWidgetsWizardPage(QWidget *parent = nullptr);
  ~CustomWidgetWidgetsWizardPage() override;

  auto widgetOptions() const -> QList<PluginOptions::WidgetOptions>;
  auto isComplete() const -> bool override;
  auto fileNamingParameters() const -> FileNamingParameters { return m_fileNamingParameters; }
  auto setFileNamingParameters(const FileNamingParameters &fnp) -> void { m_fileNamingParameters = fnp; }
  auto classCount() const -> int { return m_uiClassDefs.size(); }
  auto classNameAt(int i) const -> QString;
  auto initializePage() -> void override;

private Q_SLOTS:
  auto slotClassAdded(const QString &name) -> void;
  auto slotClassDeleted(int index) -> void;
  auto slotClassRenamed(int index, const QString &newName) -> void;
  auto slotCheckCompleteness() -> void;
  auto slotCurrentRowChanged(int) -> void;

private:
  auto updatePluginTab() -> void;

  Ui::CustomWidgetWidgetsWizardPage *m_ui;
  QList<ClassDefinition*> m_uiClassDefs;
  QStackedLayout *m_tabStackLayout;
  FileNamingParameters m_fileNamingParameters;
  bool m_complete;
};

} // namespace Internal
} // namespace QmakeProjectManager
