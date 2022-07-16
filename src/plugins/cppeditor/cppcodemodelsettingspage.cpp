// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodemodelsettingspage.hpp"
#include "ui_cppcodemodelsettingspage.h"

#include "clangdiagnosticconfigswidget.hpp"
#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"

#include <core/core-interface.hpp>
#include <projectexplorer/session.hpp>
#include <utils/algorithm.hpp>
#include <utils/infolabel.hpp>
#include <utils/itemviews.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStringListModel>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVersionNumber>

namespace CppEditor::Internal {

class CppCodeModelSettingsWidget final : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(CppEditor::Internal::CppCodeModelSettingsWidget)

public:
  CppCodeModelSettingsWidget(CppCodeModelSettings *s);
  ~CppCodeModelSettingsWidget() override;

private:
  auto apply() -> void final;
  auto setupGeneralWidgets() -> void;
  auto setupClangCodeModelWidgets() -> void;
  auto applyGeneralWidgetsToSettings() const -> bool;
  auto applyClangCodeModelWidgetsToSettings() const -> bool;

  Ui::CppCodeModelSettingsPage *m_ui = nullptr;
  CppCodeModelSettings *m_settings = nullptr;
};

CppCodeModelSettingsWidget::CppCodeModelSettingsWidget(CppCodeModelSettings *s) : m_ui(new Ui::CppCodeModelSettingsPage)
{
  m_ui->setupUi(this);
  m_settings = s;
  setupGeneralWidgets();
  setupClangCodeModelWidgets();
}

CppCodeModelSettingsWidget::~CppCodeModelSettingsWidget()
{
  delete m_ui;
}

auto CppCodeModelSettingsWidget::apply() -> void
{
  auto changed = false;

  changed |= applyGeneralWidgetsToSettings();
  changed |= applyClangCodeModelWidgetsToSettings();

  if (changed)
    m_settings->toSettings(Orca::Plugin::Core::ICore::settings());
}

auto CppCodeModelSettingsWidget::setupClangCodeModelWidgets() -> void
{
  m_ui->clangDiagnosticConfigsSelectionWidget->refresh(diagnosticConfigsModel(), m_settings->clangDiagnosticConfigId(), [](const ClangDiagnosticConfigs &configs, const Utils::Id &configToSelect) {
    return new ClangDiagnosticConfigsWidget(configs, configToSelect);
  });

  const auto isClangActive = CppModelManager::instance()->isClangCodeModelActive();
  m_ui->clangCodeModelIsDisabledHint->setVisible(!isClangActive);
  m_ui->clangCodeModelIsEnabledHint->setVisible(isClangActive);

  for (auto i = 0; i < m_ui->clangDiagnosticConfigsSelectionWidget->layout()->count(); ++i) {
    QWidget *widget = m_ui->clangDiagnosticConfigsSelectionWidget->layout()->itemAt(i)->widget();
    if (widget)
      widget->setEnabled(isClangActive);
  }
}

auto CppCodeModelSettingsWidget::setupGeneralWidgets() -> void
{
  m_ui->interpretAmbiguousHeadersAsCHeaders->setChecked(m_settings->interpretAmbigiousHeadersAsCHeaders());

  m_ui->skipIndexingBigFilesCheckBox->setChecked(m_settings->skipIndexingBigFiles());
  m_ui->bigFilesLimitSpinBox->setValue(m_settings->indexerFileSizeLimitInMb());

  const auto ignorePch = m_settings->pchUsage() == CppCodeModelSettings::PchUse_None;
  m_ui->ignorePCHCheckBox->setChecked(ignorePch);
}

auto CppCodeModelSettingsWidget::applyClangCodeModelWidgetsToSettings() const -> bool
{
  auto changed = false;

  const auto oldConfigId = m_settings->clangDiagnosticConfigId();
  const Utils::Id currentConfigId = m_ui->clangDiagnosticConfigsSelectionWidget->currentConfigId();
  if (oldConfigId != currentConfigId) {
    m_settings->setClangDiagnosticConfigId(currentConfigId);
    changed = true;
  }

  const auto oldConfigs = m_settings->clangCustomDiagnosticConfigs();
  const ClangDiagnosticConfigs currentConfigs = m_ui->clangDiagnosticConfigsSelectionWidget->customConfigs();
  if (oldConfigs != currentConfigs) {
    m_settings->setClangCustomDiagnosticConfigs(currentConfigs);
    changed = true;
  }

  return changed;
}

auto CppCodeModelSettingsWidget::applyGeneralWidgetsToSettings() const -> bool
{
  auto settingsChanged = false;

  const bool newInterpretAmbiguousHeaderAsCHeaders = m_ui->interpretAmbiguousHeadersAsCHeaders->isChecked();
  if (m_settings->interpretAmbigiousHeadersAsCHeaders() != newInterpretAmbiguousHeaderAsCHeaders) {
    m_settings->setInterpretAmbigiousHeadersAsCHeaders(newInterpretAmbiguousHeaderAsCHeaders);
    settingsChanged = true;
  }

  const bool newSkipIndexingBigFiles = m_ui->skipIndexingBigFilesCheckBox->isChecked();
  if (m_settings->skipIndexingBigFiles() != newSkipIndexingBigFiles) {
    m_settings->setSkipIndexingBigFiles(newSkipIndexingBigFiles);
    settingsChanged = true;
  }
  const int newFileSizeLimit = m_ui->bigFilesLimitSpinBox->value();
  if (m_settings->indexerFileSizeLimitInMb() != newFileSizeLimit) {
    m_settings->setIndexerFileSizeLimitInMb(newFileSizeLimit);
    settingsChanged = true;
  }

  const bool newIgnorePch = m_ui->ignorePCHCheckBox->isChecked();
  const auto previousIgnorePch = m_settings->pchUsage() == CppCodeModelSettings::PchUse_None;
  if (newIgnorePch != previousIgnorePch) {
    const CppCodeModelSettings::PCHUsage pchUsage = m_ui->ignorePCHCheckBox->isChecked() ? CppCodeModelSettings::PchUse_None : CppCodeModelSettings::PchUse_BuildSystem;
    m_settings->setPCHUsage(pchUsage);
    settingsChanged = true;
  }

  return settingsChanged;
}

CppCodeModelSettingsPage::CppCodeModelSettingsPage(CppCodeModelSettings *settings)
{
  setId(Constants::CPP_CODE_MODEL_SETTINGS_ID);
  setDisplayName(CppCodeModelSettingsWidget::tr("Code Model"));
  setCategory(Constants::CPP_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("CppEditor", "C++"));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_cpp.png");
  setWidgetCreator([settings] { return new CppCodeModelSettingsWidget(settings); });
}

class ClangdSettingsWidget::Private {
public:
  QCheckBox useClangdCheckBox;
  QCheckBox indexingCheckBox;
  QCheckBox autoIncludeHeadersCheckBox;
  QSpinBox threadLimitSpinBox;
  QSpinBox documentUpdateThreshold;
  Utils::PathChooser clangdChooser;
  Utils::InfoLabel versionWarningLabel;
  QGroupBox *sessionsGroupBox = nullptr;
  QStringListModel sessionsModel;
};

ClangdSettingsWidget::ClangdSettingsWidget(const ClangdSettings::Data &settingsData, bool isForProject) : d(new Private)
{
  const ClangdSettings settings(settingsData);
  d->useClangdCheckBox.setText(tr("Use clangd"));
  d->useClangdCheckBox.setChecked(settings.useClangd());
  d->clangdChooser.setExpectedKind(Utils::PathChooser::ExistingCommand);
  d->clangdChooser.setFilePath(settings.clangdFilePath());
  d->clangdChooser.setEnabled(d->useClangdCheckBox.isChecked());
  d->indexingCheckBox.setChecked(settings.indexingEnabled());
  d->indexingCheckBox.setToolTip(tr("If background indexing is enabled, global symbol searches will yield\n" "more accurate results, at the cost of additional CPU load when\n" "the project is first opened."));
  d->autoIncludeHeadersCheckBox.setChecked(settings.autoIncludeHeaders());
  d->autoIncludeHeadersCheckBox.setToolTip(tr("Controls whether clangd may insert header files as part of symbol completion."));
  d->threadLimitSpinBox.setValue(settings.workerThreadLimit());
  d->threadLimitSpinBox.setSpecialValueText("Automatic");
  d->documentUpdateThreshold.setMinimum(50);
  d->documentUpdateThreshold.setMaximum(10000);
  d->documentUpdateThreshold.setValue(settings.documentUpdateThreshold());
  d->documentUpdateThreshold.setSingleStep(100);
  d->documentUpdateThreshold.setSuffix(" ms");
  d->documentUpdateThreshold.setToolTip(tr("Defines the amount of time Qt Creator waits before sending document changes to the " "server.\n" "If the document changes again while waiting, this timeout resets.\n"));

  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(&d->useClangdCheckBox);
  const auto formLayout = new QFormLayout;
  const auto chooserLabel = new QLabel(tr("Path to executable:"));
  formLayout->addRow(chooserLabel, &d->clangdChooser);
  formLayout->addRow(QString(), &d->versionWarningLabel);
  const auto indexingLabel = new QLabel(tr("Enable background indexing:"));
  formLayout->addRow(indexingLabel, &d->indexingCheckBox);
  const auto autoIncludeHeadersLabel = new QLabel(tr("Insert header files on completion:"));
  formLayout->addRow(autoIncludeHeadersLabel, &d->autoIncludeHeadersCheckBox);
  const auto threadLimitLayout = new QHBoxLayout;
  threadLimitLayout->addWidget(&d->threadLimitSpinBox);
  threadLimitLayout->addStretch(1);
  const auto threadLimitLabel = new QLabel(tr("Worker thread count:"));
  formLayout->addRow(threadLimitLabel, threadLimitLayout);
  const auto documentUpdateThresholdLayout = new QHBoxLayout;
  documentUpdateThresholdLayout->addWidget(&d->documentUpdateThreshold);
  documentUpdateThresholdLayout->addStretch(1);
  const auto documentUpdateThresholdLabel = new QLabel(tr("Document update threshold:"));
  formLayout->addRow(documentUpdateThresholdLabel, documentUpdateThresholdLayout);
  layout->addLayout(formLayout);

  if (!isForProject) {
    d->sessionsModel.setStringList(settingsData.sessionsWithOneClangd);
    d->sessionsModel.sort(0);
    d->sessionsGroupBox = new QGroupBox(tr("Sessions with a single clangd instance"));
    const auto sessionsView = new Utils::ListView;
    sessionsView->setModel(&d->sessionsModel);
    sessionsView->setToolTip(tr("By default, Qt Creator runs one clangd process per project.\n" "If you have sessions with tightly coupled projects that should be\n" "managed by the same clangd process, add them here."));
    const auto outerSessionsLayout = new QHBoxLayout;
    const auto innerSessionsLayout = new QHBoxLayout(d->sessionsGroupBox);
    const auto buttonsLayout = new QVBoxLayout;
    const auto addButton = new QPushButton(tr("Add ..."));
    const auto removeButton = new QPushButton(tr("Remove"));
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch(1);
    innerSessionsLayout->addWidget(sessionsView);
    innerSessionsLayout->addLayout(buttonsLayout);
    outerSessionsLayout->addWidget(d->sessionsGroupBox);
    outerSessionsLayout->addStretch(1);
    layout->addLayout(outerSessionsLayout);

    const auto updateRemoveButtonState = [removeButton, sessionsView] {
      removeButton->setEnabled(sessionsView->selectionModel()->hasSelection());
    };
    connect(sessionsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateRemoveButtonState);
    updateRemoveButtonState();
    connect(removeButton, &QPushButton::clicked, this, [this, sessionsView] {
      const auto selection = sessionsView->selectionModel()->selection();
      QTC_ASSERT(!selection.isEmpty(), return);
      d->sessionsModel.removeRow(selection.indexes().first().row());
    });

    connect(addButton, &QPushButton::clicked, this, [this, sessionsView] {
      QInputDialog dlg(sessionsView);
      auto sessions = ProjectExplorer::SessionManager::sessions();
      auto currentSessions = d->sessionsModel.stringList();
      for (const auto &s : qAsConst(currentSessions))
        sessions.removeOne(s);
      if (sessions.isEmpty())
        return;
      sessions.sort();
      dlg.setLabelText(tr("Choose a session:"));
      dlg.setComboBoxItems(sessions);
      if (dlg.exec() == QDialog::Accepted) {
        currentSessions << dlg.textValue();
        d->sessionsModel.setStringList(currentSessions);
        d->sessionsModel.sort(0);
      }
    });

    // TODO: Remove once the concept is functional.
    d->sessionsGroupBox->hide();
  }
  layout->addStretch(1);

  static const auto setWidgetsEnabled = [](QLayout *layout, bool enabled, const auto &f) -> void {
    for (auto i = 0; i < layout->count(); ++i) {
      if (const auto w = layout->itemAt(i)->widget())
        w->setEnabled(enabled);
      else if (const auto l = layout->itemAt(i)->layout())
        f(l, enabled, f);
    }
  };
  const auto toggleEnabled = [this, formLayout](const bool checked) {
    setWidgetsEnabled(formLayout, checked, setWidgetsEnabled);
    if (d->sessionsGroupBox)
      d->sessionsGroupBox->setEnabled(checked);
  };
  connect(&d->useClangdCheckBox, &QCheckBox::toggled, toggleEnabled);
  toggleEnabled(d->useClangdCheckBox.isChecked());
  d->threadLimitSpinBox.setEnabled(d->useClangdCheckBox.isChecked());

  d->versionWarningLabel.setType(Utils::InfoLabel::Warning);
  const auto updateWarningLabel = [this] {
    class WarningLabelSetter {
    public:
      WarningLabelSetter(QLabel &label) : m_label(label) { m_label.clear(); }
      ~WarningLabelSetter() { m_label.setVisible(!m_label.text().isEmpty()); }
      auto setWarning(const QString &text) -> void { m_label.setText(text); }
    private:
      QLabel &m_label;
    };
    WarningLabelSetter labelSetter(d->versionWarningLabel);

    if (!d->clangdChooser.isValid())
      return;
    const auto clangdPath = d->clangdChooser.filePath();
    const auto clangdVersion = ClangdSettings::clangdVersion(clangdPath);
    if (clangdVersion.isNull()) {
      labelSetter.setWarning(tr("Failed to retrieve clangd version: " "Unexpected clangd output."));
      return;
    }
    if (clangdVersion < QVersionNumber(13)) {
      labelSetter.setWarning(tr("The clangd version is %1, but %2 or greater is required.").arg(clangdVersion.toString()).arg(13));
      return;
    }
  };
  connect(&d->clangdChooser, &Utils::PathChooser::pathChanged, this, updateWarningLabel);
  updateWarningLabel();

  connect(&d->useClangdCheckBox, &QCheckBox::toggled, this, &ClangdSettingsWidget::settingsDataChanged);
  connect(&d->indexingCheckBox, &QCheckBox::toggled, this, &ClangdSettingsWidget::settingsDataChanged);
  connect(&d->autoIncludeHeadersCheckBox, &QCheckBox::toggled, this, &ClangdSettingsWidget::settingsDataChanged);
  connect(&d->threadLimitSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ClangdSettingsWidget::settingsDataChanged);
  connect(&d->documentUpdateThreshold, qOverload<int>(&QSpinBox::valueChanged), this, &ClangdSettingsWidget::settingsDataChanged);
  connect(&d->clangdChooser, &Utils::PathChooser::pathChanged, this, &ClangdSettingsWidget::settingsDataChanged);
}

ClangdSettingsWidget::~ClangdSettingsWidget()
{
  delete d;
}

auto ClangdSettingsWidget::settingsData() const -> ClangdSettings::Data
{
  ClangdSettings::Data data;
  data.useClangd = d->useClangdCheckBox.isChecked();
  data.executableFilePath = d->clangdChooser.filePath();
  data.enableIndexing = d->indexingCheckBox.isChecked();
  data.autoIncludeHeaders = d->autoIncludeHeadersCheckBox.isChecked();
  data.workerThreadLimit = d->threadLimitSpinBox.value();
  data.documentUpdateThreshold = d->documentUpdateThreshold.value();
  data.sessionsWithOneClangd = d->sessionsModel.stringList();
  return data;
}

class ClangdSettingsPageWidget final : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(CppEditor::Internal::ClangdSettingsWidget)
public:
  ClangdSettingsPageWidget() : m_widget(ClangdSettings::instance().data(), false)
  {
    const auto layout = new QVBoxLayout(this);
    layout->addWidget(&m_widget);
  }

private:
  auto apply() -> void final { ClangdSettings::instance().setData(m_widget.settingsData()); }

  ClangdSettingsWidget m_widget;
};

ClangdSettingsPage::ClangdSettingsPage()
{
  setId(Constants::CPP_CLANGD_SETTINGS_ID);
  setDisplayName(ClangdSettingsWidget::tr("Clangd"));
  setCategory(Constants::CPP_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new ClangdSettingsPageWidget; });
}

class ClangdProjectSettingsWidget::Private {
public:
  Private(const ClangdProjectSettings &s) : settings(s), widget(s.settings(), true) {}

  ClangdProjectSettings settings;
  ClangdSettingsWidget widget;
  QCheckBox useGlobalSettingsCheckBox;
};

ClangdProjectSettingsWidget::ClangdProjectSettingsWidget(const ClangdProjectSettings &settings) : d(new Private(settings))
{
  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  const auto globalSettingsLayout = new QHBoxLayout;
  globalSettingsLayout->addWidget(&d->useGlobalSettingsCheckBox);
  const auto globalSettingsLabel = new QLabel("Use <a href=\"dummy\">global settings</a>");
  connect(globalSettingsLabel, &QLabel::linkActivated, this, [] { Orca::Plugin::Core::ICore::showOptionsDialog(Constants::CPP_CLANGD_SETTINGS_ID); });
  globalSettingsLayout->addWidget(globalSettingsLabel);
  globalSettingsLayout->addStretch(1);
  layout->addLayout(globalSettingsLayout);

  const auto separator = new QFrame;
  separator->setFrameShape(QFrame::HLine);
  layout->addWidget(separator);
  layout->addWidget(&d->widget);

  const auto updateGlobalSettingsCheckBox = [this] {
    if (ClangdSettings::instance().granularity() == ClangdSettings::Granularity::Session) {
      d->useGlobalSettingsCheckBox.setEnabled(false);
      d->useGlobalSettingsCheckBox.setChecked(true);
    } else {
      d->useGlobalSettingsCheckBox.setEnabled(true);
      d->useGlobalSettingsCheckBox.setChecked(d->settings.useGlobalSettings());
    }
    d->widget.setEnabled(!d->useGlobalSettingsCheckBox.isChecked());
  };
  updateGlobalSettingsCheckBox();
  connect(&ClangdSettings::instance(), &ClangdSettings::changed, this, updateGlobalSettingsCheckBox);

  connect(&d->useGlobalSettingsCheckBox, &QCheckBox::clicked, [this](bool checked) {
    d->widget.setEnabled(!checked);
    d->settings.setUseGlobalSettings(checked);
    if (!checked)
      d->settings.setSettings(d->widget.settingsData());
  });
  connect(&d->widget, &ClangdSettingsWidget::settingsDataChanged, [this] {
    d->settings.setSettings(d->widget.settingsData());
  });
}

ClangdProjectSettingsWidget::~ClangdProjectSettingsWidget()
{
  delete d;
}

} // CppEditor::Internal
