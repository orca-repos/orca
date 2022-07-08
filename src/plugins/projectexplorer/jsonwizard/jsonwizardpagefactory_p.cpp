// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardpagefactory_p.hpp"

#include "jsonfieldpage.hpp"
#include "jsonfieldpage_p.hpp"
#include "jsonfilepage.hpp"
#include "jsonkitspage.hpp"
#include "jsonprojectpage.hpp"
#include "jsonsummarypage.hpp"
#include "jsonwizardfactory.hpp"

#include <utils/qtcassert.hpp>
#include <utils/wizardpage.hpp>

#include <QCoreApplication>
#include <QVariant>

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------
// FieldPageFactory:
// --------------------------------------------------------------------

FieldPageFactory::FieldPageFactory()
{
  setTypeIdsSuffix(QLatin1String("Fields"));

  JsonFieldPage::registerFieldFactory(QLatin1String("Label"), []() { return new LabelField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("Spacer"), []() { return new SpacerField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("LineEdit"), []() { return new LineEditField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("TextEdit"), []() { return new TextEditField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("PathChooser"), []() { return new PathChooserField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("CheckBox"), []() { return new CheckBoxField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("ComboBox"), []() { return new ComboBoxField; });
  JsonFieldPage::registerFieldFactory(QLatin1String("IconList"), []() { return new IconListField; });
}

auto FieldPageFactory::create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage*
{
  Q_UNUSED(wizard)

  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto page = new JsonFieldPage(wizard->expander());

  if (!page->setup(data)) {
    delete page;
    return nullptr;
  }

  return page;
}

auto FieldPageFactory::validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);

  auto list = JsonWizardFactory::objectOrList(data, errorMessage);
  if (list.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "When parsing fields of page \"%1\": %2").arg(typeId.toString()).arg(*errorMessage);
    return false;
  }

  foreach(const QVariant &v, list) {
    const auto field = JsonFieldPage::Field::parse(v, errorMessage);
    if (!field)
      return false;
    delete field;
  }

  return true;
}

// --------------------------------------------------------------------
// FilePageFactory:
// --------------------------------------------------------------------

FilePageFactory::FilePageFactory()
{
  setTypeIdsSuffix(QLatin1String("File"));
}

auto FilePageFactory::create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage*
{
  Q_UNUSED(wizard)
  Q_UNUSED(data)
  QTC_ASSERT(canCreate(typeId), return nullptr);

  return new JsonFilePage;
}

auto FilePageFactory::validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);
  if (!data.isNull() && (data.type() != QVariant::Map || !data.toMap().isEmpty())) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"data\" for a \"File\" page needs to be unset or an empty object.");
    return false;
  }

  return true;
}

// --------------------------------------------------------------------
// KitsPageFactory:
// --------------------------------------------------------------------

static const char KEY_PROJECT_FILE[] = "projectFilePath";
static const char KEY_REQUIRED_FEATURES[] = "requiredFeatures";
static const char KEY_PREFERRED_FEATURES[] = "preferredFeatures";

KitsPageFactory::KitsPageFactory()
{
  setTypeIdsSuffix(QLatin1String("Kits"));
}

auto KitsPageFactory::create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage*
{
  Q_UNUSED(wizard)
  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto page = new JsonKitsPage;
  const auto dataMap = data.toMap();
  page->setUnexpandedProjectPath(dataMap.value(QLatin1String(KEY_PROJECT_FILE)).toString());
  page->setRequiredFeatures(dataMap.value(QLatin1String(KEY_REQUIRED_FEATURES)));
  page->setPreferredFeatures(dataMap.value(QLatin1String(KEY_PREFERRED_FEATURES)));

  return page;
}

static auto validateFeatureList(const QVariantMap &data, const QByteArray &key, QString *errorMessage) -> bool
{
  QString message;
  JsonKitsPage::parseFeatures(data.value(QLatin1String(key)), &message);
  if (!message.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "Error parsing \"%1\" in \"Kits\" page: %2").arg(QLatin1String(key), message);
    return false;
  }
  return true;
}

auto KitsPageFactory::validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);

  if (data.isNull() || data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"data\" must be a JSON object for \"Kits\" pages.");
    return false;
  }

  const auto tmp = data.toMap();
  if (tmp.value(QLatin1String(KEY_PROJECT_FILE)).toString().isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"Kits\" page requires a \"%1\" set.").arg(QLatin1String(KEY_PROJECT_FILE));
    return false;
  }

  return validateFeatureList(tmp, KEY_REQUIRED_FEATURES, errorMessage) && validateFeatureList(tmp, KEY_PREFERRED_FEATURES, errorMessage);
}

// --------------------------------------------------------------------
// ProjectPageFactory:
// --------------------------------------------------------------------

static constexpr char KEY_PROJECT_NAME_VALIDATOR[] = "projectNameValidator";
static constexpr char KEY_PROJECT_NAME_VALIDATOR_USER_MESSAGE[] = "trProjectNameValidatorUserMessage";

ProjectPageFactory::ProjectPageFactory()
{
  setTypeIdsSuffix(QLatin1String("Project"));
}

auto ProjectPageFactory::create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage*
{
  Q_UNUSED(wizard)
  Q_UNUSED(data)
  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto page = new JsonProjectPage;

  const auto tmp = data.isNull() ? QVariantMap() : data.toMap();
  const auto description = tmp.value(QLatin1String("trDescription"), QLatin1String("%{trDescription}")).toString();
  page->setDescription(wizard->expander()->expand(description));
  const auto projectNameValidator = tmp.value(QLatin1String(KEY_PROJECT_NAME_VALIDATOR)).toString();
  const auto projectNameValidatorUserMessage = JsonWizardFactory::localizedString(tmp.value(QLatin1String(KEY_PROJECT_NAME_VALIDATOR_USER_MESSAGE)));

  if (!projectNameValidator.isEmpty()) {
    const QRegularExpression regularExpression(projectNameValidator);
    if (regularExpression.isValid())
      page->setProjectNameRegularExpression(regularExpression, projectNameValidatorUserMessage);
  }

  return page;
}

auto ProjectPageFactory::validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  Q_UNUSED(errorMessage)

  QTC_ASSERT(canCreate(typeId), return false);
  if (!data.isNull() && data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"data\" must be empty or a JSON object for \"Project\" pages.");
    return false;
  }
  const auto tmp = data.toMap();
  auto projectNameValidator = tmp.value(QLatin1String(KEY_PROJECT_NAME_VALIDATOR)).toString();
  if (!projectNameValidator.isNull()) {
    const QRegularExpression regularExpression(projectNameValidator);
    if (!regularExpression.isValid()) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "Invalid regular expression \"%1\" in \"%2\". %3").arg(projectNameValidator, QLatin1String(KEY_PROJECT_NAME_VALIDATOR), regularExpression.errorString());
      return false;
    }
  }

  return true;
}

// --------------------------------------------------------------------
// SummaryPageFactory:
// --------------------------------------------------------------------

static constexpr char KEY_HIDE_PROJECT_UI[] = "hideProjectUi";

SummaryPageFactory::SummaryPageFactory()
{
  setTypeIdsSuffix(QLatin1String("Summary"));
}

auto SummaryPageFactory::create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage*
{
  Q_UNUSED(wizard)
  Q_UNUSED(data)
  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto page = new JsonSummaryPage;
  const auto hideProjectUi = data.toMap().value(QLatin1String(KEY_HIDE_PROJECT_UI));
  page->setHideProjectUiValue(hideProjectUi);
  return page;
}

auto SummaryPageFactory::validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);
  if (!data.isNull() && (data.type() != QVariant::Map)) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "\"data\" for a \"Summary\" page can be unset or needs to be an object.");
    return false;
  }

  return true;
}

} // namespace Internal
} // namespace ProjectExplorer
