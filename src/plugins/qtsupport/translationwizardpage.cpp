// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "translationwizardpage.hpp"

#include <projectexplorer/jsonwizard/jsonwizard.hpp>
#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/wizardpage.hpp>

#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPair>
#include <QVBoxLayout>

#include <algorithm>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {
namespace Internal {

class TranslationWizardPage : public WizardPage {
  Q_OBJECT public:
  TranslationWizardPage(const QString &enabledExpr);

private:
  auto initializePage() -> void override;
  auto isComplete() const -> bool override;
  auto validatePage() -> bool override;

  auto tsBaseName() const -> QString { return m_fileNameLineEdit.text(); }
  auto updateLineEdit() -> void;

  QComboBox m_languageComboBox;
  QLineEdit m_fileNameLineEdit;
  const QString m_enabledExpr;
};

TranslationWizardPageFactory::TranslationWizardPageFactory()
{
  setTypeIdsSuffix("QtTranslation");
}

auto TranslationWizardPageFactory::create(JsonWizard *wizard, Id typeId, const QVariant &data) -> WizardPage*
{
  Q_UNUSED(wizard)
  Q_UNUSED(typeId)
  return new TranslationWizardPage(data.toMap().value("enabled").toString());
}

TranslationWizardPage::TranslationWizardPage(const QString &enabledExpr) : m_enabledExpr(enabledExpr)
{
  const auto mainLayout = new QVBoxLayout(this);
  const auto descriptionLabel = new QLabel(tr("If you plan to provide translations for your project's " "user interface via the Qt Linguist tool, please select a language here. " "A corresponding translation (.ts) file will be generated for you."));
  descriptionLabel->setWordWrap(true);
  mainLayout->addWidget(descriptionLabel);
  const auto formLayout = new QFormLayout;
  mainLayout->addLayout(formLayout);
  m_languageComboBox.addItem(tr("<none>"));
  auto allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);
  allLocales.removeOne(QLocale::C);
  using LocalePair = QPair<QString, QString>;
  auto localeStrings = transform<QList<LocalePair>>(allLocales, [](const QLocale &l) {
    const auto displayName = QLocale::languageToString(l.language()).append(" (").append(QLocale::countryToString(l.country())).append(')');
    const auto tsFileBaseName = l.name();
    return qMakePair(displayName, tsFileBaseName);
  });
  sort(localeStrings, [](const LocalePair &l1, const LocalePair &l2) {
    return l1.first < l2.first;
  });
  localeStrings.erase(std::unique(localeStrings.begin(), localeStrings.end()), localeStrings.end());
  for (const auto &lp : qAsConst(localeStrings))
    m_languageComboBox.addItem(lp.first, lp.second);
  formLayout->addRow(tr("Language:"), &m_languageComboBox);
  const auto fileNameLayout = new QHBoxLayout;
  m_fileNameLineEdit.setReadOnly(true);
  fileNameLayout->addWidget(&m_fileNameLineEdit);
  fileNameLayout->addStretch(1);
  formLayout->addRow(tr("Translation file:"), fileNameLayout);
  connect(&m_languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TranslationWizardPage::updateLineEdit);
}

auto TranslationWizardPage::initializePage() -> void
{
  const auto isEnabled = m_enabledExpr.isEmpty() || static_cast<JsonWizard*>(wizard())->expander()->expand(m_enabledExpr) == "yes";
  setEnabled(isEnabled);
  if (!isEnabled)
    m_languageComboBox.setCurrentIndex(0);
  updateLineEdit();
}

auto TranslationWizardPage::isComplete() const -> bool
{
  return m_languageComboBox.currentIndex() == 0 || !tsBaseName().isEmpty();
}

auto TranslationWizardPage::validatePage() -> bool
{
  const auto w = static_cast<JsonWizard*>(wizard());
  w->setValue("TsFileName", tsBaseName().isEmpty() ? QString() : QString(tsBaseName() + ".ts"));
  w->setValue("TsLanguage", m_languageComboBox.currentData().toString());
  return true;
}

auto TranslationWizardPage::updateLineEdit() -> void
{
  m_fileNameLineEdit.setEnabled(m_languageComboBox.currentIndex() != 0);
  if (m_fileNameLineEdit.isEnabled()) {
    const auto projectName = static_cast<JsonWizard*>(wizard())->stringValue("ProjectName");
    m_fileNameLineEdit.setText(projectName + '_' + m_languageComboBox.currentData().toString());
  } else {
    m_fileNameLineEdit.clear();
    m_fileNameLineEdit.setPlaceholderText(tr("<none>"));
  }
  emit completeChanged();
}

} // namespace Internal
} // namespace QtSupport

#include <translationwizardpage.moc>
