// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "plugingenerator.hpp"
#include "pluginoptions.hpp"

#include <core/core-generated-file.hpp>
#include <cppeditor/abstracteditorsupport.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/templateengine.hpp>

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSet>

static auto headerGuard(const QString &header) -> QString
{
    return header.toUpper().replace(QRegularExpression("[^A-Z0-9]+"), QString("_"));
}

namespace QmakeProjectManager {
namespace Internal {

struct ProjectContents {
  QString tmpl;
  QString library;
  QString headers;
  QString sources;
};

// Create a binary icon file
static inline auto generateIconFile(const Utils::FilePath &source, const QString &target, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFile
{
  // Read out source
  Utils::FileReader reader;
  if (!reader.fetch(source, errorMessage))
    return Orca::Plugin::Core::GeneratedFile();
  Orca::Plugin::Core::GeneratedFile rc(target);
  rc.setBinaryContents(reader.data());
  rc.setBinary(true);
  return rc;
}

static auto qt4PluginExport(const QString &pluginName, const QString &pluginClassName) -> QString
{
  return QLatin1String("#if QT_VERSION < 0x050000\nQ_EXPORT_PLUGIN2(") + pluginName + QLatin1String(", ") + pluginClassName + QLatin1String(")\n#endif // QT_VERSION < 0x050000");
}

static auto qt5PluginMetaData(const QString &interfaceName) -> QString
{
  return QLatin1String("#if QT_VERSION >= 0x050000\n    Q_PLUGIN_METADATA(IID \"org.qt-project.Qt.") + interfaceName + QLatin1String("\")\n#endif // QT_VERSION >= 0x050000");
}

auto PluginGenerator::generatePlugin(const GenerationParameters &p, const PluginOptions &options, QString *errorMessage) -> QList<Orca::Plugin::Core::GeneratedFile>
{
  const QChar slash = QLatin1Char('/');
  const QChar blank = QLatin1Char(' ');
  QList<Orca::Plugin::Core::GeneratedFile> rc;

  auto baseDir = p.path;
  baseDir += slash;
  baseDir += p.fileName;
  const auto slashLessBaseDir = baseDir;
  baseDir += slash;

  QSet<QString> widgetLibraries;
  QSet<QString> widgetProjects;
  QMap<QString, ProjectContents> widgetProjectContents;
  QString pluginIncludes;
  QString pluginAdditions;
  QString pluginHeaders;
  QString pluginSources;
  QSet<QString> pluginIcons;

  SubstitutionMap sm;

  // First create the widget wrappers (plugins) and - if requested - skeletons
  // for the widgets.
  const int widgetCount = options.widgetOptions.size();
  for (auto i = 0; i < widgetCount; i++) {
    const auto &wo = options.widgetOptions.at(i);
    sm.clear();
    sm.insert(QLatin1String("SINGLE_INCLUDE_GUARD"), headerGuard(wo.pluginHeaderFile));
    sm.insert(QLatin1String("PLUGIN_CLASS"), wo.pluginClassName);
    sm.insert(QLatin1String("SINGLE_PLUGIN_METADATA"), options.widgetOptions.count() == 1 ? qt5PluginMetaData(QLatin1String("QDesignerCustomWidgetInterface")) : QString());
    const auto pluginHeaderContents = processTemplate(p.templatePath + QLatin1String("/tpl_single.hpp"), sm, errorMessage);
    if (pluginHeaderContents.isEmpty())
      return QList<Orca::Plugin::Core::GeneratedFile>();
    Orca::Plugin::Core::GeneratedFile pluginHeader(baseDir + wo.pluginHeaderFile);
    pluginHeader.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(wo.pluginHeaderFile, wo.pluginClassName) + pluginHeaderContents);
    rc.push_back(pluginHeader);

    sm.remove(QLatin1String("SINGLE_INCLUDE_GUARD"));
    sm.insert(QLatin1String("PLUGIN_HEADER"), wo.pluginHeaderFile);
    sm.insert(QLatin1String("WIDGET_CLASS"), wo.widgetClassName);
    sm.insert(QLatin1String("WIDGET_HEADER"), wo.widgetHeaderFile);
    sm.insert(QLatin1String("WIDGET_GROUP"), wo.group);
    QString iconResource;
    if (!wo.iconFile.isEmpty()) {
      iconResource = QLatin1String("QLatin1String(\":/");
      iconResource += Utils::FilePath::fromString(wo.iconFile).fileName();
      iconResource += QLatin1String("\")");
    }
    sm.insert(QLatin1String("WIDGET_ICON"), iconResource);
    sm.insert(QLatin1String("WIDGET_TOOLTIP"), cStringQuote(wo.toolTip));
    sm.insert(QLatin1String("WIDGET_WHATSTHIS"), cStringQuote(wo.whatsThis));
    sm.insert(QLatin1String("WIDGET_ISCONTAINER"), wo.isContainer ? QLatin1String("true") : QLatin1String("false"));
    sm.insert(QLatin1String("WIDGET_DOMXML"), cStringQuote(wo.domXml));
    sm.insert(QLatin1String("SINGLE_PLUGIN_EXPORT"), options.widgetOptions.count() == 1 ? qt4PluginExport(options.pluginName, wo.pluginClassName) : QString());

    const auto pluginSourceContents = processTemplate(p.templatePath + QLatin1String("/tpl_single.cpp"), sm, errorMessage);
    if (pluginSourceContents.isEmpty())
      return QList<Orca::Plugin::Core::GeneratedFile>();
    Orca::Plugin::Core::GeneratedFile pluginSource(baseDir + wo.pluginSourceFile);
    pluginSource.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(wo.pluginSourceFile, wo.pluginClassName) + pluginSourceContents);
    if (i == 0 && widgetCount == 1) // Open first widget unless collection
      pluginSource.setAttributes(Orca::Plugin::Core::GeneratedFile::OpenEditorAttribute);
    rc.push_back(pluginSource);

    if (wo.sourceType == PluginOptions::WidgetOptions::LinkLibrary)
      widgetLibraries.insert(QLatin1String("-l") + wo.widgetLibrary);
    else
      widgetProjects.insert(QLatin1String("include(") + wo.widgetProjectFile + QLatin1Char(')'));
    pluginIncludes += QLatin1String("#include \"") + wo.pluginHeaderFile + QLatin1String("\"\n");
    pluginAdditions += QLatin1String("    m_widgets.append(new ") + wo.pluginClassName + QLatin1String("(this));\n");
    pluginHeaders += QLatin1Char(' ') + wo.pluginHeaderFile;
    pluginSources += QLatin1Char(' ') + wo.pluginSourceFile;
    if (!wo.iconFile.isEmpty())
      pluginIcons.insert(wo.iconFile);

    if (wo.createSkeleton) {
      auto &pc = widgetProjectContents[wo.widgetProjectFile];
      if (pc.headers.isEmpty()) {
        if (wo.sourceType == PluginOptions::WidgetOptions::LinkLibrary) {
          pc.library = wo.widgetLibrary;
          pc.tmpl = p.templatePath + QLatin1String("/tpl_widget_lib.pro");
        } else {
          pc.tmpl = p.templatePath + QLatin1String("/tpl_widget_include.pri");
        }
        widgetProjectContents.insert(wo.widgetProjectFile, pc);
      } else {
        if (pc.library != wo.widgetLibrary) {
          *errorMessage = tr("Creating multiple widget libraries (%1, %2) in one project (%3) is not supported.").arg(pc.library, wo.widgetLibrary, wo.widgetProjectFile);
          return QList<Orca::Plugin::Core::GeneratedFile>();
        }
      }
      pc.headers += blank + wo.widgetHeaderFile;
      pc.sources += blank + wo.widgetSourceFile;

      sm.clear();
      sm.insert(QLatin1String("WIDGET_INCLUDE_GUARD"), headerGuard(wo.widgetHeaderFile));
      sm.insert(QLatin1String("WIDGET_BASE_CLASS"), wo.widgetBaseClassName);
      sm.insert(QLatin1String("WIDGET_CLASS"), wo.widgetClassName);
      const auto widgetHeaderContents = processTemplate(p.templatePath + QLatin1String("/tpl_widget.hpp"), sm, errorMessage);
      if (widgetHeaderContents.isEmpty())
        return QList<Orca::Plugin::Core::GeneratedFile>();
      Orca::Plugin::Core::GeneratedFile widgetHeader(baseDir + wo.widgetHeaderFile);
      widgetHeader.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(wo.widgetHeaderFile, wo.widgetClassName) + widgetHeaderContents);
      rc.push_back(widgetHeader);

      sm.remove(QLatin1String("WIDGET_INCLUDE_GUARD"));
      sm.insert(QLatin1String("WIDGET_HEADER"), wo.widgetHeaderFile);
      const auto widgetSourceContents = processTemplate(p.templatePath + QLatin1String("/tpl_widget.cpp"), sm, errorMessage);
      if (widgetSourceContents.isEmpty())
        return QList<Orca::Plugin::Core::GeneratedFile>();
      Orca::Plugin::Core::GeneratedFile widgetSource(baseDir + wo.widgetSourceFile);
      widgetSource.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(wo.widgetSourceFile, wo.widgetClassName) + widgetSourceContents);
      rc.push_back(widgetSource);
    }
  }

  // Then create the project files for the widget skeletons.
  // These might create widgetLibraries or be included into the plugin's project.
  auto it = widgetProjectContents.constBegin();
  const auto end = widgetProjectContents.constEnd();
  for (; it != end; ++it) {
    const auto &pc = it.value();
    sm.clear();
    sm.insert(QLatin1String("WIDGET_HEADERS"), pc.headers);
    sm.insert(QLatin1String("WIDGET_SOURCES"), pc.sources);
    if (!pc.library.isEmpty())
      sm.insert(QLatin1String("WIDGET_LIBRARY"), pc.library);
    const auto widgetPriContents = processTemplate(pc.tmpl, sm, errorMessage);
    if (widgetPriContents.isEmpty())
      return QList<Orca::Plugin::Core::GeneratedFile>();
    Orca::Plugin::Core::GeneratedFile widgetPri(baseDir + it.key());
    widgetPri.setContents(widgetPriContents);
    rc.push_back(widgetPri);
  }

  // Create the sources for the collection if necessary.
  if (widgetCount > 1) {
    sm.clear();
    sm.insert(QLatin1String("COLLECTION_INCLUDE_GUARD"), headerGuard(options.collectionHeaderFile));
    sm.insert(QLatin1String("COLLECTION_PLUGIN_CLASS"), options.collectionClassName);
    sm.insert(QLatin1String("COLLECTION_PLUGIN_METADATA"), qt5PluginMetaData(QLatin1String("QDesignerCustomWidgetCollectionInterface")));
    const auto collectionHeaderContents = processTemplate(p.templatePath + QLatin1String("/tpl_collection.hpp"), sm, errorMessage);
    if (collectionHeaderContents.isEmpty())
      return QList<Orca::Plugin::Core::GeneratedFile>();
    Orca::Plugin::Core::GeneratedFile collectionHeader(baseDir + options.collectionHeaderFile);
    collectionHeader.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(options.collectionHeaderFile, options.collectionClassName) + collectionHeaderContents);
    rc.push_back(collectionHeader);

    sm.remove(QLatin1String("COLLECTION_INCLUDE_GUARD"));
    sm.insert(QLatin1String("PLUGIN_INCLUDES"), pluginIncludes + QLatin1String("#include \"") + options.collectionHeaderFile + QLatin1String("\""));
    sm.insert(QLatin1String("PLUGIN_ADDITIONS"), pluginAdditions);
    sm.insert(QLatin1String("COLLECTION_PLUGIN_EXPORT"), qt4PluginExport(options.pluginName, options.collectionClassName));
    const auto collectionSourceFileContents = processTemplate(p.templatePath + QLatin1String("/tpl_collection.cpp"), sm, errorMessage);
    if (collectionSourceFileContents.isEmpty())
      return QList<Orca::Plugin::Core::GeneratedFile>();
    Orca::Plugin::Core::GeneratedFile collectionSource(baseDir + options.collectionSourceFile);
    collectionSource.setContents(CppEditor::AbstractEditorSupport::licenseTemplate(options.collectionSourceFile, options.collectionClassName) + collectionSourceFileContents);
    collectionSource.setAttributes(Orca::Plugin::Core::GeneratedFile::OpenEditorAttribute);
    rc.push_back(collectionSource);

    pluginHeaders += blank + options.collectionHeaderFile;
    pluginSources += blank + options.collectionSourceFile;
  }

  // Copy icons that are not in the plugin source base directory yet (that is,
  // probably all), add them to the resource file
  QString iconFiles;
  foreach(QString icon, pluginIcons) {
    const QFileInfo qfi(icon);
    if (qfi.dir() != slashLessBaseDir) {
      const QString newIcon = baseDir + qfi.fileName();
      const auto iconFile = generateIconFile(Utils::FilePath::fromFileInfo(qfi), newIcon, errorMessage);
      if (iconFile.path().isEmpty())
        return QList<Orca::Plugin::Core::GeneratedFile>();
      rc.push_back(iconFile);
      icon = qfi.fileName();
    }
    iconFiles += QLatin1String("        <file>") + icon + QLatin1String("</file>\n");
  }
  // Create the resource file with the icons.
  sm.clear();
  sm.insert(QLatin1String("ICON_FILES"), iconFiles);
  const auto resourceFileContents = processTemplate(p.templatePath + QLatin1String("/tpl_resources.qrc"), sm, errorMessage);
  if (resourceFileContents.isEmpty())
    return QList<Orca::Plugin::Core::GeneratedFile>();
  Orca::Plugin::Core::GeneratedFile resourceFile(baseDir + options.resourceFile);
  resourceFile.setContents(resourceFileContents);
  rc.push_back(resourceFile);

  // Finally create the project for the plugin itself.
  sm.clear();
  sm.insert(QLatin1String("PLUGIN_NAME"), options.pluginName);
  sm.insert(QLatin1String("PLUGIN_HEADERS"), pluginHeaders);
  sm.insert(QLatin1String("PLUGIN_SOURCES"), pluginSources);
  sm.insert(QLatin1String("PLUGIN_RESOURCES"), options.resourceFile);
  sm.insert(QLatin1String("WIDGET_LIBS"), QStringList(Utils::toList(widgetLibraries)).join(blank));
  sm.insert(QLatin1String("INCLUSIONS"), QStringList(Utils::toList(widgetProjects)).join(QLatin1Char('\n')));
  const auto proFileContents = processTemplate(p.templatePath + QLatin1String("/tpl_plugin.pro"), sm, errorMessage);
  if (proFileContents.isEmpty())
    return QList<Orca::Plugin::Core::GeneratedFile>();
  Orca::Plugin::Core::GeneratedFile proFile(baseDir + p.fileName + QLatin1String(".pro"));
  proFile.setContents(proFileContents);
  proFile.setAttributes(Orca::Plugin::Core::GeneratedFile::OpenProjectAttribute);
  rc.push_back(proFile);
  return rc;
}

auto PluginGenerator::processTemplate(const QString &tmpl, const SubstitutionMap &substMap, QString *errorMessage) -> QString
{
  Utils::FileReader reader;
  if (!reader.fetch(Utils::FilePath::fromString(tmpl), errorMessage))
    return QString();

  auto cont = QString::fromUtf8(reader.data());

  // Expander needed to handle extra variable "Cpp:PragmaOnce"
  auto expander = Utils::globalMacroExpander();
  QString errMsg;
  cont = Utils::TemplateEngine::processText(expander, cont, &errMsg);
  if (!errMsg.isEmpty()) {
    qWarning("Error processing custom plugin file: %s\nFile:\n%s", qPrintable(errMsg), qPrintable(cont));
    errorMessage = &errMsg;
    return QString();
  }

  const QChar atChar = QLatin1Char('@');
  auto offset = 0;
  for (;;) {
    const int start = cont.indexOf(atChar, offset);
    if (start < 0)
      break;
    const int end = cont.indexOf(atChar, start + 1);
    Q_ASSERT(end);
    const auto keyword = cont.mid(start + 1, end - start - 1);
    const auto replacement = substMap.value(keyword);
    cont.replace(start, end - start + 1, replacement);
    offset = start + replacement.length();
  }
  return cont;
}

auto PluginGenerator::cStringQuote(QString s) -> QString
{
  s.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
  s.replace(QLatin1Char('"'), QLatin1String("\\\""));
  s.replace(QLatin1Char('\t'), QLatin1String("\\t"));
  s.replace(QLatin1Char('\n'), QLatin1String("\\n"));
  return s;
}

} // namespace Internal
} // namespace CppTools
