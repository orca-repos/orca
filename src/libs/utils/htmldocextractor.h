// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

namespace Utils {

class ORCA_UTILS_EXPORT HtmlDocExtractor {
public:
  HtmlDocExtractor();

  enum Mode {
    FirstParagraph,
    Extended
  };

  auto setMode(Mode mode) -> void;
  auto applyFormatting(bool format) -> void;
  auto getClassOrNamespaceBrief(const QString &html, const QString &mark) const -> QString;
  auto getClassOrNamespaceDescription(const QString &html, const QString &mark) const -> QString;
  auto getEnumDescription(const QString &html, const QString &mark) const -> QString;
  auto getTypedefDescription(const QString &html, const QString &mark) const -> QString;
  auto getMacroDescription(const QString &html, const QString &mark) const -> QString;
  auto getFunctionDescription(const QString &html, const QString &mark, bool mainOverload = true) const -> QString;
  auto getQmlComponentDescription(const QString &html, const QString &mark) const -> QString;
  auto getQmlPropertyDescription(const QString &html, const QString &mark) const -> QString;
  auto getQMakeVariableOrFunctionDescription(const QString &html, const QString &mark) const -> QString;
  auto getQMakeFunctionId(const QString &html, const QString &mark) const -> QString;

private:
  auto getClassOrNamespaceMemberDescription(const QString &html, const QString &startMark, const QString &endMark) const -> QString;
  auto getContentsByMarks(const QString &html, QString startMark, QString endMark) const -> QString;
  auto processOutput(QString *html) const -> void;
  static auto stripAllHtml(QString *html) -> void;
  static auto stripHeadings(QString *html) -> void;
  static auto stripLinks(QString *html) -> void;
  static auto stripHorizontalLines(QString *html) -> void;
  static auto stripDivs(QString *html) -> void;
  static auto stripTagsStyles(QString *html) -> void;
  static auto stripTeletypes(QString *html) -> void;
  static auto stripImagens(QString *html) -> void;
  static auto stripBold(QString *html) -> void;
  static auto stripEmptyParagraphs(QString *html) -> void;
  static auto replaceNonStyledHeadingsForBold(QString *html) -> void;
  static auto replaceTablesForSimpleLines(QString *html) -> void;
  static auto replaceListsForSimpleLines(QString *html) -> void;

  bool m_formatContents = true;
  Mode m_mode = FirstParagraph;
};

} // namespace Utils
