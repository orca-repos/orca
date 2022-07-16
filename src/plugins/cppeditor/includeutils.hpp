// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <cplusplus/CppDocument.h>
#include <cplusplus/PreprocessorClient.h>

#include <QList>
#include <QObject>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QTextDocument)

namespace CppEditor {
namespace IncludeUtils {

using Include = CPlusPlus::Document::Include;
using IncludeType = CPlusPlus::Client::IncludeType;

class IncludeGroup {
public:
  static auto detectIncludeGroupsByNewLines(QList<Include> &includes) -> QList<IncludeGroup>;
  static auto detectIncludeGroupsByIncludeDir(const QList<Include> &includes) -> QList<IncludeGroup>;
  static auto detectIncludeGroupsByIncludeType(const QList<Include> &includes) -> QList<IncludeGroup>;
  static auto filterMixedIncludeGroups(const QList<IncludeGroup> &groups) -> QList<IncludeGroup>;
  static auto filterIncludeGroups(const QList<IncludeGroup> &groups, CPlusPlus::Client::IncludeType includeType) -> QList<IncludeGroup>;
  
  explicit IncludeGroup(const QList<Include> &includes) : m_includes(includes) {}

  auto includes() const -> QList<Include> { return m_includes; }
  auto first() const -> Include { return m_includes.first(); }
  auto last() const -> Include { return m_includes.last(); }
  auto size() const -> int { return m_includes.size(); }
  auto isEmpty() const -> bool { return m_includes.isEmpty(); }
  auto commonPrefix() const -> QString;
  auto commonIncludeDir() const -> QString; /// only valid if hasCommonDir() == true
  auto hasCommonIncludeDir() const -> bool;
  auto hasOnlyIncludesOfType(CPlusPlus::Client::IncludeType includeType) const -> bool;
  auto isSorted() const -> bool; /// name-wise
  auto lineForNewInclude(const QString &newIncludeFileName, CPlusPlus::Client::IncludeType newIncludeType) const -> int;

private:
  auto filesNames() const -> QStringList;

  QList<Include> m_includes;
};

class LineForNewIncludeDirective {
public:
  enum MocIncludeMode {
    RespectMocIncludes,
    IgnoreMocIncludes
  };

  enum IncludeStyle {
    LocalBeforeGlobal,
    GlobalBeforeLocal,
    AutoDetect
  };

  LineForNewIncludeDirective(const QTextDocument *textDocument, const CPlusPlus::Document::Ptr cppDocument, MocIncludeMode mocIncludeMode = IgnoreMocIncludes, IncludeStyle includeStyle = AutoDetect);

  /// Returns the line (1-based) at which the include directive should be inserted.
  /// On error, -1 is returned.
  auto operator()(const QString &newIncludeFileName, unsigned *newLinesToPrepend = nullptr, unsigned *newLinesToAppend = nullptr) -> int;

private:
  auto findInsertLineForVeryFirstInclude(unsigned *newLinesToPrepend, unsigned *newLinesToAppend) -> int;
  auto getGroupsByIncludeType(const QList<IncludeGroup> &groups, IncludeType includeType) -> QList<IncludeGroup>;

  const QTextDocument *m_textDocument;
  const CPlusPlus::Document::Ptr m_cppDocument;

  IncludeStyle m_includeStyle;
  QList<Include> m_includes;
};

} // namespace IncludeUtils

#ifdef WITH_TESTS
namespace Internal {
class IncludeGroupsTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectIncludeGroupsByNewLines();
    void testDetectIncludeGroupsByIncludeDir();
    void testDetectIncludeGroupsByIncludeType();
};
} // namespace Internal
#endif // WITH_TESTS

} // namespace CppEditor
