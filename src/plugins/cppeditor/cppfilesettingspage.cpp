// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppfilesettingspage.hpp"

#include "cppeditorplugin.hpp"
#include <ui_cppfilesettingspage.h>

#include <app/app_version.hpp>

#include <core/core-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <cppeditor/cppeditorconstants.hpp>

#include <utils/environment.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/stringutils.hpp>

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QLocale>
#include <QSettings>
#include <QTextCodec>
#include <QTextStream>

using namespace Utils;

namespace CppEditor::Internal {

constexpr char headerPrefixesKeyC[] = "HeaderPrefixes";
constexpr char sourcePrefixesKeyC[] = "SourcePrefixes";
constexpr char headerSuffixKeyC[] = "HeaderSuffix";
constexpr char sourceSuffixKeyC[] = "SourceSuffix";
constexpr char headerSearchPathsKeyC[] = "HeaderSearchPaths";
constexpr char sourceSearchPathsKeyC[] = "SourceSearchPaths";
constexpr char headerPragmaOnceC[] = "HeaderPragmaOnce";
constexpr char licenseTemplatePathKeyC[] = "LicenseTemplate";

const char *licenseTemplateTemplate = QT_TRANSLATE_NOOP("CppEditor::Internal::CppFileSettingsWidget",
"/**************************************************************************\n"
"** %1 license header template\n"
"**   Special keywords: %USER% %DATE% %YEAR%\n"
"**   Environment variables: %$VARIABLE%\n"
"**   To protect a percent sign, use '%%'.\n"
"**************************************************************************/\n");

auto CppFileSettings::toSettings(QSettings *s) const -> void
{
  using Utils::QtcSettings;
  const CppFileSettings def;
  s->beginGroup(Constants::CPPEDITOR_SETTINGSGROUP);
  QtcSettings::setValueWithDefault(s, headerPrefixesKeyC, headerPrefixes, def.headerPrefixes);
  QtcSettings::setValueWithDefault(s, sourcePrefixesKeyC, sourcePrefixes, def.sourcePrefixes);
  QtcSettings::setValueWithDefault(s, headerSuffixKeyC, headerSuffix, def.headerSuffix);
  QtcSettings::setValueWithDefault(s, sourceSuffixKeyC, sourceSuffix, def.sourceSuffix);
  QtcSettings::setValueWithDefault(s, headerSearchPathsKeyC, headerSearchPaths, def.headerSearchPaths);
  QtcSettings::setValueWithDefault(s, sourceSearchPathsKeyC, sourceSearchPaths, def.sourceSearchPaths);
  QtcSettings::setValueWithDefault(s, Constants::LOWERCASE_CPPFILES_KEY, lowerCaseFiles, def.lowerCaseFiles);
  QtcSettings::setValueWithDefault(s, headerPragmaOnceC, headerPragmaOnce, def.headerPragmaOnce);
  QtcSettings::setValueWithDefault(s, licenseTemplatePathKeyC, licenseTemplatePath, def.licenseTemplatePath);
  s->endGroup();
}

auto CppFileSettings::fromSettings(QSettings *s) -> void
{
  const CppFileSettings def;
  s->beginGroup(Constants::CPPEDITOR_SETTINGSGROUP);
  headerPrefixes = s->value(headerPrefixesKeyC, def.headerPrefixes).toStringList();
  sourcePrefixes = s->value(sourcePrefixesKeyC, def.sourcePrefixes).toStringList();
  headerSuffix = s->value(headerSuffixKeyC, def.headerSuffix).toString();
  sourceSuffix = s->value(sourceSuffixKeyC, def.sourceSuffix).toString();
  headerSearchPaths = s->value(headerSearchPathsKeyC, def.headerSearchPaths).toStringList();
  sourceSearchPaths = s->value(sourceSearchPathsKeyC, def.sourceSearchPaths).toStringList();
  lowerCaseFiles = s->value(Constants::LOWERCASE_CPPFILES_KEY, def.lowerCaseFiles).toBool();
  headerPragmaOnce = s->value(headerPragmaOnceC, def.headerPragmaOnce).toBool();
  licenseTemplatePath = s->value(licenseTemplatePathKeyC, def.licenseTemplatePath).toString();
  s->endGroup();
}

auto CppFileSettings::applySuffixesToMimeDB() -> bool
{
  Utils::MimeType mt;
  mt = Utils::mimeTypeForName(QLatin1String(Constants::CPP_SOURCE_MIMETYPE));
  if (!mt.isValid())
    return false;
  mt.setPreferredSuffix(sourceSuffix);
  mt = Utils::mimeTypeForName(QLatin1String(Constants::CPP_HEADER_MIMETYPE));
  if (!mt.isValid())
    return false;
  mt.setPreferredSuffix(headerSuffix);
  return true;
}

auto CppFileSettings::equals(const CppFileSettings &rhs) const -> bool
{
  return lowerCaseFiles == rhs.lowerCaseFiles && headerPragmaOnce == rhs.headerPragmaOnce && headerPrefixes == rhs.headerPrefixes && sourcePrefixes == rhs.sourcePrefixes && headerSuffix == rhs.headerSuffix && sourceSuffix == rhs.sourceSuffix && headerSearchPaths == rhs.headerSearchPaths && sourceSearchPaths == rhs.sourceSearchPaths && licenseTemplatePath == rhs.licenseTemplatePath;
}

// Replacements of special license template keywords.
static auto keyWordReplacement(const QString &keyWord, QString *value) -> bool
{
  if (keyWord == QLatin1String("%YEAR%")) {
    *value = QLatin1String("%{CurrentDate:yyyy}");
    return true;
  }
  if (keyWord == QLatin1String("%MONTH%")) {
    *value = QLatin1String("%{CurrentDate:M}");
    return true;
  }
  if (keyWord == QLatin1String("%DAY%")) {
    *value = QLatin1String("%{CurrentDate:d}");
    return true;
  }
  if (keyWord == QLatin1String("%CLASS%")) {
    *value = QLatin1String("%{Cpp:License:ClassName}");
    return true;
  }
  if (keyWord == QLatin1String("%FILENAME%")) {
    *value = QLatin1String("%{Cpp:License:FileName}");
    return true;
  }
  if (keyWord == QLatin1String("%DATE%")) {
    static QString format;
    // ensure a format with 4 year digits. Some have locales have 2.
    if (format.isEmpty()) {
      QLocale loc;
      format = loc.dateFormat(QLocale::ShortFormat);
      const QChar ypsilon = QLatin1Char('y');
      if (format.count(ypsilon) == 2)
        format.insert(format.indexOf(ypsilon), QString(2, ypsilon));
      format.replace('/', "\\/");
    }
    *value = QString::fromLatin1("%{CurrentDate:") + format + QLatin1Char('}');
    return true;
  }
  if (keyWord == QLatin1String("%USER%")) {
    *value = Utils::HostOsInfo::isWindowsHost() ? QLatin1String("%{Env:USERNAME}") : QLatin1String("%{Env:USER}");
    return true;
  }
  // Environment variables (for example '%$EMAIL%').
  if (keyWord.startsWith(QLatin1String("%$"))) {
    const auto varName = keyWord.mid(2, keyWord.size() - 3);
    *value = QString::fromLatin1("%{Env:") + varName + QLatin1Char('}');
    return true;
  }
  return false;
}

// Parse a license template, scan for %KEYWORD% and replace if known.
// Replace '%%' by '%'.
static auto parseLicenseTemplatePlaceholders(QString *t) -> void
{
  auto pos = 0;
  const QChar placeHolder = QLatin1Char('%');
  do {
    const int placeHolderPos = t->indexOf(placeHolder, pos);
    if (placeHolderPos == -1)
      break;
    const int endPlaceHolderPos = t->indexOf(placeHolder, placeHolderPos + 1);
    if (endPlaceHolderPos == -1)
      break;
    if (endPlaceHolderPos == placeHolderPos + 1) {
      // '%%' -> '%'
      t->remove(placeHolderPos, 1);
      pos = placeHolderPos + 1;
    } else {
      const auto keyWord = t->mid(placeHolderPos, endPlaceHolderPos + 1 - placeHolderPos);
      QString replacement;
      if (keyWordReplacement(keyWord, &replacement)) {
        t->replace(placeHolderPos, keyWord.size(), replacement);
        pos = placeHolderPos + replacement.size();
      } else {
        // Leave invalid keywords as is.
        pos = endPlaceHolderPos + 1;
      }
    }
  } while (pos < t->size());
}

// Convenience that returns the formatted license template.
auto CppFileSettings::licenseTemplate() -> QString
{
  const QSettings *s = Orca::Plugin::Core::ICore::settings();
  QString key = QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP);
  key += QLatin1Char('/');
  key += QLatin1String(licenseTemplatePathKeyC);
  const auto path = s->value(key, QString()).toString();
  if (path.isEmpty())
    return QString();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning("Unable to open the license template %s: %s", qPrintable(path), qPrintable(file.errorString()));
    return QString();
  }

  QTextStream licenseStream(&file);
  licenseStream.setAutoDetectUnicode(true);
  auto license = licenseStream.readAll();

  parseLicenseTemplatePlaceholders(&license);

  // Ensure at least one newline at the end of the license template to separate it from the code
  const QChar newLine = QLatin1Char('\n');
  if (!license.endsWith(newLine))
    license += newLine;

  return license;
}

// ------------------ CppFileSettingsWidget

class CppFileSettingsWidget final : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(CppEditor::Internal::CppFileSettingsWidget)
public:
  explicit CppFileSettingsWidget(CppFileSettings *settings);

  auto apply() -> void final;

  auto setSettings(const CppFileSettings &s) -> void;

private:
  auto slotEdit() -> void;
  auto licenseTemplatePath() const -> FilePath;
  auto setLicenseTemplatePath(const FilePath &) -> void;

  Ui::CppFileSettingsPage m_ui;
  CppFileSettings *m_settings = nullptr;
};

CppFileSettingsWidget::CppFileSettingsWidget(CppFileSettings *settings) : m_settings(settings)
{
  m_ui.setupUi(this);
  // populate suffix combos
  const auto sourceMt = Utils::mimeTypeForName(QLatin1String(Constants::CPP_SOURCE_MIMETYPE));
  if (sourceMt.isValid()) {
    foreach(const QString &suffix, sourceMt.suffixes())
      m_ui.sourceSuffixComboBox->addItem(suffix);
  }

  const auto headerMt = Utils::mimeTypeForName(QLatin1String(Constants::CPP_HEADER_MIMETYPE));
  if (headerMt.isValid()) {
    foreach(const QString &suffix, headerMt.suffixes())
      m_ui.headerSuffixComboBox->addItem(suffix);
  }
  m_ui.licenseTemplatePathChooser->setExpectedKind(Utils::PathChooser::File);
  m_ui.licenseTemplatePathChooser->setHistoryCompleter(QLatin1String("Cpp.LicenseTemplate.History"));
  m_ui.licenseTemplatePathChooser->addButton(tr("Edit..."), this, [this] { slotEdit(); });

  setSettings(*m_settings);
}

auto CppFileSettingsWidget::licenseTemplatePath() const -> FilePath
{
  return m_ui.licenseTemplatePathChooser->filePath();
}

auto CppFileSettingsWidget::setLicenseTemplatePath(const FilePath &lp) -> void
{
  m_ui.licenseTemplatePathChooser->setFilePath(lp);
}

static auto trimmedPaths(const QString &paths) -> QStringList
{
  QStringList res;
  foreach(const QString &path, paths.split(QLatin1Char(','), Qt::SkipEmptyParts))
    res << path.trimmed();
  return res;
}

auto CppFileSettingsWidget::apply() -> void
{
  CppFileSettings rc;
  rc.lowerCaseFiles = m_ui.lowerCaseFileNamesCheckBox->isChecked();
  rc.headerPragmaOnce = m_ui.headerPragmaOnceCheckBox->isChecked();
  rc.headerPrefixes = trimmedPaths(m_ui.headerPrefixesEdit->text());
  rc.sourcePrefixes = trimmedPaths(m_ui.sourcePrefixesEdit->text());
  rc.headerSuffix = m_ui.headerSuffixComboBox->currentText();
  rc.sourceSuffix = m_ui.sourceSuffixComboBox->currentText();
  rc.headerSearchPaths = trimmedPaths(m_ui.headerSearchPathsEdit->text());
  rc.sourceSearchPaths = trimmedPaths(m_ui.sourceSearchPathsEdit->text());
  rc.licenseTemplatePath = licenseTemplatePath().toString();

  if (rc == *m_settings)
    return;

  *m_settings = rc;
  m_settings->toSettings(Orca::Plugin::Core::ICore::settings());
  m_settings->applySuffixesToMimeDB();
  CppEditorPlugin::clearHeaderSourceCache();
}

static inline auto setComboText(QComboBox *cb, const QString &text, int defaultIndex = 0) -> void
{
  const int index = cb->findText(text);
  cb->setCurrentIndex(index == -1 ? defaultIndex : index);
}

auto CppFileSettingsWidget::setSettings(const CppFileSettings &s) -> void
{
  const QChar comma = QLatin1Char(',');
  m_ui.lowerCaseFileNamesCheckBox->setChecked(s.lowerCaseFiles);
  m_ui.headerPragmaOnceCheckBox->setChecked(s.headerPragmaOnce);
  m_ui.headerPrefixesEdit->setText(s.headerPrefixes.join(comma));
  m_ui.sourcePrefixesEdit->setText(s.sourcePrefixes.join(comma));
  setComboText(m_ui.headerSuffixComboBox, s.headerSuffix);
  setComboText(m_ui.sourceSuffixComboBox, s.sourceSuffix);
  m_ui.headerSearchPathsEdit->setText(s.headerSearchPaths.join(comma));
  m_ui.sourceSearchPathsEdit->setText(s.sourceSearchPaths.join(comma));
  setLicenseTemplatePath(FilePath::fromString(s.licenseTemplatePath));
}

auto CppFileSettingsWidget::slotEdit() -> void
{
  auto path = licenseTemplatePath();
  if (path.isEmpty()) {
    // Pick a file name and write new template, edit with C++
    path = FileUtils::getSaveFilePath(this, tr("Choose Location for New License Template File"));
    if (path.isEmpty())
      return;
    FileSaver saver(path, QIODevice::Text);
    saver.write(tr(licenseTemplateTemplate).arg(Orca::Plugin::Core::IDE_DISPLAY_NAME).toUtf8());
    if (!saver.finalize(this))
      return;
    setLicenseTemplatePath(path);
  }
  // Edit (now) existing file with C++
  Orca::Plugin::Core::EditorManager::openEditor(path, CppEditor::Constants::CPPEDITOR_ID);
}

// --------------- CppFileSettingsPage

CppFileSettingsPage::CppFileSettingsPage(CppFileSettings *settings)
{
  setId(Constants::CPP_FILE_SETTINGS_ID);
  setDisplayName(QCoreApplication::translate("CppEditor", Constants::CPP_FILE_SETTINGS_NAME));
  setCategory(Constants::CPP_SETTINGS_CATEGORY);
  setWidgetCreator([settings] { return new CppFileSettingsWidget(settings); });
}

} // namespace CppEditor::Internal
