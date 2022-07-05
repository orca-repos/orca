// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "htmldocextractor.h"

#include <QStringList>
#include <QRegularExpression>

namespace Utils {

HtmlDocExtractor::HtmlDocExtractor() = default;

auto HtmlDocExtractor::setMode(Mode mode) -> void { m_mode = mode; }

auto HtmlDocExtractor::applyFormatting(const bool format) -> void { m_formatContents = format; }

auto HtmlDocExtractor::getClassOrNamespaceBrief(const QString &html, const QString &mark) const -> QString
{
  QString contents = getContentsByMarks(html, mark + QLatin1String("-brief"), mark);
  if (!contents.isEmpty() && m_formatContents)
    contents.remove(QLatin1String("<a href=\"#details\">More...</a>"));
  processOutput(&contents);

  return contents;
}

auto HtmlDocExtractor::getClassOrNamespaceDescription(const QString &html, const QString &mark) const -> QString
{
  if (m_mode == FirstParagraph)
    return getClassOrNamespaceBrief(html, mark);

  QString contents = getContentsByMarks(html, mark + QLatin1String("-description"), mark);
  if (!contents.isEmpty() && m_formatContents)
    contents.remove(QLatin1String("Detailed Description"));
  processOutput(&contents);

  return contents;
}

auto HtmlDocExtractor::getEnumDescription(const QString &html, const QString &mark) const -> QString
{
  return getClassOrNamespaceMemberDescription(html, mark, mark);
}

auto HtmlDocExtractor::getTypedefDescription(const QString &html, const QString &mark) const -> QString
{
  return getClassOrNamespaceMemberDescription(html, mark, mark);
}

auto HtmlDocExtractor::getMacroDescription(const QString &html, const QString &mark) const -> QString
{
  return getClassOrNamespaceMemberDescription(html, mark, mark);
}

auto HtmlDocExtractor::getFunctionDescription(const QString &html, const QString &mark, const bool mainOverload) const -> QString
{
  QString cleanMark = mark;
  QString startMark = mark;
  const int parenthesis = mark.indexOf(QLatin1Char('('));
  if (parenthesis != -1) {
    startMark = mark.left(parenthesis);
    cleanMark = startMark;
    if (mainOverload) {
      startMark.append(QLatin1String("[overload1]"));
    } else {
      QString complement = mark.right(mark.length() - parenthesis);
      complement.remove(QRegularExpression("[\\(\\), ]"));
      startMark.append(complement);
    }
  }

  QString contents = getClassOrNamespaceMemberDescription(html, startMark, cleanMark);
  if (contents.isEmpty()) {
    // Maybe this is a property function, which is documented differently. Besides
    // setX/isX/hasX there are other (not so usual) names for them. A few examples of those:
    //   - toPlainText / Prop. plainText from QPlainTextEdit.
    //   - resize / Prop. size from QWidget.
    //   - move / Prop. pos from QWidget (nothing similar in the names in this case).
    // So I try to find the link to this property in the list of properties, extract its
    // anchor and then follow by the name found.
    const QString &pattern = QString("<a href=\"[a-z\\.]+?#([A-Za-z]+?)-prop\">%1</a>").arg(cleanMark);
    const QRegularExpressionMatch match = QRegularExpression(pattern).match(html);
    if (match.hasMatch()) {
      const QString &prop = match.captured(1);
      contents = getClassOrNamespaceMemberDescription(html, prop + QLatin1String("-prop"), prop);
    }
  }

  return contents;
}

auto HtmlDocExtractor::getQmlComponentDescription(const QString &html, const QString &mark) const -> QString
{
  return getClassOrNamespaceDescription(html, mark);
}

auto HtmlDocExtractor::getQmlPropertyDescription(const QString &html, const QString &mark) const -> QString
{
  QString startMark = QString::fromLatin1("<a name=\"%1-prop\">").arg(mark);
  int index = html.indexOf(startMark);
  if (index == -1) {
    startMark = QString::fromLatin1("<a name=\"%1-signal\">").arg(mark);
    index = html.indexOf(startMark);
  }
  if (index == -1)
    return QString();

  QString contents = html.mid(index + startMark.size());
  index = contents.indexOf(QLatin1String("<div class=\"qmldoc\"><p>"));
  if (index == -1)
    return QString();
  contents = contents.mid(index);
  processOutput(&contents);

  return contents;
}

auto HtmlDocExtractor::getQMakeVariableOrFunctionDescription(const QString &html, const QString &mark) const -> QString
{
  const QString startMark = QString::fromLatin1("<a name=\"%1\"></a>").arg(mark);
  int index = html.indexOf(startMark);
  if (index == -1)
    return QString();

  QString contents = html.mid(index + startMark.size());
  index = contents.indexOf(QLatin1String("<!-- @@@qmake"));
  if (index == -1)
    return QString();
  contents = contents.left(index);
  processOutput(&contents);

  return contents;
}

auto HtmlDocExtractor::getQMakeFunctionId(const QString &html, const QString &mark) const -> QString
{
  const QString startMark = QString::fromLatin1("<a name=\"%1-").arg(mark);
  const int startIndex = html.indexOf(startMark);
  if (startIndex == -1)
    return QString();

  const int startKeyIndex = html.indexOf(mark, startIndex);

  const QString endMark = QLatin1String("\"></a>");
  const int endKeyIndex = html.indexOf(endMark, startKeyIndex);
  if (endKeyIndex == -1)
    return QString();

  return html.mid(startKeyIndex, endKeyIndex - startKeyIndex);
}

auto HtmlDocExtractor::getClassOrNamespaceMemberDescription(const QString &html, const QString &startMark, const QString &endMark) const -> QString
{
  QString contents = getContentsByMarks(html, startMark, endMark);
  processOutput(&contents);

  return contents;
}

auto HtmlDocExtractor::getContentsByMarks(const QString &html, QString startMark, QString endMark) const -> QString
{
  startMark.prepend(QLatin1String("$$$"));
  endMark.prepend(QLatin1String("<!-- @@@"));

  QString contents;
  int start = html.indexOf(startMark);
  if (start != -1) {
    start = html.indexOf(QLatin1String("-->"), start);
    if (start != -1) {
      int end = html.indexOf(endMark, start);
      if (end != -1) {
        start += 3;
        contents = html.mid(start, end - start);
      }
    }
  }
  return contents;
}

auto HtmlDocExtractor::processOutput(QString *html) const -> void
{
  if (html->isEmpty())
    return;

  if (m_mode == FirstParagraph) {
    // Try to get the entire first paragraph, but if one is not found or if its opening
    // tag is not in the very beginning (using an empirical value as the limit) the html
    // is cleared to avoid too much content. In case the first paragraph looks like:
    // <p><i>This is only used on the Maemo platform.</i></p>
    // or: <p><tt>This is used on Windows only.</tt></p>
    // or: <p>[Conditional]</p>
    // include also the next paragraph.
    int index = html->indexOf(QLatin1String("<p>"));
    if (index != -1 && index < 400) {
      if (html->indexOf(QLatin1String("<p><i>")) == index || html->indexOf(QLatin1String("<p><tt>")) == index || html->indexOf(QLatin1String("<p>[Conditional]</p>")) == index)
        index = html->indexOf(QLatin1String("<p>"), index + 6); // skip the first paragraph

      index = html->indexOf(QLatin1String("</p>"), index + 3);
      if (index != -1) {
        // Most paragraphs end with a period, but there are cases without punctuation
        // and cases like this: <p>This is a description. Example:</p>
        const int period = html->lastIndexOf(QLatin1Char('.'), index);
        if (period != -1) {
          html->truncate(period + 1);
          html->append(QLatin1String("</p>"));
        } else {
          html->truncate(index + 4);
        }
      } else {
        html->clear();
      }
    } else {
      html->clear();
    }
  }

  if (!html->isEmpty() && m_formatContents) {
    stripBold(html);
    replaceNonStyledHeadingsForBold(html);
    replaceTablesForSimpleLines(html);
    replaceListsForSimpleLines(html);
    stripLinks(html);
    stripHorizontalLines(html);
    stripDivs(html);
    stripTagsStyles(html);
    stripHeadings(html);
    stripImagens(html);
    stripEmptyParagraphs(html);
  }
}

auto HtmlDocExtractor::stripAllHtml(QString *html) -> void
{
  html->remove(QRegularExpression("<.*?>"));
}

auto HtmlDocExtractor::stripHeadings(QString *html) -> void
{
  html->remove(QRegularExpression("<h\\d{1}.*?>|</h\\d{1}>"));
}

auto HtmlDocExtractor::stripLinks(QString *html) -> void
{
  html->remove(QRegularExpression("<a\\s.*?>|</a>"));
}

auto HtmlDocExtractor::stripHorizontalLines(QString *html) -> void
{
  html->remove(QRegularExpression("<hr\\s+/>"));
}

auto HtmlDocExtractor::stripDivs(QString *html) -> void
{
  html->remove(QRegularExpression("<div\\s.*?>|</div>|<div\\s.*?/\\s*>"));
}

auto HtmlDocExtractor::stripTagsStyles(QString *html) -> void
{
  html->replace(QRegularExpression("<(.*?\\s+)class=\".*?\">"), "<\\1>");
}

auto HtmlDocExtractor::stripTeletypes(QString *html) -> void
{
  html->remove(QLatin1String("<tt>"));
  html->remove(QLatin1String("</tt>"));
}

auto HtmlDocExtractor::stripImagens(QString *html) -> void
{
  html->remove(QRegularExpression("<img.*?>"));
}

auto HtmlDocExtractor::stripBold(QString *html) -> void
{
  html->remove(QLatin1String("<b>"));
  html->remove(QLatin1String("</b>"));
}

auto HtmlDocExtractor::stripEmptyParagraphs(QString *html) -> void
{
  html->remove(QLatin1String("<p></p>"));
}

auto HtmlDocExtractor::replaceNonStyledHeadingsForBold(QString *html) -> void
{
  const QRegularExpression hStart("<h\\d{1}>");
  const QRegularExpression hEnd("</h\\d{1}>");
  html->replace(hStart, QLatin1String("<p><b>"));
  html->replace(hEnd, QLatin1String("</b></p>"));
}

auto HtmlDocExtractor::replaceTablesForSimpleLines(QString *html) -> void
{
  html->replace(QRegularExpression("(?:<p>)?<table.*?>"), QLatin1String("<p>"));
  html->replace(QLatin1String("</table>"), QLatin1String("</p>"));
  html->remove(QRegularExpression("<thead.*?>"));
  html->remove(QLatin1String("</thead>"));
  html->remove(QRegularExpression("<tfoot.*?>"));
  html->remove(QLatin1String("</tfoot>"));
  html->remove(QRegularExpression("<tr.*?><th.*?>.*?</th></tr>"));
  html->replace(QLatin1String("</td><td"), QLatin1String("</td>&nbsp;<td"));
  html->remove(QRegularExpression("<td.*?><p>"));
  html->remove(QRegularExpression("<td.*?>"));
  html->remove(QRegularExpression("(?:</p>)?</td>"));
  html->replace(QRegularExpression("<tr.*?>"), QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;"));
  html->replace(QLatin1String("</tr>"), QLatin1String("<br />"));
}

auto HtmlDocExtractor::replaceListsForSimpleLines(QString *html) -> void
{
  html->remove(QRegularExpression("<(?:ul|ol).*?>"));
  html->remove(QRegularExpression("</(?:ul|ol)>"));
  html->replace(QLatin1String("<li>"), QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;"));
  html->replace(QLatin1String("</li>"), QLatin1String("<br />"));
}

} // namespace Utils
