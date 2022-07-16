// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codegensettings.hpp"

#include <core/core-interface.hpp>
#include <utils/qtcsettings.hpp>

#include <QSettings>

static constexpr QtSupport::CodeGenSettings::UiClassEmbedding embeddingDefault = QtSupport::CodeGenSettings::PointerAggregatedUiClass;

static constexpr char CODE_GEN_GROUP[] = "FormClassWizardPage";
static constexpr char TRANSLATION_KEY[] = "RetranslationSupport";
static constexpr char EMBEDDING_KEY[] = "Embedding";
static constexpr char INCLUDE_QT_MODULE_KEY[] = "IncludeQtModule";
static constexpr char ADD_QT_VERSION_CHECK_KEY[] = "AddQtVersionCheck";
static constexpr bool retranslationSupportDefault = false;
static constexpr bool includeQtModuleDefault = false;
static constexpr bool addQtVersionCheckDefault = false;

using namespace Utils;

namespace QtSupport {

CodeGenSettings::CodeGenSettings() : embedding(embeddingDefault), retranslationSupport(retranslationSupportDefault), includeQtModule(includeQtModuleDefault), addQtVersionCheck(addQtVersionCheckDefault) {}

auto CodeGenSettings::equals(const CodeGenSettings &rhs) const -> bool
{
  return embedding == rhs.embedding && retranslationSupport == rhs.retranslationSupport && includeQtModule == rhs.includeQtModule && addQtVersionCheck == rhs.addQtVersionCheck;
}

auto CodeGenSettings::fromSettings(const QSettings *settings) -> void
{
  const QString group = QLatin1String(CODE_GEN_GROUP) + '/';

  retranslationSupport = settings->value(group + TRANSLATION_KEY, retranslationSupportDefault).toBool();
  embedding = static_cast<UiClassEmbedding>(settings->value(group + EMBEDDING_KEY, int(embeddingDefault)).toInt());
  includeQtModule = settings->value(group + INCLUDE_QT_MODULE_KEY, includeQtModuleDefault).toBool();
  addQtVersionCheck = settings->value(group + ADD_QT_VERSION_CHECK_KEY, addQtVersionCheckDefault).toBool();
}

auto CodeGenSettings::toSettings(QSettings *settings) const -> void
{
  settings->beginGroup(CODE_GEN_GROUP);
  QtcSettings::setValueWithDefault(settings, TRANSLATION_KEY, retranslationSupport, retranslationSupportDefault);
  QtcSettings::setValueWithDefault(settings, EMBEDDING_KEY, int(embedding), int(embeddingDefault));
  QtcSettings::setValueWithDefault(settings, INCLUDE_QT_MODULE_KEY, includeQtModule, includeQtModuleDefault);
  QtcSettings::setValueWithDefault(settings, ADD_QT_VERSION_CHECK_KEY, addQtVersionCheck, addQtVersionCheckDefault);
  settings->endGroup();
}

} // namespace QtSupport
