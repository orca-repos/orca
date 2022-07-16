// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtprojectparameters.hpp"
#include <utils/codegeneration.hpp>

#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

QtProjectParameters::QtProjectParameters() = default;

auto QtProjectParameters::projectPath() const -> FilePath
{
  return path / fileName;
}

// Write out a QT module line.
static auto writeQtModulesList(QTextStream &str, const QStringList &modules, char op = '+') -> void
{
  if (const int size = modules.size()) {
    str << "QT       " << op << "= ";
    for (auto i = 0; i < size; ++i) {
      if (i)
        str << ' ';
      str << modules.at(i);
    }
    str << "\n\n";
  }
}

auto QtProjectParameters::writeProFile(QTextStream &str) const -> void
{
  auto allSelectedModules = selectedModules;
  // Handling of widgets module.
  const auto addWidgetsModule = (flags & WidgetsRequiredFlag) && qtVersionSupport != SupportQt4Only && !allSelectedModules.contains(QLatin1String("widgets"));

  const auto addConditionalPrintSupport = qtVersionSupport == SupportQt4And5 && allSelectedModules.removeAll(QLatin1String("printsupport")) > 0;

  if (addWidgetsModule && qtVersionSupport == SupportQt5Only)
    allSelectedModules.append(QLatin1String("widgets"));
  writeQtModulesList(str, allSelectedModules, '+');
  writeQtModulesList(str, deselectedModules, '-');
  if (addWidgetsModule && qtVersionSupport == SupportQt4And5)
    str << "greaterThan(QT_MAJOR_VERSION, 4): QT += widgets\n\n";
  if (addConditionalPrintSupport)
    str << "greaterThan(QT_MAJOR_VERSION, 4): QT += printsupport\n\n";

  const auto &effectiveTarget = target.isEmpty() ? fileName : target;
  if (!effectiveTarget.isEmpty())
    str << "TARGET = " << effectiveTarget << '\n';
  switch (type) {
  case ConsoleApp:
    // Mac: Command line apps should not be bundles
    str << "CONFIG   += console\nCONFIG   -= app_bundle\n\n";
  // fallthrough
  case GuiApp:
    str << "TEMPLATE = app\n";
    break;
  case StaticLibrary:
    str << "TEMPLATE = lib\nCONFIG += staticlib\n";
    break;
  case SharedLibrary:
    str << "TEMPLATE = lib\n\nDEFINES += " << libraryMacro(fileName) << '\n';
    break;
  case QtPlugin:
    str << "TEMPLATE = lib\nCONFIG += plugin\n";
    break;
  default:
    break;
  }

  if (!targetDirectory.isEmpty() && !targetDirectory.contains("QT_INSTALL_"))
    str << "\nDESTDIR = " << targetDirectory << '\n';

  if (qtVersionSupport != SupportQt4Only) {
    str << "\n" "# You can make your code fail to compile if you use deprecated APIs.\n" "# In order to do so, uncomment the following line.\n" "#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0\n";
  }
}

auto QtProjectParameters::writeProFileHeader(QTextStream &str) -> void
{
  const QChar hash = QLatin1Char('#');
  const QChar nl = QLatin1Char('\n');
  const QChar blank = QLatin1Char(' ');
  // Format as '#-------\n# <Header> \n#---------'
  QString header = QLatin1String(" Project created by ");
  header += QCoreApplication::applicationName();
  header += blank;
  header += QDateTime::currentDateTime().toString(Qt::ISODate);
  const auto line = QString(header.size(), QLatin1Char('-'));
  str << hash << line << nl << hash << nl << hash << header << nl << hash << nl << hash << line << nl << nl;
}

auto createMacro(const QString &name, const QString &suffix) -> QString
{
  auto rc = name.toUpper();
  const int extensionPosition = rc.indexOf(QLatin1Char('.'));
  if (extensionPosition != -1)
    rc.truncate(extensionPosition);
  rc += suffix;
  return Utils::fileNameToCppIdentifier(rc);
}

auto QtProjectParameters::exportMacro(const QString &projectName) -> QString
{
  return createMacro(projectName, QLatin1String("SHARED_EXPORT"));
}

auto QtProjectParameters::libraryMacro(const QString &projectName) -> QString
{
  return createMacro(projectName, QLatin1String("_LIBRARY"));
}

} // namespace Internal
} // namespace QmakeProjectManager
