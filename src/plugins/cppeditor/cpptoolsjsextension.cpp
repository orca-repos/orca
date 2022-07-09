// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpptoolsjsextension.hpp"

#include "cppfilesettingspage.hpp"
#include "cpplocatordata.hpp"
#include "cppworkingcopy.hpp"

#include <core/icore.hpp>

#include <projectexplorer/project.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/session.hpp>

#include <cplusplus/AST.h>
#include <cplusplus/ASTPath.h>
#include <cplusplus/Overview.h>
#include <utils/codegeneration.hpp>
#include <utils/fileutils.hpp>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace CppEditor::Internal {

static auto fileName(const QString &path, const QString &extension) -> QString
{
  return Utils::FilePath::fromStringWithExtension(path, extension).toString();
}

auto CppToolsJsExtension::headerGuard(const QString &in) const -> QString
{
  return Utils::headerGuard(in);
}

static auto parts(const QString &klass) -> QStringList
{
  return klass.split(QStringLiteral("::"));
}

auto CppToolsJsExtension::namespaces(const QString &klass) const -> QStringList
{
  auto result = parts(klass);
  result.removeLast();
  return result;
}

auto CppToolsJsExtension::hasNamespaces(const QString &klass) const -> bool
{
  return !namespaces(klass).empty();
}

auto CppToolsJsExtension::className(const QString &klass) const -> QString
{
  auto result = parts(klass);
  return result.last();
}

auto CppToolsJsExtension::classToFileName(const QString &klass, const QString &extension) const -> QString
{
  const auto raw = fileName(className(klass), extension);
  CppFileSettings settings;
  settings.fromSettings(Core::ICore::settings());
  if (!settings.lowerCaseFiles)
    return raw;

  auto fi = QFileInfo(raw);
  auto finalPath = fi.path();
  if (finalPath == QStringLiteral("."))
    finalPath.clear();
  if (!finalPath.isEmpty() && !finalPath.endsWith(QLatin1Char('/')))
    finalPath += QLatin1Char('/');
  auto name = fi.baseName().toLower();
  auto ext = fi.completeSuffix();
  if (!ext.isEmpty())
    ext = QString(QLatin1Char('.')) + ext;
  return finalPath + name + ext;
}

auto CppToolsJsExtension::classToHeaderGuard(const QString &klass, const QString &extension) const -> QString
{
  return Utils::headerGuard(fileName(className(klass), extension), namespaces(klass));
}

auto CppToolsJsExtension::openNamespaces(const QString &klass) const -> QString
{
  QString result;
  QTextStream str(&result);
  Utils::writeOpeningNameSpaces(namespaces(klass), QString(), str);
  return result;
}

auto CppToolsJsExtension::closeNamespaces(const QString &klass) const -> QString
{
  QString result;
  QTextStream str(&result);
  Utils::writeClosingNameSpaces(namespaces(klass), QString(), str);
  return result;
}

auto CppToolsJsExtension::hasQObjectParent(const QString &klassName) const -> bool
{
  // This is a synchronous function, but the look-up is potentially expensive.
  // Since it's not crucial information, we just abort if retrieving it takes too long,
  // in order not to freeze the UI.
  // TODO: Any chance to at least cache between successive invocations for the same dialog?
  //       I don't see it atm...
  QElapsedTimer timer;
  timer.start();
  static const auto timeout = 5000;

  // Find symbol.
  QList<IndexItem::Ptr> candidates;
  m_locatorData->filterAllFiles([&](const IndexItem::Ptr &item) {
    if (timer.elapsed() > timeout)
      return IndexItem::VisitorResult::Break;
    if (item->scopedSymbolName() == klassName) {
      candidates = {item};
      return IndexItem::VisitorResult::Break;
    }
    if (item->symbolName() == klassName)
      candidates << item;
    return IndexItem::VisitorResult::Recurse;
  });
  if (timer.elapsed() > timeout)
    return false;
  if (candidates.isEmpty())
    return false;
  const auto item = candidates.first();

  // Find class in AST.
  const CPlusPlus::Snapshot snapshot = CppModelManager::instance()->snapshot();
  const auto workingCopy = CppModelManager::instance()->workingCopy();
  auto source = workingCopy.source(item->fileName());
  if (source.isEmpty()) {
    QFile file(item->fileName());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return false;
    source = file.readAll();
  }
  const auto doc = snapshot.preprocessedDocument(source, Utils::FilePath::fromString(item->fileName()));
  if (!doc)
    return false;
  doc->check();
  if (!doc->translationUnit())
    return false;
  if (timer.elapsed() > timeout)
    return false;
  CPlusPlus::ClassSpecifierAST *classSpec = nullptr;
  const QList<CPlusPlus::AST*> astPath = CPlusPlus::ASTPath(doc)(item->line(), item->column());
  for (auto it = astPath.rbegin(); it != astPath.rend(); ++it) {
    if ((classSpec = (*it)->asClassSpecifier()))
      break;
  }
  if (!classSpec)
    return false;

  // Check whether constructor has a QObject parent parameter.
  CPlusPlus::Overview overview;
  const CPlusPlus::Class *const klass = classSpec->symbol;
  if (!klass)
    return false;
  for (auto it = klass->memberBegin(); it != klass->memberEnd(); ++it) {
    const CPlusPlus::Symbol *const member = *it;
    if (overview.prettyName(member->name()) != item->symbolName())
      continue;
    const CPlusPlus::Function *function = (*it)->asFunction();
    if (!function)
      function = member->type().type()->asFunctionType();
    if (!function)
      continue;
    for (auto i = 0; i < function->argumentCount(); ++i) {
      const CPlusPlus::Symbol *const arg = function->argumentAt(i);
      const QString argName = overview.prettyName(arg->name());
      const QString argType = overview.prettyType(arg->type()).split("::", Qt::SkipEmptyParts).last();
      if (argName == "parent" && argType == "QObject *")
        return true;
    }
  }

  return false;
}

auto CppToolsJsExtension::includeStatement(const QString &fullyQualifiedClassName, const QString &suffix, const QStringList &specialClasses, const QString &pathOfIncludingFile) -> QString
{
  if (fullyQualifiedClassName.isEmpty())
    return {};
  const auto className = parts(fullyQualifiedClassName).constLast();
  if (className.isEmpty() || specialClasses.contains(className))
    return {};
  if (className.startsWith('Q') && className.length() > 2 && className.at(1).isUpper())
    return "#include <" + className + ">\n";
  const auto withUnderScores = [&className] {
    auto baseName = className;
    baseName[0] = baseName[0].toLower();
    for (auto i = 1; i < baseName.length(); ++i) {
      if (baseName[i].isUpper()) {
        baseName.insert(i, '_');
        baseName[i + 1] = baseName[i + 1].toLower();
        ++i;
      }
    }
    return baseName;
  };
  QStringList candidates{className + '.' + suffix};
  auto hasUpperCase = false;
  auto hasLowerCase = false;
  for (auto i = 0; i < className.length() && (!hasUpperCase || !hasLowerCase); ++i) {
    if (className.at(i).isUpper())
      hasUpperCase = true;
    if (className.at(i).isLower())
      hasLowerCase = true;
  }
  if (hasUpperCase)
    candidates << className.toLower() + '.' + suffix;
  if (hasUpperCase && hasLowerCase)
    candidates << withUnderScores() + '.' + suffix;
  candidates.removeDuplicates();
  using namespace ProjectExplorer;
  const auto nodeMatchesFileName = [&candidates](Node *n) {
    if (const FileNode *const fileNode = n->asFileNode()) {
      if (fileNode->fileType() == FileType::Header && candidates.contains(fileNode->filePath().fileName())) {
        return true;
      }
    }
    return false;
  };
  for (const Project *const p : SessionManager::projects()) {
    const Node *theNode = p->rootProjectNode()->findNode(nodeMatchesFileName);
    if (theNode) {
      const auto sameDir = pathOfIncludingFile == theNode->filePath().toFileInfo().path();
      return QString("#include ").append(sameDir ? '"' : '<').append(theNode->filePath().fileName()).append(sameDir ? '"' : '>').append('\n');
    }
  }
  return {};
}

} // namespace CppEditor::Internal
