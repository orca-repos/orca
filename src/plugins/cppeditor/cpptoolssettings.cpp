// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpptoolssettings.hpp"

#include "cppeditorconstants.hpp"
#include "cppcodestylepreferences.hpp"
#include "cppcodestylepreferencesfactory.hpp"

#include <core/core-interface.hpp>
#include <texteditor/commentssettings.hpp>
#include <texteditor/completionsettingspage.hpp>
#include <texteditor/codestylepool.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <extensionsystem/pluginmanager.hpp>
#include <utils/qtcassert.hpp>
#include <utils/settingsutils.hpp>

#include <QSettings>

static constexpr char idKey[] = "CppGlobal";
constexpr bool kSortEditorDocumentOutlineDefault = true;
constexpr bool kShowHeaderErrorInfoBarDefault = true;
constexpr bool kShowNoProjectInfoBarDefault = true;

using namespace Orca::Plugin::Core;
using namespace TextEditor;

namespace CppEditor {
namespace Internal {

class CppToolsSettingsPrivate {
public:
  CommentsSettings m_commentsSettings;
  CppCodeStylePreferences *m_globalCodeStyle = nullptr;
};

} // namespace Internal

CppToolsSettings *CppToolsSettings::m_instance = nullptr;

CppToolsSettings::CppToolsSettings() : d(new Internal::CppToolsSettingsPrivate)
{
  QTC_ASSERT(!m_instance, return);
  m_instance = this;

  qRegisterMetaType<CppCodeStyleSettings>("CppEditor::CppCodeStyleSettings");

  d->m_commentsSettings = TextEditorSettings::commentsSettings();
  connect(TextEditorSettings::instance(), &TextEditorSettings::commentsSettingsChanged, this, &CppToolsSettings::setCommentsSettings);

  // code style factory
  ICodeStylePreferencesFactory *factory = new CppCodeStylePreferencesFactory();
  TextEditorSettings::registerCodeStyleFactory(factory);

  // code style pool
  auto pool = new CodeStylePool(factory, this);
  TextEditorSettings::registerCodeStylePool(Constants::CPP_SETTINGS_ID, pool);

  // global code style settings
  d->m_globalCodeStyle = new CppCodeStylePreferences(this);
  d->m_globalCodeStyle->setDelegatingPool(pool);
  d->m_globalCodeStyle->setDisplayName(tr("Global", "Settings"));
  d->m_globalCodeStyle->setId(idKey);
  pool->addCodeStyle(d->m_globalCodeStyle);
  TextEditorSettings::registerCodeStyle(Constants::CPP_SETTINGS_ID, d->m_globalCodeStyle);

  /*
  For every language we have exactly 1 pool. The pool contains:
  1) All built-in code styles (Qt/GNU)
  2) All custom code styles (which will be added dynamically)
  3) A global code style

  If the code style gets a pool (setCodeStylePool()) it means it can behave
  like a proxy to one of the code styles from that pool
  (ICodeStylePreferences::setCurrentDelegate()).
  That's why the global code style gets a pool (it can point to any code style
  from the pool), while built-in and custom code styles don't get a pool
  (they can't point to any other code style).

  The instance of the language pool is shared. The same instance of the pool
  is used for all project code style settings and for global one.
  Project code style can point to one of built-in or custom code styles
  or to the global one as well. That's why the global code style is added
  to the pool. The proxy chain can look like:
  ProjectCodeStyle -> GlobalCodeStyle -> BuildInCodeStyle (e.g. Qt).

  With the global pool there is an exception - it gets a pool
  in which it exists itself. The case in which a code style point to itself
  is disallowed and is handled in ICodeStylePreferences::setCurrentDelegate().
  */

  // built-in settings
  // Qt style
  auto qtCodeStyle = new CppCodeStylePreferences;
  qtCodeStyle->setId("qt");
  qtCodeStyle->setDisplayName(tr("Qt"));
  qtCodeStyle->setReadOnly(true);
  TabSettings qtTabSettings;
  qtTabSettings.m_tabPolicy = TabSettings::SpacesOnlyTabPolicy;
  qtTabSettings.m_tabSize = 4;
  qtTabSettings.m_indentSize = 4;
  qtTabSettings.m_continuationAlignBehavior = TabSettings::ContinuationAlignWithIndent;
  qtCodeStyle->setTabSettings(qtTabSettings);
  pool->addCodeStyle(qtCodeStyle);

  // GNU style
  auto gnuCodeStyle = new CppCodeStylePreferences;
  gnuCodeStyle->setId("gnu");
  gnuCodeStyle->setDisplayName(tr("GNU"));
  gnuCodeStyle->setReadOnly(true);
  TabSettings gnuTabSettings;
  gnuTabSettings.m_tabPolicy = TabSettings::MixedTabPolicy;
  gnuTabSettings.m_tabSize = 8;
  gnuTabSettings.m_indentSize = 2;
  gnuTabSettings.m_continuationAlignBehavior = TabSettings::ContinuationAlignWithIndent;
  gnuCodeStyle->setTabSettings(gnuTabSettings);
  CppCodeStyleSettings gnuCodeStyleSettings;
  gnuCodeStyleSettings.indentNamespaceBody = true;
  gnuCodeStyleSettings.indentBlockBraces = true;
  gnuCodeStyleSettings.indentSwitchLabels = true;
  gnuCodeStyleSettings.indentBlocksRelativeToSwitchLabels = true;
  gnuCodeStyle->setCodeStyleSettings(gnuCodeStyleSettings);
  pool->addCodeStyle(gnuCodeStyle);

  // default delegate for global preferences
  d->m_globalCodeStyle->setCurrentDelegate(qtCodeStyle);

  pool->loadCustomCodeStyles();

  QSettings *s = ICore::settings();
  // load global settings (after built-in settings are added to the pool)
  d->m_globalCodeStyle->fromSettings(QLatin1String(Constants::CPP_SETTINGS_ID), s);

  // legacy handling start (Qt Creator Version < 2.4)
  const auto legacyTransformed = s->value(QLatin1String("CppCodeStyleSettings/LegacyTransformed"), false).toBool();

  if (!legacyTransformed) {
    // creator 2.4 didn't mark yet the transformation (first run of creator 2.4)

    // we need to transform the settings only if at least one from
    // below settings was already written - otherwise we use
    // defaults like it would be the first run of creator 2.4 without stored settings
    const auto groups = s->childGroups();
    const auto needTransform = groups.contains(QLatin1String("textTabPreferences")) || groups.contains(QLatin1String("CppTabPreferences")) || groups.contains(QLatin1String("CppCodeStyleSettings"));
    if (needTransform) {
      CppCodeStyleSettings legacyCodeStyleSettings;
      if (groups.contains(QLatin1String("CppCodeStyleSettings"))) {
        Utils::fromSettings(QLatin1String("CppCodeStyleSettings"), QString(), s, &legacyCodeStyleSettings);
      }

      const auto currentFallback = s->value(QLatin1String("CppTabPreferences/CurrentFallback")).toString();
      TabSettings legacyTabSettings;
      if (currentFallback == QLatin1String("CppGlobal")) {
        // no delegate, global overwritten
        Utils::fromSettings(QLatin1String("CppTabPreferences"), QString(), s, &legacyTabSettings);
      } else {
        // delegating to global
        legacyTabSettings = TextEditorSettings::codeStyle()->currentTabSettings();
      }

      // create custom code style out of old settings
      QVariant v;
      v.setValue(legacyCodeStyleSettings);
      auto oldCreator = pool->createCodeStyle("legacy", legacyTabSettings, v, tr("Old Creator"));

      // change the current delegate and save
      d->m_globalCodeStyle->setCurrentDelegate(oldCreator);
      d->m_globalCodeStyle->toSettings(QLatin1String(Constants::CPP_SETTINGS_ID), s);
    }
    // mark old settings as transformed
    s->setValue(QLatin1String("CppCodeStyleSettings/LegacyTransformed"), true);
    // legacy handling stop
  }

  // mimetypes to be handled
  TextEditorSettings::registerMimeTypeForLanguageId(Constants::C_SOURCE_MIMETYPE, Constants::CPP_SETTINGS_ID);
  TextEditorSettings::registerMimeTypeForLanguageId(Constants::C_HEADER_MIMETYPE, Constants::CPP_SETTINGS_ID);
  TextEditorSettings::registerMimeTypeForLanguageId(Constants::CPP_SOURCE_MIMETYPE, Constants::CPP_SETTINGS_ID);
  TextEditorSettings::registerMimeTypeForLanguageId(Constants::CPP_HEADER_MIMETYPE, Constants::CPP_SETTINGS_ID);
}

CppToolsSettings::~CppToolsSettings()
{
  TextEditorSettings::unregisterCodeStyle(Constants::CPP_SETTINGS_ID);
  TextEditorSettings::unregisterCodeStylePool(Constants::CPP_SETTINGS_ID);
  TextEditorSettings::unregisterCodeStyleFactory(Constants::CPP_SETTINGS_ID);

  delete d;

  m_instance = nullptr;
}

auto CppToolsSettings::instance() -> CppToolsSettings*
{
  return m_instance;
}

auto CppToolsSettings::cppCodeStyle() const -> CppCodeStylePreferences*
{
  return d->m_globalCodeStyle;
}

auto CppToolsSettings::commentsSettings() const -> const CommentsSettings&
{
  return d->m_commentsSettings;
}

auto CppToolsSettings::setCommentsSettings(const CommentsSettings &commentsSettings) -> void
{
  d->m_commentsSettings = commentsSettings;
}

static auto sortEditorDocumentOutlineKey() -> QString
{
  return QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP) + QLatin1Char('/') + QLatin1String(Constants::CPPEDITOR_SORT_EDITOR_DOCUMENT_OUTLINE);
}

auto CppToolsSettings::sortedEditorDocumentOutline() const -> bool
{
  return ICore::settings()->value(sortEditorDocumentOutlineKey(), kSortEditorDocumentOutlineDefault).toBool();
}

auto CppToolsSettings::setSortedEditorDocumentOutline(bool sorted) -> void
{
  ICore::settings()->setValueWithDefault(sortEditorDocumentOutlineKey(), sorted, kSortEditorDocumentOutlineDefault);
  emit editorDocumentOutlineSortingChanged(sorted);
}

static auto showHeaderErrorInfoBarKey() -> QString
{
  return QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP) + QLatin1Char('/') + QLatin1String(Constants::CPPEDITOR_SHOW_INFO_BAR_FOR_HEADER_ERRORS);
}

auto CppToolsSettings::showHeaderErrorInfoBar() const -> bool
{
  return ICore::settings()->value(showHeaderErrorInfoBarKey(), kShowHeaderErrorInfoBarDefault).toBool();
}

auto CppToolsSettings::setShowHeaderErrorInfoBar(bool show) -> void
{
  ICore::settings()->setValueWithDefault(showHeaderErrorInfoBarKey(), show, kShowHeaderErrorInfoBarDefault);
  emit showHeaderErrorInfoBarChanged(show);
}

static auto showNoProjectInfoBarKey() -> QString
{
  return QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP) + QLatin1Char('/') + QLatin1String(Constants::CPPEDITOR_SHOW_INFO_BAR_FOR_FOR_NO_PROJECT);
}

auto CppToolsSettings::showNoProjectInfoBar() const -> bool
{
  return ICore::settings()->value(showNoProjectInfoBarKey(), kShowNoProjectInfoBarDefault).toBool();
}

auto CppToolsSettings::setShowNoProjectInfoBar(bool show) -> void
{
  ICore::settings()->setValueWithDefault(showNoProjectInfoBarKey(), show, kShowNoProjectInfoBarDefault);
  emit showNoProjectInfoBarChanged(show);
}

} // namespace CppEditor
